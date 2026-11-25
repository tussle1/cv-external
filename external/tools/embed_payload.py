#!/usr/bin/env python3
"""Embed CatVape-Injectable.lua into a C++ header (payload.h)."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
src = root / "CatVape-Injectable.lua"
out = root / "external" / "src" / "injector" / "payload.h"

if not src.exists():
    print(f"missing {src}", file=sys.stderr)
    sys.exit(1)

data = src.read_bytes()
with out.open("w", encoding="utf-8", newline="\n") as f:
    f.write("#pragma once\n")
    f.write("#include <cstddef>\n")
    f.write("#include <cstdint>\n\n")
    f.write("// Embedded CatVape-Injectable.lua payload.\n")
    f.write("// Regenerate with: python3 external/tools/embed_payload.py\n")
    f.write(f"inline constexpr std::size_t kCatVapePayloadSize = {len(data)}u;\n")
    f.write("inline constexpr unsigned char kCatVapePayload[] = {\n")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
    f.write("};\n")
print(f"wrote {out} ({len(data)} bytes)")
