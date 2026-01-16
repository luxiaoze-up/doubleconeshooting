#!/usr/bin/env python3

from __future__ import annotations

import argparse
import asyncio
import dataclasses
import datetime as _dt
import json
import struct
from pathlib import Path
from typing import Optional


@dataclasses.dataclass
class ModbusRequest:
    transaction_id: int
    protocol_id: int
    length: int
    unit_id: int
    function_code: int
    pdu: bytes
    header_endian: str = "be"  # "be" or "le"


@dataclasses.dataclass
class ControllerState:
    unit_id: int
    holding_registers: bytearray  # big-endian uint16 array stored as bytes
    input_registers: bytearray  # big-endian uint16 array stored as bytes
    coils: bytearray  # bits
    discrete_inputs: bytearray  # bits

    def read_u16(self, addr: int, count: int) -> bytes:
        start = addr * 2
        end = start + count * 2
        if start < 0 or end > len(self.holding_registers):
            raise IndexError("register out of range")
        return bytes(self.holding_registers[start:end])

    def read_input_u16(self, addr: int, count: int) -> bytes:
        start = addr * 2
        end = start + count * 2
        if start < 0 or end > len(self.input_registers):
            raise IndexError("register out of range")
        return bytes(self.input_registers[start:end])

    def write_u16(self, addr: int, values_be: bytes) -> None:
        if len(values_be) % 2 != 0:
            raise ValueError("values length must be even")
        start = addr * 2
        end = start + len(values_be)
        if start < 0 or end > len(self.holding_registers):
            raise IndexError("register out of range")
        self.holding_registers[start:end] = values_be
        # Most integrations treat input registers as a read-only mirror of internal state.
        # To keep clients simple, we mirror writes into input_registers by default.
        if end <= len(self.input_registers):
            self.input_registers[start:end] = values_be

    def _read_bits(self, blob: bytearray, addr: int, count: int) -> bytes:
        if addr < 0 or count < 0:
            raise IndexError("bit out of range")
        out = bytearray((count + 7) // 8)
        for i in range(count):
            bit_index = addr + i
            byte_index = bit_index // 8
            bit_in_byte = bit_index % 8
            if byte_index >= len(blob):
                raise IndexError("bit out of range")
            if (blob[byte_index] >> bit_in_byte) & 1:
                out[i // 8] |= 1 << (i % 8)
        return bytes(out)

    def _write_bits(self, blob: bytearray, addr: int, bits: bytes, count: int) -> None:
        if addr < 0 or count < 0:
            raise IndexError("bit out of range")
        for i in range(count):
            bit_index = addr + i
            byte_index = bit_index // 8
            bit_in_byte = bit_index % 8
            if byte_index >= len(blob):
                raise IndexError("bit out of range")
            value = (bits[i // 8] >> (i % 8)) & 1
            if value:
                blob[byte_index] |= 1 << bit_in_byte
            else:
                blob[byte_index] &= ~(1 << bit_in_byte)

    def read_coils(self, addr: int, count: int) -> bytes:
        return self._read_bits(self.coils, addr, count)

    def read_discrete_inputs(self, addr: int, count: int) -> bytes:
        return self._read_bits(self.discrete_inputs, addr, count)

    def write_coils(self, addr: int, bits: bytes, count: int) -> None:
        self._write_bits(self.coils, addr, bits, count)


def _now() -> str:
    return _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def parse_modbus_tcp_request(data: bytes, header_endian: str = "be") -> ModbusRequest:
    if len(data) < 8:
        raise ValueError("frame too short")

    if header_endian == "le":
        transaction_id, protocol_id, length = struct.unpack("<HHH", data[0:6])
    else:
        transaction_id, protocol_id, length = struct.unpack(">HHH", data[0:6])
    if len(data) < 6 + length:
        raise ValueError("incomplete frame")

    unit_id = data[6]
    function_code = data[7]
    pdu = data[7 : 6 + length]  # includes function code

    return ModbusRequest(
        transaction_id=transaction_id,
        protocol_id=protocol_id,
        length=length,
        unit_id=unit_id,
        function_code=function_code,
        pdu=pdu,
        header_endian=header_endian,
    )


def build_mbap(
    transaction_id: int,
    protocol_id: int,
    unit_id: int,
    pdu: bytes,
    header_endian: str = "be",
) -> bytes:
    # length = unit_id(1) + pdu_len
    length = 1 + len(pdu)
    if header_endian == "le":
        return struct.pack("<HHHB", transaction_id, protocol_id, length, unit_id) + pdu
    return struct.pack(">HHHB", transaction_id, protocol_id, length, unit_id) + pdu


def exception_response(req: ModbusRequest, exc_code: int) -> bytes:
    pdu = bytes([req.function_code | 0x80, exc_code])
    return build_mbap(
        req.transaction_id,
        req.protocol_id,
        req.unit_id,
        pdu,
        header_endian=req.header_endian,
    )


def handle_request(req: ModbusRequest, state: ControllerState) -> bytes:
    # Modbus/TCP clients vary in what they send as UnitId (often 0, 1, 0xFF, or a station id).
    # For best compatibility with vendor drivers, accept any UnitId and echo it back.

    fc = req.function_code

    try:
        if fc in (0x01, 0x02):
            # Read Coils (0x01) / Read Discrete Inputs (0x02)
            if len(req.pdu) < 5:
                return exception_response(req, 0x03)
            addr, count = struct.unpack(">HH", req.pdu[1:5])
            data = (
                state.read_coils(addr, count)
                if fc == 0x01
                else state.read_discrete_inputs(addr, count)
            )
            pdu = bytes([fc, len(data)]) + data
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x03:
            # Read Holding Registers
            if len(req.pdu) < 5:
                return exception_response(req, 0x03)
            addr, count = struct.unpack(">HH", req.pdu[1:5])
            data = state.read_u16(addr, count)
            pdu = bytes([fc, len(data)]) + data
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x04:
            # Read Input Registers
            if len(req.pdu) < 5:
                return exception_response(req, 0x03)
            addr, count = struct.unpack(">HH", req.pdu[1:5])
            data = state.read_input_u16(addr, count)
            pdu = bytes([fc, len(data)]) + data
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x06:
            # Write Single Register
            if len(req.pdu) < 5:
                return exception_response(req, 0x03)
            addr, value = struct.unpack(">HH", req.pdu[1:5])
            state.write_u16(addr, struct.pack(">H", value))
            # Echo request PDU
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, req.pdu, header_endian=req.header_endian)

        if fc == 0x05:
            # Write Single Coil
            if len(req.pdu) < 5:
                return exception_response(req, 0x03)
            addr, value = struct.unpack(">HH", req.pdu[1:5])
            if value not in (0x0000, 0xFF00):
                return exception_response(req, 0x03)
            bit = b"\x01" if value == 0xFF00 else b"\x00"
            state.write_coils(addr, bit, 1)
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, req.pdu, header_endian=req.header_endian)

        if fc == 0x10:
            # Write Multiple Registers
            if len(req.pdu) < 6:
                return exception_response(req, 0x03)
            addr, count, byte_count = struct.unpack(">HHB", req.pdu[1:6])
            values = req.pdu[6 : 6 + byte_count]
            if len(values) != byte_count or byte_count != count * 2:
                return exception_response(req, 0x03)
            state.write_u16(addr, values)
            # Response: fc + addr + count
            pdu = struct.pack(">BHH", fc, addr, count)
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x17:
            # Read/Write Multiple Registers
            # Request PDU: fc(1) + read_addr(2) + read_qty(2) + write_addr(2) + write_qty(2) + byte_count(1) + write_values
            if len(req.pdu) < 10:
                return exception_response(req, 0x03)
            read_addr, read_qty, write_addr, write_qty, byte_count = struct.unpack(">HHHHB", req.pdu[1:10])
            write_values = req.pdu[10 : 10 + byte_count]
            if len(write_values) != byte_count or byte_count != write_qty * 2:
                return exception_response(req, 0x03)
            state.write_u16(write_addr, write_values)
            data = state.read_u16(read_addr, read_qty)
            pdu = bytes([fc, len(data)]) + data
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x2B:
            # Encapsulated Interface Transport (MEI)
            # Commonly used as 0x2B/0x0E Read Device Identification
            if len(req.pdu) < 4:
                return exception_response(req, 0x03)
            mei_type = req.pdu[1]
            if mei_type != 0x0E:
                return exception_response(req, 0x01)

            read_dev_id_code = req.pdu[2]
            object_id = req.pdu[3]

            # Minimal "basic" identification set
            # Objects: 0x00 VendorName, 0x01 ProductCode, 0x02 MajorMinorRevision
            objects = {
                0x00: b"LEISAI",
                0x01: b"SMC608-BAS-SIM",
                0x02: b"1.0",
            }

            # Build object list starting from object_id
            obj_items = []
            for oid in sorted(objects.keys()):
                if oid < object_id:
                    continue
                value = objects[oid]
                obj_items.append(bytes([oid, len(value)]) + value)

            conformity = 0x01
            more_follows = 0x00
            next_object_id = 0x00
            number_of_objects = len(obj_items)
            payload = (
                bytes([0x2B, mei_type, read_dev_id_code, conformity, more_follows, next_object_id, number_of_objects])
                + b"".join(obj_items)
            )
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, payload, header_endian=req.header_endian)

        if fc == 0x0F:
            # Write Multiple Coils
            if len(req.pdu) < 6:
                return exception_response(req, 0x03)
            addr, count, byte_count = struct.unpack(">HHB", req.pdu[1:6])
            values = req.pdu[6 : 6 + byte_count]
            if len(values) != byte_count:
                return exception_response(req, 0x03)
            state.write_coils(addr, values, count)
            pdu = struct.pack(">BHH", fc, addr, count)
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        if fc == 0x11:
            # Report Slave ID (basic stub)
            slave_id = 0x01
            run_status = 0xFF
            data = bytes([slave_id, run_status])
            pdu = bytes([fc, len(data)]) + data
            return build_mbap(req.transaction_id, req.protocol_id, req.unit_id, pdu, header_endian=req.header_endian)

        # Unsupported function code
        return exception_response(req, 0x01)

    except IndexError:
        return exception_response(req, 0x02)  # illegal data address
    except Exception:
        return exception_response(req, 0x04)  # server device failure


class ModbusTcpServer:
    def __init__(
        self,
        host: str,
        port: int,
        unit_id: int,
        log_path: Optional[Path],
        raw_log: bool,
        init_file: Optional[Path],
    ) -> None:
        self._host = host
        self._port = port
        self._state = ControllerState(
            unit_id=unit_id,
            holding_registers=bytearray(65536 * 2),
            input_registers=bytearray(65536 * 2),
            coils=bytearray(65536 // 8),
            discrete_inputs=bytearray(65536 // 8),
        )
        self._log_path = log_path
        self._lock = asyncio.Lock()
        self._raw_log = raw_log
        self._init_file = init_file
        self._apply_init_file()

    def _apply_init_file(self) -> None:
        if not self._init_file:
            return
        try:
            data = json.loads(self._init_file.read_text(encoding="utf-8"))
        except Exception:
            return
        # Minimal init format:
        # {"holding_registers": {"0": 123, "1": 456}, "coils": {"0": 1}}
        try:
            regs = data.get("holding_registers", {})
            for k, v in regs.items():
                addr = int(k)
                value = int(v) & 0xFFFF
                self._state.write_u16(addr, struct.pack(">H", value))
        except Exception:
            pass
        try:
            coils = data.get("coils", {})
            for k, v in coils.items():
                addr = int(k)
                bit = b"\x01" if int(v) else b"\x00"
                self._state.write_coils(addr, bit, 1)
        except Exception:
            pass

    async def _log(self, line: str) -> None:
        line = f"{_now()} [{self._host}:{self._port}] {line}"
        print(line, flush=True)
        if self._log_path:
            self._log_path.parent.mkdir(parents=True, exist_ok=True)
            async with self._lock:
                self._log_path.open("a", encoding="utf-8").write(line + "\n")

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peer = writer.get_extra_info("peername")
        await self._log(f"CONNECT peer={peer}")

        try:
            while True:
                # Read MBAP header first. If the client disconnects mid-frame, log whatever we got.
                try:
                    header = await reader.readexactly(6)
                except asyncio.IncompleteReadError as e:
                    if e.partial:
                        await self._log(
                            f"RX_PARTIAL stage=header got={len(e.partial)} expected=6 hex={e.partial.hex()}"
                        )
                    else:
                        await self._log("EOF_NO_DATA")
                    break

                be_tid, be_pid, be_length = struct.unpack(">HHH", header)
                le_tid, le_pid, le_length = struct.unpack("<HHH", header)

                header_endian = "be"
                transaction_id, protocol_id, length = be_tid, be_pid, be_length

                # Heuristic: some vendor stacks send the MBAP 16-bit fields little-endian.
                # Prefer any interpretation that yields a sane length.
                if not (2 <= be_length <= 260) and (2 <= le_length <= 260):
                    header_endian = "le"
                    transaction_id, protocol_id, length = le_tid, le_pid, le_length
                    await self._log(
                        "MBAP_LITTLE_ENDIAN_DETECTED "
                        f"header_hex={header.hex()} be(tid={be_tid} pid={be_pid} len={be_length}) "
                        f"le(tid={le_tid} pid={le_pid} len={le_length})"
                    )

                # Modbus/TCP length is UnitId(1) + PDU, typical max is small (< 260).
                # If this still looks insane, log and bail (could be a different protocol).
                if length < 2 or length > 260:
                    tail = await reader.read(512)
                    await self._log(
                        "NON_MBAP_OR_CORRUPT "
                        f"header_hex={header.hex()} "
                        f"be(tid={be_tid} pid={be_pid} len={be_length}) "
                        f"le(tid={le_tid} pid={le_pid} len={le_length}) "
                        f"tail_hex={tail.hex()}"
                    )
                    break

                # Some vendor stacks (including LTSMC) appear to send a non-Modbus handshake where
                # the "length" field does not match the bytes that follow. To avoid blocking until
                # the client times out, we read up to `length` with a short timeout and proceed.
                rest = b""
                payload_deadline = asyncio.get_running_loop().time() + 0.2
                while len(rest) < length:
                    timeout = payload_deadline - asyncio.get_running_loop().time()
                    if timeout <= 0:
                        break
                    try:
                        chunk = await asyncio.wait_for(reader.read(length - len(rest)), timeout=timeout)
                    except asyncio.TimeoutError:
                        break
                    if not chunk:
                        break
                    rest += chunk

                if len(rest) < length:
                    await self._log(
                        "RX_SHORT "
                        f"tid={transaction_id} pid={protocol_id} expected={length} got={len(rest)} "
                        f"header_hex={header.hex()} partial_hex={rest.hex()}"
                    )

                frame = header + rest

                # If this isn't Modbus/TCP (protocol_id should be 0), treat it as a vendor frame.
                # Observed with LTSMC: pid=7 + little-endian MBAP + inconsistent payload length.
                # Respond with a payload padded/truncated to the advertised length to maximize compatibility.
                if protocol_id != 0:
                    await self._log(
                        "VENDOR_FRAME "
                        f"endian={header_endian} tid={transaction_id} pid={protocol_id} len_field={length} rx_len={len(rest)} "
                        f"hex={frame.hex()}"
                    )

                    vendor_payload = rest
                    if len(vendor_payload) < length:
                        vendor_payload = vendor_payload + (b"\x00" * (length - len(vendor_payload)))
                    elif len(vendor_payload) > length:
                        vendor_payload = vendor_payload[:length]

                    tx = header + vendor_payload
                    if self._raw_log:
                        await self._log(f"TX_VENDOR_RAW {tx.hex()}")
                    await self._log(
                        f"TX_VENDOR endian={header_endian} tid={transaction_id} pid={protocol_id} tx_len={len(vendor_payload)}"
                    )
                    writer.write(tx)
                    await writer.drain()
                    continue

                # Normal Modbus/TCP path: only parse if the frame is complete.
                if len(rest) != length:
                    await self._log(
                        "DROP_INCOMPLETE_MODBUS "
                        f"endian={header_endian} tid={transaction_id} pid={protocol_id} expected={length} got={len(rest)}"
                    )
                    continue

                req = parse_modbus_tcp_request(frame, header_endian=header_endian)

                if req.unit_id != self._state.unit_id:
                    await self._log(
                        f"UNIT_ID_MISMATCH expected={self._state.unit_id} got={req.unit_id} (accepting)"
                    )

                if self._raw_log:
                    await self._log(f"RX_RAW {frame.hex()}")

                # Pretty log for reverse-engineering
                if len(req.pdu) >= 5 and req.function_code in (0x01, 0x02, 0x03, 0x04):
                    addr, count = struct.unpack(">HH", req.pdu[1:5])
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x{req.function_code:02X} addr={addr} count={count}"
                    )
                elif len(req.pdu) >= 5 and req.function_code == 0x05:
                    addr, value = struct.unpack(">HH", req.pdu[1:5])
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x05 addr={addr} value=0x{value:04X}"
                    )
                elif len(req.pdu) >= 5 and req.function_code == 0x06:
                    addr, value = struct.unpack(">HH", req.pdu[1:5])
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x06 addr={addr} value=0x{value:04X} ({value})"
                    )
                elif len(req.pdu) >= 6 and req.function_code == 0x0F:
                    addr, count, byte_count = struct.unpack(">HHB", req.pdu[1:6])
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x0F addr={addr} count={count} bytes={byte_count}"
                    )
                elif len(req.pdu) >= 6 and req.function_code == 0x10:
                    addr, count, byte_count = struct.unpack(">HHB", req.pdu[1:6])
                    values = req.pdu[6 : 6 + byte_count]
                    preview_words = []
                    for i in range(0, min(len(values), 16), 2):
                        preview_words.append(struct.unpack(">H", values[i : i + 2])[0])
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x10 addr={addr} count={count} bytes={byte_count} values_hex={values.hex()} preview_u16={preview_words}"
                    )
                elif len(req.pdu) >= 10 and req.function_code == 0x17:
                    read_addr, read_qty, write_addr, write_qty, byte_count = struct.unpack(">HHHHB", req.pdu[1:10])
                    values = req.pdu[10 : 10 + byte_count]
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x17 r_addr={read_addr} r_qty={read_qty} w_addr={write_addr} w_qty={write_qty} bytes={byte_count} values_hex={values.hex()}"
                    )
                elif len(req.pdu) >= 4 and req.function_code == 0x2B:
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x2B mei=0x{req.pdu[1]:02X} code=0x{req.pdu[2]:02X} obj=0x{req.pdu[3]:02X}"
                    )
                else:
                    await self._log(
                        f"RX tid={transaction_id} uid={req.unit_id} fc=0x{req.function_code:02X} pdu_len={len(req.pdu)}"
                    )

                resp = handle_request(req, self._state)
                if self._raw_log:
                    await self._log(f"TX_RAW {resp.hex()}")
                writer.write(resp)
                await writer.drain()

        except asyncio.IncompleteReadError as e:
            # Should be largely handled by the inner reads, but keep a safety net.
            if e.partial:
                await self._log(f"RX_PARTIAL stage=unknown got={len(e.partial)} hex={e.partial.hex()}")
        except Exception as e:
            await self._log(f"ERROR peer={peer} err={e!r}")
        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass
            await self._log(f"DISCONNECT peer={peer}")

    async def run(self) -> None:
        server = await asyncio.start_server(self.handle_client, self._host, self._port)
        addrs = ", ".join(str(sock.getsockname()) for sock in server.sockets or [])
        await self._log(f"LISTEN {addrs} unit_id={self._state.unit_id}")
        async with server:
            await server.serve_forever()


