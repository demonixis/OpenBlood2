from pathlib import Path
import os,subprocess,time,json,re
p=Path('/home/yann/projects/lithtech/Blood2')
e=os.environ.copy();e.update(VK_DRIVER_FILES='/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json',SDL_AUDIODRIVER='dummy',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:allow_addr2line=1',UBSAN_OPTIONS='halt_on_error=1')
results=[]
for name,command in [
 ('baseline-build',['cmake','--build','build/sanitize','--target','platform_lifetime_tests','--parallel','3']),
 ('baseline',['timeout','30','build/sanitize/tests/platform_lifetime_tests','gpu','1']),
 ('train',['timeout','240','build/sanitize/tests/train_session_tests','/home/yann/.steam/steam/steamapps/common/Blood II/','60','build/sanitize/tests/m2-acceptance-gpu'])]:
 f=Path('/tmp/b2-m2-acceptance-sanitizer-gpu-'+name+'.log');start=time.time()
 with f.open('w') as stream:r=subprocess.run(command,cwd=p,env=e,stdout=stream,stderr=subprocess.STDOUT)
 log=f.read_text();leak=re.search(r'SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)',log)
 results.append(dict(name=name,command=command,exit=r.returncode,seconds=time.time()-start,leak_bytes=int(leak[1]) if leak else None,leak_allocations=int(leak[2]) if leak else None,completed_gameplay_assertions='TRAIN_DOOR_AUDIO\tteardown\t0' in log,environment={k:e[k] for k in ['VK_DRIVER_FILES','SDL_AUDIODRIVER','ASAN_OPTIONS','UBSAN_OPTIONS']}))
 Path('/tmp/b2-m2-acceptance-sanitizer-gpu.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name,r.returncode,results[-1]['leak_bytes'],results[-1]['leak_allocations'],flush=True)
 if name=='baseline-build' and r.returncode:break
