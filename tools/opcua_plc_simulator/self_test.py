import asyncio
import time

from asyncua import Client, ua


async def main() -> None:
    url = "opc.tcp://127.0.0.1:4840"
    client = Client(url=url)
    last_err = None
    for _ in range(20):
        try:
            await client.connect()
            break
        except Exception as e:
            last_err = e
            await asyncio.sleep(0.2)
    else:
        raise RuntimeError(f"Failed to connect to {url}: {last_err!r}")

    try:
        # 标准/通用写法：不把引号当作 Identifier 内容。
        q_power = client.get_node("ns=3;s=%Q0.1")
        q_run = client.get_node("ns=3;s=%Q0.0")
        i_fb = client.get_node("ns=3;s=%I0.0")
        iw130 = client.get_node("ns=3;s=%IW130")

        # 水路默认正常：水电磁阀 1-4 输出默认应为 True（对应 %Q12.0~%Q12.3）。
        water_outputs = [
            client.get_node("ns=3;s=%Q12.0"),
            client.get_node("ns=3;s=%Q12.1"),
            client.get_node("ns=3;s=%Q12.2"),
            client.get_node("ns=3;s=%Q12.3"),
        ]

        al = await q_power.read_attribute(ua.AttributeIds.AccessLevel)
        ual = await q_power.read_attribute(ua.AttributeIds.UserAccessLevel)
        print("AccessLevel:", getattr(al.Value, "Value", None))
        print("UserAccessLevel:", getattr(ual.Value, "Value", None))

        water_vals = [await n.read_value() for n in water_outputs]
        print("Initial water valves %Q12.0~%Q12.3 ->", water_vals)
        assert all(bool(v) is True for v in water_vals), "Water valves 1-4 should default to True in sim mode"

        iw0 = await iw130.read_value()
        print("Initial %IW130 (foreline gauge voltage word) ->", iw0)

        print("Write %Q0.1 = True")
        await q_power.write_value(True)
        print("Write %Q0.0 = True")
        await q_run.write_value(True)
        await asyncio.sleep(0.3)
        fb1 = await i_fb.read_value()
        print("Read %I0.0 ->", fb1)

        await asyncio.sleep(1.0)
        iw1 = await iw130.read_value()
        print("After pump on, %IW130 ->", iw1)

        print("Write %Q0.1 = False")
        await q_power.write_value(False)
        print("Write %Q0.0 = False")
        await q_run.write_value(False)
        await asyncio.sleep(0.3)
        fb2 = await i_fb.read_value()
        print("Read %I0.0 ->", fb2)
    finally:
        await client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
