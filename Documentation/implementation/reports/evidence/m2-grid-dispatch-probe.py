"""Original grid filters and ordered note/curve dispatch in isolated x86 execution.
Usage: python probe.py EXTERNAL_GAME_ROOT TRANSIENT_DISPATCH_FIXTURES
Fixtures from ima_cursor_tests dispatch_export contain private data: NEVER commit them.
Pitch/variation/output tick inputs are explicit; note tail and curve expansion execute.
"""
from pathlib import Path
import struct,hashlib,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
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
mem=0x100000;emu.mem_map(mem,0x400000);stack=mem+0x3e0000;frame=stack+0x1000;player=mem;band=mem+0x1000
read=lambda a,f:struct.unpack(f,emu.mem_read(a,struct.calcsize(f)))
put=lambda a,f,*v:emu.mem_write(a,struct.pack(f,*v))
signed=lambda v:(v+2147483648)%4294967296-2147483648
short=lambda v:(v+32768)%65536-32768
source=Path(sys.argv[2]).read_bytes();offset=0;assert len(source)<64*1024*1024
state={}
def take():
 global offset
 v=struct.unpack_from('<I',source,offset)[0];offset+=4;return v
def alloc(n):
 global used
 p=mem+used;used+=(n+15)&~15;assert used<0x3d0000;emu.mem_write(p,bytes(n));return p
def linked(nodes):
 for a,b in zip(nodes,nodes[1:]):put(a,'<I',b)
 return nodes[0] if nodes else 0
def hook(e,address,size,_):
 r=address-base
 def ret(cleanup=0,value=0):
  sp=e.reg_read(UC_X86_REG_ESP);target=read(sp,'<I')[0]
  e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,target)
 if r==0x1714d:
  node=e.reg_read(UC_X86_REG_EDI);assert node in state['pitches']
  put(frame-6,'<h',state['pitches'][node]);e.reg_write(UC_X86_REG_EIP,base+0x173ee)
 elif r==0x1b673:
  assert state['used']<len(state['draws']);v=state['draws'][state['used']];state['used']+=1;ret(value=v)
 elif r==0x1b6b4:
  n=read(e.reg_read(UC_X86_REG_ESP)+4,'<I')[0];assert n in (21,22);p=alloc(32)
  if n==22:state['curve_allocated'].add(p)
  ret(value=p)
 elif r==0x1b6e3:
  p=read(e.reg_read(UC_X86_REG_ESP)+4,'<I')[0];assert p in state['curve_allocated'] and p not in state['freed'];state['freed'].add(p);ret()
 elif r==0x166a5:ret() # Note-off metadata; this probe stops before active-note dispatch, tested separately.
 elif r==0x4c58:
  t,status,a,b,flags=read(e.reg_read(UC_X86_REG_ESP)+4,'<iIIII');assert flags==0
  state['messages'].append((t,status&255,a&255,b&255));ret(20)
emu.hook_add(UC_HOOK_CODE,hook)
class Digest:
 def __init__(self):self.fnv=14695981039346656037;self.sha=hashlib.sha256();self.steps=0;self.messages=0
 def add(self,rows):
  b=struct.pack('<I',len(rows))+b''.join(struct.pack('<iIII',*r) for r in rows)
  self.sha.update(b)
  for v in b:self.fnv=((self.fnv^v)*1099511628211)&0xffffffffffffffff
  self.steps+=1;self.messages+=len(rows)
 def out(self):return dict(steps=self.steps,messages=self.messages,fnv64=self.fnv,sha256=self.sha.hexdigest())
digests=[Digest(),Digest()];steps=0
while offset<len(source):
 used=0x4000;emu.mem_write(player,bytes(0x200));emu.mem_write(band,bytes(0x400));emu.mem_write(frame-0x200,bytes(0x240))
 tick=signed(take());mute=take();variations=[take() for _ in range(16)];transposes=[take()&255 for _ in range(16)]
 draws=[take() for _ in range(take())];assert len(draws)<=4096
 state.update(draws=draws,used=0,pitches={},messages=[],curve_allocated=set(),freed=set())
 nodes=[]
 for _ in range(take()):
  kind,typ,ch,time,var=[take() for _ in range(5)];node=alloc(32);nodes.append(node);put(node+4,'<BBhH',typ,ch,short(time),var)
  if kind==0:
   velocity,duration,tr,dr,vr,pitch=[take() for _ in range(6)];state['pitches'][node]=short(pitch)
   put(node+13,'<B',velocity);put(node+16,'<hBBB',short(duration),tr,dr,vr)
  else:
   assert kind==1;controller,count=take(),take();put(node+18,'<B',controller);points=[]
   for j in range(count):
    shape,flags,start,value,end,target=[take() for _ in range(6)];p=alloc(32);points.append(p)
    put(p+4,'<IBBhhhh',node,shape,flags,short(start),short(value),short(end),short(target))
   put(node+10,'<I',linked(points))
 expected=[(signed(take()),take(),take(),take()) for _ in range(take())]
 if nodes:
  put(player+0x2d,'<16H',*variations);put(band+0x334,'<16B',*transposes)
  put(frame-0x1c,'<I',mute);put(frame+8,'<i',tick);put(frame-0x38,'<I',band);put(base+0x20708,'<I',0)
  emu.reg_write(UC_X86_REG_EBP,frame);emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_ESI,player);emu.reg_write(UC_X86_REG_EDI,linked(nodes))
  emu.emu_start(base+0x17103,base+0x1759f,timeout=1000000,count=2000000)
  assert emu.reg_read(UC_X86_REG_EIP)==base+0x1759f
 assert state['used']==len(draws) and state['curve_allocated']==state['freed']
 assert state['messages']==expected,(steps,state['messages'][:5],expected[:5])
 digests[steps>=1024].add(state['messages']);steps+=1
assert steps==9198
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,entry_rva='0x17103..0x1759f',generated=digests[0].out(),private=digests[1].out(),scope='Original grid mute/variation filtering, note event tail and curve expansion/send order. Explicit checked point owners, pre-resolved pitches, variations and tick. Allocation/free/rand/send intercepted. Section selection and subsequent note-off phase have separate evidence; loader owner fixups, synth and transport remain outside this probe.'),indent=2))
