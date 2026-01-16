from __future__ import annotations

from pathlib import Path

from pypdf import PdfReader


def main() -> None:
    pdf = Path(r"d:\00.My_workspace\DoubleConeShooting\docs\产品资料\运动控制器\SMC608-BAS用户手册V1.0.pdf")
    if not pdf.exists():
        raise SystemExit(f"PDF not found: {pdf}")

    # 1-based page numbers from our scan results
    pages_1based = sorted(
        {
            4,
            5,
            8,
            9,
            12,
            13,
            18,
            20,
            21,
            25,
            29,
            34,
            35,
            36,
            37,
            38,
            39,
            40,
            41,
            42,
            43,
            44,
            # Extra hits where port / command / protocol details often live
            59,
            60,
            61,
            63,
            64,
            65,
            66,
            67,
            72,
            73,
            74,
            75,
            76,
            81,
            83,
            95,
            97,
        }
    )

    reader = PdfReader(str(pdf))
    print(f"PDF: {pdf.name}")
    print(f"Total pages: {len(reader.pages)}")
    print("=== Extracting pages (1-based):", pages_1based)

    out_dir = Path(r"d:\00.My_workspace\DoubleConeShooting\reports")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "smc608_protocol_pages_extract.txt"

    with out_path.open("w", encoding="utf-8") as f:
        for p1 in pages_1based:
            idx = p1 - 1
            if idx < 0 or idx >= len(reader.pages):
                continue
            text = reader.pages[idx].extract_text() or ""
            f.write("\n" + "=" * 120 + "\n")
            f.write(f"PAGE {p1}\n")
            f.write("=" * 120 + "\n")
            f.write(text)
            f.write("\n")

    print(f"Wrote: {out_path}")


if __name__ == "__main__":
    main()
