#!/usr/bin/env python3
"""tools/gi/bake_all.py against a stub game binary (gi_contracts.md lot G6).

Usage: gi_bake_all_test.py <bake_all.py> <scratch directory>. Unix only (the stub is a script).
"""

import json
import os
import shutil
import signal
import subprocess
import sys
import time

STUB = r'''#!PYTHON -I
import json, os, signal, sys, time
arguments = sys.argv[1:]
log = os.environ["STUB_LOG"]
with open(log, "a") as stream:
    stream.write(json.dumps(arguments) + "\n")
WORLDS = [("worlds/a.dat", "single"), ("worlds/b.dat", "single"), ("worlds/c.dat", "single"),
          ("worlds/slow.dat", "addon"), ("worlds/m.dat", "multi")]
def gi_file(path):
    return path[:-4] + ".0123456789ab.ob2gi"
def value(name):
    return arguments[arguments.index(name) + 1]
if "--list-worlds" in arguments:
    print(json.dumps({"edition": "retail", "worlds": [
        {"path": p, "kind": k, "digest": "00", "gi_file": gi_file(p)} for p, k in WORLDS]}))
    sys.exit(0)
if "--gi-verify" in arguments:
    with open(value("--gi-verify")) as stream:
        content = stream.read()
    print("status " + ("ready" if content == "ok" else "stale"))
    if content != "ok":
        print("stale placement settings")
    sys.exit(0 if content == "ok" else 4)
world = value("--bake-gi")
out = value("--gi-out")
if world == "worlds/b.dat":
    print('GI_RESULT status=failed exit=4 world=worlds/b.dat error="broken \\"face\\" list" ms=1')
    sys.exit(4)
if world == "worlds/slow.dat":
    cancelled = []
    signal.signal(signal.SIGINT, lambda number, frame: cancelled.append(number))
    open(os.environ["STUB_MARKER"], "w").close()
    while not cancelled:
        time.sleep(0.01)
    print('GI_RESULT status=cancelled exit=12 world=worlds/slow.dat error="cancelled" ms=5')
    sys.exit(12)
file = os.path.join(out, gi_file(world))
if "--gi-if-stale" in arguments and os.path.exists(file):
    print("GI_RESULT status=current exit=0 world=%s file=%s bytes=2 ms=1" % (world, file))
    sys.exit(0)
os.makedirs(os.path.dirname(file), exist_ok=True)
with open(file, "w") as stream:
    stream.write("ok")
print("GI_PROGRESS stage=survey done=1 total=1")
print("GI_RESULT status=baked exit=0 world=%s quality=%s bounces=2 rays_per_probe=256 jobs=%s file=%s "
      "placement=current probes=3 reflections=1 bytes=2 ms=7" % (world, value("--gi-quality"),
                                                                value("--gi-jobs"), file))
'''

failures = 0


def check(condition, message):
    global failures
    if not condition:
        failures += 1
        print("FAIL: " + message)


