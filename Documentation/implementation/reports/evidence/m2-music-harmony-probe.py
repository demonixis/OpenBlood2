"""Isolated original chord preparation/classification and REPs header projection.
No native execution or soundtrack playback; personality graph children are not executed.
Optional Unicorn 2.1.4 is evidence tooling, not a runtime dependency.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
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

mem=0x100000;emu.mem_map(mem,0x40000)
chord=mem+0x1000;scale=mem+0x2000;following=mem+0x3000;stack=mem+0x3f000;stop=mem+0x3ff00
put=lambda a,f,*v:emu.mem_write(a,struct.pack(f,*v))
def call(rva,args):
 put(stack,'<'+'I'*(len(args)+1),stop,*args);emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+rva,stop,timeout=1000000,count=100000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop

def observe(mask,override,root,flags,base_mask,default,next_root,next_beat,has_next):
 for address in [chord,scale,following]:emu.mem_write(address,bytes(64))
 put(scale+8,'<I',base_mask);put(scale+25,'<b',default&127);put(scale+27,'<B',bool(default&128))
 call(0x16654,[scale]);bf=bytes(emu.mem_read(scale+33,1))
 put(chord+8,'<I',mask);put(chord+25,'<b',root);put(chord+34,'<I',override)
 if has_next:
  put(chord,'<I',following);put(following+25,'<b',next_root);put(following+32,'<b',next_beat)
 call(0x16654,[chord]);pf=emu.mem_read(chord+33,1)[0]|(128 if flags&8 else 0);put(chord+33,'<B',pf)
 call(0x18dc3,[chord,scale])
 assert bool(emu.mem_read(chord+27,1)[0])==bool(default&128)
 return bf+bytes([pf])+bytes(emu.mem_read(chord+28,2))

def digest(rows):
 h=hashlib.sha256();fnv=14695981039346656037;count=0
 for row in rows:
  result=observe(*row);count+=1;h.update(result)
  for b in result:fnv=((fnv^b)*1099511628211)&0xffffffffffffffff
 return dict(cases=count,sha256=h.hexdigest(),fnv64=fnv)

fixtures=[]
for row in [(0x91,0,0,0,0xab5ab5,128,7,0,True),(0x89,0,2,8,0xab5,0,0,0,True),(0x91,1,1,0,0xab5,0,7,1,True),(0xffffffff,0,-128,8,0x80000000,255,127,-1,True),(0,0,0,0,0,0,0,0,False),(0x3c0000,0,0,0,0xffffff,0,0,0,False)]:
 out=observe(*row);fixtures.append(dict(input=row,base_flags=out[0],pitch_flags=out[1],variation_flags=struct.unpack('<H',out[2:])[0]))
seed=0x6d2b79f5
signed8=lambda v:(v+128)%256-128
def draw():
 global seed
 seed=(seed*1664525+1013904223)&0xffffffff;return seed
masks=[0,1,0x91,0x89,0xab5,0xab5ab5,0x3c0000,0xffffffff]
def generated():
 for i in range(4096):
  mask=draw() if i&1 else masks[(i//2)%8]
  override=draw() if i&2 else 0
  cr=signed8(draw()>>24);flags=draw()>>24;bm=draw() if i&4 else masks[(i//8)%8]
  default=draw()>>24;nr=signed8(draw()>>24);nb=0 if i&8 else signed8(draw()>>24)
  yield mask,override,cr,flags,bm,default,nr,nb,bool(i&16)
fuzz=digest(generated())

def chunks(d):
 pos=12
 while pos<len(d):
  n=struct.unpack_from('<I',d,pos+4)[0];assert pos+8+n<=len(d)
  yield d[pos:pos+4],d[pos+8:pos+8+n]
  pos+=8+n+(n&1)

def private_rows():
 for f in sorted((root/'Music').rglob('*.sec')):
  parts=dict(chunks(f.read_bytes()));header=parts[b'secn'];raw=parts[b'chrd'];assert len(header)==120 and int.from_bytes(raw[:2],'little')==117 and len(raw)==119;raw=raw[2:]
  per=(f.parent/'SILENCE.PER').read_bytes();bm=struct.unpack_from('<I',per,28)[0]
  yield struct.unpack_from('<I',raw,36)[0],struct.unpack_from('<I',raw,40)[0],signed8(raw[48]),raw[32],bm,header[52],0,0,False
private=digest(private_rows());assert private['cases']==156

# Execute the original header read/skip and field projection only. Opaque NEC/PNS graph traversal is skipped.
stream=mem+0x4000;vtable=mem+0x4100;obj=mem+0x5000;alloc=mem+0x8000
read_hook=mem+0x3ff10;seek_hook=mem+0x3ff20;copy_hook=mem+0x3ff30
for a in [read_hook,seek_hook,copy_hook]:emu.mem_write(a,b'\xc3')
put(stream,'<I',vtable);put(vtable+12,'<I',read_hook);put(vtable+20,'<I',seek_hook)
# IAT patch changes only the emulated address space, never the external binary.
emu.mem_protect(base,size,7);put(base+0x21168,'<I',copy_hook);emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
payload=b'';cursor=0;heap=alloc

def hook(uc,address,n,user):
 global cursor,heap
 sp=uc.reg_read(UC_X86_REG_ESP);arg=lambda i:struct.unpack('<I',uc.mem_read(sp+4*i,4))[0]
 def ret(value=0,pop=0):
  uc.reg_write(UC_X86_REG_EAX,value);uc.reg_write(UC_X86_REG_ESP,sp+4+pop);uc.reg_write(UC_X86_REG_EIP,arg(0))
 if address==base+0x1b6b4:
  count=arg(1);assert count<=0x1000;result=heap;heap+=(count+15)&~15;assert heap<mem+0x10000
  uc.mem_write(result,bytes(count));ret(result)
 elif address==read_hook:
  dest,count=arg(2),arg(3);assert cursor+count<=len(payload)
  uc.mem_write(dest,payload[cursor:cursor+count]);cursor+=count;ret(0,16)
 elif address==seek_hook:
  lo,hi,origin=arg(2),arg(3),arg(4);assert hi==0 and origin==1 and cursor+lo<=len(payload)
  cursor+=lo;ret(0,20)
 elif address==copy_hook:
  dest,src=arg(1),arg(2);raw=bytes(uc.mem_read(src,80));end=raw.index(0);uc.mem_write(dest,raw[:end+1]);ret(dest)
h=emu.hook_add(UC_HOOK_CODE,hook)
headers=[]
for f in sorted((root/'Music').rglob('*.PER')):
 payload=f.read_bytes();cursor=8;heap=alloc;emu.mem_write(obj,bytes(128));put(stack,'<III',stop,stream,len(payload)-8)
 emu.reg_write(UC_X86_REG_ECX,obj);emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+0x93cc,base+0x94f6,count=100000);assert emu.reg_read(UC_X86_REG_EIP)==base+0x94f6
 assert cursor==12+int.from_bytes(payload[8:12],'big')
 emu.emu_start(base+0x962d,stop,count=100000);assert emu.reg_read(UC_X86_REG_EIP)==stop
 runtime=struct.unpack('<I',emu.mem_read(obj+48,4))[0]
 raw=bytes(emu.mem_read(runtime+4,20));name=raw.split(b'\0')[0].decode('ascii')
 bm=struct.unpack('<I',emu.mem_read(runtime+128,4))[0]
 assert name=='Silence' and bm==0xab5ab5
 headers.append(dict(path=str(f.relative_to(root)),bytes=len(payload),sha256=hashlib.sha256(payload).hexdigest(),name=name,scale_mask=bm))
assert len(headers)==5
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,rvas=['0x16654','0x18dc3','0x18f15','0x93cc..0x94f6','0x962d..0x96ae'],fixtures=fixtures,generated=fuzz,private=private,headers=headers,scope='Original field extraction and retained chord arithmetic; opaque personality graph and live scheduling not executed.'),indent=2))
