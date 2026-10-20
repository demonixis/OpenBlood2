"""Read-only isolated execution of installed MSynth25 timed queue insertion, due-note ordering and retained patch/control values.
Requires optional Unicorn 2.1.4. No DLL entrypoint or host OS imports run.
No commercial code, samples, disassembly or asset bytes are exported.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import random
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
    emu.mem_protect(base + 0x18000, 0x1000, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
    return emu

raw=bytearray();queue_cases=0;value_cases=0;patch_cases=0
rng=random.Random(0xB2002026)
for case in range(100):
 emu=machine();queue=0x100000;pool=0x102000;stack=0x10e000;event=0x101000
 put=lambda a,v:emu.mem_write(a,struct.pack('<I',v&0xffffffff))
 get=lambda a:struct.unpack('<I',emu.mem_read(a,4))[0]
 for i in range(80):put(pool+i*12,pool+(i+1)*12 if i<79 else 0)
 put(base+0x183e0,pool)
 def call(rva,args):
  put(stack,0x10f100)
  for i,v in enumerate(args):put(stack+4+i*4,v)
  emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_ECX,queue)
  emu.emu_start(base+rva,0x10f100,timeout=100000,count=10000)
  assert emu.reg_read(UC_X86_REG_EIP)==0x10f100
  return emu.reg_read(UC_X86_REG_EAX)
 records=[(rng.randrange(-20,21),i%16,30+i,50+i) for i in range(64)]
 for tick,ch,key,vel in records:
  emu.mem_write(event,struct.pack('<iBBBB',tick,ch,key,vel,0))
  assert call(0xa200,[tick,event])==1
 # Same queue implementation used by the control timelines; every equal-time tie keeps insertion order.
 expected=sorted(records,key=lambda r:r[0]);node=get(queue);observed=[]
 while node:
  tick,value=struct.unpack('<iI',emu.mem_read(node+4,8));observed.append((tick,(value>>16)&255,(value>>8)&255,value&255));node=get(node)
 assert observed==expected
 for horizon in (-21,-20,-1,0,1,20,21):
  # Non-destructive read returns latest <= horizon, including all equal-time ties.
  latest=[r for r in expected if r[0]<=horizon]
  put(queue+8,123)
  expected_value=123 if not latest else latest[-1][1]<<16|latest[-1][2]<<8|latest[-1][3]
  assert call(0xa460,[horizon])==expected_value
  assert call(0xa4b0,[horizon])==expected_value
  raw.extend(struct.pack('<iI',horizon,expected_value));value_cases+=2
 for horizon in (-21,-20,-1,0,1,20,21):
  result=[]
  while call(0xa230,[horizon,event]):
   tick,ch,key,vel,_=struct.unpack('<iBBBB',emu.mem_read(event,8));result.append((tick,ch,key,vel))
  due=[r for r in expected if r[0]<=horizon];expected=[r for r in expected if r[0]>horizon]
  assert result==due
  for r in result:raw.extend(struct.pack('<iBBB',*r))
  queue_cases+=1
 assert get(queue)==0
 # Bank selectors are immediate byte latches; scheduled program captures their current packed value.
 for msb,lsb,program,tick in [(2,3,7,20),(0,1,9,10),(7,2,4,20),(0,0,1,21)]:
  assert call(0xa4e0,[msb])==1 and call(0xa4f0,[lsb])==1
  assert call(0xa500,[tick,program])==1
  patch_cases+=1
 for tick,expected_value in [(9,123),(10,(1<<7)|9),(19,(1<<7)|9),(20,(7<<14)|(2<<7)|4),(21,1)]:
  actual=call(0xa4b0,[tick]);assert actual==expected_value,(tick,actual,expected_value)
  raw.extend(struct.pack('<iI',tick,actual));patch_cases+=1
print(json.dumps(dict(binary='MSynth25.dll',sha256=digest,unicorn=unicorn.__version__,queue_horizons=queue_cases,control_patch_queries=value_cases,bank_patch_cases=patch_cases,decisions_sha256=hashlib.sha256(raw).hexdigest(),scope='Original sorted insertion and equal-time stability; inclusive due extraction, last-due value lookup and submission-time bank capture. Synthetic pool only, no native DLL, OS imports, full synth, PCM or music tick-to-sample conversion.'),indent=2))
