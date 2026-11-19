# SPDX-License-Identifier: GPL-3.0-or-later
"""Assertions executed by GDB with the guest paused at hardware breakpoints."""
import struct
import traceback

import gdb

EDITED_FILE = b"Hello from Linux Compatibility LibOS\n"
VGA = 0x09400000
TTY_MODE = "'src/tty.c'::mode"


def value(expression):
    return int(gdb.parse_and_eval(expression)) & 0xFFFFFFFF


def memory(address, count):
    return bytes(gdb.selected_inferior().read_memory(address, count))


def word(address):
    return struct.unpack("<I", memory(address, 4))[0]


def domain(slot, field):
    return value(f"domains[{slot - 1}].{field}")


def go(symbol, condition=None):
    address = value("&" + symbol)
    breakpoint = gdb.Breakpoint(f"*{address:#x}", type=gdb.BP_HARDWARE_BREAKPOINT)
    try:
        if condition:
            breakpoint.condition = condition
        gdb.execute("continue")
        assert value("$eip") == address, f"unexpected stop before {symbol}"
    finally:
        breakpoint.delete()


def stage(number):
    go("libos_observe", f"*(unsigned int *)($esp + 4) == {number}")


def input_ready(name):
    with open(RUN_DIRECTORY + "/" + name, "w") as marker:
        marker.write("ready\n")


def pte(slot, address):
    pde = word(domain(slot, "space.pages[0]") + 4 * (address >> 22))
    if not pde & 1:
        return 0
    return word((pde & ~4095) + 4 * ((address >> 12) & 1023))


def user(slot):
    assert value("current_domain->id") == slot and domain(slot, "state") == 2
    assert value("$cs") == 0x1B and value("$ss") == 0x23
    assert value("$cr3") == domain(slot, "space.pages[0]")
    assert value("$cr3") != value("&page_directory")
    assert 0x083FC000 <= value("$esp") < 0x08400000
    assert value("$eflags") & 0x3200 == 0x200
    assert value("tss.esp0") == value(f"&domains[{slot - 1}].kernel_stack") + 8192
    assert pte(slot, 0x08000000) & 7 == 5
    assert pte(slot, value("$esp")) & 7 == 7
    print("CPL3: CS=%#x SS=%#x CR3=%#x ESP=%#x" %
          tuple(value(register) for register in ("$cs", "$ss", "$cr3", "$esp")))


def abi():
    assert [value(name) for name in ("libos_version", "libos_features",
                                    "libos_page_limit", "libos_binding_limit")] == [0x10002, 119, 4, 4]


def arena(slot):
    physical = [pte(slot, 0x09000000 + i * 4096) for i in range(4)]
    assert all(entry & 7 == 7 for entry in physical)
    physical = [entry & ~4095 for entry in physical]
    assert len(set(physical)) == domain(slot, "resource_count") == 4
    owned = []
    for index in range(16):
        prefix = f"resources[{index}]."
        if value(prefix + "state") != 1:
            continue
        assert value(prefix + "cap.owner_id") == slot
        assert value(prefix + "cap.owner_epoch") == domain(slot, "epoch")
        assert value(prefix + "cap.rights") == 47 and value(prefix + "references") == 1
        owned.append(value(prefix + "physical_address"))
    assert set(owned) == set(physical)
    return physical


KERNEL_KBD = "'kernel/keyboard.c'::keyboard"


def devices():
    assert pte(1, VGA) & 0xFFFFF09F == 0xB801F
    for device, rights in (("vga_device", 3), (KERNEL_KBD, 1)):
        assert value(device + ".grant_id") == value(device + ".cap.owner_id") == 1
        assert value(device + ".cap.owner_epoch") == domain(1, "epoch")
        assert value(device + ".cap.rights") == rights and value(device + ".active") == 1


def finished(expected, image_pages, extra=()):
    go("multi_reaped")
    assert value("current_domain") == 0 and value("$cr3") == value("&page_directory")
    for slot, (state, status) in enumerate(expected, 1):
        assert domain(slot, "last_state") == state
        assert domain(slot, "exit_status") == status
        for field in ("state", "space.live", "space.count", "space.dynamic_table",
                      "space.device_table", "resource_count", "pool_owned"):
            assert domain(slot, field) == 0, field
    assert not any(value(f"resources[{i}].state") for i in range(16))
    assert not any(value(f"events[{i}].active") for i in range(8))
    for field in ("vga_device.grant_id", KERNEL_KBD + ".grant_id",
                  KERNEL_KBD + ".enabled", KERNEL_KBD + ".count"):
        assert value(field) == 0
    for address in set(image_pages).union(extra):
        assert value(f"states[{address >> 12}]") == 2 and not any(memory(address, 4096))
    free_pages = memory(value("&states"), 16384).count(2)
    assert free_pages == value("multi_free_before")
    irq0, irq1 = value("irq_count"), value("keyboard_irqs")
    assert irq0 == value("eoi_count") and irq1 == value("keyboard_eois")
    if SCENARIO in ("editor", "editor-long", "launcher", "tty"):
        assert irq0 > 0 and irq1 > 0
    print(f"Recovery: {free_pages} free pages; IRQ0={irq0}, IRQ1={irq1}; zeroed mappings")


