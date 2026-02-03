#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""测试编码器采集器的数据读取（TCP）。

协议来源：项目内 Common::EncoderAcquisitionClient
- 帧长度：7 字节
- 帧格式：0x7E + channel(1B) + raw(4B, big-endian) + 0x7F

默认按双采集器 20 通道组织：
- 192.168.1.199 -> global channel 0-9
- 192.168.1.198 -> global channel 10-19

示例：
  python scripts/test_encoder_collector_read.py
  python scripts/test_encoder_collector_read.py --ports 5000 5000 --duration 30
  python scripts/test_encoder_collector_read.py --show-frames --duration 5

注意：仓库配置文件 devices_config.json 里 encoderCollectorPorts 可能是 20200；
这里默认使用你提供的 5000，可用 --ports 覆盖。
"""

from __future__ import annotations

import argparse
import dataclasses
import socket
import threading
import time
from typing import Iterable, Optional


FRAME_HEAD = 0x7E
FRAME_TAIL = 0x7F
FRAME_LEN = 7


@dataclasses.dataclass
class Reading:
    raw_value: int = 0
    timestamp_monotonic: float = 0.0
    valid: bool = False


@dataclasses.dataclass
class Stats:
    bytes_rx: int = 0
    frames_ok: int = 0
    frames_bad: int = 0
    reconnects: int = 0
    last_error: str = ""
    last_frame_time: float = 0.0


def _format_bps(n_bytes: float, seconds: float) -> str:
    if seconds <= 0:
        return "0 B/s"
    bps = n_bytes / seconds
    for unit in ("B/s", "KB/s", "MB/s", "GB/s"):
        if bps < 1024.0:
            return f"{bps:.1f} {unit}"
        bps /= 1024.0
    return f"{bps:.1f} TB/s"


def _format_fps(frames: float, seconds: float) -> str:
    if seconds <= 0:
        return "0 fps"
    return f"{frames / seconds:.1f} fps"


def _parse_frames(buffer: bytearray) -> Iterable[tuple[int, int]]:
    """从 buffer 里解析出完整帧，yield (channel, raw)，并就地移除已消费的数据。"""
    while len(buffer) >= FRAME_LEN:
        try:
            head_idx = buffer.index(FRAME_HEAD)
        except ValueError:
            buffer.clear()
            return

        # 不够一个完整帧：先把 head 前面的垃圾丢掉
        if len(buffer) - head_idx < FRAME_LEN:
            if head_idx > 0:
                del buffer[:head_idx]
            return

        if buffer[head_idx + FRAME_LEN - 1] != FRAME_TAIL:
            # head 不是帧头（或不同步），丢弃一个字节继续找
            del buffer[: head_idx + 1]
            continue

        frame = buffer[head_idx : head_idx + FRAME_LEN]
        channel = frame[1]
        raw = (frame[2] << 24) | (frame[3] << 16) | (frame[4] << 8) | frame[5]
        del buffer[: head_idx + FRAME_LEN]
        yield channel, raw


class CollectorClient:
    def __init__(
        self,
        ip: str,
        port: int,
        channel_offset: int,
        channel_count: int,
        shared_readings: list[Reading],
        shared_lock: threading.Lock,
        *,
        recv_timeout_s: float = 0.3,
        reconnect_delay_s: float = 1.0,
        dump_path: Optional[str] = None,
        show_frames: bool = False,
    ) -> None:
        self.ip = ip
        self.port = port
        self.channel_offset = channel_offset
        self.channel_count = channel_count
        self._shared_readings = shared_readings
        self._shared_lock = shared_lock

        self._recv_timeout_s = recv_timeout_s
        self._reconnect_delay_s = reconnect_delay_s
        self._dump_path = dump_path
        self._show_frames = show_frames

        self._sock: Optional[socket.socket] = None
        self._thread: Optional[threading.Thread] = None
        self._running = threading.Event()
        self._connected = threading.Event()
        self._buffer = bytearray()

        self._stats_lock = threading.Lock()
        self._stats = Stats()

        self._dump_fp = None

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._running.set()
        self._thread = threading.Thread(target=self._run, name=f"collector@{self.ip}:{self.port}", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=2.0)
        self._close_socket()
        if self._dump_fp:
            try:
                self._dump_fp.close()
            except Exception:
                pass
            self._dump_fp = None

    def is_connected(self) -> bool:
        return self._connected.is_set()

    def snapshot_stats(self) -> Stats:
        with self._stats_lock:
            return dataclasses.replace(self._stats)

    def _set_error(self, msg: str) -> None:
        with self._stats_lock:
            self._stats.last_error = msg

    def _inc(self, *, bytes_rx: int = 0, ok: int = 0, bad: int = 0, reconnect: int = 0) -> None:
        with self._stats_lock:
            self._stats.bytes_rx += bytes_rx
            self._stats.frames_ok += ok
            self._stats.frames_bad += bad
            self._stats.reconnects += reconnect

    def _touch_frame_time(self) -> None:
        with self._stats_lock:
            self._stats.last_frame_time = time.monotonic()

    def _connect_socket(self) -> bool:
        self._close_socket()
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self._recv_timeout_s)
            sock.connect((self.ip, self.port))
        except Exception as e:
            self._set_error(f"connect failed: {e}")
            self._connected.clear()
            self._close_socket()
            return False

        self._sock = sock
        self._connected.set()
        self._inc(reconnect=1)

        if self._dump_path and self._dump_fp is None:
            # 以二进制追加写入，便于离线分析
            self._dump_fp = open(self._dump_path, "ab")

        return True

    def _close_socket(self) -> None:
        self._connected.clear()
        if self._sock is not None:
            try:
                self._sock.close()
            except Exception:
                pass
            self._sock = None

    def _handle_frame(self, channel: int, raw: int) -> None:
        if channel >= self.channel_count:
            return
        global_channel = self.channel_offset + int(channel)
        now = time.monotonic()
        with self._shared_lock:
            if 0 <= global_channel < len(self._shared_readings):
                r = self._shared_readings[global_channel]
                r.raw_value = int(raw)
                r.timestamp_monotonic = now
                r.valid = True

        self._touch_frame_time()
        self._inc(ok=1)

        if self._show_frames:
            # 仅用于调试，频率可能很高
            print(f"[{self.ip}:{self.port}] ch={channel} (global={global_channel}) raw={raw}")

    def _run(self) -> None:
        while self._running.is_set():
            if not self.is_connected():
                if not self._connect_socket():
                    time.sleep(self._reconnect_delay_s)
                    continue

            assert self._sock is not None
            try:
                data = self._sock.recv(64)
            except socket.timeout:
                continue
            except Exception as e:
                self._set_error(f"recv failed: {e}")
                self._close_socket()
                continue

            if not data:
                self._set_error("peer closed")
                self._close_socket()
                continue

            if self._dump_fp:
                try:
                    self._dump_fp.write(data)
                except Exception:
                    pass

            self._inc(bytes_rx=len(data))
            self._buffer.extend(data)

            # 解析帧
            before_ok = self.snapshot_stats().frames_ok
            for ch, raw in _parse_frames(self._buffer):
                self._handle_frame(ch, raw)

            # 如果 buffer 里数据涨得太大，说明同步有问题：做一次硬清理
            # 以免长期占用内存。
            if len(self._buffer) > 4096 and (self.snapshot_stats().frames_ok == before_ok):
                self._inc(bad=1)
                self._buffer.clear()


def _build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="测试编码器采集器数据读取（TCP 7字节帧 0x7E..0x7F）")
    p.add_argument("--ips", nargs="+", default=["192.168.1.199", "192.168.1.198"], help="采集器 IP 列表")
    p.add_argument("--ports", nargs="+", type=int, default=[5000, 5000], help="采集器端口列表（数量可少于 ips，缺省用最后一个）")
    p.add_argument("--channels-per-collector", type=int, default=10, help="每个采集器通道数（默认 10）")
    p.add_argument("--duration", type=float, default=10.0, help="运行时长秒（0 表示一直运行）")
    p.add_argument("--print-interval", type=float, default=1.0, help="状态打印间隔秒")
    p.add_argument("--show-frames", action="store_true", help="打印每一帧（可能刷屏）")
    p.add_argument("--dump-dir", default="", help="如设置，则把每个采集器收到的原始字节流追加写到该目录")
    p.add_argument(
        "--watch-channels",
        default="",
        help="要显示的全局通道（逗号分隔）。空表示显示所有已收到的通道。例：0,1,2,6,7,10",
    )
    p.add_argument("--stale-seconds", type=float, default=1.0, help="超过该秒数未更新视为 stale")
    return p


def _parse_watch_channels(s: str) -> Optional[list[int]]:
    s = s.strip()
    if not s:
        return None
    out: list[int] = []
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        out.append(int(part))
    return out


def main() -> int:
    args = _build_arg_parser().parse_args()

    ips: list[str] = list(args.ips)
    ports_raw: list[int] = list(args.ports)
    if not ips:
        raise SystemExit("ips is empty")
    if not ports_raw:
        ports_raw = [5000]

    # ports 数量可少于 ips：不足部分用最后一个端口补齐
    ports: list[int] = []
    for i in range(len(ips)):
        ports.append(ports_raw[i] if i < len(ports_raw) else ports_raw[-1])

    channels_per = int(args.channels_per_collector)
    total_channels = channels_per * len(ips)

    shared_readings = [Reading() for _ in range(total_channels)]
    shared_lock = threading.Lock()

    watch_channels = _parse_watch_channels(args.watch_channels)

    clients: list[CollectorClient] = []
    for i, (ip, port) in enumerate(zip(ips, ports)):
        offset = i * channels_per
        dump_path = None
        if args.dump_dir:
            ts = time.strftime("%Y%m%d_%H%M%S")
            dump_path = f"{args.dump_dir.rstrip('\\/')}/encoder_{ip}_{port}_{ts}.bin"
        clients.append(
            CollectorClient(
                ip,
                port,
                channel_offset=offset,
                channel_count=channels_per,
                shared_readings=shared_readings,
                shared_lock=shared_lock,
                dump_path=dump_path,
                show_frames=bool(args.show_frames),
            )
        )

    for c in clients:
        c.start()

    start = time.monotonic()
    last_print = 0.0

    prev_stats = [c.snapshot_stats() for c in clients]
    prev_t = start

    try:
        while True:
            now = time.monotonic()

            if args.duration > 0 and (now - start) >= args.duration:
                break

            if (now - last_print) >= args.print_interval:
                dt = now - prev_t
                lines: list[str] = []

                lines.append("=" * 80)
                lines.append(f"t={now - start:.1f}s  collectors={len(clients)}  total_channels={total_channels}")

                for idx, c in enumerate(clients):
                    st = c.snapshot_stats()
                    d_bytes = st.bytes_rx - prev_stats[idx].bytes_rx
                    d_frames = st.frames_ok - prev_stats[idx].frames_ok
                    conn = "ON" if c.is_connected() else "OFF"
                    err = st.last_error
                    last_age = (now - st.last_frame_time) if st.last_frame_time > 0 else float("inf")
                    lines.append(
                        f"#{idx} {c.ip}:{c.port} [{conn}]  { _format_bps(d_bytes, dt) }  { _format_fps(d_frames, dt) }  "
                        f"reconnects={st.reconnects}  last_frame_age={last_age:.2f}s  err={err}"
                    )

                # 打印通道值
                with shared_lock:
                    if watch_channels is None:
                        channel_indices = [i for i, r in enumerate(shared_readings) if r.valid]
                    else:
                        channel_indices = [i for i in watch_channels if 0 <= i < len(shared_readings)]

                    if channel_indices:
                        parts: list[str] = []
                        for ch in channel_indices:
                            r = shared_readings[ch]
                            age = now - r.timestamp_monotonic
                            if age > args.stale_seconds:
                                parts.append(f"ch{ch}=STALE({age:.2f}s)")
                            else:
                                parts.append(f"ch{ch}={r.raw_value}")
                        lines.append("channels: " + "  ".join(parts))
                    else:
                        lines.append("channels: (no valid data yet)")

                print("\n".join(lines))

                prev_stats = [c.snapshot_stats() for c in clients]
                prev_t = now
                last_print = now

            time.sleep(0.02)

    except KeyboardInterrupt:
        pass
    finally:
        for c in clients:
            c.stop()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
