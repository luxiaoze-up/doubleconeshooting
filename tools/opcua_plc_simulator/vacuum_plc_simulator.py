import asyncio
import dataclasses
import os
import queue
import re
import argparse
import threading
import time
import traceback
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Tuple

import tkinter as tk
from tkinter import ttk

from asyncua import Server, ua
from asyncua.crypto.permission_rules import SimpleRoleRuleset, UserRole


class AnonymousReadWriteRuleset(SimpleRoleRuleset):
    """允许匿名用户进行读写（满足本机模拟 PLC 的开发/测试场景）。"""

    def __init__(self) -> None:
        super().__init__()
        # 让 Anonymous 拥有与 User 相同的权限（含 Read/Write 等）
        self._permission_dict[UserRole.Anonymous] = set(self._permission_dict[UserRole.User])


# ============================================================
# Mapping 解析：从 C++ vacuum_system_plc_mapping.h 自动提取点位
# ============================================================


@dataclass(frozen=True)
class ParsedAddress:
    name: str
    plc_type: str  # INPUT/OUTPUT/INPUT_WORD/OUTPUT_WORD
    byte_offset: int
    bit_offset: int

    @property
    def address_string(self) -> str:
        # 与 Common::PLC::PLCAddress 构造逻辑保持一致
        if self.plc_type == "INPUT_WORD":
            return f"%IW{self.byte_offset}"
        if self.plc_type == "OUTPUT_WORD":
            return f"%QW{self.byte_offset}"

        prefix = "I" if self.plc_type == "INPUT" else "Q" if self.plc_type == "OUTPUT" else "M"
        return f"%{prefix}{self.byte_offset}.{self.bit_offset}"

    @property
    def is_bool(self) -> bool:
        return self.plc_type in {"INPUT", "OUTPUT"}

    @property
    def is_word(self) -> bool:
        return self.plc_type in {"INPUT_WORD", "OUTPUT_WORD"}

    @property
    def is_output(self) -> bool:
        return self.plc_type in {"OUTPUT", "OUTPUT_WORD"}


def parse_mapping_header(mapping_h_path: str) -> List[ParsedAddress]:
    """解析 vacuum_system_plc_mapping.h，提取所有 PLCAddress(...) 返回点位。

    约束：仅解析形如：
      return PLCAddress(PLCAddressType::INPUT, 0, 0);
      return PLCAddress(PLCAddressType::INPUT_WORD, 130, -1);
    """

    with open(mapping_h_path, "r", encoding="utf-8") as f:
        text = f.read()

    # 捕获函数名 + return PLCAddress(...)
    # 示例：static PLCAddress ScrewPumpPowerFeedback() { return PLCAddress(PLCAddressType::INPUT, 0, 0); }
    pattern = re.compile(
        r"static\s+PLCAddress\s+(?P<name>\w+)\s*\(\s*\)\s*\{[^{}]*?return\s+PLCAddress\(\s*PLCAddressType::(?P<type>\w+)\s*,\s*(?P<byte>-?\d+)\s*,\s*(?P<bit>-?\d+)\s*\)\s*;",
        re.MULTILINE,
    )

    results: List[ParsedAddress] = []
    for m in pattern.finditer(text):
        name = m.group("name")
        plc_type = m.group("type")
        byte_offset = int(m.group("byte"))
        bit_offset = int(m.group("bit"))
        if plc_type not in {"INPUT", "OUTPUT", "INPUT_WORD", "OUTPUT_WORD"}:
            continue
        results.append(ParsedAddress(name=name, plc_type=plc_type, byte_offset=byte_offset, bit_offset=bit_offset))

    # 去重：同一地址可能被多个名字引用（极少见），以 address_string 去重
    dedup: Dict[str, ParsedAddress] = {}
    for a in results:
        dedup.setdefault(a.address_string, a)

    # 保持稳定顺序：按地址字符串排序
    return sorted(dedup.values(), key=lambda x: (x.plc_type, x.byte_offset, x.bit_offset, x.name))


# ============================================================
# 状态模型 + 物理仿真
# ============================================================


@dataclass
class ValveMotion:
    target_open: bool
    start_ts: float
    duration_s: float


