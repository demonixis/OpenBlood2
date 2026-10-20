"""Read-only isolated execution of installed MSynth25 controller decoding and pitch bend.
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
    emu.mem_protect(base, image_size, UC_PROT_READ | UC_PROT_EXEC)
    return emu


def decode(status, first, second):
    emu=machine(); stack=0x10e000; synth=0x100000; slot=3; tick=7301; captured=[]
    put=lambda a,v: emu.mem_write(a,struct.pack('<I',v))
    for offset,v in [(0x30,status&0xf0),(0x34,first),(0x38,second)]:put(stack+offset,v)
    for reg,v in [(UC_X86_REG_ESP,stack),(UC_X86_REG_EDI,synth),(UC_X86_REG_ESI,slot),(UC_X86_REG_EBP,tick)]:emu.reg_write(reg,v)
    def hook(u,address,size,user):
        if address==base+0xa160:
            sp=u.reg_read(UC_X86_REG_ESP); ret,t,value=struct.unpack('<III',u.mem_read(sp,12))
            captured.append((u.reg_read(UC_X86_REG_ECX)-synth,t,value))
            u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+12);u.reg_write(UC_X86_REG_EIP,ret)
    emu.hook_add(unicorn.UC_HOOK_CODE,hook)
    emu.emu_start(base+0x3ec4,base+0x3f11,timeout=100000,count=500)
    assert emu.reg_read(UC_X86_REG_EIP)==base+0x3f11
    return captured

# No timing list is required to test the installed synth's retained-value pitch arithmetic.
emu=machine();state=0x101000;stack=0x10e000
emu.mem_write(stack,struct.pack('<I',0x10f100))
emu.reg_write(UC_X86_REG_ECX,state);emu.reg_write(UC_X86_REG_ESP,stack)
emu.emu_start(base+0xa330,0x10f100,timeout=100000,count=100)
assert struct.unpack('<IIII',emu.mem_read(state,16))==(0,0,8192,200)
raw=bytearray();count=0
for cc,offset in [(7,0x34c),(10,0x4cc),(11,0x40c)]:
    for value in range(128):
        result=decode(0xb0,cc,value);assert result==[(offset+3*12,7301,value)]
        raw.extend(struct.pack('<IIII',cc,*result[0]));count+=1
for lo,hi in [(0,0),(127,127),(255,255),(128,64),(0,64)]+[(i*37%256,i*71%256) for i in range(256)]:
    value=(hi<<7)|lo;result=decode(0xe0,lo,hi)
    assert result==[(0x24c+3*16,7301,value)]
    raw.extend(struct.pack('<IIII',0xe0,*result[0]));count+=1
for value in range(32768):
    emu.mem_write(state,struct.pack('<IIII',0,0,value,200))
    emu.mem_write(stack,struct.pack('<II',0x10f100,0))
    emu.reg_write(UC_X86_REG_ECX,state);emu.reg_write(UC_X86_REG_ESP,stack)
    emu.emu_start(base+0xa360,0x10f100,timeout=100000,count=100)
    assert emu.reg_read(UC_X86_REG_EIP)==0x10f100
    cents=struct.unpack('<i',struct.pack('<I',emu.reg_read(UC_X86_REG_EAX)))[0]
    assert cents==(value-8192)*200//8192
    raw.extend(struct.pack('<i',cents))
pitch_fingerprint=14695981039346656037
for byte in raw[-32768*4:]:pitch_fingerprint=((pitch_fingerprint^byte)*1099511628211)&0xffffffffffffffff
fingerprint=14695981039346656037
for byte in raw:fingerprint=((fingerprint^byte)*1099511628211)&0xffffffffffffffff
print(json.dumps(dict(binary='MSynth25.dll',sha256=digest,unicorn=unicorn.__version__,decode_cases=count,pitch_cases=32768,pitch_fnv64=pitch_fingerprint,fnv64=fingerprint,decisions_sha256=hashlib.sha256(raw).hexdigest(),scope='Original CC7/10/11 and pitch-byte decoding with queue insertion intercepted; original default-range signed floor pitch arithmetic. No timing-list scheduling, PCM, OS calls or native DLL execution.'),indent=2))
