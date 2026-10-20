from pathlib import Path
import xml.etree.ElementTree as ET
import json
x=ET.parse('/tmp/b2-m2-acceptance-final-headless-release.xml').getroot();rows={};summary=[]
for t in x.iter('testcase'):
 if not t.get('name','').startswith('train_session.private_'):continue
 rate=t.get('name').rsplit('_',1)[1]
 for line in t.findtext('system-out','').splitlines():
  if line.startswith('TRAIN_TRACE\t'):
   _,phase,name,support,calls=line.split('\t')
   rows.setdefault(name,{'support':support})[rate+'_'+phase]=int(calls)
  elif line.startswith(('TRAIN_EFFECTS','Completed train','Knife FIRE_KEY','Message lifetime:','Route summary:')):
   summary.append({'rate':rate,'observation':line})
cols=['30_SubwayA','60_SubwayA','30_CityHub','60_CityHub','30_teardown','60_teardown']
assert len(rows)==174,len(rows)
out='service\tsupport\t'+'\t'.join(cols)+'\n'
for name,row in sorted(rows.items()):
 assert all(c in row for c in cols)
 out+=name+'\t'+row['support']+'\t'+'\t'.join(str(row[c]) for c in cols)+'\n'
p=Path('/home/yann/projects/lithtech/Documentation/Implementation/evidence')
(p/'m2-acceptance-services.tsv').write_text(out)
report={'catalogue_rows':len(rows),'checkpoints':{c:{'reached':sum(r[c]>0 for r in rows.values()),'unexercised':sum(r[c]==0 for r in rows.values())} for c in cols},'unexercised_across_route':[n for n,r in rows.items() if not any(r[c] for c in cols)],'observations':summary}
Path('/tmp/b2-m2-acceptance-service-summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