class PLCModel:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self.bool_values: Dict[str, bool] = {}
        self.word_values: Dict[str, int] = {}

        # 物理状态（不直接暴露给 OPC-UA）
        # 约定：
        # - foreline_pressure_mbar: 前级/泵组侧压力（前级真空计）
        # - chamber_pressure_mbar: 腔体侧压力（主真空计1/2）
        # 初始为大气状态，从大气开始执行后续操作。
        self.foreline_pressure_mbar: float = 1000.0
        self.chamber_pressure_mbar: float = 1000.0
        self._valve_motion: Dict[str, ValveMotion] = {}

        # 速度参数（tau 越小变化越快）。这些参数用于满足“粗抽/精抽速度可控”的需求。
        # - coarse_tau: 粗抽阶段（螺杆/罗茨）前级抽气时间常数
        # - fine_tau: 精抽阶段（分子泵）前级抽气时间常数
        # - equalize_tau: 连通时腔体与前级压力趋同的时间常数
        # - vent_tau: 放气时压力回升到大气的时间常数
        # - leak_tau: 隔离且未抽气/未放气时腔体漏气回升时间常数
        self.coarse_tau: float = 1.2
        self.fine_tau: float = 2.5
        self.equalize_tau: float = 2.0
        self.vent_tau: float = 0.6
        self.leak_tau: float = 6.0

        # 一些可在 GUI 中手动设置的“环境开关”
        self.phase_sequence_ok: bool = True
        self.motion_system_online: bool = True

    def set_bool(self, addr: str, value: bool) -> None:
        with self._lock:
            self.bool_values[addr] = bool(value)

    def get_bool(self, addr: str, default: bool = False) -> bool:
        with self._lock:
            return bool(self.bool_values.get(addr, default))

    def set_word(self, addr: str, value: int) -> None:
        with self._lock:
            self.word_values[addr] = int(max(0, min(65535, value)))

    def get_word(self, addr: str, default: int = 0) -> int:
        with self._lock:
            return int(self.word_values.get(addr, default))

    def snapshot(self) -> Tuple[Dict[str, bool], Dict[str, int]]:
        with self._lock:
            return dict(self.bool_values), dict(self.word_values)

    def tick(self, dt_s: float) -> None:
        """每个 tick 根据输出信号更新输入反馈与模拟量。"""
        with self._lock:
            # --- 环境与固定输入 ---
            self.bool_values.setdefault("%I0.5", True)
            self.bool_values["%I0.5"] = bool(self.phase_sequence_ok)  # 相序保护/相序 OK 信号

            self.bool_values.setdefault("%I9.6", True)
            self.bool_values["%I9.6"] = bool(self.motion_system_online)

            # 闸板阀5动作允许：在线才允许
            self.bool_values.setdefault("%I9.7", True)
            self.bool_values["%I9.7"] = bool(self.motion_system_online)

            # --- 泵：输出 -> 反馈 + 转速 ---
            # 螺杆泵：%Q0.1 上电输出, %Q0.0 启停；反馈 %I0.0
            screw_power = self.get_bool("%Q0.1")
            self.bool_values["%I0.0"] = screw_power

            # 罗茨泵：%Q0.2 上电输出；反馈 %I0.1
            roots_power = self.get_bool("%Q0.2")
            self.bool_values["%I0.1"] = roots_power

            # 分子泵：%Q0.3/0.4/0.5 上电；%Q13.0/13.1/13.2 启停；反馈 %I0.2/0.3/0.4
            for idx, (pwr_q, run_q, fb_i, spd_iw, max_rpm) in enumerate(
                [
                    ("%Q0.3", "%Q13.0", "%I0.2", "%IW24", 60000),
                    ("%Q0.4", "%Q13.1", "%I0.3", "%IW36", 60000),
                    ("%Q0.5", "%Q13.2", "%I0.4", "%IW48", 60000),
                ],
                start=1,
            ):
                powered = self.get_bool(pwr_q)
                running = powered and self.get_bool(run_q)
                self.bool_values[fb_i] = powered

                current = self.word_values.get(spd_iw, 0)
                if running:
                    target = int(max_rpm)
                    step = int(8000 * dt_s)  # 转速爬升速度
                    current = min(target, current + step)
                else:
                    step = int(12000 * dt_s)
                    current = max(0, current - step)
                self.word_values[spd_iw] = int(current)

            # --- 阀门：输出 -> 反馈（带动作延时） ---
            # 电磁阀 1-4：输出单 bit 表示开(1)/关(0)，输入有开到位/关到位
            emap = [
                ("%Q0.6", "%I0.6", "%I0.7"),
                ("%Q0.7", "%I1.0", "%I1.1"),
                ("%Q1.0", "%I1.2", "%I1.3"),
                ("%Q8.0", "%I1.4", "%I1.5"),
            ]
            for out_q, open_i, close_i in emap:
                desired_open = self.get_bool(out_q)
                self._apply_binary_valve(out_q, desired_open, open_i, close_i, duration_s=0.6)

            # 放气阀 1-2：同上
            vmap = [
                ("%Q8.1", "%I8.0", "%I8.1"),
                ("%Q8.2", "%I8.2", "%I8.3"),
            ]
            for out_q, open_i, close_i in vmap:
                desired_open = self.get_bool(out_q)
                self._apply_binary_valve(out_q, desired_open, open_i, close_i, duration_s=0.8)

            # 闸板阀 1-5：开输出/关输出 分别控制
            gmap = [
                ("GV1", "%Q8.3", "%Q8.4", "%I8.4", "%I8.5"),
                ("GV2", "%Q8.5", "%Q8.6", "%I8.6", "%I8.7"),
                ("GV3", "%Q8.7", "%Q9.0", "%I9.0", "%I9.1"),
                ("GV4", "%Q9.1", "%Q9.2", "%I9.2", "%I9.3"),
                ("GV5", "%Q9.3", "%Q9.4", "%I9.4", "%I9.5"),
            ]
            for gid, open_q, close_q, open_i, close_i in gmap:
                desired_open = self.get_bool(open_q) and not self.get_bool(close_q)
                desired_close = self.get_bool(close_q) and not self.get_bool(open_q)
                if desired_open == desired_close:
                    # 无动作或矛盾，保持当前
                    pass
                elif desired_open:
                    self._apply_binary_valve(gid, True, open_i, close_i, duration_s=1.2)
                else:
                    self._apply_binary_valve(gid, False, open_i, close_i, duration_s=1.2)

            # --- 真空/气压物理：
            # 1) 前级/腔体分区
            # 2) 闸板阀 1-4 任一打开则连通，主真空计(腔体)读数逐渐趋同前级
            # 3) 粗抽/精抽速度可控
            # 4) 放气时压力持续上升直至大气
            self._simulate_pressure(dt_s)

            # %IW130 为“电阻规电压”模拟量：在系统里作为前级真空计（G1）来源。
            self.word_values["%IW130"] = self._pressure_to_voltage_word(self.foreline_pressure_mbar)

            # 气路压力传感器（4-20mA）：默认 12mA
            self.word_values["%IW132"] = self._mA_to_word(12.0)

    def _apply_binary_valve(
        self,
        key: str,
        desired_open: bool,
        open_feedback_addr: str,
        close_feedback_addr: str,
        duration_s: float,
    ) -> None:
        open_now = bool(self.bool_values.get(open_feedback_addr, False))
        close_now = bool(self.bool_values.get(close_feedback_addr, True if not open_now else False))

        # 初始化默认：如果两者都没写过，默认关到位
        if open_feedback_addr not in self.bool_values and close_feedback_addr not in self.bool_values:
            open_now = False
            close_now = True
            self.bool_values[open_feedback_addr] = open_now
            self.bool_values[close_feedback_addr] = close_now

        if desired_open and open_now:
            self._valve_motion.pop(key, None)
            self.bool_values[open_feedback_addr] = True
            self.bool_values[close_feedback_addr] = False
            return
        if (not desired_open) and close_now:
            self._valve_motion.pop(key, None)
            self.bool_values[open_feedback_addr] = False
            self.bool_values[close_feedback_addr] = True
            return

        motion = self._valve_motion.get(key)
        if motion is None or motion.target_open != desired_open:
            self._valve_motion[key] = ValveMotion(target_open=desired_open, start_ts=time.time(), duration_s=duration_s)
            return

        if time.time() - motion.start_ts >= motion.duration_s:
            self.bool_values[open_feedback_addr] = bool(motion.target_open)
            self.bool_values[close_feedback_addr] = not bool(motion.target_open)
            self._valve_motion.pop(key, None)

    def _simulate_pressure(self, dt_s: float) -> None:
        # 核心目标：
        # - 默认从大气开始；
        # - 放气过程：压力持续上升至大气；
        # - 闸板阀 1-4 任一打开：腔体与前级连通，主真空计1/2读数逐渐趋同前级；
        # - 抽气过程：粗抽/精抽分档，并且速度可控；
        # - 特殊场景：仅前级泵开且 GV1-4 全关 -> 只改变前级压力。

        ATM_MBAR = 1000.0

        vent_open = self.get_bool("%Q8.1") or self.get_bool("%Q8.2")

        # 判断“腔体与前级是否连通”：闸板阀1-4任一打开（用开到位反馈，更贴近真实动作延时）。
        connected = (
            self.get_bool("%I8.4")  # GV1 open fb
            or self.get_bool("%I8.6")  # GV2 open fb
            or self.get_bool("%I9.0")  # GV3 open fb
            or self.get_bool("%I9.2")  # GV4 open fb
        )

        # 泵运行状态：
        # - 粗抽：螺杆泵上电 + 启停
        # - 罗茨：上电（在该简化模型里视为粗抽增强）
        # - 精抽：任一分子泵上电 + 启停
        screw_running = self.get_bool("%Q0.1") and self.get_bool("%Q0.0")
        roots_on = self.get_bool("%Q0.2")
        mol_running = (
            (self.get_bool("%Q0.3") and self.get_bool("%Q13.0"))
            or (self.get_bool("%Q0.4") and self.get_bool("%Q13.1"))
            or (self.get_bool("%Q0.5") and self.get_bool("%Q13.2"))
        )

        # 1) 前级压力更新（抽气/放气）
        if vent_open:
            foreline_target = ATM_MBAR
            foreline_tau = max(0.05, float(self.vent_tau))
        else:
            if mol_running:
                foreline_target = 1e-4
                foreline_tau = max(0.05, float(self.fine_tau))
            elif screw_running and roots_on:
                foreline_target = 0.1
                foreline_tau = max(0.05, float(self.coarse_tau) * 1.3)
            elif screw_running:
                foreline_target = 10.0
                foreline_tau = max(0.05, float(self.coarse_tau))
            else:
                foreline_target = ATM_MBAR
                foreline_tau = max(0.05, float(self.leak_tau))

        alpha_f = 1.0 - pow(2.718281828, -dt_s / foreline_tau)
        self.foreline_pressure_mbar = (1.0 - alpha_f) * self.foreline_pressure_mbar + alpha_f * foreline_target

        # 2) 腔体压力更新（受连通/放气/隔离漏气影响）
        if vent_open:
            chamber_target = ATM_MBAR
            chamber_tau = max(0.05, float(self.vent_tau))
            alpha_c = 1.0 - pow(2.718281828, -dt_s / chamber_tau)
            self.chamber_pressure_mbar = (1.0 - alpha_c) * self.chamber_pressure_mbar + alpha_c * chamber_target
        else:
            if connected:
                # 连通时：腔体向前级逐渐趋同（同时前级可能在抽气）。
                eq_tau = max(0.05, float(self.equalize_tau))
                alpha_eq = 1.0 - pow(2.718281828, -dt_s / eq_tau)
                self.chamber_pressure_mbar = (1.0 - alpha_eq) * self.chamber_pressure_mbar + alpha_eq * self.foreline_pressure_mbar
            else:
                # 隔离时：腔体不随前级抽气，只缓慢漏气回升。
                chamber_target = ATM_MBAR
                chamber_tau = max(0.05, float(self.leak_tau))
                alpha_c = 1.0 - pow(2.718281828, -dt_s / chamber_tau)
                self.chamber_pressure_mbar = (1.0 - alpha_c) * self.chamber_pressure_mbar + alpha_c * chamber_target

        # 3) 限幅
        self.foreline_pressure_mbar = float(max(1e-6, min(ATM_MBAR, self.foreline_pressure_mbar)))
        self.chamber_pressure_mbar = float(max(1e-6, min(ATM_MBAR, self.chamber_pressure_mbar)))

    @staticmethod
    def _pressure_to_voltage_word(pressure_mbar: float) -> int:
        # 0-10V -> WORD(0..32767)
        # 说明：本仓库设备侧（vacuum_system_device.cpp）按 raw*10/32767.0 进行电压换算。
        # 为保证 OPC-UA 模拟器输出能被设备侧正确解析，这里使用 0..32767 满量程。
        # 使用对数映射：高真空(1e-6)≈0V，大气(1e3)≈10V
        import math

        p = max(1e-6, min(1e3, float(pressure_mbar)))
        # log10(p) in [-6, 3] -> [0, 10]
        v = (math.log10(p) + 6.0) * (10.0 / 9.0)
        v = max(0.0, min(10.0, v))
        return int(round(v / 10.0 * 32767))

    @staticmethod
    def _mA_to_word(ma: float) -> int:
        # 4-20mA -> 0..32767（同上，匹配设备侧换算尺度）
        ma = max(0.0, min(20.0, float(ma)))
        return int(round(ma / 20.0 * 32767))


