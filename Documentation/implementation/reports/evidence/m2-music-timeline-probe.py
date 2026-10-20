"""Original IMA meter and time reconciliation, isolated Unicorn 2.1.4.
No native loading/import execution or commercial bytes exported. Chord pitch
harmonization is intercepted: only ordered times and mute words are compared.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);data=(root/'IMUSIC25.DLL').read_bytes()
sha=hashlib.sha256(data).hexdigest()
assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert base==0x35440000 and size==0x25000 and u16(pe+4)==0x14c
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size
 emu.mem_write(base+r,data[o:o+n])
emu.mem_write(base+0x2070c,bytes(4));emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
mem=0x100000;emu.mem_map(mem,0x200000);stack=mem+0x10000;stop=mem+0x100
section=mem+0x1000;style=mem+0x2000;state={}
def read(o,fmt):return struct.unpack(fmt,emu.mem_read(o,struct.calcsize(fmt)))
def put(o,fmt,*values):emu.mem_write(o,struct.pack(fmt,*values))
def hook(e,address,size,_):
 if address not in (base+0x1b6b4,base+0x1b6e3,base+0x192a9):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=read(sp,'<I')[0];result=0
 if address==base+0x1b6b4:
  assert read(sp+4,'<I')[0]==18 and state['allocated']<8192
  result=mem+0x80000+64*state['allocated'];state['allocated']+=1;e.mem_write(result,bytes(64))
 elif address==base+0x1b6e3:
  node=read(sp+4,'<I')[0];assert node in state['input'] and node not in state['freed'];state['freed'].add(node)
 else:state['harmonization_skipped']+=1
 e.reg_write(UC_X86_REG_EAX,result);e.reg_write(UC_X86_REG_ESP,sp+4);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(beats,unit,measures,old_measure,commands,mutes,chords):
 state.update(allocated=0,input=set(),freed=set(),harmonization_skipped=0)
 emu.mem_write(section,bytes(256));emu.mem_write(style,bytes(256))
 put(section+0x24,'<I',style);put(section+0x5c,'<hhh',measures,old_measure,192);put(style+0x84,'<hh',beats,unit)
 nodes={}
 for k,records in enumerate((commands,mutes,chords)):
  addresses=[mem+0x20000+k*0x10000+i*64 for i in range(len(records))];nodes[k]=addresses
  put(section+(0x20,0x2c,0x18)[k],'<I',addresses[0] if addresses else 0)
  for i,(node,record) in enumerate(zip(addresses,records)):
   state['input'].add(node);emu.mem_write(node,bytes(64));put(node,'<I',addresses[i+1] if i+1<len(addresses) else 0)
   if k==0:
    time=record;quotient=abs(time)//old_measure*(-1 if time<0 else 1)
    put(node+4,'<ih',time,(quotient+32768)%65536-32768)
   elif k==1:
    time,value=record;put(node+4,'<i',time);put(node+10,'<H',value)
   else:
    measure,beat=record;put(node+30,'<hb',measure,beat)
 put(stack,'<III',stop,0,section);emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+0x19441,stop,timeout=1000000,count=4000000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and state['harmonization_skipped']==1
 beat,measure=read(section+0x60,'<h')[0],read(section+0x5e,'<h')[0];duration=read(section+0x66,'<i')[0]
 out=struct.pack('<hhi',beat,measure,duration);rows=[]
 for k,offset in enumerate((0x20,0x2c,0x18)):
  node=read(section+offset,'<I')[0];row=[];seen=set()
  while node:
   assert node not in seen and node not in state['freed'];seen.add(node)
   tick=read(node+4,'<i')[0];row.append((tick,read(node+10,'<H')[0]) if k==1 else tick)
   node=read(node,'<I')[0]
  out+=struct.pack('<I',len(row))
  for item in row:out+=struct.pack('<iH',*item) if k==1 else struct.pack('<i',item)
  rows.append(row)
 assert set(nodes[1]).issubset(state['freed'])
 expected_beat=768//(unit or 4);expected_measure=expected_beat*beats
 assert (beat,measure,duration)==(expected_beat,expected_measure,expected_measure*measures)
 expected_commands=[]
 for t in commands:
  q=abs(t)//old_measure*(-1 if t<0 else 1);expected_commands.append(((q+32768)%65536-32768)*measure)
 expected_mutes=[];cursor=0;value=mutes[0][1] if mutes else 0
 for t in range(0,duration,beat):
  while cursor<len(mutes) and mutes[cursor][0]<=t:value=mutes[cursor][1];cursor+=1
  expected_mutes.append((t,value))
 expected_chords=[m*measure+b*beat for m,b in chords if b<beats]
 assert rows==[expected_commands,expected_mutes,expected_chords]
 assert state['freed']==set(nodes[1])|{node for node,(_,b) in zip(nodes[2],chords) if b>=beats}
 return out,dict(beat=beat,measure=measure,duration=duration,commands=rows[0],mutes=rows[1],chords=rows[2])
def style_header(p):
 frame=stack+0x2000;wrapper=mem+0x3000
 emu.mem_write(style,bytes(256));emu.mem_write(wrapper,bytes(256));emu.mem_write(frame-0x90,bytes(p))
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame);emu.reg_write(UC_X86_REG_ESI,style);emu.reg_write(UC_X86_REG_EDI,wrapper)
 emu.emu_start(base+0xeb4c,base+0xeb9c,timeout=100000,count=10000)
 for src,dst in [(32,0x84),(34,0x86),(36,0x88),(38,0x8a),(40,0x8c),(42,0xa7)]:assert bytes(emu.mem_read(style+dst,2))==p[src:src+2]
 assert bytes(emu.mem_read(wrapper+0x44,4))==p[32:36]
synthetic=[]
for beats,unit in [(4,4),(6,8),(4,0),(3,7),(8,4)]:
 _,result=observe(beats,unit,2,768,[-769,-767,1536,768*32768],[(500,7),(200,9),(576,11),(576,13),(999999,15)],[(-2,-1),(7,beats-1),(0,beats),(0,127),(0,-128)])
 synthetic.append(result)
_,result=observe(4,4,1,1,[-2147483648,2147483647],[],[]);synthetic.append(result)
p=bytearray(94)
struct.pack_into('<hhhhhh',p,32,-32768,-1,32767,1,2,3);style_header(p)
def chunks(b):
 a=12
 while a<len(b):
  n=struct.unpack_from('<I',b,a+4)[0];assert a+8+n<=len(b)
  yield b[a:a+4].decode('ascii'),b[a+8:a+8+n];a+=8+n+(n&1)
style_count=0
for f in sorted((root/'Music').rglob('*.sty')):
 h=dict(chunks(f.read_bytes()))['styl'];assert len(h)==94;style_header(h);style_count+=1
fnv=14695981039346656037;digest=hashlib.sha256();bound=0;missing=[];counts=[0,0,0]
for f in sorted((root/'Music').rglob('*.sec')):
 c=dict(chunks(f.read_bytes()));s=c['secn'];label=c['sref'].decode('utf-16le').split('\0')[0]
 matches=[x for x in f.parent.iterdir() if x.name.casefold()==(label+'.sty').casefold()]
 if not matches:missing.append(str(f.relative_to(root)));continue
 assert len(matches)==1
 h=dict(chunks(matches[0].read_bytes()))['styl'];assert s[56:72]==h[46:62]
 beats,unit=struct.unpack_from('<hh',h,32);measures,old_measure=struct.unpack_from('<hh',s,40)
 commands=[struct.unpack_from('<i',c['cmnd'],i)[0] for i in range(2,len(c['cmnd']),8)]
 mutes=[struct.unpack_from('<iH',c['mute'],i) for i in range(2,len(c['mute']),8)]
 chords=[(struct.unpack_from('<h',c['chrd'],i+34)[0],struct.unpack_from('<b',c['chrd'],i+33)[0]) for i in range(2,len(c['chrd']),117)]
 result,rows=observe(beats,unit,measures,old_measure,commands,mutes,chords);bound+=1
 for i,k in enumerate(('commands','mutes','chords')):counts[i]+=len(rows[k])
 digest.update(result)
 for v in result:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,entry_rva='0x19441',style_copy_rva='0xeb4c..0xeb9c',intercepted=['allocation/free','0x192a9 chord harmonization'],synthetic_cases=7,synthetic_timelines=synthetic,style_headers=style_count,bound_sections=bound,missing_style_sections=len(missing),missing_style_paths=missing,records=dict(zip(('commands','mutes','chords'),counts)),timeline_sha256=digest.hexdigest(),timeline_fnv64=fnv,scope='Meter, duration, ordered command/chord times and resampled mute values only; no pitch harmonization, pattern choice or playback'),indent=2))