async def main_async(args: argparse.Namespace) -> None:
    log_path = Path(args.log) if args.log else None
    init_file = Path(args.init) if args.init else None
    servers = [
        ModbusTcpServer(
            host=ip,
            port=args.port,
            unit_id=args.unit_id,
            log_path=log_path,
            raw_log=args.raw_log,
            init_file=init_file,
        )
        for ip in args.ips
    ]
    await asyncio.gather(*(s.run() for s in servers))


def main() -> None:
    parser = argparse.ArgumentParser(description="SMC608-BAS motion controller Modbus/TCP simulator (port 502)")
    parser.add_argument(
        "--ips",
        nargs="+",
        default=["192.168.1.11", "192.168.1.12", "192.168.1.13"],
        help="IP addresses to bind (need to exist on host, e.g. add as loopback aliases)",
    )
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--unit-id", type=int, default=8, help="Modbus unit id (manual mentions station=8)")
    default_log = str((Path(__file__).resolve().parent / "sim.log").as_posix())
    parser.add_argument("--log", type=str, default=default_log)
    parser.add_argument("--raw-log", action="store_true", help="Log raw Modbus/TCP frames in hex")
    parser.add_argument(
        "--init",
        type=str,
        default="",
        help="Optional JSON file to initialize registers/coils (minimal format)",
    )

    args = parser.parse_args()
    try:
        print(f"Using log file: {args.log}", flush=True)
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        return


if __name__ == "__main__":
    main()
