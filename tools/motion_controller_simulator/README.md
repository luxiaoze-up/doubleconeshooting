# Motion Controller Simulator (SMC608-BAS)

这个工具用于在本机启动 **3 个 Modbus/TCP 设备实例**，以便让 LTSMC `type=2`（TCP）连接到“仿真的网络运动控制器”。

目前实现目标：
- 监听 `:502`（从 LTSMC 连接探测结果确认）
- 支持常见 Modbus 功能码：`0x01/0x02/0x03/0x04/0x05/0x06/0x0F/0x10/0x11`
- 对读请求返回当前缓存值（默认 0），对写请求回显/确认
- 记录所有访问的寄存器/线圈地址，方便后续反推“位置/状态/运动指令”对应的寄存器映射

## 在 WSL2 下运行（推荐）

1) 给 loopback 添加 3 个 IP（让进程能 bind 这三个地址）：

```bash
sudo ip addr add 192.168.1.11/32 dev lo
sudo ip addr add 192.168.1.12/32 dev lo
sudo ip addr add 192.168.1.13/32 dev lo
```

2) 端口 502 是特权端口，需要 root（或者自行做端口转发/能力设置）：

```bash
sudo python3 /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/modbus_tcp_motion_controller_sim.py
```

也可以直接执行脚本（需要先赋予可执行权限）：

```bash
chmod +x /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/modbus_tcp_motion_controller_sim.py
sudo /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/modbus_tcp_motion_controller_sim.py
```

如果你想把每一帧报文都打出来，增加 `--raw-log`：

```bash
sudo python3 /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/modbus_tcp_motion_controller_sim.py --raw-log
```

如果你想预置寄存器/线圈初值，可以用 `--init`：

```bash
sudo python3 /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/modbus_tcp_motion_controller_sim.py --init /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/sim_init.example.json
```

日志默认输出到：`tools/motion_controller_simulator/sim.log`。

注意：为避免“从不同目录启动导致日志写到别处”，建议直接用绝对路径指定 `--log`。

## 下一步（联调）

- 运行 LTSMC 客户端（或现有 motion controller server）连接到 `192.168.1.11/12/13`。
- 观察 `sim.log` 中出现的 `addr/count`，即可逐步补齐：
  - 位置寄存器
  - 运动状态寄存器
  - 点位运动/停止等命令寄存器

如果你的上层是 **GUI 通过 Tango 命令**驱动运动：
- 先启动 simulator（本工具）
- 再启动 `motion_controller_server/ctrl1|ctrl2|ctrl3`
- GUI 对 `sys/motion/1` 等设备执行 `moveAbsolute/moveRelative/stopMove/readPos` 等命令

然后用这个脚本快速汇总“最常被读/写的寄存器段”（通常就是实际控制命令所在）：

```bash
python3 /mnt/d/00.My_workspace/DoubleConeShooting/scripts/tools/analyze_motion_controller_sim_log.py --log /mnt/d/00.My_workspace/DoubleConeShooting/tools/motion_controller_simulator/sim.log
```

## 常见问题：sim.log 没有内容

这意味着 simulator **没有收到连接** 或者 **日志写到了别的路径**。

按顺序检查（WSL）：

1) lo 上是否真的加了 192.168.1.11/12/13：

```bash
ip addr show dev lo | grep 192.168.1
```

2) 502 是否在监听：

```bash
ss -ltnp | grep :502
```

3) 用 LTSMC 冒烟测试强制发起连接（不依赖 GUI）：

```bash
python3 /mnt/d/00.My_workspace/DoubleConeShooting/scripts/tools/ltsmc_smoke_test_wsl.py
```
