"""Bounded original IMA section/pattern decisions in Unicorn 2.1.4.
No native execution, imports, I/O from emulated code, or commercial data output.
Timelines are prepared by the previously verified meter rules; decisions execute
original instructions. The current-chord global fallback and seek paths are excluded.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);data=(root/'IMUSIC25.DLL').read_bytes();sha=hashlib.sha256(data).hexdigest()
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
mem=0x100000;emu.mem_map(mem,0x100000)
section=mem+0x1000;style=mem+0x2000;player=mem+0x3000;frame=mem+0x4000
commands=mem+0x10000;chords=mem+0x20000;mutes=mem+0x30000;patterns=mem+0x50000
stack=mem+0xf0000;stop=mem+0xff000;result=mem+0x6000
put=lambda a,f,*v:emu.mem_write(a,struct.pack(f,*v))
read=lambda a,f:struct.unpack(f,emu.mem_read(a,struct.calcsize(f)))
low=lambda x:(x+32768)%65536-32768
div=lambda x,y:abs(x)//y*(-1 if x<0 else 1)
def run(rva,end):
 emu.emu_start(base+rva,end,timeout=1000000,count=2000000)
 assert emu.reg_read(UC_X86_REG_EIP)==end

def call(rva,args):
 put(stack,'<'+'I'*(len(args)+1),stop,*[v&0xffffffff for v in args]);emu.reg_write(UC_X86_REG_ESP,stack)
 run(rva,stop)

def pattern_choice(rows,count,flags):
 assert len(rows)<512
 put(style+20,'<I',patterns if rows else 0)
 for i,(pf,length) in enumerate(rows):
  p=patterns+i*128;emu.mem_write(p,bytes(128));put(p,'<I',p+128 if i+1<len(rows) else 0)
  put(p+36,'<I',pf);put(p+82,'<h',length)
 call(0x17a4f,[style,count,flags]);p=emu.reg_read(UC_X86_REG_EAX)
 if not p:return -1
 assert p>=patterns and (p-patterns)%128==0 and (p-patterns)//128<len(rows)
 return (p-patterns)//128

def observe(measures,old_measure,beat,measure,cm,cs,ms,tick,ps,extra):
 kept=[(m,b) for m,b in cs if b<measure//beat];assert kept
 for a in (section,style,player):emu.mem_write(a,bytes(256))
 put(section+0x5c,'<h',measures);put(section+0x20,'<I',commands if cm else 0)
 for i,(time,flags) in enumerate(cm):
  p=commands+i*32;m=low(div(time,old_measure));put(p,'<IihI',p+32 if i+1<len(cm) else 0,m*measure,m,flags)
 put(section+0x18,'<I',chords)
 for i,(m,b) in enumerate(kept):
  p=chords+i*64;emu.mem_write(p,bytes(64));put(p,'<Ii',p+64 if i+1<len(kept) else 0,m*measure+b*beat)
  put(p+30,'<hb',m,b)
 samples=[];cursor=0;value=ms[0][1] if ms else 0
 for t in range(0,measures*measure,beat):
  while cursor<len(ms) and ms[cursor][0]<=t:value=ms[cursor][1];cursor+=1
  samples.append((t,value))
 for i,(t,v) in enumerate(samples):
  p=mutes+i*16;put(p,'<IiHH',p+16 if i+1<len(samples) else 0,t,0,v)
 put(frame-0x14,'<I',section);put(frame-8,'<i',tick);emu.reg_write(UC_X86_REG_EBP,frame);emu.reg_write(UC_X86_REG_ESI,player)
 run(0x16afc,base+0x16b1a);ch=emu.reg_read(UC_X86_REG_EBX);ci=(ch-chords)//64;assert 0<=ci<len(kept)
 emu.reg_write(UC_X86_REG_EDX,mutes if samples else 0);run(0x16970,base+0x16996)
 mp=emu.reg_read(UC_X86_REG_EDX);mi=(mp-mutes)//16 if samples else 65535;mute=read(player+0x57,'<H')[0]
 current=low(div(tick,measure))
 call(0x17b33,[section,ch,tick,tick,current,result,result+4])
 count=read(result,'<h')[0];flags=read(result+4,'<I')[0]
 selected=pattern_choice(ps,count,flags);other=pattern_choice(ps,*extra)
 return struct.pack('<hHHHhIii',current,ci,mi,mute,count,flags,selected,other)

class Digest:
 def __init__(self):self.h=hashlib.sha256();self.fnv=14695981039346656037;self.count=0
 def add(self,b):
  self.h.update(b);self.count+=1
  for x in b:self.fnv=((self.fnv^x)*1099511628211)&0xffffffffffffffff
 def out(self):return dict(cases=self.count,sha256=self.h.hexdigest(),fnv64=self.fnv)
seed=0x198b2
def draw():
 global seed
 seed=(seed*1664525+1013904223)&0xffffffff;return seed
flags=[0,4,8,16,0x204,0x208,0x210,0x224,0x404,0x10404,0x13fff,0xffffffff]
generated=Digest()
for case in range(4096):
 measures=1+draw()%16;cm=[];cs=[];ms=[];ps=[]
 for j in range(draw()%6):
  t=(int(draw()%26)-5)*768+(draw()%2)*17;f=flags[draw()%len(flags)];cm.append((t,f))
 for j in range(1+draw()%8):
  m=int(draw()%23)-4;b=int(draw()%6)-1;cs.append((m,0 if j==0 else b))
 for j in range(draw()%6):
  t=int(draw()%14000)-2000;v=draw()&65535;ms.append((t,v))
 tick=int(draw()%16000)-2000
 if case%17==0:tick=-2147483648
 if case%19==0:tick=2147483647
 if case%23==0:tick=0
 for j in range(draw()%12):
  f=flags[draw()%len(flags)];n=int(draw()%18)-1;ps.append((f,n))
 extra=(low(draw()),draw())
 # Long requests would spend millions of instructions in the original. The portable equivalent
 # is independently tested at 32767; original randomized requests are bounded to -2..14.
 extra=(int(extra[0]%17)-2,extra[1])
 generated.add(observe(measures,768,192,768,cm,cs,ms,tick,ps,extra))

def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4];assert a+8+n<=z
  yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
private=Digest();bound=missing=0
for path in sorted((root/'Music').rglob('*.sec')):
 c=dict(chunks(path.read_bytes()));h=c[b'secn'];label=c[b'sref'].decode('utf-16le').split('\0')[0]
 matches=[p for p in path.parent.iterdir() if p.name.casefold()==(label+'.sty').casefold()]
 if not matches:missing+=1;continue
 assert len(matches)==1
 style_chunks=list(chunks(matches[0].read_bytes()));sh=dict(style_chunks)[b'styl'];assert sh[46:62]==h[56:72]
 beats,unit=struct.unpack_from('<hh',sh,32);beat=768//(unit or 4);measure=beat*beats
 measures,old_measure=struct.unpack_from('<hh',h,40)
 cm=[struct.unpack_from('<iI',c[b'cmnd'],i) for i in range(2,len(c[b'cmnd']),8)]
 cs=[(struct.unpack_from('<h',c[b'chrd'],i+34)[0],struct.unpack_from('<b',c[b'chrd'],i+33)[0]) for i in range(2,len(c[b'chrd']),117)]
 ms=[struct.unpack_from('<iH',c[b'mute'],i) for i in range(2,len(c[b'mute']),8)]
 ps=[(struct.unpack_from('<I',p,8)[0],struct.unpack_from('<h',p,18)[0]) for tag,p in style_chunks if tag==b'patt']
 for tick in [-1,0,1,beat-1,beat,measure-1,measure,measures*measure-1,measures*measure]:
  private.add(observe(measures,old_measure,beat,measure,cm,cs,ms,tick,ps,(12,0x204)))
 bound+=1
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,rvas=['0x16afc..0x16b1a','0x16970..0x16996','0x17b33','0x17a4f'],generated=generated.out(),private=private.out(),bound_sections=bound,missing_style_sections=missing,scope='Original current chord/mute lookup, normal section pattern request, and pattern selection only; no global fallback, seek, activation or playback'),indent=2))
