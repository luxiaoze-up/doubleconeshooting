from __future__ import annotations

import ctypes
import time
from pathlib import Path


def main() -> None:
    project_root = Path(__file__).resolve().parents[2]
    so_path = project_root / "lib" / "libLTSMC.so"
    if not so_path.exists():
        raise SystemExit(f"libLTSMC.so not found: {so_path}")

    smc = ctypes.CDLL(str(so_path))

    smc.smc_set_connect_timeout.argtypes = [ctypes.c_uint32]
    smc.smc_set_connect_timeout.restype = ctypes.c_int16

    smc.smc_board_init.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_char_p, ctypes.c_uint32]
    smc.smc_board_init.restype = ctypes.c_int16

    ip = b"192.168.1.11"

    print("Setting connect timeout...")
    ret = smc.smc_set_connect_timeout(15000)
    print("smc_set_connect_timeout ret:", ret)

    print("Calling smc_board_init(connectNo=0, type=2, ip=192.168.1.11)...")
    t0 = time.time()
    ret = smc.smc_board_init(0, 2, ip, 0)
    dt = time.time() - t0
    print("smc_board_init ret:", ret, "elapsed_s:", round(dt, 3))

    # Keep the process around briefly so ss can observe the connection state.
    time.sleep(5)


if __name__ == "__main__":
    main()
