import os, subprocess, json, time
from pathlib import Path
base=Path('/home/yann/projects/lithtech/Blood2')
prefix=Path('/tmp/b2-m2-acceptance-final')
env=os.environ.copy()
for key in ('SDL_AUDIODRIVER','SDL_AUDIO_DRIVER'):env.pop(key,None)
env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1:allow_addr2line=1'
env['UBSAN_OPTIONS']='halt_on_error=1'
commands=[]
def run(name,args,extra=None):
 e=env.copy();e.update(extra or {})
 start=time.time()
 with open(str(prefix)+'-'+name+'.log','w') as f:
  result=subprocess.run(args,cwd=base,env=e,stdout=f,stderr=subprocess.STDOUT)
 commands.append(dict(name=name,command=args,exit=result.returncode,seconds=time.time()-start,environment={k:e[k] for k in ('ASAN_OPTIONS','UBSAN_OPTIONS','SDL_AUDIODRIVER','VK_DRIVER_FILES') if k in e}))
 Path(str(prefix)+'-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 print(name,result.returncode,round(time.time()-start,2),flush=True)
 return result.returncode == 0
for cfg in ['debug','release','sanitize']:
 targets=['blood2_app','train_session_tests','blood_spurt_tests','world_tests','services_tests','object_contract_tests','object_snapshot_tests','service_coverage_tests']
 if cfg!='sanitize':targets+=['blood_spurt_gpu_tests']
 if not run('build-'+cfg+'-verified',['cmake','--build','build/'+cfg,'--target',*targets,'--parallel','3']):break
 pattern=r'^(objects|object_snapshots|world|services|coverage|train_session|blood_spurts)\.'
 if not run('headless-'+cfg,['ctest','--test-dir','build/'+cfg,'-R',pattern,'--output-on-failure','--output-junit',str(prefix)+'-headless-'+cfg+'.xml']):break
 if cfg!='sanitize':
  if not run('gpu-'+cfg,['ctest','--test-dir','build/'+cfg,'-R',r'^gpu\.(lifecycle|world|blood_spurt|blood_spurt_private|train_session)$','-V','--output-junit',str(prefix)+'-gpu-'+cfg+'.xml'],{'SDL_AUDIODRIVER':'dummy','VK_DRIVER_FILES':'/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json'}):break
