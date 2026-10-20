"""Bounded isolated IMA band load/apply arithmetic; optional Unicorn 2.1.4.
No DLL initialization, imported OS services, native execution or asset export.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX
root=Path(sys.argv[1]);binary=root/'IMUSIC25.DLL'
assert 0<binary.stat().st_size<1024*1024
data=binary.read_bytes();assert len(data)<1024*1024
sha=hashlib.sha256(data).hexdigest();assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0]
u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert data[pe:pe+4]==b'PE\0\0' and u16(pe+4)==0x14c and u16(opt)==0x10b
assert base==0x35440000 and size==0x25000
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size
 emu.mem_write(base+r,data[o:o+n])
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
mem=0x100000;emu.mem_map(mem,0x100000);obj=mem+0x1000;stack=mem+0x90000;stop=mem+0x100
state={}
def word(o):return struct.unpack('<I',emu.mem_read(o,4))[0]
def safe(a,n):assert mem<=a<=mem+0x100000-n and n<=1024

def hook(e,address,size,_):
 if address not in [base+x for x in (0x1a8b4,0x1b61d,0x1b644,0x1b6f9,0x4c58)]:return
 sp=e.reg_read(UC_X86_REG_ESP);ret=word(sp);cleanup=0
 if address==base+0x1a8b4:
  # Loader initializes sixteen empty strings; no allocator/imports required.
  assert word(sp+4)==0;cleanup=4
 elif address==base+0x1b61d:
  d,s,n=word(sp+4),word(sp+8),word(sp+12);safe(d,n);safe(s,n);e.mem_write(d,bytes(e.mem_read(s,n)))
 elif address==base+0x1b644:
  d,v,n=word(sp+4),word(sp+8)&255,word(sp+12);safe(d,n);e.mem_write(d,bytes([v])*n)
 elif address==base+0x4c58:
  assert word(sp+4)==123 and word(sp+20)==0 and len(state['events'])<80
  state['events'].append(bytes([word(sp+8)&255,word(sp+12)&255,word(sp+16)&255]));cleanup=20
 # Debug trace helper (0x1b6f9) intentionally omitted; no external side effect.
 e.reg_write(UC_X86_REG_EAX,0);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(p):
 assert len(p)==202
 emu.mem_write(obj,bytes(0x400));emu.mem_write(stack-0x118,bytes(p))
 emu.mem_write(obj+0x30c,struct.pack('<H',bool(p[170])))
 emu.reg_write(UC_X86_REG_EBP,stack);emu.reg_write(UC_X86_REG_ESP,stack-0x200);emu.reg_write(UC_X86_REG_EBX,obj)
 emu.emu_start(base+0x16c2,base+0x17d3,timeout=100000,count=100000)
 assert emu.reg_read(UC_X86_REG_EIP)==base+0x17d3
 assert bytes(emu.mem_read(obj+0x2fc,16))==p[154:170]
 assert word(obj+0x38)==(struct.unpack_from('<b',p,104)[0]&0xffffffff)
 assert bytes(emu.mem_read(obj+0x314,48))==p[56:104]
 patches=[];expected=[]
 for i in range(16):
  patch=(struct.unpack_from('<H',p,106+i*2)[0]<<7)|p[138+i]
  if not p[170] and not patch&0x200000:patch|=0x400000
  if i==5:patch|=0x80000000
  assert word(obj+0x40+32*i)==patch and emu.mem_read(obj+0x58+32*i,1)[0]==p[40+i]
  patches.append(patch)
  gm=patch&0x600000==0x400000
  ch=(i+4)&15
  expected.extend([bytes([0xb0|ch,7,p[56+i]]),bytes([0xb0|ch,10,p[72+i]]),bytes([0xb0|ch,0,0 if gm else (patch>>14)&127]),bytes([0xb0|ch,32,0 if gm else (patch>>7)&127]),bytes([0xc0|ch,p[40+i] if gm else patch&127,0])])
 state['events']=[]
 emu.mem_write(stack,struct.pack('<II',stop,123));emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_ECX,obj)
 emu.emu_start(base+0x1e9d,stop,timeout=100000,count=100000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and state['events']==expected
 return b''.join(state['events'])
# Exercise every packed flag combination, GM fallback, signed transpose/header,
# distinct channel/controller/program values and nonzero bank MSB/LSB.
synthetic=0
for collection in (False,True):
 for flags in (0,0x4000,0x8000,0xc000):
  p=bytearray(202);p[104]=255;p[170]=65 if collection else 0
  for i in range(16):
   p[40+i]=80+i;p[56+i]=90+i;p[72+i]=20+i;p[88+i]=240+i;p[138+i]=i+1;p[154+i]=i
   struct.pack_into('<H',p,106+2*i,flags|0x1234)
  observe(p);synthetic+=1
def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  assert a+8<=z
  n=struct.unpack_from('<I',b,a+4)[0];assert a+8+n<=z
  p=b[a+8:a+8+n];tag=b[a:a+4];yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
count=0;fnv=14695981039346656037;digest=hashlib.sha256();kinds={}
for f in sorted((root/'Music').rglob('*')):
 if f.suffix not in ('.sty','.sec'):continue
 assert f.stat().st_size<=32*1024*1024
 for tag,p in chunks(f.read_bytes()):
  if tag!=b'band':continue
  events=observe(p);digest.update(events)
  for byte in events:fnv=((fnv^byte)*1099511628211)&0xffffffffffffffff
  count+=1;kinds[f.suffix]=kinds.get(f.suffix,0)+1
print(json.dumps(dict(binary=binary.name,sha256=sha,load_arithmetic_rvas=['0x16c2','0x17d3'],apply_rva='0x1e9d',synthetic_cases=synthetic,bands=count,bands_by_kind=kinds,setup_events=count*80,event_sha256=digest.hexdigest(),event_fnv64=fnv,scope='Original channel records and ordered volume/pan/bank/program messages only; strings, collection loading, selection and playback not emulated'),indent=2))
