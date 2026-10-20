"""Original normal-mode grid cursor, isolated from audio/notification/playlist side effects.
Usage: python probe.py EXTERNAL_GAME_ROOT TRANSIENT_FIXTURE_FILE
Produce fixtures with ima_cursor_tests export; they contain external data and MUST NOT
be committed. Only aggregate results leave this probe. No native DLL/OS execution.
"""
from pathlib import Path
import hashlib, struct, json, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC
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
emu.mem_protect(base+0x1e000,0x1000,UC_PROT_READ|UC_PROT_WRITE)
mem=0x100000;emu.mem_map(mem,0x2000000);stack=mem+0x1ff0000;stop=stack+0xf000
read=lambda a,f:struct.unpack(f,emu.mem_read(a,struct.calcsize(f)))
put=lambda a,f,*v:emu.mem_write(a,struct.pack(f,*v))
low=lambda x:(x+32768)%65536-32768
signed=lambda x:(x+2147483648)%4294967296-2147483648
source=Path(sys.argv[2]).read_bytes();offset=0
assert len(source)<64*1024*1024

def take():
 global offset
 x=struct.unpack_from('<I',source,offset)[0];offset+=4;return x

def alloc(n):
 global used
 p=mem+used;used+=(n+15)&~15;assert used<0x1f00000
 emu.mem_write(p,bytes(n));return p

def linked(nodes):
 for a,b in zip(nodes,nodes[1:]):put(a,'<I',b)
 return nodes[0] if nodes else 0

state={}
def hook(e,address,size,_):
 r=address-base
 def jump(rva):e.reg_write(UC_X86_REG_EIP,base+rva)
 def ret():
  sp=e.reg_read(UC_X86_REG_ESP);target=read(sp,'<I')[0]
  e.reg_write(UC_X86_REG_ESP,sp+4);e.reg_write(UC_X86_REG_EIP,target)
 if r in (0x167f8,0x179dc):ret() # Category/intensity notifications explicitly excluded.
 elif r==0x1b644:
  sp=e.reg_read(UC_X86_REG_ESP);dest,value,n=read(sp+4,'<III');assert n<=1024
  e.mem_write(dest,bytes([value&255])*n);e.reg_write(UC_X86_REG_EAX,dest);ret()
 elif r==0x1b673:
  assert state['draws']<16
  v=(state['step']*313+state['draws']*997+32760)&32767
  e.reg_write(UC_X86_REG_EAX,v);state['draws']+=1;ret()
 elif r==0x16b67:state['changed']=read(e.reg_read(UC_X86_REG_EBP)-1,'<B')[0]
 elif r==0x16b8e:state['restart']=1
 elif r==0x16bc6:state['selection']=1
 elif r==0x16e5d:state['grid']=e.reg_read(UC_X86_REG_EDI);state['chord']=e.reg_read(UC_X86_REG_EBX)
 elif r==0x16e6e:state['variation']=1
 elif r==0x16fe8:jump(0x170e3) # Metronome/beat notifications are not part of this cursor.
 elif r==0x17103:state['rows']+=1
 elif r==0x17116:state['unmuted']+=1
 elif r==0x1712b:
  state['admitted']+=1;jump(0x17595) # Keep admission decisions; skip MIDI/pitch/voice/curve work.
 elif r==0x17666:jump(0x176ce) # Playlist lookahead notification excluded.
 elif r==0x176f7:
  state['ended']=1;jump(0x179c2) # Original end reset executed; repeats/playlist excluded.
emu.hook_add(UC_HOOK_CODE,hook)
class Digest:
 def __init__(self):self.h=hashlib.sha256();self.fnv=14695981039346656037;self.steps=0;self.sections=0
 def add(self,b):
  self.h.update(b);self.steps+=1
  for x in b:self.fnv=((self.fnv^x)*1099511628211)&0xffffffffffffffff
 def out(self):return dict(sections=self.sections,steps=self.steps,sha256=self.h.hexdigest(),fnv64=self.fnv)
