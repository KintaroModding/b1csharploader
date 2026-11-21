"""Checks the byte signatures of the native loader against a game exe.

After a game update, run it on the new exe. Each signature must match exactly once:
0 matches means the loader cannot start, more than 1 means it can patch the wrong code.

    python check_signatures.py --exe "<game folder>/b1/Binaries/Win64/b1-Win64-Shipping.exe"

The signatures are read from CSharpLoaderDll/dllmain.cpp. The exe is only read.
"""
import argparse
import mmap
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "CSharpLoaderDll" / "dllmain.cpp"
IMAGE_SCN_MEM_EXECUTE = 0x20000000


def read_signatures(source):
    # signature("48 8D ...") or a signature split over two lines: signature(\n "48 8D ...")
    text = source.read_text(encoding="utf-8-sig")
    return re.findall(r'signature(?:\s+\w+)?\(\s*"([0-9A-Fa-f? ]+)"', text)


def to_regex(sig):
    parts = [b"." if tok.startswith("?") else re.escape(bytes([int(tok, 16)])) for tok in sig.split()]
    return re.compile(b"".join(parts), re.DOTALL)


def sections(image):
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    count = struct.unpack_from("<H", image, pe + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe + 20)[0]
    table = pe + 24 + optional_size
    for i in range(count):
        offset = table + i * 40
        name = image[offset:offset + 8].rstrip(b"\0").decode(errors="replace")
        raw_size, raw_offset = struct.unpack_from("<II", image, offset + 16)
        characteristics = struct.unpack_from("<I", image, offset + 36)[0]
        yield name, raw_offset, raw_size, characteristics


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, type=Path, help="path of b1-Win64-Shipping.exe")
    args = parser.parse_args()

    signatures = read_signatures(SOURCE)
    if not signatures:
        print(f"no signatures found in {SOURCE}", file=sys.stderr)
        return 2

    ok = True
    with open(args.exe, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as image:
        # like the loader: the executable sections first, the whole file when they do not have it
        executable = [s for s in sections(image) if s[3] & IMAGE_SCN_MEM_EXECUTE]
        for sig in signatures:
            regex = to_regex(sig)
            hits = [(name, m.start() - start) for name, start, size, _ in executable
                    for m in regex.finditer(image, start, start + size)]
            if not hits:
                hits = [("file", m.start()) for m in regex.finditer(image)]
            status = "ok" if len(hits) == 1 else ("MISSING" if not hits else "NOT UNIQUE")
            ok = ok and len(hits) == 1
            where = " ".join(f"{name}+{offset:#x}" for name, offset in hits[:4])
            print(f"{status:10} {len(hits):3}  {sig}\n{'':15}{where}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
