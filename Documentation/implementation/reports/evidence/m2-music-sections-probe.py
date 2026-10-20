"""Isolated original IMA section readers; optional Unicorn 2.1.4.
No native DLL loading, imported OS execution or commercial bytes exported.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import *
root=Path(sys.argv[1]);binary=root/'IMUSIC25.DLL'
assert binary.stat().st_size<1024*1024
data=binary.read_bytes();sha=hashlib.sha256(data).hexdigest()
assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert data[pe:pe+4]==b'PE\0\0' and u16(pe+4)==0x14c and base==0x35440000 and size==0x25000
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size
 emu.mem_write(base+r,data[o:o+n])
mem=0x100000;emu.mem_map(mem,0x100000)
stream=mem;vtable=mem+0x100;chunk=mem+0x200;stack=mem+0x10000;head=mem+0x240
read,seek,stop,wide=mem+0x300,mem+0x310,mem+0x320,mem+0x330
emu.mem_write(base+0x211d0,struct.pack('<I',wide))
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
emu.mem_write(stream,struct.pack('<I',vtable));emu.mem_write(vtable+12,struct.pack('<I',read));emu.mem_write(vtable+20,struct.pack('<I',seek))
state={}
def word(o):return struct.unpack('<I',emu.mem_read(o,4))[0]
def hook(e,address,size,_):
 if address not in (read,seek,wide,base+0x1b6b4,base+0x1b6f9):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=word(sp);value=0
 if address==read:
  dest,n,out=word(sp+8),word(sp+12),word(sp+16);p=state['position'];b=state['payload']
  assert n<=118 and p+n<=len(b)
  e.mem_write(dest,b[p:p+n]);e.mem_write(out,struct.pack('<I',n));state['position']+=n;cleanup=16
 elif address==seek:
  n=word(sp+8);assert word(sp+12)==0 and word(sp+16)==1 and state['position']+n<=len(state['payload'])
  state['position']+=n;cleanup=20
 elif address==wide:
  src,dest,capacity=word(sp+12),word(sp+20),word(sp+24)
  assert capacity==12 and word(sp+16)==0xffffffff
  label=[]
  for i in range(16):
   c=struct.unpack('<H',e.mem_read(src+2*i,2))[0]
   if not c:break
   assert 32<=c<127;label.append(c)
  assert len(label)<12
  e.mem_write(dest,bytes(label)+b'\0');cleanup=32;value=len(label)+1
 elif address==base+0x1b6f9:cleanup=0
 else:
  n=word(sp+4);assert n in (18,42) and len(state['nodes'])<4096
  value=mem+0x20000+64*len(state['nodes']);state['nodes'].append(value);e.mem_write(value,bytes(64));cleanup=0
 e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(tag,payload):
 state.update(payload=payload,position=0,nodes=[])
 emu.mem_write(chunk,struct.pack('<II',0,len(payload)));emu.mem_write(stack,struct.pack('<IIII',stop,stream,chunk,head));emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+{'cmnd':0xba4b,'mute':0xbb79,'chrd':0xb899}[tag],stop,timeout=100000,count=1000000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and emu.reg_read(UC_X86_REG_EAX)==0 and state['position']==len(payload)
 node=word(head);out=[]
 while node:
  assert node==state['nodes'][len(out)]
  if tag=='cmnd':record=bytes(emu.mem_read(node+4,4))+bytes(emu.mem_read(node+10,4))
  elif tag=='mute':record=bytes(emu.mem_read(node+4,4))+bytes(emu.mem_read(node+10,2))+bytes(emu.mem_read(node+14,2))
  else:
   # Canonical mapped fields only: flags, beat, measure, chord/scale masks, root.
   flags=sum(bit for bit,offset in [(1,24),(2,26),(4,27)] if emu.mem_read(node+offset,1)[0])
   if emu.mem_read(node+33,1)[0]&128:flags|=8
   record=bytes([flags])+bytes(emu.mem_read(node+32,1))+bytes(emu.mem_read(node+30,2))+bytes(emu.mem_read(node+8,4))+bytes(emu.mem_read(node+34,4))+bytes(emu.mem_read(node+25,1))
  out.append(record);node=word(node)
 assert len(out)==len(state['nodes']);return out
synthetic=0
for tag in ('cmnd','mute'):
 records=[bytes(range(8)),struct.pack('<iHH',-999,0xffff,0x8000)]
 for stride in (8,10):
  assert observe(tag,struct.pack('<H',stride)+b''.join(r+bytes(stride-8) for r in records))==records;synthetic+=1
 assert observe(tag,struct.pack('<H',8))==[];synthetic+=1
for flags in range(16):
 p=bytearray(117);p[:4]=b'C\0\0\0';struct.pack_into('<BBhIIIb',p,32,flags,3,-2,0x891,0xab5,0x12345678,-5)
 expected=bytes(p[32:44])+bytes(p[48:49])
 assert observe('chrd',struct.pack('<H',117)+p)==[expected];synthetic+=1

def observe_header(p):
 state.update(nodes=[])
 obj=mem+0x4000;frame=stack+0x1000
 emu.mem_write(obj,bytes(256));emu.mem_write(frame-0xa4,bytes(p))
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame);emu.reg_write(UC_X86_REG_EDI,obj)
 emu.emu_start(base+0xb5d3,base+0xb644,timeout=100000,count=100000)
 for src,dst,n in [(0,4,4),(36,0x48,2),(38,0x4e,2),(40,0x5c,2),(42,0x5e,2),(44,0x60,2),(46,0x85,2),(48,0x8b,4)]:
  assert bytes(emu.mem_read(obj+dst,n))==p[src:src+n]
 chord=word(obj+0x1c)
 assert emu.mem_read(chord+25,1)[0]==p[52]&127 and bool(emu.mem_read(chord+27,1)[0])==bool(p[52]&128)
 emu.emu_start(base+0xb65c,base+0xb66a,timeout=100000,count=10000)
 measures,measure=struct.unpack_from('<hh',p,40)
 assert struct.unpack('<i',emu.mem_read(obj+0x66,4))[0]==measures*measure
 tempo,fraction=struct.unpack_from('<h',p,36)[0],struct.unpack_from('<H',p,46)[0]
 if tempo>0:
  emu.reg_write(UC_X86_REG_EDI,obj)
  emu.emu_start(base+0x18ced,base+0x18d10,timeout=100000,count=10000)
  observed=emu.reg_read(UC_X86_REG_EAX)
  assert observed==3840000000//((tempo<<6)|(fraction>>10))
  return observed
 return None
header_cases=[]
for tempo,fraction in [(120,0),(120,65535),(1,0),(32767,65535)]:
 p=bytearray(120);struct.pack_into('<hhhhhH',p,36,tempo,-32768,32767,32767,192,fraction);p[52]=255
 header_cases.append(dict(tempo=tempo,fraction=fraction,microseconds=observe_header(p)));synthetic+=1

command_measure_cases=[]
for ticks,time in [(768,-769),(768,-767),(768,768*32768),(1,-2147483648),(1,2147483647)]:
 obj=mem+0x4000;node=mem+0x5000;frame=stack+0x1000
 emu.mem_write(obj+0x5e,struct.pack('<h',ticks));emu.mem_write(node+4,struct.pack('<i',time))
 emu.reg_write(UC_X86_REG_EDI,obj);emu.reg_write(UC_X86_REG_ECX,node);emu.reg_write(UC_X86_REG_EBP,frame)
 emu.emu_start(base+0xb417,base+0xb429,timeout=100000,count=10000)
 result=struct.unpack('<h',emu.mem_read(node+8,2))[0]
 quotient=abs(time)//ticks*(-1 if time<0 else 1);expected=(quotient+32768)%65536-32768
 assert result==expected
 command_measure_cases.append(dict(ticks_per_measure=ticks,time=time,measure=result));synthetic+=1

def chunks(b):
 a=12
 while a<len(b):
  n=struct.unpack_from('<I',b,a+4)[0];assert a+8+n<=len(b)
  yield b[a:a+4].decode('ascii'),b[a+8:a+8+n]
  a+=8+n+(n&1)
counts={k:0 for k in ('cmnd','mute','chrd')};digest=hashlib.sha256();fnv=14695981039346656037;files=0;tempos=set();lengths=set()
for file in sorted((root/'Music').rglob('*.sec')):
 assert file.stat().st_size<32*1024*1024;files+=1
 for tag,p in chunks(file.read_bytes()):
  if tag=='secn':
   assert len(p)==120
   observe_header(p)
   tempo,unknown,measures,measure,beat,fraction=struct.unpack_from('<hhhhhH',p,36)
   tempos.add(tempo);lengths.add(measures*measure)
  if tag not in counts:continue
  stride=struct.unpack_from('<H',p)[0];assert stride== (117 if tag=='chrd' else 8)
  observed=observe(tag,p)
  expected=[]
  for i in range(2,len(p),stride):
   r=p[i:i+stride];expected.append(r if tag!='chrd' else bytes([r[32]&15])+r[33:44]+r[48:49])
  assert observed==expected;counts[tag]+=len(observed)
  for record in observed:
   digest.update(record)
   for c in record:fnv=((fnv^c)*1099511628211)&0xffffffffffffffff
print(json.dumps(dict(binary=binary.name,sha256=sha,header_cases=header_cases,command_measure_cases=command_measure_cases,header_rvas=['0xb5d3..0xb644','0xb65c..0xb66a','0x18ced..0x18d10'],entries={'cmnd':'0xba4b','mute':'0xbb79','chrd':'0xb899'},synthetic_cases=synthetic,sections=files,records=counts,mapped_sha256=digest.hexdigest(),mapped_fnv64=fnv,tempo_integers=sorted(tempos),duration_ticks=sorted(lengths),scope='Original reader order and mapped fields; chord name conversion intercepted, no personality or sequencer execution'),indent=2))
