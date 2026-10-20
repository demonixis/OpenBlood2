from pathlib import Path
import shutil, subprocess, tempfile
root=Path('/home/yann/projects/lithtech/Blood2')
with tempfile.TemporaryDirectory(prefix='b2-service-catalog-') as tmp:
    fixture=Path(tmp); (fixture/'src/game/legacy').mkdir(parents=True)
    files=[root/'src/game/legacy/server_de.h', root/'src/game/server_adapter.h',*root.glob('src/game/server_*.cpp')]
    for f in files: shutil.copyfile(f,fixture/f.relative_to(root))
    command=[str(root/'build/debug/tests/service_coverage_tests'),'catalog',str(fixture)]
    def check(label, expected):
        r=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(label,'exit=',r.returncode, '\n'+r.stdout)
        assert r.returncode==expected
    check('unchanged source fixture',0)
    interface=fixture/'src/game/legacy/server_de.h'; original=interface.read_text()
    at=original.rfind('};');interface.write_text(original[:at]+'    virtual bool MissingReviewedService() = 0;\n'+original[at:])
    check('undeclared catalogue extension',1)
    interface.write_text(original)
    adapter=fixture/'src/game/server_adapter.h';original=adapter.read_text()
    assert 'trace_service("GetPathManager");' in original
    adapter.write_text(original.replace('trace_service("GetPathManager");',''))
    check('missing service instrumentation',1)
print('Controls changed only temporary source fixtures; active repositories were not edited.')
