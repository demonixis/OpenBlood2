import subprocess,os,time,json
from pathlib import Path
root=Path('/home/yann/projects/lithtech/Blood2');prefix='/tmp/b2-session-start';results=[]
cases='builtin_model original_objects nested_creation reasons_properties scheduling deferred_removal aggregate_dispatch handles_selection links_lists messages_strings message_churn message_limit unsupported_teardown creation_failures'.split()
for config in ['debug','release','sanitize']:
 env=os.environ.copy()
 if config=='sanitize':
  env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 build=['cmake','--build',f'build/{config}','--target','object_contract_tests','object_snapshot_tests','--parallel','3']
 with open(f'{prefix}-{config}-object-build.log','w') as out:
  r=subprocess.run(build,cwd=root,env=env,stdout=out,stderr=subprocess.STDOUT)
 if r.returncode: raise SystemExit(r.returncode)
 for target, names in [('object_contract_tests',cases),('object_snapshot_tests',['membership','callbacks','churn'])]:
  for case in names:
   args=[f'build/{config}/tests/{target}',case];start=time.monotonic()
   with open(f'{prefix}-{config}-{target}-{case}.log','w') as out:
    r=subprocess.run(args,cwd=root,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=30 if target=='object_snapshot_tests' else 10)
   results.append(dict(config=config,command=args,code=r.returncode,seconds=time.monotonic()-start))
   Path(f'{prefix}-objects.json').write_text(json.dumps(results,indent=2)+'\n')
