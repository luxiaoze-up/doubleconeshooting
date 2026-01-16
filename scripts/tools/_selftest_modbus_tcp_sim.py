from __future__ import annotations

import asyncio
import importlib.util
import sys
import struct
from pathlib import Path


def _load_sim_module():
    sim_path = Path(r"d:\00.My_workspace\DoubleConeShooting\tools\motion_controller_simulator\modbus_tcp_motion_controller_sim.py")
    spec = importlib.util.spec_from_file_location("modbus_tcp_motion_controller_sim", sim_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("failed to load simulator module")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


async def main() -> None:
    sim = _load_sim_module()

    host = "127.0.0.1"
    port = 1502
    unit_id = 8

    server = sim.ModbusTcpServer(
        host=host,
        port=port,
        unit_id=unit_id,
        log_path=None,
        raw_log=False,
        init_file=None,
    )

    async def run_server():
        await server.run()

    task = asyncio.create_task(run_server())
    await asyncio.sleep(0.2)

    reader, writer = await asyncio.open_connection(host, port)

    tid = 1
    pid = 0
    fc = 0x03
    addr = 0
    count = 2
    pdu = struct.pack(">BHH", fc, addr, count)
    length = 1 + len(pdu)
    frame = struct.pack(">HHH", tid, pid, length) + bytes([unit_id]) + pdu

    writer.write(frame)
    await writer.drain()

    hdr = await reader.readexactly(6)
    r_tid, r_pid, r_len = struct.unpack(">HHH", hdr)
    rest = await reader.readexactly(r_len)

    assert r_tid == tid
    assert r_pid == pid
    assert rest[0] == unit_id
    assert rest[1] == fc
    assert rest[2] == 4
    assert rest[3:7] == b"\x00\x00\x00\x00"

    writer.close()
    await writer.wait_closed()

    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

    print("OK: Modbus/TCP simulator basic read works")


if __name__ == "__main__":
    asyncio.run(main())
