from pathlib import Path
import re,json,statistics
results=[]
for cfg in ('debug','release'):
 for run in (1,2,3):
  for version in ('before','after'):
   text=Path(f'/tmp/b2-model-prep-{cfg}-timing-{run}-{version}.log').read_text()
   entry={'configuration':cfg,'run':run,'version':version,'worlds':{}}
   current=None
   for line in text.splitlines():
    if line.startswith('WORLD '):
     current=line.split()[1];entry['worlds'][current]={}
    m=re.match(r'^(step|world|model|sprite|fx) mean=(\S+) p50=(\S+) p95=(\S+) max=(\S+)$',line)
    if m:entry['worlds'][current][m[1]]=dict(zip(('mean','p50','p95','max'),map(float,m.groups()[1:])))
   assert len(entry['worlds'])==2 and all(len(x)==5 for x in entry['worlds'].values())
   results.append(entry)
summary=[]
for cfg in ('debug','release'):
 for world in ('worlds/01_subwaya.dat','worlds/02_cityhub.dat'):
  row={'configuration':cfg,'world':world}
  for v in ('before','after'):
   items=[x['worlds'][world] for x in results if x['configuration']==cfg and x['version']==v]
   row[v]={'step_p50_median':statistics.median(x['step']['p50'] for x in items),'step_max_median':statistics.median(x['step']['max'] for x in items),'model_mean_median':statistics.median(x['model']['mean'] for x in items),'model_p95_median':statistics.median(x['model']['p95'] for x in items),'model_max_median':statistics.median(x['model']['max'] for x in items)}
  summary.append(row)
  print(cfg,world,row['before'],'->',row['after'])
Path('/tmp/b2-model-prep-timing.json').write_text(json.dumps({'runs':results,'summary':summary},indent=2)+'\n')
