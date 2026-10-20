"""Isolated original IMA section-band assignment and style-default selection.
Optional Unicorn 2.1.4; no native DLL loading or imported operating-system calls.
Band decoding is covered by the earlier probe; allocation/refcounts are intercepted.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);b=(root/'IMUSIC25.DLL').read_bytes()
sha=hashlib.sha256(b).hexdigest();assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',b,o)[0];u32=lambda o:struct.unpack_from('<I',b,o)[0]
pe=u32(60);op=pe+24;base=u32(op+28);size=u32(op+56)
assert base==0x35440000 and size==0x25000
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,size)
for i in range(u16(pe+6)):
 s=op+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(b) and r+n<=size
 u.mem_write(base+r,b[o:o+n])
u.mem_map(0x100000,0x100000)
obj=0x101000;style=0x102000;vt=0x103000;st=0x1f0000;stop=0x100100;hookaddr=0x100200
w=lambda a,v:u.mem_write(a,struct.pack('<I',v&0xffffffff))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
# Critical sections/refcounts terminate in local RET hooks. Original getter remains intact.
u.mem_write(hookaddr,b'\xc3');w(base+0x211c8,hookaddr);w(base+0x211cc,hookaddr);w(vt+4,hookaddr)
w(vt+0x3c,base+0xd9eb)
u.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
state={}
def hook(e,a,size,_):
 if a not in (hookaddr,base+0x1b6b4,base+0x10fc,base+0x15fe):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=r(sp);clean=4;value=0
 if a==base+0x1b6b4:
  assert r(sp+4)==0x344;value=state['new'];clean=0
 elif a==base+0x10fc:value=e.reg_read(UC_X86_REG_ECX)
 elif a==base+0x15fe:clean=8
 e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+clean);e.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def run(a,z):
 u.emu_start(base+a,base+z,timeout=100000,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==base+z

def choose(count,early,flags,style_early):
 u.mem_write(obj,bytes(0x100));u.mem_write(style,bytes(0x100));w(style,vt)
 # Execute the original prepend fragment for each band loaded after styl.
 for i,flag in enumerate(flags):
  p=0x110000+i*0x400;u.mem_write(p,bytes(0x344));w(p,vt);w(p+0x38,flag)
  if i<style_early:continue
  u.reg_write(UC_X86_REG_EDI,style);u.reg_write(UC_X86_REG_EBX,p);u.reg_write(UC_X86_REG_ESP,st)
  run(0xe93f,0xe967)
 for i in range(count):
  p=0x120000+i*0x400;u.mem_write(p,bytes(0x344));w(p,vt);state['new']=p
  u.reg_write(UC_X86_REG_EBP,st);w(st-0x20,0x4e424141)
  u.reg_write(UC_X86_REG_EBX,obj);u.reg_write(UC_X86_REG_EDI,int(i>=early));u.reg_write(UC_X86_REG_ESP,st-0x100)
  run(0xb37c,0xb6af)
 w(obj+0x34,style);u.reg_write(UC_X86_REG_EBX,obj);u.reg_write(UC_X86_REG_ESP,st)
 run(0xb749,0xb791)
 p=r(obj+0x3c)
 if p>=0x120000:return 1,(p-0x120000)//0x400+1
 if p>=0x110000:return 2,(p-0x110000)//0x400+1
 assert p==0;return 0,0
fnv=14695981039346656037;digest=hashlib.sha256();cases=0
for mask in range(16):
 flags=[(-128 if i%2 else -1) if mask&(1<<i) else 0 for i in range(4)]
 for se in range(5):
  for count in range(4):
   for early in range(count+1):
    value=choose(count,early,flags,se);raw=struct.pack('<II',*value);digest.update(raw)
    # Independent priority observations, including absence rather than unmarked fallback.
    marked=[i for i in range(se,4) if flags[i]]
    assert value==((1,count) if count>early else (2,marked[-1]+1) if marked else (0,0))
    for byte in raw:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
    cases+=1
# Installed file observations use only structural chunks and original selected ordinal.
def chunks(data,a=12,z=None,depth=1):
 z=len(data) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',data,a+4)[0];assert a+8+n<=z
  tag=data[a:a+4];p=data[a+8:a+8+n];yield tag,p,a,depth
  if tag in (b'RIFF',b'LIST'):yield from chunks(data,a+12,a+8+n,depth+1)
  a+=8+n+(n&1)
private=0
for f in sorted((root/'Music').rglob('*.sec')):
 assert f.stat().st_size<=32*1024*1024
 data=f.read_bytes();headers=[];bands=[]
 for tag,p,off,depth in chunks(data):
  if tag==b'secn' and depth==1:headers.append(off)
  if tag==b'band' and depth==2:bands.append(off)
 assert len(headers)==len(bands)==1 and bands[0]>headers[0]
 assert choose(1,0,[0]*4,0)==(1,1)
 private+=1
print(json.dumps(dict(binary_sha256=sha,synthetic_cases=cases,fnv64=fnv,sha256=digest.hexdigest(),private_section_choices=private,
 rvas=dict(section_assignment=['0xb37c','0xb6af'],style_prepend=['0xe93f','0xe967'],section_fallback=['0xb749','0xb791'],style_default_getter='0xd9eb'),
 limits='Isolated loaded-band selection; band decoder previously verified; allocation, constructor, refcounts, critical sections and band read are intercepted. No MIDI or collection playback; absent-style GM construction excluded.'),indent=2))
