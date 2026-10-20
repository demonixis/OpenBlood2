import collections,json,pathlib,re,subprocess
rows=[]
for name in ['vulkan','gpu1-modules','gpu1-final-modules','train']:
    path=pathlib.Path('/tmp/b2-gpu-lifetime-'+name+'.log')
    t=path.read_text()
    summary=re.search(r'SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)',t)
    if not summary: continue
    pid=re.search(r'==(\d+)==ERROR: LeakSanitizer', t)[1]
    module_text='\n'.join(line for line in t.splitlines() if re.match(r'\s*'+pid+r':', line))
    mappings=[]
    for m in re.finditer(r'file=(.+?) \[\d+\];  generating link map\n\s*\d+:\s+dynamic: (0x\w+)  base: (0x\w+)   size: (0x\w+)',module_text):
        mappings.append({'file':m[1], 'base':int(m[3],16), 'size':int(m[4],16)})
    groups=[]; totals=collections.defaultdict(lambda:{'bytes':0,'allocations':0})
    for g in re.finditer(r'(Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\) allocated from:\n(.*?)(?=\n(?:Direct|Indirect) leak|\nSUMMARY:)',t,re.S):
        frames=g[4].splitlines(); first=next(x for x in frames if '#1 ' in x)
        caller=re.search(r'#1 (0x\w+)',first)[1]
        candidates=[m for m in mappings if m['base']<=int(caller,16)<m['base']+m['size']]
        origins=sorted(set(m['file'] for m in candidates))
        assert origins and all(pathlib.Path(o).name.startswith(('libnvidia-', 'libdbus-1.so.')) for o in origins), (name, first, origins)
        origin=pathlib.Path(origins[0]).name if len(origins)==1 else 'ambiguous: '+', '.join(origins)
        totals[origin]['bytes']+=int(g[2]);totals[origin]['allocations']+=int(g[3])
        unknown=[]
        for a in re.findall(r'(0x\w+)  \(<unknown module>\)',g[4]):
            hits=[{'file':m['file'],'offset':hex(int(a,16)-m['base'])} for m in mappings if m['base']<=int(a,16)<m['base']+m['size']]
            assert hits and all('libnvidia-' in h['file'] for h in hits), (name,a,hits)
            unknown.append({'address':a,'candidate_mappings':hits})
        groups.append({'kind':g[1],'bytes':int(g[2]),'allocations':int(g[3]),'allocator_caller':first.strip(),'module':origin,'unloaded_frames':unknown})
    assert sum(g['bytes'] for g in groups)==int(summary[1])
    assert sum(g['allocations'] for g in groups)==int(summary[2])
    rows.append({'name':name,'bytes':int(summary[1]),'allocations':int(summary[2]),'by_allocator_caller_module':dict(totals),'groups':groups,'interpretation':'Allocation call-site attribution, not ownership proof. Mapping candidates retain reused address ranges.'})
    print(name,summary[1],summary[2],dict(totals))
pathlib.Path('/tmp/b2-gpu-lifetime-attribution.json').write_text(json.dumps(rows,indent=2)+'\n')
