"""Read-only isolated IMRT25 tempo-node and event-clock probe (Unicorn 2.1.4).
No native DLL entrypoint/imports execute; no commercial bytes are exported.
Locks/logging are intercepted; MulDiv is modelled as its documented signed,
nearest/half-away integer contract, not tested as Windows implementation code.
"""
from pathlib import Path
import hashlib,json,struct,sys,random
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);data=(root/'IMRT25.DLL').read_bytes()
sha=hashlib.sha256(data).hexdigest();assert sha=='2dc50ca832084601d1f10ac9d5f5fc67fd153154cd244db1a55a3147c70d7bd2'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert base==0x35470000 and u16(pe+4)==0x14c
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size;emu.mem_write(base+r,data[o:o+n])
mem=0x100000;emu.mem_map(mem,0x10000);stack=mem+0xe000;stop=mem+0xf000;obj=mem+0x1000
put=lambda a,fmt,*v:emu.mem_write(a,struct.pack(fmt,*v))
get=lambda a,fmt:struct.unpack(fmt,emu.mem_read(a,struct.calcsize(fmt)))
for off,target in [(0x50d4,mem+0x100),(0x50e0,mem+0x110),(0x50e4,mem+0x120)]:put(base+off,'<I',target)
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
def hook(e,a,size,_):
 if a not in (mem+0x100,mem+0x110,mem+0x120,base+0x2d84):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=get(sp,'<I')[0];args=0;v=0
 if a==mem+0x100:
  x,y,z=get(sp+4,'<iii');assert z>0;p=x*y;v=(abs(p)+z//2)//z*(-1 if p<0 else 1);assert -2**31<=v<2**31;args=12
 elif a!=base+0x2d84:args=4
 e.reg_write(UC_X86_REG_EAX,v&0xffffffff);e.reg_write(UC_X86_REG_ESP,sp+4+args);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def call(rva,this,args):
 put(stack,'<I',stop)
 for i,v in enumerate(args):put(stack+4+4*i,'<I',v&0xffffffff)
 emu.reg_write(UC_X86_REG_ECX,this);emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+rva,stop,timeout=100000,count=100000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop
 return emu.reg_read(UC_X86_REG_EAX)
raw=bytearray();maps=queries=0
def observe(tempos,ticks):
 global maps,queries
 emu.mem_write(obj,bytes(256));nodes=[];expected=[];time=0
 for i,(tick,whole,frac) in enumerate(tempos):
  node=mem+0x2000+i*32;emu.mem_write(node,bytes(32));call(0x29b2,node,[whole,frac,100,4,tick])
  if i:call(0x2a3e,node,[nodes[-1]]);put(nodes[-1],'<I',node)
  nodes.append(node);effective=160 if whole<10 else 5600 if whole>=350 else whole*16+(frac>>12)
  if i:time+=(tick-tempos[i-1][0])*50000000//expected[-1][1]
  actual_whole,actual_frac=get(node+10,'<HH');assert actual_whole*16+(actual_frac>>12)==effective
  assert get(node+24,'<Q')[0]==time and get(node+20,'<I')[0]==time//10000
  expected.append((tick,effective,time))
 put(obj+0x80,'<II',nodes[0],nodes[-1]);maps+=1
 for tick in ticks:
  active=max(i for i,p in enumerate(tempos) if p[0]<=tick);t,q,ns=expected[active]
  ms=ns//10000+((tick-t)*5000+q//2)//q
  actual=call(0x1c12,obj,[tick]);assert actual==ms,(tempos,tick,actual,ms)
  raw.extend(struct.pack('<iI',tick,ms));queries+=1
 return expected
for whole in [1,9,10,120,121,349,350,32767]:
 for frac in [0,1023,4095,4096,32768,65535]:
  observe([(0,whole,frac)],range(2000))
for i in range(120):
 tempos=[(0,121,32768),(1+i,125,0),(192+i,349,65535),(192+i,120,4095),(1000+i,9,1234)]
 observe(tempos,[0,1,i+1,191+i,192+i,193+i,999+i,1000+i,2000+i,10000+i])
def chunks(b):
 a=12
 while a<len(b):
  n=struct.unpack_from('<I',b,a+4)[0];assert a+8+n<=len(b)
  yield b[a:a+4],b[a+8:a+8+n];a+=8+n+(n&1)
private=0;durations=[];private_start=len(raw)
for f in sorted((root/'Music').rglob('*.sec')):
 h=dict(chunks(f.read_bytes()))[b'secn'];whole,=struct.unpack_from('<h',h,36);fraction,=struct.unpack_from('<H',h,46)
 measures,meter=struct.unpack_from('<hh',h,40);duration=measures*meter
 observe([(0,whole,fraction)],[0,1,191,192,193,duration]);private+=1
def fnv(b):
 h=14695981039346656037
 for v in b:h=((h^v)*1099511628211)&0xffffffffffffffff
 return h
print(json.dumps(dict(private_fnv64=fnv(raw[private_start:]),synthetic_fnv64=fnv(raw[:private_start]),binary='IMRT25.DLL',sha256=sha,unicorn=__import__('unicorn').__version__,maps=maps,queries=queries,private_sections=private,decisions_sha256=hashlib.sha256(raw).hexdigest(),scope='Normal-speed clamped/quantized tempo, duplicate boundaries, retained 100ns carry and integer millisecond event conversion. MulDiv/locks/log intercepted; no native runtime, scheduler, PCM or playlist.',entry_rvas=['0x29b2','0x2a3e','0x1c12']),indent=2))
