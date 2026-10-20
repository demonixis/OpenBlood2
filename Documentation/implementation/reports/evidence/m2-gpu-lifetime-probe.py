import json, os, pathlib, subprocess, time
base = pathlib.Path('/tmp/b2-gpu-lifetime')
env = dict(os.environ, VK_DRIVER_FILES='/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json', SDL_AUDIODRIVER='dummy', ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:allow_addr2line=1', UBSAN_OPTIONS='halt_on_error=1')
rows = []
for mode, cycles, extra in [('video','1',{}), ('window','1',{}), ('gpu','1',{'LD_DEBUG':'files'}), ('video','3',{}), ('gpu','3',{}), ('gpu','1',{'SDL_SHUTDOWN_DBUS_ON_QUIT':'1'}), ('gpu','1',{'SDL_VIDEO_DRIVER':'x11'})]:
    name = mode + cycles + ('-modules' if 'LD_DEBUG' in extra else '-dbus-shutdown' if 'SDL_SHUTDOWN_DBUS_ON_QUIT' in extra else '-x11' if 'SDL_VIDEO_DRIVER' in extra else '')
    command = ['build/sanitize/tests/platform_lifetime_tests', mode, cycles]
    log = str(base) + '-' + name + '.log'
    start = time.monotonic()
    with open(log, 'w') as f:
        result = subprocess.run(command, env=env|extra, stdout=f, stderr=subprocess.STDOUT, timeout=90)
    row = {'name':name, 'command':command, 'environment':{k:env[k] for k in ['VK_DRIVER_FILES','SDL_AUDIODRIVER','ASAN_OPTIONS','UBSAN_OPTIONS']}|extra, 'returncode':result.returncode, 'seconds':time.monotonic()-start, 'log':log}
    rows.append(row)
    pathlib.Path(str(base)+'-probes.json').write_text(json.dumps(rows, indent=2)+'\n')
    print(name, result.returncode, round(row['seconds'], 2), flush=True)
