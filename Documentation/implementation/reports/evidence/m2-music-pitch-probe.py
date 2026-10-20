"""Isolated original note pitch dispatch/core, explicit runtime harmony only.
No native execution, imports, resource reconciliation or soundtrack playback.
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

mem=0x100000;emu.mem_map(mem,0x20000)
note=mem+0x1000;pattern=mem+0x2000;chord=mem+0x3000;scale=mem+0x4000
stack=mem+0x8000;frame=mem+0x10000

def put(o,fmt,*v):emu.mem_write(o,struct.pack(fmt,*v))
def observe(n,p,c):
 # n: music,modifier,raw,mode,channel; p: effective mode,range bit,lower,upper
 # c: enabled,chord mask,scale mask,chord root,scale root,flags
 music,modifier,raw,mode,ch=n;pm,limit,lower,upper=p;enabled,cm,sm,cr,sr,flags=c
 emu.mem_write(frame-0x200,bytes(0x220));emu.mem_write(note,bytes(32))
 emu.mem_write(pattern,bytes(0x400));emu.mem_write(chord,bytes(64));emu.mem_write(scale,bytes(64))
 put(note+5,'<B',ch);put(note+10,'<B',modifier);put(note+12,'<B',raw);put(note+14,'<H',music);put(note+21,'<B',mode)
 put(pattern+0x298+ch,'<B',pm);put(pattern+0x90,'<H',int(limit)<<ch)
 put(pattern+0x70+ch,'<b',upper);put(pattern+0x80+ch,'<b',lower)
 put(chord+8,'<I',cm);put(chord+25,'<b',cr);put(chord+33,'<B',flags);put(scale+25,'<b',sr)
 put(frame-0x1e,'<h',int(enabled));put(frame-0x18,'<I',pattern);put(frame-0x28,'<I',scale)
 put(frame-0x5c,'<I',sm);put(frame-8,'<I',ch)
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame)
 emu.reg_write(UC_X86_REG_EDI,note);emu.reg_write(UC_X86_REG_EBX,chord)
 emu.emu_start(base+0x1714d,base+0x173ee,timeout=1000000,count=600000)
 if emu.reg_read(UC_X86_REG_EIP)!=base+0x173ee:
  # The only admitted nontermination observation is the zero-mask mode-4 loop.
  assert base+0x17288<=emu.reg_read(UC_X86_REG_EIP)<=base+0x1729a
  assert struct.unpack('<I',emu.mem_read(frame-0x10,4))[0]==0
  return bytes(3)
 return bytes([1])+bytes(emu.mem_read(frame-6,2))

def hash_results(rows):
 fnv=14695981039346656037;digest=hashlib.sha256();count=failed=0
 for n,p,c in rows:
  out=observe(n,p,c);count+=1;failed+=not out[0];digest.update(out)
  for v in out:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
 return dict(cases=count,refused_infinite_walks=failed,fnv64=fnv,sha256=digest.hexdigest())

c=(True,0x91,0xab5,0,0,0);p=(3,False,0,127)
fixtures=[]
def add(n,pp=p,cc=c):
 result=observe(n,pp,cc);fixtures.append(dict(note=n,channel=pp,harmony=cc,ok=bool(result[0]),pitch=struct.unpack('<h',result[1:])[0]))
for mode in range(0x80,0x90):add((0x4129,0x25,60,mode,0))
for flags in [0x10,0x27,0x47,0x57,0x77]:add((0x4218,0x9f,60,0x83,0),(3,True,48,72),(True,0x91,0xab5,-7,11,flags))
for n,cc in [((0x4100,0,60,0x83,0),(True,1,0xab5,0,0,0)),((0x4100,0x9f,60,0x83,0),c),((0x4100,0x9f,60,0x83,0),(True,1,0x80000000,-128,127,0)),((0x4000,0x80,60,0x84,0),(True,1,0x80000000,0,0,0)),((0x4000,0xf8,60,0x84,0),(True,1,0xffffffff,0,0,0)),((0x4000,0x10,60,0x84,0),(True,1,0,0,0,0)),((0x4000,0x80,60,0x84,0),c),((0,0,255,0x83,0),c),((0x4000,0,255,0x83,0),(False,0,0,0,0,0)),((0x4000,0,12,0x81,0),c),((0x4000,0,128,0x81,0),c)]:add(n,cc=cc)
# Inverted range applies upper then lower, without rechecking upper.
add((0x4000,0,90,0x81,0),(1,True,100,20),(True,0,0,0,0,0x10))

seed=0x6d2b79f5
def draw():
 global seed
 seed=(seed*1664525+1013904223)&0xffffffff;return seed
signed8=lambda v:(v+128)%256-128
masks=[0,1,0x91,0xab5,0xfff,0x80000000,0xffffffff,0x80100001]
def generated():
 for i in range(2048):
  ch=i%16;music=draw()&65535;modifier=draw()&127;raw=draw()&255
  mode=(i%16)|(0x80 if i&1 else 0);pm=(i//16)%16 or (1 if ch==5 else 2 if ch==4 else 3)
  lower=signed8(draw()>>24);upper=signed8(draw()>>24)
  cr=signed8(draw()>>24);sr=signed8(draw()>>24);flags=draw()>>24
  yield (music,modifier,raw,mode,ch),(pm,bool(i&2),lower,upper),(bool(i%11),masks[(i//8)%8],masks[(i//64)%8],cr,sr,flags)
fuzz=hash_results(generated())

def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  tag=b[a:a+4];n=struct.unpack_from('<I',b,a+4)[0];assert a+8+n<=z
  yield tag,b[a+8:a+8+n]
  a+=8+n+(n&1)
contexts=[c,(True,0x889,0xad5,5,-7,0x57),(False,0,0,0,0,0)]
files=notes=0
def private_rows():
 global files,notes
 for f in sorted((root/'Music').rglob('*.sty')):
  files+=1;assert f.stat().st_size<=32*1024*1024
  def walk(b):
   global notes
   for tag,payload in chunks(b):
    if tag in (b'LIST',b'RIFF') and payload[:4]==b'AAPT':
     parts=list(chunks(payload,a=4));header=next(v for k,v in parts if k==b'patt');assert len(header)==625
     for k,v in parts:
      if k!=b'LIST':continue
      for nk,nv in chunks(v,a=4):
       if nk!=b'note':continue
       assert struct.unpack_from('<H',nv)[0]==18
       for pos in range(2,len(nv),18):
        raw=nv[pos:pos+18];ch=raw[1];notes+=1
        pm=header[609+ch] or (1 if ch==5 else 2 if ch==4 else 3)
        pp=(pm,bool(struct.unpack_from('<H',header,92)[0]&(1<<ch)),signed8(header[76+ch]),signed8(header[60+ch]))
        n=(struct.unpack_from('<H',raw,10)[0],raw[6],raw[8],raw[17],ch)
        for ctx in contexts:yield n,pp,ctx
    elif tag in (b'LIST',b'RIFF'):yield from walk(b'RIFF'+struct.pack('<I',len(payload))+payload)
  yield from walk(f.read_bytes())
private=hash_results(private_rows());assert files==39 and notes==7030,(files,notes)
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,dispatch_rva='0x1714d..0x173ee',core_rva='0x11b82..0x11c7a',fixtures=fixtures,generated=fuzz,files=files,notes=notes,private=private,scope='Explicit synthetic effective harmony; no personality reconciliation, authored song pitch or playback claim.'),indent=2))
