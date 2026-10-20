from pathlib import Path
import json,subprocess,re,sys
root=Path('/home/yann/projects/lithtech/Blood2'); config=sys.argv[1];build=root/'build'/config
regex=r'^(ima_cursor\.|ima_selection\.|ima_harmony\.|ima_note_pitch\.|ima_note_event\.|ima_variation\.|ima_timeline\.|ima_section\.|ima_band\.|ima_style\.|dls_sampler\.|dls_bank\.|music_assets\.|world\.|objects\.|sound\.|audio_stream\.|config\.)'
tests=json.loads(subprocess.check_output(['ctest','--test-dir',str(root/'build/debug'),'--show-only=json-v1'],text=True))['tests']
selected=[x for x in tests if re.search(regex,x['name'])]
targets=sorted({Path(x['command'][0]).name for x in selected})+(['blood2_app'] if config!='sanitize' else [])
with open(f'/tmp/b2-music-routing-{config}-build-final.log','w') as f:
 code=subprocess.call(['cmake','--build',str(build),'--target',*targets,'-j','1'],stdout=f,stderr=subprocess.STDOUT)
if code:sys.exit(code)
print(config,'build passed;',len(selected),'selected cases',flush=True)
env=__import__('os').environ.copy()
if config=='sanitize':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
with open(f'/tmp/b2-music-routing-{config}-tests.log','w') as f:
 code=subprocess.call(['ctest','--test-dir',str(build),'-R',regex,'--output-on-failure','--output-junit',f'/tmp/b2-music-routing-{config}-tests.xml','-j','2'],stdout=f,stderr=subprocess.STDOUT,env=env)
print(config,'tests exit',code,flush=True)
sys.exit(code)
