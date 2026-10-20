from pathlib import Path
import json,hashlib,shutil,subprocess,xml.etree.ElementTree as X,re
root=Path('/home/yann/projects/lithtech');repo=root/'Blood2';dest=root/'Documentation/Implementation/evidence';prefix='b2-session-start'
results={}
for name in ['debug','release','sanitize','debug-gpu','release-gpu']:
 r=X.parse('/tmp/'+prefix+'-'+name+'.xml').getroot()
 assert int(r.get('failures','0'))==0 and int(r.get('skipped','0'))==0,(name,r.attrib)
 log=Path('/tmp/'+prefix+'-'+name+'.log').read_text()
 t=re.search(r'Total Test time \(real\) =\s*([0-9.]+) sec',log)
 results[name]={'tests':int(r.get('tests')),'failures':0,'seconds':float(t.group(1)),'cases':[{'name':v.get('name'),'time':v.get('time'),'status':v.get('status')} for v in r.findall('testcase')]}
objects=json.loads(Path('/tmp/'+prefix+'-objects.json').read_text());assert len(objects)==51 and all(v['code']==0 for v in objects)
gpu=json.loads(Path('/tmp/'+prefix+'-gpu.json').read_text());assert len(gpu)==4 and all(v['verified'] for v in gpu)
verification=json.loads(Path('/tmp/'+prefix+'-verification.json').read_text());assert all(v['code']==0 for v in verification)
assert hashlib.sha256((root/'README.md').read_bytes()).hexdigest()=='57c4e2c795cab83ccb1ab2a28a2ec152352582f17cde8c24ddf8bf53c794b423'
ledger=json.loads((repo/'docs/implementation/sources/m2_session_start_sources.json').read_text())
for file,digest in ledger['frozen_sources'].items(): assert hashlib.sha256((repo/file).read_bytes()).hexdigest()==digest,file
logs=[]
for file in sorted(Path('/tmp').glob(prefix+'-*')):
 if file.suffix not in ('.log','.xml','.json','.py'): continue
 target=dest/('m2-session-start-'+file.name[len(prefix)+1:])
 shutil.copyfile(file,target)
 logs.append({'path':target.name,'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
shutil.copyfile('/tmp/b2-npc-source-integrity.json',dest/'m2-session-start-source-integrity.json')
manifest={'date':'2026-10-18','implementation_parent':ledger['implementation_parent'],'implementation_commit':None,'headless_regressions':results,'object_lifetime_checks':objects,'nvidia_startup_checks':gpu,'commands':verification,'environment':{'gpu':{'VK_DRIVER_FILES':'/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json','SDL_AUDIODRIVER':'dummy'},'sanitizers':{'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'},'headless_audio':'SDL_AUDIODRIVER and SDL_AUDIO_DRIVER unset for regression suites'},'initial_failure':'First world.session_start_private assumed object clock zero across reload; corrected test asserts preserved entry-time clock. Production clock unchanged.','limits':['Selected 98 distinct non-GPU cases in each build, not the complete registered suite.','Four direct graphical startup checks plus five existing GPU cases in each Debug/Release; no native Metal execution.','Earlier GPU LeakSanitizer report has unresolved dynamically loaded module attribution; not rerun or suppressed here.','Startup is a checkpoint, not future-path or full M2 acceptance.'],'root_readme_sha256':hashlib.sha256((root/'README.md').read_bytes()).hexdigest(),'files':logs}
(dest/'m2-session-start.json').write_text(json.dumps(manifest,indent=2)+'\n')
p=root/'Documentation/Implementation/m2_session_start.md';s=p.read_text()
table='The 81 selected session/resource/behavior tests pass in each build, plus 17\nobject lifetime/snapshot checks per build: **98 distinct headless cases** in each\nconfiguration. Clang ASan/UBSan runs with leak detection and halt-on-error enabled.\nThis is a targeted regression run, not the full registered test suite.\n\n| Configuration | Session suite | Object checks | Session-suite time |\n| --- | --- | --- | --- |\n'
for key,label in [('debug','Debug'),('release','Release without inspector'),('sanitize','Clang ASan/UBSan')]:
 table+=f"| {label} | {results[key]['tests']}/{results[key]['tests']} | 17/17 | {results[key]['seconds']:.2f} s |\n"
table+='\nNVIDIA checks pass **5/5 in Debug and 5/5 in Release**, plus direct graphical\nstartup for both SubwayA and CityHub in both builds (four additional checks).\nAll fourteen confirm the NVIDIA RTX 3060 Laptop Vulkan device. The final builds\nuse `cmake --build build/<configuration> --target ... --parallel 3`; exact targets,\nCTest filters, per-case results, environments and failed attempts are retained in\n[the evidence ledger](evidence/m2-session-start.json).\n\nAll 16 changed C++ files pass formatting; Markdown links and whitespace checks\npass. All 10,923 inventoried reference files and six reference repositories remain\nunchanged. The user-edited workspace README is byte-identical.\n'
assert 'Validation results are being collected before this increment is committed.' in s
p.write_text(s.replace('Validation results are being collected before this increment is committed.',table))
print(json.dumps({k:{'tests':v['tests'],'seconds':v['seconds']} for k,v in results.items()},indent=2))
print(len(logs),'evidence files packaged')
