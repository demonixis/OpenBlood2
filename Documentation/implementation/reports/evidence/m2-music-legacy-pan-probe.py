"""Bounded private probe of the installed synth's constant-pan dispatch.

Requires Unicorn 2.1.4 in a separate tool environment, not a game dependency.
Reads the user's DLL into an isolated x86 emulator; no DLL entrypoint/import,
OS service, PCM, binary bytes or disassembly is executed/exported on the host.
Only numeric observations and the input hash are printed.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (
    UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EBP,
    UC_X86_REG_EDI, UC_X86_REG_EBX,
)

binary_path = Path(sys.argv[1])
assert 0 < binary_path.stat().st_size <= 1024 * 1024
with binary_path.open("rb") as binary:
    data = binary.read(1024 * 1024 + 1)
assert len(data) <= 1024 * 1024
digest = hashlib.sha256(data).hexdigest()
assert digest == "b30b8be71e7da385deccb18f9f2c2fcd2d25f4355b9a55235242e45389c6fb3e"
u16 = lambda offset: struct.unpack_from("<H", data, offset)[0]
u32 = lambda offset: struct.unpack_from("<I", data, offset)[0]
pe = u32(0x3c)
assert data[pe:pe + 4] == b"PE\0\0" and u16(pe + 4) == 0x14c
optional = pe + 24
assert u16(optional) == 0x10b
base, image_size = u32(optional + 28), u32(optional + 56)
assert base == 0x35490000 and image_size == 0x1d000
sections = optional + u16(pe + 20)


def observe(records):
    emu = Uc(UC_ARCH_X86, UC_MODE_32)
    emu.mem_map(base, image_size)
    for index in range(u16(pe + 6)):
        section = sections + 40 * index
        rva, count, offset = u32(section + 12), u32(section + 16), u32(section + 20)
        assert offset + count <= len(data) and rva + count <= image_size
        emu.mem_write(base + rva, data[offset:offset + count])
    emu.mem_protect(base, image_size, UC_PROT_READ | UC_PROT_EXEC)
    scratch = 0x100000
    emu.mem_map(scratch, 0x4000)
    obj, header, stack = scratch, scratch + 0x1000, scratch + 0x3000
    emu.mem_write(header, struct.pack("<II", 8, len(records)))
    for index, (destination, scale) in enumerate(records):
        emu.mem_write(header + 8 + 12 * index, struct.pack("<HHHHi", 0, 0, destination, 0, scale))
    emu.mem_write(obj + 0x40, struct.pack("<h", 1234))
    emu.mem_write(stack + 0x14, struct.pack("<I", header))
    for register, value in [(UC_X86_REG_ESP, stack), (UC_X86_REG_EBP, header + 8),
                            (UC_X86_REG_EDI, obj), (UC_X86_REG_EBX, 0)]:
        emu.reg_write(register, value)
    # Begin at the source dispatcher, finish after the last 12-byte connection.
    # The pan-only path makes no calls. Instruction and elapsed-time limits are independent.
    emu.emu_start(base + 0x80b1, base + 0x86d8, timeout=100000, count=1000)
    assert emu.reg_read(UC_X86_REG_EIP) == base + 0x86d8
    assert emu.reg_read(UC_X86_REG_EBX) == len(records)
    return struct.unpack("<h", emu.mem_read(obj + 0x40, 2))[0]


results = []
for value in [-500, -406, -400, -300, -200, -6, 0, 6, 200, 306, 354, 400, 500]:
    old, standard = observe([(2, value)]), observe([(4, value * 65536)])
    assert old == standard == int(value * 16 / 125)
    results.append(dict(integer_pan=value, legacy_destination_2=old, standard_destination_4=standard))
order = [dict(records=records, observed=observe(records), expected=expected) for records, expected in [
    ([(2, -500), (4, 500 * 65536)], 64),
    ([(4, 500 * 65536), (2, -500)], -64),
    ([(2, -500), (4, -500 * 65536)], -64),
]]
assert all(item["observed"] == item["expected"] for item in order)
print(json.dumps(dict(
    binary="MSynth25.dll", sha256=digest, unicorn=unicorn.__version__,
    entry_rva="0x80b1", stop_rva="0x86d8", pan_field_offset="0x40",
    cases=results, order_cases=order,
    scope="Isolated original constant-pan dispatch; not complete synth/audio playback or retail parity",
), indent=2))
