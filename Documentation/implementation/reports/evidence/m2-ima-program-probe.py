from pathlib import Path
import hashlib,json,struct,sys,random
from unicorn import *
from unicorn.x86_const import *
data=(Path(sys.argv[1])/'IMUSIC25.DLL').read_bytes();sha=hashlib.sha256(data).hexdigest();assert sha=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0]
p=u32(60);opt=p+24;base=u32(opt+28);size=u32(opt+56)
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,size)
for i in range(u16(p+6)):
 s=opt+u16(p+20)+i*40;r,n,o=u32(s+12),u32(s+16),u32(s+20);u.mem_write(base+r,data[o:o+n])
mem=0x100000;u.mem_map(mem,0x20000);obj=mem+0x1000;player=mem+0x2000;sink=mem+0x3000;vtable=mem+0x3100;stub=mem+0x3200;stack=mem+0x1e000
put=lambda a,f,*v:u.mem_write(a,struct.pack(f,*v));get=lambda a,f:struct.unpack(f,u.mem_read(a,struct.calcsize(f)))
put(base+0x1e6a0,'<I',player);put(player+8,'<I',sink);put(sink,'<I',vtable);put(vtable+16,'<I',stub)
u.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
received=[];freed=[]
def hook(e,a,n,_):
 if a not in (stub,base+0x1b6e3):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=get(sp,'<I')[0]
 if a==stub:
  m=get(sp+8,'<I')[0];received.append((get(m+4,'<i')[0],*get(m+10,'<BBB')));pop=8
 else:freed.append(get(sp+4,'<I')[0]);pop=0
 e.reg_write(UC_X86_REG_ESP,sp+4+pop);e.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(10817);digest=bytearray();cases=500;total=0
for case in range(cases):
 count=case%33;expected=[];nodes=[mem+0x4000+i*32 for i in range(count)]
 u.mem_write(obj,bytes(256));put(obj+0x24,'<I',nodes[0] if nodes else 0);put(obj+0x6d,'<i',123);put(obj+0x75,'<i',768)
 for i,node in enumerate(nodes):
  tick=rng.randrange(-32768,100000);status=0x80+rng.randrange(16);key=rng.randrange(128);vel=rng.randrange(128)
  put(node,'<Ii',nodes[i+1] if i+1<count else 0,tick);put(node+10,'<BBB',status,key,vel);expected.append((tick,status,key,vel))
 received.clear();freed.clear();bp=stack-256;put(bp-0x2c,'<i',17)
 u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EBX,obj+0x24);u.reg_write(UC_X86_REG_EBP,bp);u.reg_write(UC_X86_REG_ESP,stack-512)
 u.emu_start(base+0x17996,base+0x179c2,count=100000)
 assert received==expected and freed==nodes and get(obj+0x24,'<I')[0]==0 and get(obj+0x71,'<i')[0]==908
 for row in received:digest.extend(struct.pack('<iBBB',*row))
 total+=count
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=sha,cases=cases,terminal_notes=total,decisions_sha256=hashlib.sha256(digest).hexdigest(),unicorn=__import__('unicorn').__version__,rvas=['0x17996..0x179c2','0x4e49..0x4f0f'],scope='Original finite-section termination and off-message construction. Synthetic active lists; sink/free intercepted; no playlist, OS or audio executed.'),indent=2))
