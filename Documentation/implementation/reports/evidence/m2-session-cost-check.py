from pathlib import Path
import subprocess,os,sys,json,time
root=Path('/home/yann/projects/lithtech/Blood2'); mode=sys.argv[1]
env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
results=[]
def run(name,cmd,extra=None):
    current=env.copy();current.update(extra or {});start=time.monotonic()
    log='/tmp/b2-m2-audit-'+name+'.log'
    with open(log,'w') as out:
        code=subprocess.call(cmd,cwd=root,env=current,stdout=out,stderr=subprocess.STDOUT)
    record=dict(name=name,command=cmd,cwd=str(root),exit_code=code,seconds=round(time.monotonic()-start,2),log=log,environment={k:current[k] for k in ('SDL_AUDIODRIVER','ASAN_OPTIONS','UBSAN_OPTIONS') if k in current})
    if extra:record['environment'].update(extra)
    if mode.startswith('device-'):record['environment_unset']=['SDL_AUDIODRIVER','SDL_AUDIO_DRIVER']
    results.append(record)
    Path('/tmp/b2-m2-audit-'+mode+'-result.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,code,record['seconds'],flush=True)
    return code
if mode.startswith('device-'):
    config=mode.removeprefix('device-'); assert config in ('debug','release','sanitize')
    env.pop('SDL_AUDIODRIVER',None);env.pop('SDL_AUDIO_DRIVER',None)
    run(config+'-device-clean-env',['ctest','--no-tests=error','--test-dir','build/'+config,'-R',r'^cinematics\.device$','-V','--output-junit','/tmp/b2-m2-audit-'+config+'-device-clean-env.xml'])
elif mode.startswith('gpu'):
    configurations=('debug','release') if mode=='gpu' else (mode.removeprefix('gpu-'),)
    assert all(c in ('debug','release') for c in configurations)
    for config in configurations:
        name=config+'-gpu'
        run(name,['ctest','--no-tests=error','--test-dir','build/'+config,'-R',r'^gpu\.(lifecycle|cinematics|level_change|train_session|player_keepalive)$','-j','1','-V','--output-junit','/tmp/b2-m2-audit-'+name+'.xml'],{'VK_DRIVER_FILES':'/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json'})
elif mode=='particle-report':
    run('sanitize-particle-report',['timeout','360s','build/sanitize/tests/particle_stream_tests','private','/home/yann/.steam/steam/steamapps/common/Blood II/'])
else:
    assert mode in ('debug','release','sanitize')
    if not run(mode+'-build-final',['cmake','--build','build/'+mode,'--parallel','1']):
        name=mode+'-all-remaining' if mode=='sanitize' else mode+'-all-headless'
        command=['ctest','--no-tests=error','--test-dir','build/'+mode,'-LE','gpu','-j','2','-V','--output-junit','/tmp/b2-m2-audit-'+name+'.xml']
        if mode=='sanitize':command+=['-E',r'^ai_runtime\.clearance_private$']
        run(name,command)
raise SystemExit(any(r['exit_code'] for r in results))