# ============================================================
# OPC-UA Server + 同步/仿真线程
# ============================================================


class OpcUaPlcSimulator:
    def __init__(self, mapping_h_path: str, endpoint: str = "opc.tcp://0.0.0.0:4840") -> None:
        self.mapping_h_path = mapping_h_path
        self.endpoint = endpoint

        self.model = PLCModel()
        self._stop_event = threading.Event()
        self._thread: Optional[threading.Thread] = None

        # GUI -> 后台命令
        self.gui_cmd_queue: "queue.Queue[Tuple[str, str, object]]" = queue.Queue()

        # 运行期对象
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._server: Optional[Server] = None
        self._nodes_bool: Dict[str, ua.NodeId] = {}
        self._nodes_word: Dict[str, ua.NodeId] = {}
        # addr -> Node（仅暴露“标准/通用”的不带引号 NodeId：ns=3;s=%Q0.1）
        self._node_handles: Dict[str, object] = {}

    def start_background(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run, name="opcua-plc-sim", daemon=True)
        self._thread.start()

    def stop_background(self) -> None:
        self._stop_event.set()

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive() and not self._stop_event.is_set()

    def _run(self) -> None:
        try:
            asyncio.run(self._async_main())
        except Exception:
            traceback.print_exc()
            self._stop_event.set()

    async def run_headless(self, run_seconds: float = 0.0) -> None:
        """无 GUI 运行：在当前线程直接启动 OPC-UA 服务器与仿真。

        run_seconds=0 表示一直运行，直到 Ctrl+C 或外部终止进程。
        """
        self._stop_event.clear()

        if run_seconds and run_seconds > 0:
            async def _stop_later() -> None:
                await asyncio.sleep(float(run_seconds))
                self._stop_event.set()

            asyncio.create_task(_stop_later())

        await self._async_main()

    async def _async_main(self) -> None:
        server = Server()
        await server.init()
        server.set_endpoint(self.endpoint)
        server.set_server_name("Vacuum PLC Simulator (asyncua)")

        # 匿名/无安全：本机联调用。默认 asyncua 会拒绝 Anonymous 写入，所以这里显式放开权限。
        server.set_security_policy(
            [ua.SecurityPolicyType.NoSecurity],
            permission_ruleset=AnonymousReadWriteRuleset(),
        )

        # 兼容 asyncua：没有 set_namespace_array。
        # 目标：确保 namespace index 3 存在（客户端使用 ns=3;s="%I0.0" 这类 NodeId）。
        ns_array = await server.get_namespace_array()
        while len(ns_array) < 4:
            await server.register_namespace(f"urn:doubleconeshooting:sim:ns{len(ns_array)}")
            ns_array = await server.get_namespace_array()
        ns_idx = 3

        # Objects 下建立一个文件夹节点（浏览用，不影响 NodeId 访问）
        objects = server.nodes.objects
        plc_folder = await objects.add_object(ua.NodeId("PLC", ns_idx), ua.QualifiedName("PLC", ns_idx))

        parsed = parse_mapping_header(self.mapping_h_path)

        # 模拟模式下：默认让水路 1-4 为“正常”，对应水电磁阀 1-4 输出为 True。
        # 这与 C++ 设备服务在 sim_mode_ 下的行为对齐，避免一键抽真空等流程被水路联锁阻塞。
        default_true_output_bools = {"%Q12.0", "%Q12.1", "%Q12.2", "%Q12.3"}

        async def _make_output_writable(var) -> None:
            # 让 %Q/%QW 点位对客户端可写（否则会返回 BadUserAccessDenied）。
            # asyncua 的 set_writable 会设置 AccessLevel/UserAccessLevel 的 CurrentWrite 位。
            await var.set_writable(True)

        # 创建变量节点
        for addr in parsed:
            # 仅使用“标准/通用”的不带引号 NodeId 文本：ns=3;s=%Q0.1
            nodeid = ua.NodeId(addr.address_string, ns_idx)
            qname = ua.QualifiedName(addr.address_string, ns_idx)

            if addr.is_bool:
                init_bool = bool(addr.is_output and addr.address_string in default_true_output_bools)
                var = await plc_folder.add_variable(nodeid, qname, ua.Variant(init_bool, ua.VariantType.Boolean))
                if addr.is_output:
                    await _make_output_writable(var)
                self._node_handles[addr.address_string] = var
                self.model.set_bool(addr.address_string, init_bool)
            else:
                var = await plc_folder.add_variable(nodeid, qname, ua.Variant(0, ua.VariantType.UInt16))
                if addr.is_output:
                    await _make_output_writable(var)
                self._node_handles[addr.address_string] = var
                self.model.set_word(addr.address_string, 0)

        self._server = server

        async with server:
            print(f"[SIM] OPC-UA server listening: {self.endpoint} (ns=3; anonymous)")
            print(f"[SIM] Namespace array length: {len(await server.get_namespace_array())}")
            # 主循环：同步客户端写入 + 物理仿真 + 写回输入
            last = time.time()
            while not self._stop_event.is_set():
                now = time.time()
                dt = max(0.02, min(0.2, now - last))
                last = now

                # 处理 GUI 命令
                await self._drain_gui_cmds()

                # 从 OPC-UA 读出所有“输出点位”的当前值（捕获客户端写入）
                await self._sync_outputs_from_server()

                # 物理仿真
                self.model.tick(dt)

                # 写回输入与模拟量（以及把内部状态回写到所有节点，保证一致）
                await self._sync_all_to_server()

                await asyncio.sleep(0.05)

    async def _drain_gui_cmds(self) -> None:
        while True:
            try:
                cmd, key, value = self.gui_cmd_queue.get_nowait()
            except queue.Empty:
                return

            if cmd == "set_env":
                if key == "phase_sequence_ok":
                    self.model.phase_sequence_ok = bool(value)
                elif key == "motion_system_online":
                    self.model.motion_system_online = bool(value)

            elif cmd == "toggle_output_bool":
                current = self.model.get_bool(key)
                self.model.set_bool(key, not current)

    async def _sync_outputs_from_server(self) -> None:
        # 只同步输出点位：%Q* 和 %QW*
        for addr, node in self._node_handles.items():
            if addr.startswith("%Q"):
                v = await node.read_value()
                if isinstance(v, bool):
                    self.model.set_bool(addr, bool(v))
                else:
                    try:
                        self.model.set_word(addr, int(v))
                    except Exception:
                        pass

    async def _sync_all_to_server(self) -> None:
        bools, words = self.model.snapshot()
        for addr, node in self._node_handles.items():
            if addr in bools:
                await node.write_value(ua.Variant(bool(bools[addr]), ua.VariantType.Boolean))
            elif addr in words:
                await node.write_value(ua.Variant(int(words[addr]), ua.VariantType.UInt16))


