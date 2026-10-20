import subprocess,os,time,json
from pathlib import Path
root=Path('/home/yann/projects/lithtech/Blood2');prefix='/tmp/b2-session-start';results=[]
env=os.environ.copy();env['VK_DRIVER_FILES']='/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json';env['SDL_AUDIODRIVER']='dummy'
for config in ['release','debug']:
 for world in ['01_SubwayA','02_CityHub']:
  capture=root/f'build/{config}/tests/session-start-{world}.png'
  args=[f'build/{config}/blood2','--config',f'build/{config}/tests/empty.ini','--data-dir','/home/yann/.steam/steam/steamapps/common/Blood II/','--view-world','--world',world,'--frames','12','--capture',str(capture)]
  log=f'{prefix}-{config}-gpu-start-{world}.log';start=time.monotonic()
  with open(log,'w') as out:
   r=subprocess.run(args,cwd=root,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=60)
  text=Path(log).read_text();ok=r.returncode==0 and 'NVIDIA GeForce RTX 3060 Laptop GPU' in text and 'Startup checkpoint passed before first frame:' in text
  results.append(dict(name=f'{config}-{world}',command=args,code=r.returncode,verified=ok,seconds=time.monotonic()-start,capture=str(capture)))
  Path(prefix+'-gpu.json').write_text(json.dumps(results,indent=2)+'\n')
