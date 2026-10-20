from pathlib import Path
import subprocess,json,shlex
r=Path('/home/yann/projects/lithtech/Blood2');b=r/'build/debug';tmp=Path('/tmp/b2-ai-ammo-control');tmp.mkdir(exist_ok=True)
old=tmp/'AIState.cpp';old.write_bytes(subprocess.check_output(['git','show','e54401b654171f76419d2a5dbd8c8783cd828a04:src/game/original/AIState.cpp'],cwd=r))
e=next(e for e in json.loads((b/'compile_commands.json').read_text()) if e['file'].endswith('/original/AIState.cpp'))
a=shlex.split(e['command']);a[a.index('-o')+1]=str(tmp/'old.o');a[a.index('-c')+1]=str(old);a+=['-I'+str(r/'src/game/original')]
subprocess.run(a,cwd=e['directory'],check=True)
s=subprocess.check_output(['ninja','-t','commands','tests/ai_runtime_tests'],cwd=b,text=True).splitlines()[-1]
a=shlex.split(s)[2:-2];a[a.index('-o')+1]=str(tmp/'probe');a=[v for v in a if not v.startswith('-Wl,--dependency-file=')];a.insert(a.index('libblood2_game.a'),str(tmp/'old.o'))
subprocess.run(a,cwd=b,check=True)
p=subprocess.run([str(tmp/'probe'),'ammo_search'],cwd=r,text=True,capture_output=True)
print(p.stdout+p.stderr,end='');print('Negative control exit:',p.returncode)
assert p.returncode==1 and 'a->AI_Update()' in p.stderr
