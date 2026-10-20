import subprocess, os, json, time
from pathlib import Path
root=Path('/home/yann/projects/lithtech/Blood2')
prefix=Path('/tmp/b2-session-start')
results=[]
targets=['world_tests','player_tests','blood2_app','sprite_tests','sprite_view_tests','scene_explosion_tests','sound_tests','exit_tests','gideon_runtime_tests','service_coverage_tests','train_session_tests']
pattern=r'^(world\.|player\.|sound\.|sprites?\.|sprite_view\.|sceneexplosion\.|exits\.|gideon_runtime\.|coverage\.|cli.world_modes|train_session\.)'
def run(name,args,env):
    start=time.monotonic()
    with open(f'{prefix}-{name}.log','w') as out:
        r=subprocess.run(args,cwd=root,env=env,stdout=out,stderr=subprocess.STDOUT)
    results.append(dict(name=name,command=args,code=r.returncode,seconds=time.monotonic()-start))
    Path(f'{prefix}-verification.json').write_text(json.dumps(results,indent=2)+'\n')
    return r.returncode==0
for config in ['release','sanitize']:
    env=os.environ.copy()
    env.pop('SDL_AUDIODRIVER',None);env.pop('SDL_AUDIO_DRIVER',None)
    if config=='sanitize':
        env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
    build_targets=targets+(['level_change_gpu_tests'] if config=='release' else [])
    if not run(f'{config}-build',['cmake','--build',f'build/{config}','--target',*build_targets,'--parallel','3'],env):
        raise SystemExit(1)
    run(config,['ctest','--test-dir',f'build/{config}','-R',pattern,'--output-on-failure','--output-junit',f'{prefix}-{config}.xml'],env)
