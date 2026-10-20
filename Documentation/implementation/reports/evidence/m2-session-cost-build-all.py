from pathlib import Path
import concurrent.futures,subprocess,sys
root=Path('/home/yann/projects/lithtech/Blood2')
def build(config):
    with open('/tmp/b2-m2-audit-'+config+'-build-all.log','w') as f:
        result=subprocess.call(['cmake','--build',str(root/'build'/config),'--parallel','1'],stdout=f,stderr=subprocess.STDOUT)
    print(config,'build exit',result,flush=True)
    return result
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    results=list(pool.map(build,('debug','release','sanitize')))
sys.exit(any(results))
