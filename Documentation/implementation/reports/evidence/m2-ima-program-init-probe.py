"""Standalone project-owned nested-aggregate initialization reproduction; no game data."""
from pathlib import Path
import json,subprocess,tempfile,shutil,hashlib
prefix=Path(__file__).name.removesuffix('-probe.py')
sources={k:Path(__file__).with_name(prefix+'-'+k+'.cpp') for k in ('repro','fixed')}
results=[]
with tempfile.TemporaryDirectory(prefix='b2-midi-init-') as d:
 for name in ('c++','clang++'):
  compiler=shutil.which(name)
  if not compiler:continue
  version=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0]
  for level in ('-O0','-O3'):
   for kind,source in sources.items():
    binary=Path(d)/(name.replace('+','x')+level+kind)
    command=[compiler,'-std=c++20',level,str(source),'-o',str(binary)]
    subprocess.run(command,check=True,capture_output=True,text=True)
    run=subprocess.run([str(binary)],capture_output=True,text=True)
    assert run.returncode in (0,1)
    values=[list(map(int,line.replace(':','').split())) for line in run.stdout.splitlines()]
    assert len(values)==16
    good=all(row==[i,100,127,64,8192] for i,row in enumerate(values))
    assert kind!='fixed' or good
    results.append(dict(compiler=version,optimization=level,variant=kind,passed=good,exit_code=run.returncode,channels=values))
print(json.dumps(dict(scope='Standalone nested aggregate + percussion slot write, before/after explicit fill. No game assets or external binary.',sources={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in sources.items()},results=results),indent=2))
