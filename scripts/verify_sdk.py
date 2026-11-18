#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path

sdk = Path(__file__).resolve().parents[1] / "sdk"
manifest = json.loads((sdk / "manifest.json").read_text())
assert manifest["source_commit"] == "7d5d574e8643c3645fa3ce2a126dcbd4ae43662f"
assert manifest["abi"] == {"major": 1, "minor": 2}
headers = sdk / "include" / "abi"
assert {entry.name for entry in headers.iterdir()} == manifest["sha256"].keys()
for name, expected in manifest["sha256"].items():
    assert hashlib.sha256((headers / name).read_bytes()).hexdigest() == expected, name
print("PASS: pinned Liberums SDK")