def app_mappings():
    user(1)
    devices()
    pages = arena(1)
    tables = [domain(1, "space.dynamic_table"), domain(1, "space.device_table")]
    return pages + tables


def app_released():
    stage(4)
    assert all(pte(1, 0x09000000 + i * 4096) == 0 for i in range(4))
    assert pte(1, VGA) == 0 and domain(1, "resource_count") == 0


def editor(image_pages):
    status = 71 if SCENARIO == "oom" else 0
    go("kilo_run" if status else "editorProcessKeypress")
    pages = app_mappings()
    if SCENARIO == "editor":
        assert memory(VGA, 38)[::2] == b"Hello from Liberums"
        go("editorProcessKeypress", "E.cx == 19 && E.cy == 0")
        assert value("column") == 19 and value("row") == 0
        input_ready("cursor-audited")

    stage(3)
    expected = {
        "editor": EDITED_FILE,
        "editor-long": b"x" * 4095 + b"\n",
        "oom": b"\t" * 4095 + b"\n",
    }[SCENARIO]
    assert value("editor_readback_size") == len(expected)
    assert memory(value("&editor_readback"), len(expected)) == expected
    assert value("heap_live_bytes") == 0
    assert value("heap_oom_count") == int(status != 0)
    peak = value("heap_peak_bytes")
    assert 0 < peak < 4 * 4096
    if status:
        requested = value("heap_failed_bytes")
        assert requested > 4 * 4096
        print(f"Kilo OOM: requested={requested} peak={peak}")
    print(f"Kilo reopened {len(expected)} verified bytes; heap peak={peak}")
    app_released()
    finished([(3, status)], image_pages, pages)


def launcher(image_pages):
    go("kilo_run")
    pages = app_mappings()
    generations = [value(device + ".cap.generation") for device in ("vga_device", KERNEL_KBD)]
    cr3 = value("$cr3")

    def returned(count):
        stage(5)
        assert value("current_domain->id") == 1 and value("$cr3") == cr3
        assert value("app_returns") == count and value("app_last_status") == 0
        assert value("app_leak_count") == 0 and value("heap_live_bytes") == 0
        assert value(TTY_MODE + ".c_lflag") == 3  # ICANON | ECHO
        devices()
        assert [value(device + ".cap.generation") for device in ("vga_device", KERNEL_KBD)] == generations
        assert arena(1) == pages[:4]

    def cat():
        go("sbase_cat")
        stack = value("$esp")
        assert word(stack + 4) == 2
        argument = word(word(stack + 8) + 4)
        assert memory(argument, 11) == b"/hello.txt\0"
        go("write", f"*(int *)($esp + 4) == 1 && *(unsigned int *)($esp + 12) == {len(EDITED_FILE)}")
        buffer = word(value("$esp") + 8)
        assert memory(buffer, len(EDITED_FILE)) == EDITED_FILE

    returned(1)
    cat()
    returned(2)
    returned(3)
    cat()
    returned(4)
    returned(5)
    assert value("heap_oom_count") == 0
    print(f"Launcher returned from {value('app_returns')} applications; heap peak={value('heap_peak_bytes')}")
    app_released()
    finished([(3, 0)], image_pages, pages)


