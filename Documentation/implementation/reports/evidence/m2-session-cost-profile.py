import gdb,threading,os,signal,time,json,collections
counts=collections.Counter();leaves=collections.Counter();samples=0
pid=gdb.selected_inferior().pid
for i in range(500):
    timer=threading.Timer(.025,lambda:os.kill(pid,signal.SIGINT))
    timer.start()
    try:
        gdb.execute('continue',to_string=True)
    except gdb.error:
        pass
    timer.cancel()
    if not gdb.selected_inferior().pid:break
    f=gdb.newest_frame();stack=[]
    while f:
        name=f.name() or '?';stack.append(name);f=f.older()
    if stack:
        leaves[stack[0]]+=1
        for name in set(stack):counts[name]+=1
        samples+=1
    if i%100==0:print('SAMPLES',samples)
with open('/tmp/b2-m2-audit-profile.json','w') as out:
    json.dump(dict(samples=samples,inclusive=counts.most_common(70),leaf=leaves.most_common(45)),out,indent=2)
if gdb.selected_inferior().pid:gdb.execute('continue',to_string=True)
