from __future__ import annotations

import ctypes
import time
from pathlib import Path


def main() -> None:
    so_path = Path("/mnt/d/00.My_workspace/DoubleConeShooting/lib/libLTSMC.so")
    if not so_path.exists():
        raise SystemExit(f"libLTSMC.so not found: {so_path}")

    smc = ctypes.CDLL(str(so_path))

    smc.smc_set_connect_timeout.argtypes = [ctypes.c_uint32]
    smc.smc_set_connect_timeout.restype = ctypes.c_int16

    smc.smc_set_debug_mode.argtypes = [ctypes.c_uint16, ctypes.c_char_p]
    smc.smc_set_debug_mode.restype = ctypes.c_int16

    smc.smc_set_connect_debug_time.argtypes = [ctypes.c_uint16, ctypes.c_uint32]
    smc.smc_set_connect_debug_time.restype = ctypes.c_int16

    smc.smc_get_debug_mode.argtypes = [ctypes.POINTER(ctypes.c_uint16), ctypes.c_char_p]
    smc.smc_get_debug_mode.restype = ctypes.c_int16

    smc.smc_board_init.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_char_p, ctypes.c_uint32]
    smc.smc_board_init.restype = ctypes.c_int16

    ip = b"192.168.1.11"

    log_path = b"/tmp/ltsmc_debug.log"
    try:
        ret = smc.smc_set_debug_mode(1, log_path)
        print("smc_set_debug_mode ret:", ret, "log:", log_path.decode())
        ret = smc.smc_set_connect_debug_time(0, 60)
        print("smc_set_connect_debug_time ret:", ret)

        mode = ctypes.c_uint16(0)
        buf = ctypes.create_string_buffer(260)
        ret = smc.smc_get_debug_mode(ctypes.byref(mode), buf)
        print("smc_get_debug_mode ret:", ret, "mode:", mode.value, "file:", buf.value.decode(errors="ignore"))
    except Exception as e:
        print("Enable debug failed:", e)

    print("Setting connect timeout...")
    ret = smc.smc_set_connect_timeout(60000)
    print("smc_set_connect_timeout ret:", ret)

    print("Calling smc_board_init(connectNo=0, type=2, ip=192.168.1.11)...")
    t0 = time.time()
    ret = smc.smc_board_init(0, 2, ip, 0)
    dt = time.time() - t0
    print("smc_board_init ret:", ret, "elapsed_s:", round(dt, 3))

    time.sleep(5)


if __name__ == "__main__":
    main()