results=[Digest(),Digest()]
for case in range(260):
 used=0x1000
 section=alloc(256);style=alloc(128);player=alloc(256);basechord=alloc(64)
 measures,measure,duration=[take() for _ in range(3)]
 random,cycle,previous,rootvalue=[take() for _ in range(4)];variations=[take() for _ in range(16)]
 put(section+0x5c,'<hh',measures,measure);put(section+0x24,'<I',style);put(section+0x1c,'<I',basechord)
 put(player+0x14,'<I',basechord);put(player+0x1c,'<I',section);put(player+0x28,'<IB',previous,rootvalue&255)
 put(player+0x2d,'<16H',*variations);put(player+0x4d,'<HH',random,cycle);put(player+0x5a,'<B',2);put(player+0x75,'<I',duration)
 commands=[]
 for _ in range(take()):
  time,measureindex,flags=take(),take(),take();p=alloc(32);commands.append(p)
  put(p+4,'<ihI',signed(time),low(measureindex),flags)
 put(section+0x20,'<I',linked(commands))
 chords=[]
 for _ in range(take()):
  time,mask,rootvalue,flags,m,b=[take() for _ in range(6)];p=alloc(64);chords.append(p)
  put(p+4,'<iI',signed(time),mask);put(p+0x19,'<B',rootvalue&255);put(p+0x1c,'<HhB',flags,low(m),b&255)
 put(section+0x18,'<I',linked(chords))
 mutes=[]
 for _ in range(take()):
  t,v=take(),take();p=alloc(16);mutes.append(p);put(p+4,'<iHH',signed(t),0,v)
 put(player+0x18,'<I',linked(mutes))
 patterns=[]
 for _ in range(take()):
  flags,length=take(),take();timing=[signed(take()) for _ in range(4)]
  table=[take() for _ in range(256)];p=alloc(0x400);patterns.append(p)
  put(p+0x20,'<iI',signed(length),flags);put(p+0x48,'<hhh',low(length//timing[0]),timing[0],timing[1])
  put(p+0x50,'<hh',timing[2],timing[3]);put(p+0x92,'<256H',*table)
  grids=[]
  for _ in range(take()):
   g=alloc(16);grids.append(g);notes=[]
   for _ in range(take()):
    ch,var=take(),take();n=alloc(32);notes.append(n);put(n+4,'<BBhH',9,ch,0,var)
   put(g+4,'<I',linked(notes))
  put(p+4,'<I',linked(grids))
 put(style+20,'<I',linked(patterns))
 steps=take();assert 0<steps<=65536
 digest=results[case>=128];digest.sections+=1
 for step in range(steps):
  tick=read(player+0xc,'<i')[0];ordinal=read(player+0x51,'<h')[0]
  state=dict(step=step,draws=0,restart=0,selection=0,changed=0,variation=0,rows=0,unmuted=0,admitted=0,ended=0)
  put(stack,'<III',stop,tick,player);emu.reg_write(UC_X86_REG_ESP,stack)
  emu.emu_start(base+0x16908,stop,timeout=1000000,count=1000000)
  assert emu.reg_read(UC_X86_REG_EIP)==stop
  pattern=read(player+0x10,'<I')[0];pi=patterns.index(pattern)
  values=[tick,pi,ordinal,bool(state['grid']),read(player+0x5f,'<h')[0],read(pattern+0x48,'<h')[0],
   read(player+0x57,'<H')[0],chords.index(state['chord']),state['restart'],state['selection'],state['changed'],
   state['variation'],state['draws'],state['admitted'],state['rows']-state['unmuted'],state['unmuted']-state['admitted'],
   read(player+0xc,'<i')[0],read(player+0x51,'<h')[0],read(player+0x53,'<H')[0],
   read(player+0x28,'<I')[0],read(player+0x2c,'<b')[0],state['ended'],*read(player+0x2d,'<16H')]
  b=struct.pack('<38I',*[v&0xffffffff for v in values]);expected=source[offset:offset+len(b)];offset+=len(b)
  if b!=expected:
   other=struct.unpack('<38I',expected)
   raise AssertionError((case,step,[(i,a,z) for i,(a,z) in enumerate(zip(values,other)) if a&0xffffffff!=z]))
  digest.add(b)
 assert state['ended']
assert offset==len(source)
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,rva='0x16908',generated=results[0].out(),private=results[1].out(),
 scope='Original normal-mode grid/chord/mute/pattern/variation/note-admission decisions and end reset. '
 'Prepared inputs come from previously verified readers/helpers. Section event list and active notes empty; '
 'notifications, metronome, MIDI emission, playlist lookahead/repeats/transitions explicitly intercepted. '
 'This is not a full tick/audio equivalence claim.'),indent=2))
