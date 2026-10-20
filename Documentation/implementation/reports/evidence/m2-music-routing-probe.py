"""Read-only isolated execution of installed MSynth25 instrument routing.
Requires optional Unicorn 2.1.4. No DLL entrypoint or host OS imports run.
No commercial code, samples, disassembly or asset bytes are exported.
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
    UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_ECX,
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



def machine():
    emu = Uc(UC_ARCH_X86, UC_MODE_32)
    emu.mem_map(base, image_size)
    for index in range(u16(pe + 6)):
        section = sections + 40 * index
        rva, count, offset = u32(section + 12), u32(section + 16), u32(section + 20)
        assert offset + count <= len(data) and rva + count <= image_size
        emu.mem_write(base + rva, data[offset:offset + count])
    emu.mem_map(0x100000, 0x10000)
    # Only the two critical-section imports used by the pure list lookup are stubbed.
    emu.mem_write(0x10f000, b"\xc2\x04\x00")
    for rva in [0x191b4, 0x191b8]:
        emu.mem_write(base + rva, struct.pack('<I', 0x10f000))
    emu.mem_protect(base, image_size, UC_PROT_READ | UC_PROT_EXEC)
    return emu


def packing(bank, program):
    emu = machine()
    header, output, stack = 0x101000, 0x102000, 0x10e000
    emu.mem_write(header + 8, struct.pack('<III', 1, bank, program))
    emu.mem_write(stack + 0x10, bytes(4))
    for reg, value in [(UC_X86_REG_ESI, header), (UC_X86_REG_EDI, output), (UC_X86_REG_ESP, stack)]:
        emu.reg_write(reg, value)
    emu.emu_start(base + 0x8b96, base + 0x8be9, timeout=100000, count=100)
    assert emu.reg_read(UC_X86_REG_EIP) == base + 0x8be9
    return struct.unpack('<I', emu.mem_read(output + 8, 4))[0]


def lookup(percussion, program, key):
    emu = machine()
    synth, collection, stack = 0x100000, 0x102000, 0x10e000
    patches = [(0x12 << 14) | (0x34 << 7) | 57, 0x80000000, 0x80000000 | (1 << 14) | (2 << 7) | 3]
    ranges = [(0, 127), (40, 50), (60, 70)]
    instruments = [0x103000 + i * 0x100 for i in range(3)]
    put = lambda addr, value: emu.mem_write(addr, struct.pack('<I', value))
    put(synth + 0x1c, collection)
    put(collection + 4, instruments[0])
    for i, (addr, patch, (low, high)) in enumerate(zip(instruments, patches, ranges)):
        region = addr + 0x40
        put(addr, instruments[i + 1] if i + 1 < 3 else 0)
        put(addr + 4, region)
        put(addr + 8, patch)
        emu.mem_write(addr + 0x12, struct.pack('<H', 1))
        emu.mem_write(region + 0x28, struct.pack('<H', 1))
        emu.mem_write(region + 0x2d, bytes([high, low]))
    patch = (0x80000000 if percussion else 0) | (1 << 14) | (2 << 7) | program
    # Enter the original note lookup + percussion fallback block. It calls the complete
    # original 0x7940 list/key/refcount matcher. No allocation or synthesis is invoked.
    emu.mem_write(stack + 0x15, bytes([key]))
    put(stack + 0x1c, patch)
    emu.reg_write(UC_X86_REG_EBP, synth)
    emu.reg_write(UC_X86_REG_ESP, stack)
    def stop(u, address, size, user):
        if address in [base + 0x36f8, base + 0x390b]:
            u.emu_stop()
    emu.hook_add(unicorn.UC_HOOK_CODE, stop)
    emu.emu_start(base + 0x36b2, base + 0x390c, timeout=100000, count=5000)
    assert emu.reg_read(UC_X86_REG_EIP) in [base + 0x36f8, base + 0x390b]
    result = emu.reg_read(UC_X86_REG_EAX)
    assert result == 0 or result in instruments
    return instruments.index(result) + 1 if result else 0


# Original reset maps MIDI channels to synth slots; only the slot mapped to MIDI 9 is a drum slot.
reset = machine()
reset.mem_write(0x10e000, struct.pack('<I', 0x10f100))
reset.reg_write(UC_X86_REG_ESP, 0x10e000)
reset.reg_write(UC_X86_REG_ECX, 0x100000)
reset.emu_start(base + 0x2d00, 0x10f100, timeout=100000, count=2000)
assert reset.reg_read(UC_X86_REG_EIP) == 0x10f100
channel_map = list(reset.mem_read(0x100af8, 16))
drum_slots = list(reset.mem_read(0x100b08, 16))
assert sorted(channel_map) == list(range(16))
assert [channel_map[i] for i, drum in enumerate(drum_slots) if drum] == [9]

values = []
for i in range(512):
    bank, program = (i * 0x1020305) & 0xffffffff, (i * 37) & 127
    value = packing(bank, program)
    assert value == program | ((bank & 127) << 7) | ((bank & 0x7f00) << 6) | (bank & 0x80000000)
    values.append(value)
for percussion in [False, True]:
    for program in [0, 3, 99]:
        for key in [0, 39, 40, 50, 51, 59, 60, 70, 71, 127, 128]:
            value = lookup(percussion, program, key)
            expected = (3 if program == 3 and (60 <= key <= 70 or key == 128) else
                        2 if 40 <= key <= 50 or key == 128 else 0) if percussion else 0
            assert value == expected, (percussion, program, key, value, expected)
            values.append(value)
raw = b''.join(struct.pack('<I', value) for value in values)
fingerprint = 14695981039346656037
for byte in raw:
    fingerprint = ((fingerprint ^ byte) * 1099511628211) & 0xffffffffffffffff
print(json.dumps(dict(binary='MSynth25.dll', sha256=digest, unicorn=unicorn.__version__,
                     packed_locale_cases=512, routing_cases=66, fnv64=fingerprint,
                     default_channel_map=channel_map, default_percussion_midi_channel=9,
                     decisions_sha256=hashlib.sha256(raw).hexdigest(),
                     scope='Isolated original locale conversion and resident instrument/key lookup with percussion fallback; no synthesis, residency loader or OS GM collection'), indent=2))
