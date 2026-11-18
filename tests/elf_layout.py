#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import struct
import sys
from pathlib import Path


def inspect(path):
    data = path.read_bytes()
    assert 52 < len(data) <= 0x40000
    assert data[:7] == b"\x7fELF\x01\x01\x01"
    kind, machine, version, entry, offset = struct.unpack_from("<HHIII", data, 16)
    assert (kind, machine, version, entry) == (2, 3, 1, 0x08000000)
    ph_size, ph_count = struct.unpack_from("<HH", data, 42)
    assert (ph_size, ph_count) == (32, 4)
    pages = set()
    notes, load_flags = [], []
    for i in range(ph_count):
        kind, start, va, _, file_size, mem_size, flags, align = struct.unpack_from(
            "<8I", data, offset + i * ph_size
        )
        assert start + file_size <= len(data)
        if kind == 4:
            notes.append(data[start:start + file_size])
            continue
        assert kind == 1 and align == 4096 and start % align == va % align == 0
        assert mem_size >= file_size and 0x08000000 <= va < va + mem_size <= 0x08040000
        new_pages = set(range(va, va + mem_size, 4096))
        assert not pages.intersection(new_pages)
        pages.update(new_pages)
        load_flags.append(flags)
    note = struct.pack("<III12sIII", 9, 12, 0x4C414249, b"LIBERUMS", 1, 2, 117)
    assert notes == [note]
    assert load_flags == [5, 4, 6] and len(pages) <= 64
    print(f"PASS: {path} ELF32, {len(data)} bytes, {len(pages)} mapped image pages")


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        inspect(Path(argument))