def main():
    script, scratch = sys.argv[1], sys.argv[2]
    shutil.rmtree(scratch, ignore_errors=True)
    os.makedirs(scratch)
    stub = os.path.join(scratch, "blood2")
    with open(stub, "w") as stream:
        stream.write(STUB.replace("PYTHON", sys.executable))
    os.chmod(stub, 0o755)
    out = os.path.join(scratch, "out")
    report_path = os.path.join(scratch, "report.json")
    log = os.path.join(scratch, "calls.jsonl")
    marker = os.path.join(scratch, "slow.marker")
    environment = dict(os.environ, STUB_LOG=log, STUB_MARKER=marker)
    common = [sys.executable, "-I", script, "--blood2", stub, "--out", out, "--report", report_path,
              "--placement-dir", os.path.join(scratch, "placement"), "--threads", "3"]

    def run(*extra):
        result = subprocess.run(common + list(extra), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                universal_newlines=True, env=environment, timeout=60)
        return result.returncode, result.stdout, result.stderr

    def report():
        with open(report_path) as stream:
            return json.load(stream)

    def statuses(document):
        return {entry["world"]: entry["status"] for entry in document["worlds"]}

    # First run: a and c bake, b fails without stopping the run; multi worlds are not selected.
    code, stdout, _ = run("--worlds", "a", "b.dat", "worlds/c.dat")
    check(code == 1, "a failed world exits 1, got %d" % code)
    check("GI_BAKE worlds/a.dat PASS" in stdout and "GI_BAKE worlds/c.dat PASS" in stdout, "PASS lines")
    check("GI_BAKE worlds/b.dat FAIL" in stdout and 'error="broken \\"face\\" list"' in stdout, "FAIL line")
    check("GI_BAKE_TOTAL worlds=3 pass=2 skip=0 fail=1" in stdout, "total line")
    document = report()
    check(statuses(document) == {"worlds/a.dat": "PASS", "worlds/b.dat": "FAIL", "worlds/c.dat": "PASS"},
          "report statuses %s" % statuses(document))
    check(document["totals"]["pass"] == 2 and document["totals"]["fail"] == 1, "report totals")
    check(document["worlds"][0]["probes"] == 3 and document["worlds"][0]["quality"] == "final", "report fields")
    check(document["worlds"][1]["error"] == 'broken "face" list', "report error")
    calls = [json.loads(line) for line in open(log)]
    bakes = [call for call in calls if "--bake-gi" in call]
    check(all("--gi-if-stale" in call and call[call.index("--gi-jobs") + 1] == "3" for call in bakes),
          "bakes pass --gi-if-stale and --gi-jobs")
    check(all(call[call.index("--log-level") + 1] == "error" for call in calls), "quiet log level")

    # Second run: everything current is skipped.
    code, stdout, _ = run("--worlds", "a", "c")
    check(code == 0, "all skipped exits 0, got %d" % code)
    check("GI_BAKE_TOTAL worlds=2 pass=0 skip=2 fail=0" in stdout, "second run skips: " + stdout)
    code, stdout, _ = run("--worlds", "a", "--force")
    check(code == 0 and "GI_BAKE worlds/a.dat PASS" in stdout, "--force rebakes")
    check("--gi-if-stale" not in [json.loads(line) for line in open(log)][-1], "--force drops --gi-if-stale")
    check(all("--gi-direct" not in json.loads(line) for line in open(log)), "original bakes pass no --gi-direct")

    # --direct rebaked (lot GD2) reaches the binary and the report.
    code, stdout, _ = run("--worlds", "a", "--force", "--direct", "rebaked")
    last = [json.loads(line) for line in open(log)][-1]
    check(code == 0 and "--gi-direct" in last and last[last.index("--gi-direct") + 1] == "rebaked",
          "--direct rebaked passes --gi-direct rebaked")
    check(report()["direct"] == "rebaked", "report direct source")

    # --list asks the binary; a missing file is not run.
    with open(os.path.join(out, "worlds", "c.0123456789ab.ob2gi"), "w") as stream:
        stream.write("old")
    code, stdout, _ = run("--worlds", "a", "c", "m", "--list")
    check(code == 0, "--list exits 0")
    check("GI_LIST worlds/a.dat READY" in stdout, "--list ready")
    check("GI_LIST worlds/c.dat STALE file=worlds/c.0123456789ab.ob2gi stale=placement,settings" in stdout,
          "--list stale: " + stdout)
    check("GI_LIST worlds/m.dat MISSING" in stdout, "--list missing")

    # --dry-run runs no bake; --prune keeps every mounted world's file, selected or not.
    calls_before = len(open(log).readlines())
    code, stdout, _ = run("--set", "multi", "--dry-run")
    check(code == 0 and "--bake-gi worlds/m.dat" in stdout, "--dry-run prints the command")
    check(not any("--bake-gi" in json.loads(line) for line in open(log).readlines()[calls_before:]),
          "--dry-run bakes nothing")
    for name in ("worlds/old.000000000000.ob2gi", "worlds/m.0123456789ab.ob2gi"):
        with open(os.path.join(out, name), "w") as stream:
            stream.write("ok")
    code, stdout, _ = run("--worlds", "a", "--prune")
    check(code == 0 and "GI_PRUNE worlds/old.000000000000.ob2gi" in stdout, "--prune names the orphan")
    check(not os.path.exists(os.path.join(out, "worlds/old.000000000000.ob2gi")), "--prune removes the orphan")
    check(os.path.exists(os.path.join(out, "worlds/m.0123456789ab.ob2gi")), "--prune keeps unselected worlds")
    check(report()["pruned"] == ["worlds/old.000000000000.ob2gi"], "report lists pruned files")

    # Unknown world and missing binary are setup errors.
    code, _, stderr = run("--worlds", "nowhere")
    check(code == 2 and "no mounted world named nowhere" in stderr, "unknown world exits 2")

    # SIGINT to the script reaches the running bake; later worlds are not run.
    for name in ("a", "c"):
        os.remove(os.path.join(out, "worlds", name + ".0123456789ab.ob2gi"))
    process = subprocess.Popen(common + ["--worlds", "slow", "a", "c"], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, universal_newlines=True, env=environment)
    deadline = time.monotonic() + 30
    while not os.path.exists(marker) and time.monotonic() < deadline:
        time.sleep(0.01)
    check(os.path.exists(marker), "slow bake started")
    process.send_signal(signal.SIGINT)
    stdout, _ = process.communicate(timeout=30)
    check(process.returncode == 12, "a cancelled run exits 12, got %d" % process.returncode)
    check("GI_BAKE worlds/slow.dat CANCELLED" in stdout and "not_run=2" in stdout, "cancel lines: " + stdout)
    document = report()
    check(document["cancelled"] and statuses(document)["worlds/a.dat"] == "NOT_RUN", "report records the cancel")
    check(not os.path.exists(os.path.join(out, "worlds", "a.0123456789ab.ob2gi")), "no bake after the cancel")

    print("gi_bake_all: %d failure(s)" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