# ============================================================
# Tkinter GUI
# ============================================================


class SimulatorGUI:
    def __init__(self, sim: OpcUaPlcSimulator) -> None:
        self.sim = sim
        self.root = tk.Tk()
        self.root.title("OPC-UA 模拟 PLC（真空系统）")

        # 默认窗口更宽一些，方便阅读长状态行；仍支持用户自行缩放。
        try:
            self.root.geometry("1200x760")
            self.root.minsize(980, 620)
        except Exception:
            pass

        self._build()
        self._tick_ui()

    def _build(self) -> None:
        frm = ttk.Frame(self.root, padding=10)
        frm.grid(row=0, column=0, sticky="nsew")

        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        frm.columnconfigure(0, weight=1)
        frm.columnconfigure(1, weight=1)
        # 让底部状态区随窗口拉伸
        frm.rowconfigure(2, weight=1)

        # 顶部控制
        top = ttk.Frame(frm)
        top.grid(row=0, column=0, columnspan=2, sticky="ew")

        self.status_var = tk.StringVar(value="未启动")
        ttk.Label(top, text="状态:").grid(row=0, column=0, sticky="w")
        ttk.Label(top, textvariable=self.status_var).grid(row=0, column=1, sticky="w")

        ttk.Button(top, text="启动服务器", command=self._on_start).grid(row=0, column=2, padx=8)
        ttk.Button(top, text="停止服务器", command=self._on_stop).grid(row=0, column=3)

        # 环境开关
        env = ttk.LabelFrame(frm, text="环境/联锁", padding=8)
        env.grid(row=1, column=0, sticky="nsew", pady=(10, 0))

        self.phase_ok = tk.BooleanVar(value=True)
        self.motion_online = tk.BooleanVar(value=True)

        ttk.Checkbutton(env, text="相序正常(%I0.5)", variable=self.phase_ok, command=self._on_env_change).grid(
            row=0, column=0, sticky="w"
        )
        ttk.Checkbutton(env, text="运动系统在线(%I9.6)", variable=self.motion_online, command=self._on_env_change).grid(
            row=1, column=0, sticky="w"
        )

        # 输出控制
        out = ttk.LabelFrame(frm, text="输出控制（%Q / %QW）", padding=8)
        out.grid(row=1, column=1, sticky="nsew", pady=(10, 0))

        self._output_buttons: List[Tuple[str, ttk.Button]] = []
        outputs = [
            ("螺杆泵上电 %Q0.1", "%Q0.1"),
            ("螺杆泵启停 %Q0.0", "%Q0.0"),
            ("罗茨泵上电 %Q0.2", "%Q0.2"),
            ("分子泵1上电 %Q0.3", "%Q0.3"),
            ("分子泵1启停 %Q13.0", "%Q13.0"),
            ("分子泵1启用配置 %Q13.3", "%Q13.3"),
            ("分子泵2上电 %Q0.4", "%Q0.4"),
            ("分子泵2启停 %Q13.1", "%Q13.1"),
            ("分子泵2启用配置 %Q13.4", "%Q13.4"),
            ("分子泵3上电 %Q0.5", "%Q0.5"),
            ("分子泵3启停 %Q13.2", "%Q13.2"),
            ("分子泵3启用配置 %Q13.5", "%Q13.5"),
            ("放气阀1 %Q8.1", "%Q8.1"),
            ("放气阀2 %Q8.2", "%Q8.2"),
        ]

        for r, (label, addr) in enumerate(outputs):
            btn = ttk.Button(out, text=label, command=lambda a=addr: self._toggle_output(a))
            btn.grid(row=r, column=0, sticky="ew", pady=2)
            out.columnconfigure(0, weight=1)
            self._output_buttons.append((addr, btn))

        # 状态显示
        stat = ttk.LabelFrame(frm, text="关键状态（反馈/模拟量）", padding=8)
        stat.grid(row=2, column=0, columnspan=2, sticky="nsew", pady=(10, 0))

        stat.columnconfigure(0, weight=1)
        stat.rowconfigure(0, weight=1)

        self._info_text = tk.Text(
            stat,
            height=26,
            wrap="word",
            font=("Consolas", 10),
            padx=6,
            pady=6,
        )
        vsb = ttk.Scrollbar(stat, orient="vertical", command=self._info_text.yview)
        self._info_text.configure(yscrollcommand=vsb.set)
        self._info_text.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        self._info_text.configure(state="disabled")

    def _on_start(self) -> None:
        self.sim.start_background()

    def _on_stop(self) -> None:
        self.sim.stop_background()

    def _on_env_change(self) -> None:
        self.sim.gui_cmd_queue.put(("set_env", "phase_sequence_ok", self.phase_ok.get()))
        self.sim.gui_cmd_queue.put(("set_env", "motion_system_online", self.motion_online.get()))

    def _toggle_output(self, addr: str) -> None:
        self.sim.gui_cmd_queue.put(("toggle_output_bool", addr, True))

    def _tick_ui(self) -> None:
        # 更新状态文本
        if self.sim.running:
            self.status_var.set(f"运行中：{self.sim.endpoint}（ns=3，匿名）")
        else:
            self.status_var.set("未运行")

        b, w = self.sim.model.snapshot()
        foreline = self.sim.model.foreline_pressure_mbar
        chamber = self.sim.model.chamber_pressure_mbar

        def _bf(addr: str) -> bool:
            return bool(b.get(addr, False))

        def _wf(addr: str) -> int:
            return int(w.get(addr, 0))

        def _mbar_to_pa(mbar: float) -> float:
            return float(mbar) * 100.0

        connected = (
            _bf("%I8.4")
            or _bf("%I8.6")
            or _bf("%I9.0")
            or _bf("%I9.2")
        )

        lines = [
            "气压/真空(两区模型):",
            f"前级压力(G1): {foreline:.6g} mbar  (~{_mbar_to_pa(foreline):.6g} Pa)",
            f"腔体压力(G2/G3): {chamber:.6g} mbar  (~{_mbar_to_pa(chamber):.6g} Pa)",
            f"连通状态(闸板阀1-4任一开): {'是' if connected else '否'}",
            "",
            "泵/转速:",
            f"螺杆泵  输出%Q0.1={_bf('%Q0.1')} 启停%Q0.0={_bf('%Q0.0')}  反馈%I0.0={_bf('%I0.0')}",
            f"罗茨泵  输出%Q0.2={_bf('%Q0.2')}  反馈%I0.1={_bf('%I0.1')}",
            f"分子泵1 输出%Q0.3={_bf('%Q0.3')} 启停%Q13.0={_bf('%Q13.0')} 启用%Q13.3={_bf('%Q13.3')}  反馈%I0.2={_bf('%I0.2')}  转速%IW24={_wf('%IW24')}",
            f"分子泵2 输出%Q0.4={_bf('%Q0.4')} 启停%Q13.1={_bf('%Q13.1')} 启用%Q13.4={_bf('%Q13.4')}  反馈%I0.3={_bf('%I0.3')}  转速%IW36={_wf('%IW36')}",
            f"分子泵3 输出%Q0.5={_bf('%Q0.5')} 启停%Q13.2={_bf('%Q13.2')} 启用%Q13.5={_bf('%Q13.5')}  反馈%I0.4={_bf('%I0.4')}  转速%IW48={_wf('%IW48')}",
            "",
            "阀门(电磁阀/放气阀):",
            f"电磁阀1 输出%Q0.6={_bf('%Q0.6')}  开到位%I0.6={_bf('%I0.6')}  关到位%I0.7={_bf('%I0.7')}",
            f"电磁阀2 输出%Q0.7={_bf('%Q0.7')}  开到位%I1.0={_bf('%I1.0')}  关到位%I1.1={_bf('%I1.1')}",
            f"电磁阀3 输出%Q1.0={_bf('%Q1.0')}  开到位%I1.2={_bf('%I1.2')}  关到位%I1.3={_bf('%I1.3')}",
            f"电磁阀4 输出%Q8.0={_bf('%Q8.0')}  开到位%I1.4={_bf('%I1.4')}  关到位%I1.5={_bf('%I1.5')}",
            f"放气阀1 输出%Q8.1={_bf('%Q8.1')}  开到位%I8.0={_bf('%I8.0')}  关到位%I8.1={_bf('%I8.1')}",
            f"放气阀2 输出%Q8.2={_bf('%Q8.2')}  开到位%I8.2={_bf('%I8.2')}  关到位%I8.3={_bf('%I8.3')}",
            "",
            "闸板阀(GV1~GV5):",
            f"GV1 开%Q8.3={_bf('%Q8.3')} 关%Q8.4={_bf('%Q8.4')}  开到位%I8.4={_bf('%I8.4')} 关到位%I8.5={_bf('%I8.5')}",
            f"GV2 开%Q8.5={_bf('%Q8.5')} 关%Q8.6={_bf('%Q8.6')}  开到位%I8.6={_bf('%I8.6')} 关到位%I8.7={_bf('%I8.7')}",
            f"GV3 开%Q8.7={_bf('%Q8.7')} 关%Q9.0={_bf('%Q9.0')}  开到位%I9.0={_bf('%I9.0')} 关到位%I9.1={_bf('%I9.1')}",
            f"GV4 开%Q9.1={_bf('%Q9.1')} 关%Q9.2={_bf('%Q9.2')}  开到位%I9.2={_bf('%I9.2')} 关到位%I9.3={_bf('%I9.3')}",
            f"GV5 开%Q9.3={_bf('%Q9.3')} 关%Q9.4={_bf('%Q9.4')}  开到位%I9.4={_bf('%I9.4')} 关到位%I9.5={_bf('%I9.5')}",
            "",
            "真空计/模拟量:",
            f"电阻规电压(WORD) %IW130(前级): {_wf('%IW130')}",
            f"气压传感(WORD) %IW132: {_wf('%IW132')}",
            "",
            f"运动系统在线%I9.6={_bf('%I9.6')}  动作允许%I9.7={_bf('%I9.7')}  相序正常%I0.5={_bf('%I0.5')}",
        ]
        text = "\n".join(lines)

        # 保留用户当前滚动位置：定时刷新不应把滚动条强制跳回顶部/底部。
        y0, y1 = self._info_text.yview()
        self._info_text.configure(state="normal")
        self._info_text.delete("1.0", "end")
        self._info_text.insert("1.0", text)
        self._info_text.configure(state="disabled")

        # 如果用户原本在最底部（y1==1.0），则保持在底部；否则恢复原位置。
        if y1 >= 0.999:
            self._info_text.yview_moveto(1.0)
        else:
            self._info_text.yview_moveto(y0)

        # 让按钮显示当前开关状态
        for addr, btn in self._output_buttons:
            state = self.sim.model.get_bool(addr)
            btn.configure(text=f"{btn.cget('text').split('[')[0].strip()} [{'ON' if state else 'OFF'}]")

        self.root.after(200, self._tick_ui)

    def run(self) -> None:
        self.root.mainloop()


