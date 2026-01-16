from pypdf import PdfReader
from pathlib import Path

pdf = Path(r"d:\00.My_workspace\DoubleConeShooting\docs\产品资料\运动控制器\SMC608-BAS用户手册V1.0.pdf")
reader = PdfReader(str(pdf))
print("pages", len(reader.pages))

keywords = [
    "以太网",
    "TCP",
    "UDP",
    "socket",
    "端口",
    "通讯",
    "通信",
    "协议",
    "报文",
    "帧",
    "指令",
    "命令",
    "Modbus",
    "OPC",
    "Ethernet",
    "IP",
]

hits: dict[str, list[int]] = {k: [] for k in keywords}

for i, page in enumerate(reader.pages, start=1):
    text = page.extract_text() or ""
    text_l = text.lower()
    for k in keywords:
        if k.isascii():
            if k.lower() in text_l:
                hits[k].append(i)
        else:
            if k in text:
                hits[k].append(i)

for k, pages in hits.items():
    if pages:
        suffix = " ..." if len(pages) > 30 else ""
        print(f"{k}: {pages[:30]}{suffix}")
