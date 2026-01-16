from __future__ import annotations

import argparse
import re
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple


RX_RE = re.compile(
    r"RX\s+tid=(?P<tid>\d+)\s+uid=(?P<uid>\d+)\s+fc=0x(?P<fc>[0-9A-Fa-f]{2})\s+(?P<rest>.*)$"
)
ADDR_COUNT_RE = re.compile(r"addr=(?P<addr>\d+)\s+count=(?P<count>\d+)")
WRITE10_RE = re.compile(r"addr=(?P<addr>\d+)\s+count=(?P<count>\d+)\s+bytes=(?P<bytes>\d+)\s+values_hex=(?P<hex>[0-9A-Fa-f]*)")
WRITE06_RE = re.compile(r"addr=(?P<addr>\d+)\s+value=0x(?P<value>[0-9A-Fa-f]{4})")
WRITE05_RE = re.compile(r"addr=(?P<addr>\d+)\s+value=0x(?P<value>[0-9A-Fa-f]{4})")


@dataclass
class Hit:
    ip: str
    fc: int
    addr: int
    count: int


def parse_ip_prefix(line: str) -> str:
    # format: "YYYY-mm-dd ... [ip:port] ..."
    m = re.search(r"\[(?P<ip>\d+\.\d+\.\d+\.\d+):\d+\]", line)
    return m.group("ip") if m else "?"


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze Modbus/TCP motion controller sim log")
    parser.add_argument(
        "--log",
        default="tools/motion_controller_simulator/sim.log",
        help="Path to sim.log",
    )
    parser.add_argument("--top", type=int, default=30)
    args = parser.parse_args()

    path = Path(args.log)
    if not path.exists():
        raise SystemExit(f"log not found: {path}")

    if path.stat().st_size == 0:
        print(f"log is empty: {path}")
        print("常见原因：")
        print("- simulator 实际写到了别的路径（建议启动时看它打印的 'Using log file:'）")
        print("- simulator 没有收到任何连接（上层没连上 / IP 没绑到 lo / 502 没监听成功）")
        print("快速验证（WSL）：")
        print("- ip addr show dev lo | grep 192.168.1")
        print("- ss -ltnp | grep :502")
        print("- python3 scripts/tools/ltsmc_smoke_test_wsl.py （让驱动主动连三台 IP）")
        return

    reads: Dict[Tuple[str, int], Counter[Tuple[int, int]]] = defaultdict(Counter)
    writes10: Dict[Tuple[str, int], Counter[Tuple[int, int]]] = defaultdict(Counter)
    writes06: Dict[Tuple[str, int], Counter[int]] = defaultdict(Counter)
    writes05: Dict[Tuple[str, int], Counter[int]] = defaultdict(Counter)

    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if " RX " not in line:
                continue
            ip = parse_ip_prefix(line)

            m = RX_RE.search(line)
            if not m:
                continue
            fc = int(m.group("fc"), 16)
            rest = m.group("rest")
            uid = int(m.group("uid"))

            if fc in (0x01, 0x02, 0x03, 0x04):
                m2 = ADDR_COUNT_RE.search(rest)
                if not m2:
                    continue
                addr = int(m2.group("addr"))
                count = int(m2.group("count"))
                reads[(ip, uid)][(addr, count)] += 1
            elif fc == 0x10:
                m2 = WRITE10_RE.search(rest)
                if not m2:
                    continue
                addr = int(m2.group("addr"))
                count = int(m2.group("count"))
                writes10[(ip, uid)][(addr, count)] += 1
            elif fc == 0x06:
                m2 = WRITE06_RE.search(rest)
                if not m2:
                    continue
                addr = int(m2.group("addr"))
                writes06[(ip, uid)][addr] += 1
            elif fc == 0x05:
                m2 = WRITE05_RE.search(rest)
                if not m2:
                    continue
                addr = int(m2.group("addr"))
                writes05[(ip, uid)][addr] += 1

    def dump_counter(title: str, counters: Dict, top: int) -> None:
        print("\n" + title)
        print("-" * len(title))
        for (ip, uid), c in sorted(counters.items()):
            print(f"\n[{ip}] unit_id={uid}")
            for (key, n) in c.most_common(top):
                print(f"  {key}  x{n}")

    dump_counter("Top READ ranges (fc 01/02/03/04)", reads, args.top)
    dump_counter("Top WRITE ranges (fc 10)", writes10, args.top)

    print("\nTop WRITE single register (fc 06)")
    print("-" * 33)
    for (ip, uid), c in sorted(writes06.items()):
        print(f"\n[{ip}] unit_id={uid}")
        for addr, n in c.most_common(args.top):
            print(f"  addr={addr}  x{n}")

    print("\nTop WRITE single coil (fc 05)")
    print("-" * 29)
    for (ip, uid), c in sorted(writes05.items()):
        print(f"\n[{ip}] unit_id={uid}")
        for addr, n in c.most_common(args.top):
            print(f"  addr={addr}  x{n}")

    print("\n下一步建议：")
    print("- 把 WRITE(0x10/0x06/0x05) 命中最多的 addr 段当作候选‘命令/目标位置/触发位’")
    print("- 把 READ(0x03/0x04) 高频轮询的 addr 段当作候选‘位置/状态/报警’")


if __name__ == "__main__":
    main()
