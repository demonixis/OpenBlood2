import json,os,pathlib,subprocess,time
rows=[]
env=dict(os.environ,VK_DRIVER_FILES='/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json',SDL_AUDIODRIVER='dummy',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:allow_addr2line=1',UBSAN_OPTIONS='halt_on_error=1')
for mode in ['video','window','gpu']:
 command=['build/sanitize/tests/platform_lifetime_tests',mode,'3']
 log='/tmp/b2-gpu-lifetime-final-'+mode+'.log'; start=time.monotonic()
 with open(log,'w') as f:
  result=subprocess.run(command,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=30)
 rows.append({'command':command,'log':log,'exit_code':result.returncode,'seconds':time.monotonic()-start})
 print(mode,result.returncode,flush=True)
pathlib.Path('/tmp/b2-gpu-lifetime-final-probes.json').write_text(json.dumps(rows,indent=2)+'\n')
