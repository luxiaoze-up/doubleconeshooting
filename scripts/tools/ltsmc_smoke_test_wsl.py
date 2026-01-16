from __future__ import annotations

import ctypes
import time
from pathlib import Path


def _bind_if_present(smc: ctypes.CDLL, name: str, argtypes, restype) -> bool:
    fn = getattr(smc, name, None)
    if fn is None:
        return False
    fn.argtypes = argtypes
    fn.restype = restype
    return True


def main() -> None:
    so_path = Path("/mnt/d/00.My_workspace/DoubleConeShooting/lib/libLTSMC.so")
    if not so_path.exists():
        raise SystemExit(f"libLTSMC.so not found: {so_path}")

    smc = ctypes.CDLL(str(so_path))

    # Core connect lifecycle
    smc.smc_set_connect_timeout.argtypes = [ctypes.c_uint32]
    smc.smc_set_connect_timeout.restype = ctypes.c_int16

    smc.smc_get_connect_status.argtypes = [ctypes.c_uint16]
    smc.smc_get_connect_status.restype = ctypes.c_int16

    smc.smc_board_init.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_char_p, ctypes.c_uint32]
    smc.smc_board_init.restype = ctypes.c_int16

    smc.smc_board_close.argtypes = [ctypes.c_uint16]
    smc.smc_board_close.restype = ctypes.c_int16

    # Optional post-init probes (safe, pointer-based)
    _bind_if_present(smc, "smc_get_card_version", [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint32)], ctypes.c_int16)
    _bind_if_present(
        smc,
        "smc_get_card_soft_version",
        [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)],
        ctypes.c_int16,
    )
    _bind_if_present(smc, "smc_get_total_axes", [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint32)], ctypes.c_int16)
    _bind_if_present(
        smc,
        "smc_get_total_ionum",
        [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_uint16)],
        ctypes.c_int16,
    )
    _bind_if_present(smc, "smc_get_ipaddr", [ctypes.c_uint16, ctypes.c_char_p], ctypes.c_int16)
    _bind_if_present(smc, "smc_get_release_version", [ctypes.c_uint16, ctypes.c_char_p], ctypes.c_int16)
    _bind_if_present(smc, "smc_read_sn", [ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint64)], ctypes.c_int16)
    _bind_if_present(smc, "smc_check_done", [ctypes.c_uint16, ctypes.c_uint16], ctypes.c_int16)
    _bind_if_present(
        smc,
        "smc_get_position_unit",
        [ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_double)],
        ctypes.c_int16,
    )

    ips = ["192.168.1.11", "192.168.1.12", "192.168.1.13"]

    smc.smc_set_connect_timeout(10000)

    for i, ip in enumerate(ips):
        connect_no = i  # 0/1/2
        print(f"--- connect_no={connect_no} ip={ip} ---")
        ret = smc.smc_board_init(connect_no, 2, ip.encode("ascii"), 0)
        print("smc_board_init ret:", ret)
        status = smc.smc_get_connect_status(connect_no)
        print("smc_get_connect_status:", status)

        # Try a few lightweight queries to force real traffic.
        if ret == 0:
            if hasattr(smc, "smc_get_card_version"):
                card_ver = ctypes.c_uint32(0)
                r = smc.smc_get_card_version(connect_no, ctypes.byref(card_ver))
                print("smc_get_card_version:", r, "value=", card_ver.value)

            if hasattr(smc, "smc_get_card_soft_version"):
                firm = ctypes.c_uint32(0)
                sub = ctypes.c_uint32(0)
                r = smc.smc_get_card_soft_version(connect_no, ctypes.byref(firm), ctypes.byref(sub))
                print("smc_get_card_soft_version:", r, "firm=", firm.value, "sub=", sub.value)

            if hasattr(smc, "smc_get_total_axes"):
                total_axes = ctypes.c_uint32(0)
                r = smc.smc_get_total_axes(connect_no, ctypes.byref(total_axes))
                print("smc_get_total_axes:", r, "value=", total_axes.value)

            if hasattr(smc, "smc_get_total_ionum"):
                total_in = ctypes.c_uint16(0)
                total_out = ctypes.c_uint16(0)
                r = smc.smc_get_total_ionum(connect_no, ctypes.byref(total_in), ctypes.byref(total_out))
                print("smc_get_total_ionum:", r, "in=", total_in.value, "out=", total_out.value)

            if hasattr(smc, "smc_get_ipaddr"):
                buf = ctypes.create_string_buffer(64)
                r = smc.smc_get_ipaddr(connect_no, buf)
                print("smc_get_ipaddr:", r, "value=", buf.value.decode("ascii", errors="ignore"))

            if hasattr(smc, "smc_get_release_version"):
                buf = ctypes.create_string_buffer(64)
                r = smc.smc_get_release_version(connect_no, buf)
                print("smc_get_release_version:", r, "value=", buf.value.decode("ascii", errors="ignore"))

            if hasattr(smc, "smc_read_sn"):
                sn = ctypes.c_uint64(0)
                r = smc.smc_read_sn(connect_no, ctypes.byref(sn))
                print("smc_read_sn:", r, "value=", sn.value)

            # Axis-0 probes (should translate to register reads)
            if hasattr(smc, "smc_check_done"):
                r = smc.smc_check_done(connect_no, 0)
                print("smc_check_done(axis0):", r)

            if hasattr(smc, "smc_get_position_unit"):
                pos = ctypes.c_double(0.0)
                r = smc.smc_get_position_unit(connect_no, 0, ctypes.byref(pos))
                print("smc_get_position_unit(axis0):", r, "pos=", pos.value)

        time.sleep(0.1)

    for i in range(len(ips)):
        smc.smc_board_close(i)


if __name__ == "__main__":
    main()