def _default_mapping_path() -> str:
    # 相对仓库根目录：include/device_services/vacuum_system_plc_mapping.h
    here = os.path.abspath(os.path.dirname(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    return os.path.join(root, "include", "device_services", "vacuum_system_plc_mapping.h")


def main() -> None:
    parser = argparse.ArgumentParser(description="OPC-UA 模拟 PLC（真空系统）")
    parser.add_argument("--mapping", default=_default_mapping_path(), help="vacuum_system_plc_mapping.h 路径")
    parser.add_argument("--endpoint", default="opc.tcp://127.0.0.1:4840", help="OPC-UA endpoint")
    parser.add_argument("--nogui", action="store_true", help="不启动 GUI，仅启动 OPC-UA 服务器与仿真")
    parser.add_argument("--run-seconds", type=float, default=0.0, help="仅 --nogui：运行指定秒数后退出（0=一直运行）")
    parser.add_argument("--coarse-tau", type=float, default=1.2, help="粗抽速度时间常数(s)，越小抽得越快")
    parser.add_argument("--fine-tau", type=float, default=2.5, help="精抽速度时间常数(s)，越小抽得越快")
    parser.add_argument("--equalize-tau", type=float, default=2.0, help="闸板阀连通时腔体/前级趋同时间常数(s)")
    parser.add_argument("--vent-tau", type=float, default=0.6, help="放气回升到大气时间常数(s)")
    parser.add_argument("--leak-tau", type=float, default=6.0, help="隔离漏气回升时间常数(s)")
    args = parser.parse_args()

    if args.nogui:
        sim = OpcUaPlcSimulator(mapping_h_path=str(args.mapping), endpoint=str(args.endpoint))
        sim.model.coarse_tau = float(args.coarse_tau)
        sim.model.fine_tau = float(args.fine_tau)
        sim.model.equalize_tau = float(args.equalize_tau)
        sim.model.vent_tau = float(args.vent_tau)
        sim.model.leak_tau = float(args.leak_tau)
        try:
            asyncio.run(sim.run_headless(run_seconds=float(args.run_seconds)))
        except KeyboardInterrupt:
            pass
        return

    sim = OpcUaPlcSimulator(mapping_h_path=str(args.mapping), endpoint=str(args.endpoint))
    sim.model.coarse_tau = float(args.coarse_tau)
    sim.model.fine_tau = float(args.fine_tau)
    sim.model.equalize_tau = float(args.equalize_tau)
    sim.model.vent_tau = float(args.vent_tau)
    sim.model.leak_tau = float(args.leak_tau)
    sim.start_background()

    gui = SimulatorGUI(sim)
    gui.run()


if __name__ == "__main__":
    main()
