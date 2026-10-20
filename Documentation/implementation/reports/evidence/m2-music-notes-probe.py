"""Isolated original IMA note-reader probe; requires optional Unicorn 2.1.4.
No native DLL loading, entrypoint, imported OS call or commercial bytes exported.
The three intercepted facilities are bounded memory reads, seeks and zeroed allocation.
"""
from pathlib import Path
import hashlib,struct,json,sys
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
root=Path(sys.argv[1]); binary=root/'IMUSIC25.DLL'
assert 0<binary.stat().st_size<1024*1024
with binary.open('rb') as f: data=f.read(1024*1024+1)
assert len(data)<1024*1024
print_hash=hashlib.sha256(data).hexdigest()
assert print_hash=='5500f83148d67d73831d413bb2f3f41a2ce33444514590836e5d021232f5122d'
u16=lambda o:struct.unpack_from('<H',data,o)[0]
u32=lambda o:struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c); opt=pe+24; base=u32(opt+28); size=u32(opt+56)
assert data[pe:pe+4]==b'PE\0\0' and u16(pe+4)==0x14c and u16(opt)==0x10b
assert base==0x35440000 and size==0x25000
emu=Uc(UC_ARCH_X86,UC_MODE_32);emu.mem_map(base,size)
for i in range(u16(pe+6)):
 s=opt+u16(pe+20)+40*i;r,n,o=u32(s+12),u32(s+16),u32(s+20)
 assert o+n<=len(data) and r+n<=size
 emu.mem_write(base+r,data[o:o+n])
emu.mem_protect(base,size,UC_PROT_READ|UC_PROT_EXEC)
mem=0x100000;emu.mem_map(mem,0x100000)
stream=mem;vtable=mem+0x100;chunk=mem+0x200;stack=mem+0x10000
read,seek,stop=mem+0x300,mem+0x310,mem+0x320
emu.mem_write(stream,struct.pack('<I',vtable));emu.mem_write(vtable+12,struct.pack('<I',read));emu.mem_write(vtable+20,struct.pack('<I',seek))
emu.mem_write(read,b'\xc3');emu.mem_write(seek,b'\xc3')
state={}
def word(o):return struct.unpack('<I',emu.mem_read(o,4))[0]
def hook(e,address,size,_):
 if address not in (read,seek,base+0x1b6b4):return
 sp=e.reg_read(UC_X86_REG_ESP);ret=word(sp)
 if address==read:
  dest,n,out=word(sp+8),word(sp+12),word(sp+16);p=state['position'];b=state['payload']
  assert n<=18 and p+n<=len(b)
  e.mem_write(dest,b[p:p+n]);e.mem_write(out,struct.pack('<I',n));state['position']+=n;cleanup=16;value=0
 elif address==seek:
  n=word(sp+8);assert word(sp+12)==0 and word(sp+16)==1 and state['position']+n<=len(state['payload'])
  state['position']+=n;cleanup=20;value=0
 else:
  assert word(sp+4)==22 and len(state['nodes'])<4096
  value=mem+0x20000+32*len(state['nodes']);state['nodes'].append(value);e.mem_write(value,bytes(32));cleanup=0
 e.reg_write(UC_X86_REG_EAX,value);e.reg_write(UC_X86_REG_ESP,sp+4+cleanup);e.reg_write(UC_X86_REG_EIP,ret)
emu.hook_add(UC_HOOK_CODE,hook)
def observe(payload):
 state.update(payload=payload,position=0,nodes=[])
 emu.mem_write(chunk,struct.pack('<II',0,len(payload)));emu.mem_write(stack,struct.pack('<III',stop,stream,chunk));emu.reg_write(UC_X86_REG_ESP,stack)
 emu.emu_start(base+0x8d5f,stop,timeout=100000,count=1000000)
 assert emu.reg_read(UC_X86_REG_EIP)==stop and state['position']==len(payload)
 node=emu.reg_read(UC_X86_REG_EAX);out=[]
 while node:
  assert node==state['nodes'][len(out)]
  out.append(bytes(emu.mem_read(node+4,18)));node=word(node)
 assert len(out)==len(state['nodes'])
 return out
# Distinct synthetic fields, negative signed words, and an extension skipped by the old loader.
records=[bytes(range(18)),struct.pack('<BBhHBBBBHhBBBB',9,15,-24,0x8001,0,0,60,100,0x100f,960,4,0,7,0x87)]
assert observe(struct.pack('<H',18)+b''.join(records))==records
assert observe(struct.pack('<H',20)+b''.join(r+b'xy' for r in records))==records
assert observe(struct.pack('<H',18))==[]
def chunks(b,a=12,z=None):
 z=len(b) if z is None else z
 while a<z:
  n=struct.unpack_from('<I',b,a+4)[0];p=b[a+8:a+8+n];tag=b[a:a+4]
  assert a+8+n<=z
  yield tag,p
  if tag in (b'RIFF',b'LIST'):yield from chunks(b,a+12,a+8+n)
  a+=8+n+(n&1)
count=notes=0;digest=hashlib.sha256();bybank={}
for file in sorted((root/'Music').rglob('*.sty')):
 n=0
 assert file.stat().st_size<=32*1024*1024
 for tag,p in chunks(file.read_bytes()):
  if tag!=b'note':continue
  assert struct.unpack_from('<H',p)[0]==18 and (len(p)-2)%18==0
  out=observe(p);assert b''.join(out)==p[2:]
  count+=1;notes+=len(out);n+=len(out);digest.update(b''.join(out))
 bybank[file.parent.name]=bybank.get(file.parent.name,0)+n
print(json.dumps(dict(binary='IMUSIC25.DLL',sha256=print_hash,entry_rva='0x8d5f',reader_core_bytes=18,synthetic_cases=3,note_chunks=count,notes=notes,decoded_sha256=digest.hexdigest(),notes_by_bank=bybank,scope='Original loader order and exact field preservation only; no sequencer or native DLL execution'),indent=2))
