from pathlib import Path
import hashlib,json,re,shutil,subprocess,xml.etree.ElementTree as ET
root=Path('/home/yann/projects/lithtech');repo=root/'Blood2';dest=root/'Documentation/Implementation/evidence';raw=dest/'m2-acceptance';raw.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
results={}
for name in ['headless-debug','headless-release','headless-sanitize','gpu-debug','gpu-release']:
 p=Path('/tmp/b2-m2-acceptance-final-'+name+'.xml');x=ET.parse(p).getroot();assert int(x.get('failures','0'))==0 and int(x.get('skipped','0'))==0,(name,x.attrib)
 log=Path('/tmp/b2-m2-acceptance-final-'+name+'.log').read_text();match=re.search(r'Total Test time \(real\) =\s*([0-9.]+) sec',log)
 results[name]=dict(tests=int(x.get('tests')),failures=0,seconds=float(match[1]),cases=[dict(name=t.get('name'),seconds=float(t.get('time')),status=t.get('status')) for t in x.iter('testcase')])
gpu=json.load(open('/tmp/b2-m2-acceptance-sanitizer-gpu.json'));assert len(gpu)==3 and gpu[0]['exit']==0
assert all(r['exit']==1 and r['leak_allocations']==816 for r in gpu[1:]),gpu
assert gpu[2]['completed_gameplay_assertions'],gpu
assert sha(root/'README.md')=='57c4e2c795cab83ccb1ab2a28a2ec152352582f17cde8c24ddf8bf53c794b423'
source=json.load(open(repo/'docs/implementation/sources/m2_acceptance_sources.json'))
assert all(sha(repo/f)==h for f,h in source['frozen_sources'].items())
verification=json.load(open('/tmp/b2-m2-acceptance-verification.json'));assert all(r['exit']==0 for r in verification)
subprocess.run(['git','diff','--no-ext-diff'],cwd=repo,stdout=open('/tmp/b2-m2-acceptance-code.patch','w'),check=True)
files=[]
for f in sorted(Path('/tmp').glob('b2-m2-acceptance*')):
 if not f.is_file() or f.suffix not in ('.log','.xml','.json','.py','.patch'):continue
 target=raw/f.name.removeprefix('b2-m2-acceptance-');shutil.copyfile(f,target);files.append(dict(path=str(target.relative_to(dest)),bytes=target.stat().st_size,sha256=sha(target)))
files.append(dict(path='m2-acceptance-services.tsv',bytes=(dest/'m2-acceptance-services.tsv').stat().st_size,sha256=sha(dest/'m2-acceptance-services.tsv')))
manifest=dict(date='2026-10-18',implementation_parent=source['implementation_parent'],implementation_commit=None,scope='Final selected M2 functional review; formal closure open on external NVIDIA/D-Bus GPU sanitizer failure.',tests=results,commands=json.load(open('/tmp/b2-m2-acceptance-final-commands.json')),gpu_sanitizer=gpu,service_summary=json.load(open('/tmp/b2-m2-acceptance-service-summary.json')),verification=verification,prior_evidence=[dict(path=x,sha256=sha(dest/x)) for x in ['m2-session-start.json','m2-session-cost.json','m2-gpu-lifetime.json']],root_readme_sha256=sha(root/'README.md'),logging='Release/sanitize passed-output retention raised to 1 MiB in ignored CTestCustom.cmake to preserve complete service rows. No assertion/deadline change. Debug successful JUnit output retains CTest default truncation; verbose GPU log retains full trace.',files=files)
(dest/'m2-acceptance.json').write_text(json.dumps(manifest,indent=2)+'\n')
text='''All three configurations build successfully, with at most three compiler jobs.
The **64 selected headless cases pass in each configuration** (192 executions),
including malformed readers, startup/refusal, original callbacks, lifetime,
private-data transitions, both complete train rates and the new negative controls.
This is targeted regression coverage; the earlier complete non-GPU audit remains
separate historical evidence, not a claim that this turn reran the whole suite.

| Configuration | Headless | NVIDIA GPU checks | Headless time |
| --- | ---: | ---: | ---: |
'''
for key,label in [('debug','Debug, inspector enabled'),('release','Release, inspector disabled'),('sanitize','Clang ASan/UBSan')]:
 result=results['headless-'+key];text+=f"| {label} | {result['tests']}/{result['tests']} | {'5/5' if key!='sanitize' else 'Process fails LSan; see below'} | {result['seconds']:.2f} s |\n"
text+='''
The ten ordinary GPU checks cover device/application lifecycle, world presentation,
synthetic/private blood effects and complete train-to-CityHub rendering. All log
NVIDIA GeForce RTX 3060 Laptop GPU through the NVIDIA Vulkan ICD. The CityHub
checkpoint was visually reviewed. Dummy audio verifies submission/lifetime, not
a listening comparison. No native Metal execution is claimed.

Fresh unsuppressed sanitizer GPU checks remain failures: SDL-only reports
'''
text+=f"**{gpu[1]['leak_bytes']:,} bytes / {gpu[1]['leak_allocations']} allocations**, and the current full route reports\n**{gpu[2]['leak_bytes']:,} / {gpu[2]['leak_allocations']}** after reaching its final teardown assertion. Both exit 1.\n"
text+='''Leak detection and halt-on-error stay enabled; no suppression or successful-process
reclassification is introduced. This matches the separate platform reproduction
in allocation count and retained NVIDIA/D-Bus failure pattern. The older standalone
Vulkan/loader-attribution evidence is retained, not rerun or resymbolicated here.

The [evidence ledger](evidence/m2-acceptance.json) retains exact commands, configuration,
individual results, complete Release/sanitizer service rows, current source delta,
source hashes and failed attempts. Passing-test output retention was increased only
for logs; test deadlines and assertions are unchanged. Changed C++ formatting,
maintained Markdown links and whitespace checks pass. All 10,923 inventoried
reference files and six reference repositories remain unchanged. The user-edited
workspace README is byte-identical; no commercial images or binaries are committed.
'''
f=root/'Documentation/Implementation/m2_acceptance.md';s=f.read_text();assert 'RESULTS_PENDING' in s;f.write_text(s.replace('RESULTS_PENDING',text))
print(json.dumps({k:{a:v[a] for a in ('tests','seconds')} for k,v in results.items()},indent=2));print(len(files),'evidence files packaged')
