"""Isolated original IMA style-point expansion/send. Optional Unicorn 2.1.4.
Usage: python probe.py EXTERNAL_GAME_ROOT [SYNTHETIC_FIXTURE_OUTPUT]
No native DLL, OS or synth execution; no commercial tables/records exported.
"""
from pathlib import Path
import hashlib,struct,json,sys,math
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
root=Path(sys.argv[1]);data=(root/'IMUSIC25.DLL').read_bytes();sha=hashlib.sha256(data).hexdigest()
assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c);opt=pe+24;base=u32(opt+28);size=u32(opt+56)
assert base==0x35440000 and size==0x25000 and u16(pe+4)==0x14c
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size;emu.mem_write(base+r,data[o:o+n])
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
mem=0x100000;emu.mem_map(mem,0x100000);curve=mem;point=mem+0x100;stack=mem+0x10000;stop=mem+0x200;state={}
def read(o,fmt):return struct.unpack(fmt,emu.mem_read(o,struct.calcsize(fmt)))
def put(o,fmt,*v):emu.mem_write(o,struct.pack(fmt,*v))
def hook(e,address,size,_):
 if address not in (base+0x1b6b4,base+0x1b6e3,base+0x4c58):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=read(sp,'<I')[0];cleanup=0;value=0
 if address==base+0x1b6b4:
  assert read(sp+4,'<I')[0]==22 and len(state['allocated'])<193
  value=mem+0x20000+32*len(state['allocated']);state['allocated'].add(value);emu.mem_write(value,bytes(32))
 elif address==base+0x1b6e3:
  p=read(sp+4,'<I')[0];assert p in state['allocated'] and p not in state['freed'];state['freed'].add(p)
 else:
  t,status,a,b,flags=read(sp+4,'<iIIII');assert flags==0
  state['messages'].append((t,status&255,a&255,b&255));cleanup=20
 e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(typ,ch,controller,p,tick):
 emu.mem_write(curve,bytes(32));put(curve+4,'<BB',typ,ch);put(curve+8,'<H',0xffff);put(curve+18,'<B',controller)
 emu.mem_write(point,struct.pack('<II',0,curve)+struct.pack('<BBhhhh',*p))
 put(stack,'<IIII',stop,point,tick&0xffffffff,0);emu.reg_write(UC_X86_REG_ESP,stack)
 state.update(allocated=set(),freed=set(),messages=[])
 emu.emu_start(base+0x12168,stop,timeout=1000000,count=2000000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and state['allocated']==state['freed']
 return state['messages']
# Regenerated mathematics, not copied lookup bytes. Exact comparison to all 772 entries.
logtable=[int(19200*math.log10(1+9*i/192)) for i in range(193)]
sinetable=[int(9600*(1-math.cos(math.pi*i/192))) for i in range(193)];sinetable[96]=9600
for rva,table in [(0x1d8c0,[100*i for i in range(193)]),(0x1da48,sinetable),(0x1dbd0,logtable),(0x1dd58,[19200-logtable[192-i] for i in range(193)])]:
 assert tuple(table)==read(base+rva,'<193h'),hex(rva)
wrap=lambda v:(v+32768)%65536-32768
trunc=lambda a,b:abs(a)//b*(1 if a>=0 else -1)
def expected(typ,ch,controller,p,tick):
 shape,flags,start,value,end,target=p;rows=[]
 def add(t,v,replace=False):
  bits=v&65535
  row=(tick+t,({3:0xe0,4:0xb0,5:0xd0,6:0xa0}[typ])+((ch+4)&15),bits&127 if typ==3 else controller,((bits>>7)&254) if typ==3 else bits&255)
  if replace:rows[-1]=row
  else:rows.append(row)
 if shape==1:add(start,value if flags&2 else target);return rows
 table={2:[19200-logtable[192-i] for i in range(193)],3:logtable,4:sinetable}.get(shape,[100*i for i in range(193)])
 delta=wrap(target-value);duration=wrap(end-start)
 def at(index):
  v=wrap(value+trunc(table[index]*delta,19200))
  return wrap(value-v+target) if flags&2 else v
 # Source initial endpoint index is 0 without reversal, 192 with reversal.
 lastval=at(192 if flags&1 else 0);lasttime=start;add(start,lastval)
 for i in range(1,192):
  t=wrap(start+trunc(i*duration,192))
  if t-lasttime<3:continue
  v=at(192-i if flags&1 else i)
  if v==lastval or t==lasttime:continue
  add(t,v);lasttime,lastval=t,v
 v=at(0 if flags&1 else 192)
 if v!=lastval:add(end,v,end==lasttime)
 return rows
cases=[]
points=[(-1,1513,798,8192),(0,0,192,127),(100,40,101,80),(20,40,20,80),(100,40,-30,80),(0,50,192,50),(-32768,32767,32767,-32768),(32767,-32768,-32768,32767),(-100,-200,100,0)]
for typ in [3,4,5,6]:
 for shape in [0,1,2,3,4,255]:
  for flags in [0,1,2,3,128,255]:
   for values in points:
    p=(shape,flags,*values);ch=(len(cases)*7)%16;controller=(len(cases)*19)%256;tick=10000
    actual=observe(typ,ch,controller,p,tick);want=expected(typ,ch,controller,p,tick)
    assert actual==want,(typ,ch,controller,p,actual[:3],want[:3],actual[-1:],want[-1:])
    cases.append((typ,ch,controller,p,tick,actual))
# Explicit owner and caller tick: this isolates style point expansion, not original loader owner fixups/admission.
def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4];assert a+8+n<=z
  yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
def encode(rows):return b''.join(struct.pack('<iBBB',*row) for row in rows)
def aggregate(items):
 h=hashlib.sha256();fnv=14695981039346656037;events=0
 for rows in items:
  b=struct.pack('<I',len(rows))+encode(rows);events+=len(rows);h.update(b)
  for v in b:fnv=((fnv^v)*1099511628211)&0xffffffffffffffff
 return dict(decisions=len(items),messages=events,sha256=h.hexdigest(),fnv64=fnv)
private=[];bybank={}
for f in sorted((root/'Music').rglob('*.sty')):
 for tag,p in chunks(f.read_bytes()):
  if tag!=b'curv':continue
  assert struct.unpack_from('<HH',p)==(7,10);pos=4
  while pos<len(p):
   typ,ch,offset,var,controller,count=struct.unpack_from('<BBhHBH',p,pos);pos+=9
   for j in range(count):
    pointdata=struct.unpack_from('<BBhhhh',p,pos);pos+=10
    actual=observe(typ,ch,controller,pointdata,10000);assert actual==expected(typ,ch,controller,pointdata,10000)
    private.append(actual);bybank[f.parent.name]=bybank.get(f.parent.name,0)+len(actual)
if len(sys.argv)>2:
 # Synthetic explicit inputs and original observed messages only, no private bytes.
 with open(sys.argv[2],'w') as f:
  for typ,ch,controller,p,tick,rows in cases:
   f.write(' '.join(map(str,(typ,ch,controller,*p,tick,len(rows),*(v for row in rows for v in row))))+'\n')
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,entry_rva='0x12168 (calls 0x11ca9)',regenerated_table_entries=772,synthetic=aggregate([c[-1] for c in cases]),private=aggregate(private),messages_by_bank=bybank,scope='Explicit point owner/controller and tick; original allocation/free intercepted and send captured. No loader summary/owner fixup, admission, merged order, synth or transport claim.'),indent=2))