def run():
    go("multi_prepared")
    expected_modules = {
        "hello": [0], "editor": [0], "tty": [0], "oom": [0],
        "editor-long": [0], "launcher": [0],
        "no-grant": [0, 0], "no-keyboard-grant": [0, 0],
        "revoke": [0], "forced-revoke": [0], "fault-and-rejection": [0, -8, 0],
    }[SCENARIO]
    assert value("boot.module_count") == len(expected_modules)
    for index, result in enumerate(expected_modules):
        assert value(f"multi_loads[{index}].result") == (result & 0xFFFFFFFF)
    spaces = [
        [domain(slot, f"space.pages[{i}]") for i in range(domain(slot, "space.count"))]
        for slot in range(1, expected_modules.count(0) + 1)
    ]
    image_pages = [page for pages in spaces for page in pages]
    assert len(set(image_pages)) == len(image_pages)
    for slot in range(1, len(spaces) + 1):
        for stack in range(0x083FC000, 0x08400000, 4096):
            entry = pte(slot, stack)
            assert entry & 7 == 7 and not any(memory(entry & ~4095, 4096))
    stage(1)
    user(1)
    abi()
    if SCENARIO == "hello":
        finished([(3, 0)], image_pages)
    elif SCENARIO in ("editor", "editor-long", "oom"):
        editor(image_pages)
    elif SCENARIO == "launcher":
        launcher(image_pages)
    elif SCENARIO == "tty":
        stage(2)
        pages = arena(1)
        assert domain(1, "preemptions") > 0
        fixture_go("tty_read")
        devices()
        assert memory(0x09400000 + 23 * 160, 2) == b"R\x04"
        input_ready("canonical-audited")
        fixture_go("libos_observe", "*(unsigned int *)($esp + 4) == 3")
        assert all(pte(1, 0x09000000 + i * 4096) == 0 for i in range(4))
        fixture_go("libos_observe", "*(unsigned int *)($esp + 4) == 4")
        assert value("tty_overflows") == 1
        finished([(3, 0)], image_pages, pages)
    elif SCENARIO == "no-grant":
        assert value("vga_device.grant_id") == value(KERNEL_KBD + ".grant_id") == 2
        go("vga_device_acquire")
        assert value("current_domain->id") == 1
        finished([(3, 77), (3, 0)], image_pages)
    elif SCENARIO == "no-keyboard-grant":
        assert value("vga_device.grant_id") == 1 and value(KERNEL_KBD + ".grant_id") == 2
        go("keyboard_acquire")
        assert value("current_domain->id") == 1 and pte(1, 0x09400000) & 1
        go("domain_exit", "current_domain->id == 1")
        assert pte(1, 0x09400000) == 0 and value("vga_device.active") == 0
        finished([(3, 77), (3, 0)], image_pages)
    elif SCENARIO in ("revoke", "forced-revoke"):
        stage(2)
        pages = arena(1)
        assert value("phase9_control.submit") == 0
        forced = SCENARIO == "forced-revoke"
        gdb.execute(f"set var fixture_mode = {2 if forced else 1}")
        index = next(i for i in range(16) if value(f"resources[{i}].state") == 1)
        fields = {"id": 1, "epoch": domain(1, "epoch"), "index": index + 1,
                  "generation": value(f"resources[{index}].cap.generation"), "delay": 3 if forced else 1000,
                  "submit": 1}
        for field, setting in fields.items():
            gdb.execute(f"set var phase9_control.{field} = {setting}")
        go("phase9_request_done")
        assert value("phase9_control.result") == 0
        go("revoke_completed")
        assert value("acknowledged") == int(not forced) and value("forced") == int(forced)
        assert pte(1, 0x09000000) == 0 and domain(1, "resource_count") == 3
        if forced:
            address = value("&fixture_mode")
            entry = pte(1, address)
            assert entry & 7 == 7
            gdb.selected_inferior().write_memory((entry & ~4095) | (address & 4095), bytes(4))
            go("domain_fault")
            frame = word(value("$esp") + 4)
            assert word(frame + 32) == 14 and word(frame + 36) == 4
            assert word(frame + 44) == 0x1B and value("$cr2") == 0x09000000
            finished([(4, 0)], image_pages, pages)
        else:
            finished([(3, 96)], image_pages, pages)
    else:
        stage(2)
        pages = arena(1)
        go("domain_fault")
        frame = word(value("$esp") + 4)
        assert word(frame + 32) == 6 and word(frame + 36) == 0 and word(frame + 44) == 0x1B
        assert domain(2, "state") == 1 and domain(1, "space.pages[0]") != domain(2, "space.pages[0]")
        finished([(4, 0), (3, 0)], image_pages, pages)
        assert domain(1, "fault_vector") == 6


def fixture_go(symbol, condition=None):
    # Break on an early C assertion failure as well to expose its source line.
    exit_bp = gdb.Breakpoint(f"*{value('&domain_exit'):#x}", type=gdb.BP_HARDWARE_BREAKPOINT)
    try:
        go(symbol, condition)
    except AssertionError as error:
        raise AssertionError(f"guest fixture failed at line {value('fixture_line')}") from error
    finally:
        exit_bp.delete()



def checked_run():
    try:
        run()
    except Exception:
        traceback.print_exc()
        gdb.execute("quit 1")
    print("PASS: " + SCENARIO)
