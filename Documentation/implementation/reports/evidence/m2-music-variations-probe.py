"""Isolated IMA 2.5 pattern defaults/channel-variation probe (optional Unicorn 2.1.4).
Loads only the pinned external image into an emulator. No native loading, imported
OS execution, audio or exported commercial bytes. Random returns are explicit.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_WRITE,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);binary=root/'IMUSIC25.DLL';assert binary.stat().st_size<1024*1024
data=binary.read_bytes();sha=hashlib.sha256(data).hexdigest()
assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert base==0x35440000 and size==0x25000 and u16(pe+4)==0x14c
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size
 emu.mem_write(base+r,data[o:o+n])
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
emu.mem_protect(base+0x1e000,0x1000,UC_PROT_READ|UC_PROT_WRITE)
mem=0x100000;emu.mem_map(mem,0x10000)
pattern=mem+0x1000;holder=mem+0x2000;player=mem+0x3000;chord=mem+0x4000
stack=mem+0x8000;frame=mem+0x9000;stop=mem+0x100
state={}
def read(o,fmt):return struct.unpack(fmt,emu.mem_read(o,struct.calcsize(fmt)))
def put(o,fmt,*v):emu.mem_write(o,struct.pack(fmt,*v))
def run(start,end):
 emu.emu_start(base+start,end,timeout=100000,count=100000)
 assert emu.reg_read(UC_X86_REG_EIP)==end

def hook(e,address,size,_):
 if address!=base+0x1b673:return
 sp=e.reg_read(UC_X86_REG_ESP);ret=read(sp,'<I')[0]
 assert state['used']<len(state['draws'])
 e.reg_write(UC_X86_REG_EAX,state['draws'][state['used']]);state['used']+=1
 e.reg_write(UC_X86_REG_ESP,sp+4);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)

def header(p):
 assert len(p)==625
 emu.mem_write(pattern,bytes(0x400));put(holder,'<I',pattern);emu.mem_write(frame-0x2a8,bytes(p))
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame);emu.reg_write(UC_X86_REG_ESI,holder)
 run(0x8748,base+0x8880)
 for src,dst,n in [(0,0x1c,12),(12,0x4a,4),(16,0x50,4),(52,0x66,8),(60,0x70,32),(92,0x90,2),(606,0x292,2),(608,0x6e,1)]:
  assert bytes(emu.mem_read(pattern+dst,n))==p[src:src+n]
 assert bytes(emu.mem_read(pattern+0x298,16))==p[609:625]
 put(stack,'<II',stop,pattern);emu.reg_write(UC_X86_REG_ESP,stack);run(0x81cd,stop)
 table=list(read(pattern+0x92,'<256H'));modes=bytes(emu.mem_read(pattern+0x298,16))
 for ch in range(16):
  row=list(struct.unpack_from('<16H',p,94+32*ch));missing=0x1800
  for flags in row:missing &= ~flags
  assert table[ch*16:(ch+1)*16]==[v|missing for v in row]
  assert modes[ch]==(p[609+ch] or (1 if ch==5 else 2 if ch==4 else 3))
 return table,struct.pack('<256H',*table)+modes

current=[0,1,2,4,8,0x4000,0x8000,0xffff,0x8001,3,0x7fff,0x1234,0xaaaa,0x5555,0xff00,0xff]
scenarios=[(None,0xffff,0xffff),(0,0,0xffff),(1,0x5555,0xffff),(0x8000,0xffff,0),(0x1800,0xffff,0xffff),(0xffff,0x8001,0x7ffe)]
def choice(table,required,random,cycle):
 emu.mem_write(frame-0x180,bytes(0x200));emu.mem_write(player,bytes(0x100));emu.mem_write(chord,bytes(0x40))
 put(frame-0x18,'<I',pattern);put(frame+8,'<I',1234);put(chord+0x1c,'<H',required or 0)
 put(player+0x2d,'<16H',*current);put(player+0x4d,'<HH',random,cycle)
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame)
 emu.reg_write(UC_X86_REG_ESI,player);emu.reg_write(UC_X86_REG_EBX,chord if required is not None else 0)
 state.update(draws=[(i*997+32760)&32767 for i in range(random.bit_count())],used=0)
 run(0x16e7e,base+0x16fd9)
 candidates=[];selected=read(player+0x2d,'<16H');fallback=used=0
 for ch in range(16):
  raw=[v for v in range(16) if required is not None and table[ch*16+v]&required==required]
  if not raw:fallback|=1<<ch;raw=list(range(16))
  count=read(frame-0x58+ch*2,'<H')[0];indices=bytes(emu.mem_read(frame-0x158+ch*16,count))
  assert list(indices)==raw
  candidates.append(sum(1<<v for v in raw))
  expected=current[ch]
  if random&(1<<ch):expected=1<<raw[state['draws'][used]%len(raw)];used+=1
  elif cycle&(1<<ch):expected=1 if expected&0x8000 else (expected<<1)&65535
  assert selected[ch]==expected
 assert used==state['used']==len(state['draws'])
 return struct.pack('<16H16HHH',*candidates,*selected,fallback,used)

for case in range(3):
 p=bytearray(625)
 for i in range(52,625):p[i]=(i*13+case*87)&255
 for ch in range(16):
  for v in range(16):
   flags=0 if case==0 else ((1<<v) if case==1 else (ch*977+v*3881)&65535)
   struct.pack_into('<H',p,94+32*ch+2*v,flags)
 p[609:625]=bytes(16) if case!=2 else bytes(range(240,256))
 table,_=header(p)
 for scenario in scenarios:choice(table,*scenario)

def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4]
  assert a+8+n<=z
  yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
count=0;digest=hashlib.sha256();fnv=14695981039346656037
for f in sorted((root/'Music').rglob('*.sty')):
 assert f.stat().st_size<=32*1024*1024
 for tag,p in chunks(f.read_bytes()):
  if tag!=b'patt':continue
  table,effective=header(p);out=p[52:625]+effective
  for scenario in scenarios:out+=choice(table,*scenario)
  digest.update(out)
  for v in out:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
  count+=1
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,header_copy_rva='0x8748..0x8880',mode_defaults_rva='0x81cd',choice_rva='0x16e7e..0x16fd9',intercepted=['0x1b673 rand: explicit 15-bit returns'],synthetic_headers=3,synthetic_choices=18,patterns=count,private_choices=count*len(scenarios),canonical_sha256=digest.hexdigest(),canonical_fnv64=fnv,scope='Raw pattern fields, original channel defaults and one variation decision only; no scheduling, pitch conversion or audio'),indent=2))
