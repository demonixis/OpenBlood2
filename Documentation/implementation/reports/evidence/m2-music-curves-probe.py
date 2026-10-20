"""Original IMA style curve reader, isolated x86 execution. Requires optional Unicorn 2.1.4.
Usage: python probe.py EXTERNAL_GAME_ROOT
Only aggregate hashes/counts leave this tool; no native DLL/OS or synth execution.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDI
root=Path(sys.argv[1]);data=(root/'IMUSIC25.DLL').read_bytes();sha=hashlib.sha256(data).hexdigest()
assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert base==0x35440000 and size==0x25000 and u16(pe+4)==0x14c
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size;emu.mem_write(base+r,data[o:o+n])
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
mem=0x100000;emu.mem_map(mem,0x100000)
stream=mem;vtable=mem+0x100;chunk=mem+0x200;stack=mem+0x10000
read,seek,stop=mem+0x300,mem+0x310,mem+0x320
emu.mem_write(stream,struct.pack('<I',vtable));emu.mem_write(vtable+12,struct.pack('<I',read));emu.mem_write(vtable+20,struct.pack('<I',seek))
state={}
def word(o):return struct.unpack('<I',emu.mem_read(o,4))[0]
def hook(e,address,size,_):
 if address==base+0x8b80:
  p=e.reg_read(UC_X86_REG_EDI)
  state['headers'][p]=bytes(e.mem_read(p+4,6))+bytes(e.mem_read(p+18,1)) # Before source summary overwrites controller.
  return
 if address not in (read,seek,base+0x1b6b4):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=word(sp)
 if address==read:
  dest,n,out=word(sp+8),word(sp+12),word(sp+16);p=state['position'];b=state['payload']
  assert n<=10 and p+n<=len(b)
  e.mem_write(dest,b[p:p+n]);e.mem_write(out,struct.pack('<I',n));state['position']+=n;cleanup=16;value=0
 elif address==seek:
  n=word(sp+8);assert word(sp+12)==0 and word(sp+16)==1 and state['position']+n<=len(state['payload'])
  state['position']+=n;cleanup=20;value=0
 else:
  n=word(sp+4);assert n in (22,18) and len(state['nodes'])<4096
  value=mem+0x20000+32*len(state['nodes']);state['nodes'].append(value);e.mem_write(value,bytes(32));cleanup=0
 e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(payload):
 state.update(payload=payload,position=0,nodes=[],headers={})
 emu.mem_write(chunk,struct.pack('<II',0,len(payload)));emu.mem_write(stack,struct.pack('<III',stop,stream,chunk));emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+0x8a30,stop,timeout=100000,count=1000000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and state['position']==len(payload)
 node=emu.reg_read(UC_X86_REG_EAX);out=[];seen=set()
 while node:
  assert node not in seen and node in state['headers'];seen.add(node)
  points=[];point=word(node+10)
  while point:
   assert point not in seen and point in state['nodes'];seen.add(point)
   points.append(bytes(emu.mem_read(point+8,10)));point=word(point)
  out.append((state['headers'][node],points));node=word(node)
 assert len(seen)==len(state['nodes'])
 return out
encode=lambda rows:b''.join(h+struct.pack('<H',len(p))+b''.join(p) for h,p in rows)
h1=struct.pack('<BBhHB',3,15,-100,0x8001,233);h2=struct.pack('<BBhHB',255,255,200,0,128)
p1=struct.pack('<BBhhhh',4,3,-1,-32768,32767,-123);p2=struct.pack('<BBhhhh',1,255,20,90,-20,50)
rows=[(h1,[p1,p2]),(h2,[]),(h1,[p2])]
assert observe(struct.pack('<HH',7,10)+encode(rows))==rows
assert observe(struct.pack('<HH',7,10))==[]
# Original skips extensions to its eight-byte header / ten-byte point core; portable profile refuses extensions.
extended=struct.pack('<HH',9,12)+h1+b'xy'+struct.pack('<H',2)+p1+b'zz'+p2+b'zz'
assert observe(extended)==[(h1,[p1,p2])]
def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4]
  assert a+8+n<=z;yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
count=curves=points=0;digest=hashlib.sha256();fnv=14695981039346656037;bybank={}
for file in sorted((root/'Music').rglob('*.sty')):
 assert file.stat().st_size<=32*1024*1024
 for tag,p in chunks(file.read_bytes()):
  if tag!=b'curv':continue
  assert struct.unpack_from('<HH',p)==(7,10)
  rows=observe(p);encoded=encode(rows);assert encoded==p[4:]
  count+=1;curves+=len(rows);points+=sum(len(x[1]) for x in rows);digest.update(encoded)
  for v in encoded:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
  bybank[file.parent.name]=bybank.get(file.parent.name,0)+len(rows)
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,entry_rva='0x8a30',synthetic_cases=3,curve_chunks=count,curves=curves,points=points,decoded_sha256=digest.hexdigest(),fnv64=fnv,curves_by_bank=bybank,scope='Original reader raw fields and list order. Header controller captured before derived summary overwrites; summary/evaluation not claimed. Bounded read/seek/allocation intercepted. No native DLL or synth execution.'),indent=2))
