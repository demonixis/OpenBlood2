from pathlib import Path
import subprocess,json,shlex
import sys
r=Path(sys.argv[1]).resolve();b=r/'build/debug';tmp=Path('/tmp/b2-polygrid-control');tmp.mkdir(exist_ok=True)
old=tmp/'VolumeBrush.cpp';old.write_bytes(subprocess.check_output(['git','show','43aeaf8032dfb9cc7db5ad3277ce5cdc1904ec45:src/game/original/VolumeBrush.cpp'],cwd=r))
e=next(e for e in json.loads((b/'compile_commands.json').read_text()) if e['file'].endswith('/original/VolumeBrush.cpp'))
a=shlex.split(e['command']);a[a.index('-o')+1]=str(tmp/'old.o');a[a.index('-c')+1]=str(old)
# Keep include lookup of the selected original source directory.
a+=['-I'+str(r/'src/game/original')]
subprocess.run(a,cwd=e['directory'],check=True)
s=subprocess.check_output(['ninja','-t','commands','tests/polygrid_tests'],cwd=b,text=True).splitlines()[-1]
a=shlex.split(s)[2:-2];a[a.index('-o')+1]=str(tmp/'probe');a=[v for v in a if not v.startswith('-Wl,--dependency-file=')];a.insert(a.index('libblood2_game.a'),str(tmp/'old.o'))
subprocess.run(a,cwd=b,check=True)
p=subprocess.run([str(tmp/'probe'),'lifetime',str(tmp/'fixtures')],cwd=r,text=True,capture_output=True)
print(p.stdout+p.stderr,end='');print('Negative control exit:',p.returncode)
assert p.returncode==1 and 'g.server.polygrids.active.size() == 1' in p.stderr
