from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_icon.py <logo.png> <output.ico>", file=sys.stderr)
        return 2

    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    if not source.exists():
        print(f"missing logo source: {source}", file=sys.stderr)
        return 3

    output.parent.mkdir(parents=True, exist_ok=True)
    image = Image.open(source).convert("RGBA")
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    image.save(output, format="ICO", sizes=sizes)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
