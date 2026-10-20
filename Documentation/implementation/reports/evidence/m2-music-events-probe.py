"""Original IMA note-event tail, isolated with optional Unicorn 2.1.4.
Explicit already-resolved pitch/grid time/transpose/percussion table and random
returns: this is not the preceding harmonizer or actual soundtrack scheduling.
No native DLL execution, OS imports or commercial bytes exported.
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
mem=0x100000;emu.mem_map(mem,0x10000)
note=mem+0x1000;player=mem+0x2000;band=mem+0x3000;percussion=mem+0x4000;off=mem+0x5000
stack=mem+0x8000;frame=mem+0x9000;state={}
def read(o,fmt):return struct.unpack(fmt,emu.mem_read(o,struct.calcsize(fmt)))
def put(o,fmt,*v):emu.mem_write(o,struct.pack(fmt,*v))
def hook(e,address,size,_):
 if address not in (base+0x1b673,base+0x1b6b4,base+0x4c58):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=read(sp,'<I')[0];result=cleanup=0
 if address==base+0x1b673:
  assert state['used']<len(state['draws']);result=state['draws'][state['used']];state['used']+=1
 elif address==base+0x1b6b4:
  assert read(sp+4,'<I')[0]==21 and state['allocated']==0
  result=off;state['allocated']+=1;emu.mem_write(off,bytes(32))
 else:
  t,status,key,velocity,flags=read(sp+4,'<iIIII');assert flags==0 and len(state['on'])==0
  state['on'].append((t,status&255,key&255,velocity&255));cleanup=20
 e.reg_write(UC_X86_REG_EAX,result);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
signed8=lambda v:(v+128)%256-128
signed16=lambda v:(v+32768)%65536-32768
signed32=lambda v:(v+2147483648)%4294967296-2147483648
mapping=bytes((i*7+13)&127 for i in range(128))
def observe(raw,pitch,tick,transpose,mapbytes=b''):
 assert len(raw)==18 and raw[1]<16
 emu.mem_write(frame-0x200,bytes(0x220));emu.mem_write(note,bytes(4)+raw)
 emu.mem_write(player,bytes(0x100));emu.mem_write(band,bytes(0x400))
 put(base+0x20708,'<I',percussion if mapbytes else 0);put(base+0x2070c,'<I',0)
 if mapbytes:emu.mem_write(percussion+0x85,mapbytes)
 put(frame+8,'<i',tick);put(frame-6,'<h',pitch);put(frame-0x38,'<I',band)
 put(band+0x334+raw[1],'<b',transpose)
 emu.reg_write(UC_X86_REG_ESP,stack);emu.reg_write(UC_X86_REG_EBP,frame)
 emu.reg_write(UC_X86_REG_ESI,player);emu.reg_write(UC_X86_REG_EDI,note)
 ranges=raw[14:17];draws=[32767,123,7654][:sum(bool(v) for v in ranges)] if pitch else []
 state.update(used=0,draws=draws,allocated=0,on=[])
 emu.emu_start(base+0x173ee,base+0x17595,timeout=100000,count=10000)
 assert emu.reg_read(UC_X86_REG_EIP)==base+0x17595
 assert state['used']==len(draws)
 if pitch==0:
  assert not state['on'] and not state['allocated'];return b'\0\0'
 cursor=0
 def jitter(v):
  nonlocal cursor
  if not v:return 0
  signed=signed8(v);draw=draws[cursor];cursor+=1
  return draw%abs(signed)-(signed//2)
 time=signed32(tick+struct.unpack_from('<h',raw,2)[0]+jitter(ranges[0]))
 duration=max(1,struct.unpack_from('<h',raw,12)[0]+jitter(ranges[1]))
 if raw[1]==5 and mapbytes:
  assert 0<=pitch<128;pitch=mapbytes[pitch]
 pitch=signed16(pitch+transpose)
 if pitch<1:pitch+=(12-pitch)//12*12
 if pitch>127:pitch-=(pitch-116)//12*12
 velocity=max(1,min(127,signed8(raw[9])+jitter(ranges[2])))
 on=(time,0x90|((raw[1]+4)&15),pitch,velocity)
 expected_off=(signed32(time+duration),0x80|((raw[1]+4)&15),pitch,velocity)
 observed_off=(read(off+4,'<i')[0],*read(off+10,'<BBB'))
 assert state['on']==[on] and observed_off==expected_off and read(player+0x24,'<I')[0]==off
 assert read(off+14,'<B')[0]==raw[1]+1 and state['allocated']==1
 return bytes([1,len(draws)])+struct.pack('<iBBB',*on)+struct.pack('<iBBB',*observed_off)

def record(ch=0,offset=-24,velocity=100,duration=192,ranges=(0,0,0)):
 return struct.pack('<BBhHBBBBHhBBBB',9,ch,offset,65535,0,0,60,velocity,0,duration,*ranges,1)
synthetic=[]
for ranges in [(0,0,0),(1,0,0),(0,3,0),(0,0,127),(4,5,6),(128,129,255),(255,0,128),(127,127,127)]:
 synthetic.append(observe(record(ranges=ranges),60,1000,-12).hex())
for pitch,transpose in [(0,12),(-32768,-128),(-1,0),(1,-128),(127,127),(32767,127)]:
 synthetic.append(observe(record(ch=15,velocity=255,duration=-32768,ranges=(128,255,127)),pitch,0,transpose).hex())
for ch in [0,5]:synthetic.append(observe(record(ch=ch),60,1000,12,mapping).hex())
# Record the legacy wrap separately; the portable profile deliberately rejects overflow.
wrapped=observe(record(offset=32767),60,2147483647,0).hex()

def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4]
  assert a+8+n<=z;yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
notes=events=0;fnv=14695981039346656037;digest=hashlib.sha256()
for f in sorted((root/'Music').rglob('*.sty')):
 assert f.stat().st_size<=32*1024*1024
 for tag,p in chunks(f.read_bytes()):
  if tag!=b'note':continue
  assert struct.unpack_from('<H',p)[0]==18 and (len(p)-2)%18==0
  for start in range(2,len(p),18):
   raw=p[start:start+18];pitch=signed8(raw[8])
   for tick,transpose,mapbytes in [(10000,0,b''),(12345,-17,mapping)]:
    out=observe(raw,pitch,tick,transpose,mapbytes);events+=out[0]*2;digest.update(out)
    for v in out:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
   notes+=1
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,entry_rva='0x173ee..0x17595',intercepted=['rand with explicit values','one bounded allocation','note-on send captured, not played'],synthetic_cases=len(synthetic),synthetic_results=synthetic,legacy_overflow_observation=wrapped,notes=notes,diagnostic_decisions=notes*2,events=events,canonical_sha256=digest.hexdigest(),canonical_fnv64=fnv,scope='Post-harmonization note timing, transpose, optional explicit percussion remap, velocity and paired messages. Private probes use raw pitch and synthetic context, not actual song pitches/times.'),indent=2))
