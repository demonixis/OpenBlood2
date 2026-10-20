"""Read-only isolated execution of installed MSynth25 MIDI note target selection and fast release.
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


def targets(slot,key,nonexclusive,records,off):
    emu=machine();stack=0x10e000;synth=0x100000;region=0x101000;captured=[]
    put=lambda a,v:emu.mem_write(a,struct.pack('<I',v))
    put(stack+0x10,7301)
    emu.mem_write(stack+0x14,bytes([slot,key,0,0]))
    put(synth+0x188,0x102000 if records else 0)
    for i,(channel,note,held,sustained) in enumerate(records):
        address=0x102000+i*0x200
        put(address, address+0x200 if i+1<len(records) else 0)
        put(address+0xe0,held);put(address+0xec,sustained)
        emu.mem_write(address+0xf0,struct.pack('<HH',channel,note))
    emu.mem_write(region+0x2c,bytes([nonexclusive]))
    for reg,v in [(UC_X86_REG_ESP,stack),(UC_X86_REG_EBP,synth),(UC_X86_REG_EAX,region)]:emu.reg_write(reg,v)
    def hook(u,address,size,user):
        if address in (base+0xd0a0,base+0xd130):
            sp=u.reg_read(UC_X86_REG_ESP);ret,tick=struct.unpack('<II',u.mem_read(sp,8))
            captured.append((u.reg_read(UC_X86_REG_ECX)-0x102000)//0x200)
            assert tick==7301
            u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,ret)
    emu.hook_add(unicorn.UC_HOOK_CODE,hook)
    end=base+(0x3911 if off else 0x375a)
    emu.emu_start(base+(0x35a8 if off else 0x3712),end,timeout=100000,count=1000)
    assert emu.reg_read(UC_X86_REG_EIP)==end
    return captured

raw=bytearray();count=0
for slot in range(16):
 for key in (0,60,127):
  records=[((slot+1)%16,key,1,0),(slot,key,0,0),(slot,key,1,1),(slot,key,1,0),(slot,key,1,0),(slot,(key+1)%128,1,0)]
  for off,nonexclusive in [(True,0),(False,0),(False,1)]:
   result=targets(slot,key,nonexclusive,records,off)
   expected=[3] if off else ([] if nonexclusive else [1,2,3,4])
   assert result==expected,(slot,key,off,nonexclusive,result)
   raw.extend(bytes([slot,key,off,nonexclusive,len(result),*result]));count+=1
# Empty/missing lists produce no note release, not a new voice or arbitrary key selection.
assert targets(0,60,0,[],True)==[]
assert targets(0,60,0,[(1,60,1,0)],True)==[]
# Execute the original fast-stop envelope cap with envelope evaluation intercepted.
# This checks the original integer cap and timestamps, not replacement PCM equivalence.
fast_cases=0
for held,sustained in [(1,0),(0,0),(0,1)]:
 for requested in (1,1000):
  for length in (0,100,10000):
   emu=machine();stack=0x10e000;voice=0x102000;fmt=0x103000
   put=lambda a,v:emu.mem_write(a,struct.pack('<I',v))
   for off,v in [(0xe0,held),(0xec,sustained),(0xd4,100),(0xd8,200),(0xc4,fmt),(0x28,1000),(0x44,length)]:put(voice+off,v)
   put(fmt,48000);put(stack,0x10f100);put(stack+4,requested)
   emu.reg_write(UC_X86_REG_ECX,voice);emu.reg_write(UC_X86_REG_ESP,stack)
   def envelope(u,address,size,user):
    if address==base+0xbfd0:
     sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
     u.reg_write(UC_X86_REG_EAX,500);u.reg_write(UC_X86_REG_ESP,sp+16);u.reg_write(UC_X86_REG_EIP,ret)
   emu.hook_add(unicorn.UC_HOOK_CODE,envelope)
   emu.emu_start(base+0xd130,0x10f100,timeout=100000,count=500)
   assert emu.reg_read(UC_X86_REG_EIP)==0x10f100
   get=lambda off:struct.unpack('<I',emu.mem_read(voice+off,4))[0]
   assert get(0x44)==min(length//2,48000//70)
   assert get(0x54)==(max(requested,101) if held or sustained else 200)
   assert get(0xe4)==1 and get(0xe0)==0 and get(0xec)==0
   raw.extend(struct.pack('<III',get(0x44),get(0x54),get(0xd8)));fast_cases+=1
print(json.dumps(dict(binary='MSynth25.dll',sha256=digest,unicorn=unicorn.__version__,selection_cases=count+2,fast_release_cases=fast_cases,
 decisions_sha256=hashlib.sha256(raw).hexdigest(),scope='Original newest-held note-off and self-nonexclusive same-key target selection. Selection stop routines intercepted; fast release integer cap executed with envelope evaluation intercepted. No full synth, PCM, transport or native DLL execution.'),indent=2))
