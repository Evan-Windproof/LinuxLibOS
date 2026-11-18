#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Boot the pinned kernel under QEMU TCG and drive real PS/2 input via QMP."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def wait_for(check, description, seconds=12):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        result = check()
        if result:
            return result
        time.sleep(0.025)
    raise RuntimeError(f"timed out waiting for {description}")


class Qmp:
    def __init__(self, endpoint):
        self.socket = socket.socket(socket.AF_UNIX)
        self.socket.settimeout(5)
        self.socket.connect(str(endpoint))
        self.stream = self.socket.makefile("rwb")
        assert "QMP" in json.loads(self.stream.readline())
        self.command("qmp_capabilities")

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.stream.close()
        self.socket.close()

    def command(self, name, **arguments):
        self.stream.write(json.dumps({"execute": name, "arguments": arguments}).encode() + b"\n")
        self.stream.flush()
        while True:
            response = json.loads(self.stream.readline())
            if "error" in response:
                raise RuntimeError(response["error"])
            if "return" in response:
                return response["return"]

    def key(self, key):
        response = self.command("human-monitor-command", **{"command-line": f"sendkey {key} 20"})
        assert not response, response
        time.sleep(0.055)

    def key_edge(self, key, down):
        self.command("input-send-event", events=[
            {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": key}}}
        ])

    def screen(self, path):
        response = self.command("human-monitor-command", **{
            "command-line": f'pmemsave 0xb8000 4000 "{path}"'
        })
        assert not response, response
        return path.read_bytes()[::2]


def type_text(qmp, text):
    for letter in text:
        name = "spc" if letter == " " else letter.lower()
        qmp.key("shift-" + name if letter.isupper() else name)


def editor_input(qmp, serial, directory):
    del serial
    screen = directory / "vga.raw"

    def visible(text):
        wait_for(lambda: text in qmp.screen(screen), repr(text))

    visible(b"Hello from Liberums")
    visible(b"HELP: Ctrl-S = save")
    qmp.key("esc")
    for _ in range(19):
        qmp.key("right")
    wait_for(lambda: (directory / "cursor-audited").exists(), "GDB cursor audit")
    for _ in range(8):
        qmp.key("backspace")
    type_text(qmp, "Linux Compatibility LibOS")
    visible(b"Hello from Linux Compatibility LibOS")
    qmp.key("ctrl-s")
    visible(b"37 bytes saved in RAM")
    qmp.key("ctrl-q")


def tty_input(qmp, serial, directory):
    del directory
    wait_for(lambda: "TTY canonical ready" in serial.read_text(), "canonical fixture")
    for key in ("shift-a", "x", "backspace", "b", "ret"):
        qmp.key(key)
    wait_for(lambda: "TTY raw ready" in serial.read_text(), "raw fixture")
    down = False
    for _ in range(100):
        if "TTY overflow recovered" in serial.read_text():
            break
        down = not down
        qmp.key_edge("a", down)
        time.sleep(0.035)
    if down:
        qmp.key_edge("a", False)
    wait_for(lambda: "TTY overflow recovered" in serial.read_text(), "overflow recovery")
    qmp.key("z")


def tools_and_qmp(args, label, directory, modules, options, input_fn):
    out = BUILD / "integration"
    serial = out / f"{label}.serial.log"
    serial.write_text("")
    emulator_log = out / f"{label}.qemu.log"
    gdb_log = out / f"{label}.gdb.log"
    endpoint = directory / "qmp"
    debug = directory / "debug"
    command = [args.qemu, "-machine", "pc", "-accel", "tcg", "-cpu", "qemu32",
               "-m", "32M", "-smp", "1", "-display", "none", "-nic", "none",
               "-serial", f"file:{serial}", "-monitor", "none", "-no-reboot", "-no-shutdown",
               "-qmp", f"unix:{endpoint},server=on,wait=off", "-S",
               "-gdb", f"unix:{debug},server=on,wait=off",
               "-kernel", str(args.kernel), "-append", "multi-external " + options,
               "-initrd", ",".join(str(module) for module in modules)]
    with emulator_log.open("w") as output, subprocess.Popen(command, stdout=output, stderr=output) as vm:
        try:
            def listening():
                if vm.poll() is not None:
                    raise RuntimeError(emulator_log.read_text())
                return endpoint.exists() and debug.exists()

            wait_for(listening, "QEMU sockets")
            gdb_command = [
                args.gdb, "-nx", "-batch", str(args.kernel),
                "-ex", "set confirm off", "-ex", "set debuginfod enabled off",
                "-ex", f"target remote {debug}", "-ex", f"add-symbol-file {modules[0]}",
                "-ex", f"python SCENARIO={label!r}; RUN_DIRECTORY={str(directory)!r}",
                "-ex", f"source {ROOT / 'tests/gdb_checks.py'}",
                "-ex", "python checked_run()",
                "-ex", "detach",
            ]
            with Qmp(endpoint) as qmp, ThreadPoolExecutor(max_workers=1) as executor, gdb_log.open("w") as output:
                job = executor.submit(input_fn, qmp, serial, directory) if input_fn else None
                result = subprocess.run(gdb_command, stdout=output, stderr=output,
                                        timeout=45, cwd=ROOT)
                audit = gdb_log.read_text()
                if result.returncode or f"PASS: {label}" not in audit:
                    raise RuntimeError(audit)
                if job:
                    job.result()
            wait_for(lambda: "multi external execution completed" in serial.read_text(),
                     "kernel lifecycle completion")
            guest_log = serial.read_text()
            assert "PANIC" not in guest_log
            if label == "oom":
                assert "Kilo OOM: editorUpdateSyntax requested 4095 bytes" in guest_log
            if label == "editor":
                assert "RAM file reopened; session closed" in guest_log
            print(f"PASS: {label}", flush=True)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path, required=True)
    parser.add_argument("--qemu", default="qemu-system-i386")
    parser.add_argument("--gdb", default="gdb")
    parser.add_argument("--scenario", action="append", help="run only named scenarios")
    args = parser.parse_args()
    os.environ["DEBUGINFOD_URLS"] = ""
    grants = "multi-vga=1 multi-keyboard=1"
    cases = {
        "hello": (["hello.elf"], "", None),
        "editor": (["editor.elf"], grants, editor_input),
        "tty": (["guest-test.elf"], grants, tty_input),
        "no-grant": (["editor.elf", "hello.elf"], "multi-vga=2 multi-keyboard=2", None),
        "no-keyboard-grant": (["editor.elf", "hello.elf"], "multi-vga=1 multi-keyboard=2", None),
        "oom": (["editor-oom.elf"], grants, None),
        "revoke": (["guest-test.elf"], "selftest=phase9", None),
        "forced-revoke": (["guest-test.elf"], "selftest=phase9", None),
        "fault-and-rejection": (["fault-fixture.elf", "bad.elf", "hello.elf"], "", None),
    }
    selected = args.scenario or list(cases)
    for label in selected:
        if label not in cases:
            parser.error(f"unknown scenario {label}")
    out = BUILD / "integration"
    out.mkdir(exist_ok=True)
    report = out / "results.json"
    report.unlink(missing_ok=True)
    kernel_hash = sha(args.kernel)
    results = []
    for label in selected:
        modules, options, input_fn = cases[label]
        with tempfile.TemporaryDirectory(prefix="libos-") as temp:
            directory = Path(temp)
            bad = directory / "bad.elf"
            bad.write_bytes(b"invalid ELF" * 8)
            files = [bad if name == "bad.elf" else BUILD / name for name in modules]
            tools_and_qmp(args, label, directory, files, options, input_fn)
            assert sha(args.kernel) == kernel_hash, "kernel changed during integration"
            results.append({"scenario": label, "elf_sha256": [sha(path) for path in files]})
    report.write_text(json.dumps({"kernel_sha256": kernel_hash, "passed": results}, indent=2) + "\n")
    print(f"PASS: {len(results)} QEMU/GDB scenarios, unchanged kernel {kernel_hash}")


if __name__ == "__main__":
    main()
