#!/usr/bin/env python3
"""Bakes the GI of a set of worlds through `blood2 --bake-gi` (gi_contracts.md, lot G6).

The game binary owns every format and staleness decision: this script only selects worlds from
`--list-worlds --json`, runs one bake per world, maps its GI_RESULT line to PASS, SKIP, FAIL or
CANCELLED and writes bake_report.json. It never reads a .ob2gi file. Standard library only; run it
with `python3 -I tools/gi/bake_all.py`.

Exit codes: 0 every world passed or was current, 1 a world failed, 2 usage or setup error, 12 the
run was cancelled (Ctrl-C or SIGTERM, forwarded to the running bakes).
"""

import argparse
import concurrent.futures
import datetime
import json
import os
import re
import signal
import subprocess
import sys
import threading
import time

REPOSITORY = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_OUT = os.path.join(REPOSITORY, "data", "gi", "baked")
DEFAULT_REPORT = os.path.join(REPOSITORY, "data", "gi", "bake_report.json")
DEFAULT_PLACEMENT = os.path.join(REPOSITORY, "data", "gi", "placement")
SETS = {"single": {"single", "addon"}, "multi": {"multi"}, "all": None}
EXIT_CANCELLED = 12
# key=value tokens of a GI_RESULT line; values may be JSON strings with spaces (error=).
TOKEN = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S+)')
REPORT_FIELDS = ("quality", "direct", "bounces", "rays_per_probe", "jobs", "placement", "probes", "reflections",
                 "threads", "rays", "miss", "inside", "digest", "bytes", "ms")


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Bake the GI of every selected world (lot G6).")
    parser.add_argument("--blood2", help="game binary (default: build/release/blood2, else build/dev/blood2)")
    parser.add_argument("--data-dir", help="Blood II data directory (default: the game's own configuration)")
    parser.add_argument("--edition", choices=("retail", "nightmares", "demo"), default="retail")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--set", choices=tuple(SETS), default="single",
                           help="world kinds to bake (default single: single-player and add-on worlds)")
    selection.add_argument("--worlds", nargs="+", metavar="WORLD",
                           help="worlds by path, file name or stem, case-insensitive")
    parser.add_argument("--quality", choices=("preview", "final", "custom"), default="final")
    parser.add_argument("--direct", choices=("original", "rebaked"), default="rebaked",
                        help="direct light on lightmapped faces: re-baked (default, lot GD2) or the stored lightmaps")
    parser.add_argument("--bounces", type=int, help="custom quality bounces (1-3)")
    parser.add_argument("--rays", type=int, help="custom quality rays per probe (16-4096)")
    parser.add_argument("--jobs", type=int, default=1, help="worlds baked at once (default 1)")
    parser.add_argument("--threads", type=int, default=0,
                        help="threads per bake, --gi-jobs (default: processors / jobs)")
    parser.add_argument("--out", default=DEFAULT_OUT, help="baked GI directory (default data/gi/baked)")
    parser.add_argument("--report", default=DEFAULT_REPORT, help="report file (default data/gi/bake_report.json)")
    parser.add_argument("--placement-dir", default=DEFAULT_PLACEMENT, help="GI placement files")
    parser.add_argument("--force", action="store_true", help="rebake current files (no --gi-if-stale)")
    parser.add_argument("--prune", action="store_true",
                        help="remove .ob2gi files no mounted world expects (not after a cancelled run)")
    parser.add_argument("--list", action="store_true", help="show each world's file state; bake nothing")
    parser.add_argument("--dry-run", action="store_true", help="print the bake commands; run nothing")
    parser.add_argument("--verbose", action="store_true", help="echo each bake's output")
    arguments = parser.parse_args(argv)
    if arguments.jobs < 1 or arguments.jobs > 64:
        parser.error("--jobs must be 1-64")
    if arguments.threads < 0 or arguments.threads > 256:
        parser.error("--threads must be 0-256")
    if (arguments.bounces is not None or arguments.rays is not None) and arguments.quality != "custom":
        parser.error("--bounces and --rays need --quality custom")
    return arguments


def find_blood2(arguments):
    if arguments.blood2:
        return os.path.abspath(arguments.blood2)
    suffix = ".exe" if os.name == "nt" else ""
    for preset in ("release", "dev"):
        candidate = os.path.join(REPOSITORY, "build", preset, "blood2" + suffix)
        if os.path.isfile(candidate):
            return candidate
    return None


def base_command(blood2, arguments):
    command = [blood2]
    if arguments.data_dir:
        command += ["--data-dir", os.path.abspath(arguments.data_dir)]
    if arguments.edition == "nightmares":
        command.append("--nightmares")
    elif arguments.edition == "demo":
        command.append("--demo")
    return command + ["--log-level", "error"]


def list_worlds(blood2, arguments):
    command = base_command(blood2, arguments) + ["--list-worlds", "--json"]
    result = subprocess.run(command, cwd=REPOSITORY, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True)
    if result.returncode != 0:
        raise RuntimeError("--list-worlds failed with exit %d:\n%s" % (result.returncode, result.stderr.strip()))
    try:
        listing = json.loads(result.stdout)
        worlds = listing["worlds"]
    except (ValueError, KeyError, TypeError) as exception:
        raise RuntimeError("unreadable --list-worlds output: %s" % exception)
    return listing.get("edition", ""), worlds


def select_worlds(worlds, arguments):
    if arguments.worlds:
        selected = []
        for name in arguments.worlds:
            key = name.replace("\\", "/").lower()
            matches = [world for world in worlds if key in world_names(world["path"])]
            if not matches:
                raise RuntimeError("no mounted world named %s" % name)
            selected += [world for world in matches if world not in selected]
        return selected
    kinds = SETS[arguments.set]
    return [world for world in worlds if kinds is None or world["kind"] in kinds]


def world_names(path):
    path = path.lower()
    name = path.rsplit("/", 1)[-1]
    return {path, name, os.path.splitext(name)[0], os.path.splitext(path)[0]}


def bake_command(base, world, arguments, threads):
    command = base + ["--bake-gi", world["path"], "--gi-quality", arguments.quality,
                      "--gi-jobs", str(threads), "--gi-out", os.path.abspath(arguments.out),
                      "--gi-placement-dir", os.path.abspath(arguments.placement_dir)]
    if arguments.bounces is not None:
        command += ["--gi-bounces", str(arguments.bounces)]
    if arguments.rays is not None:
        command += ["--gi-rays", str(arguments.rays)]
    if arguments.direct != "rebaked":  # The game's default since Gate G.
        command += ["--gi-direct", arguments.direct]
    if not arguments.force:
        command.append("--gi-if-stale")
    return command


def parse_result(line):
    fields = {}
    for key, value in TOKEN.findall(line[len("GI_RESULT"):]):
        if value.startswith('"'):
            try:
                value = json.loads(value)
            except ValueError:
                pass
        fields[key] = value
    return fields


class Runner:
    """Runs bakes, forwarding a cancellation signal to every running child."""

    def __init__(self, verbose):
        self.verbose = verbose
        self.cancelled = threading.Event()
        self.lock = threading.RLock()  # The signal handler may interrupt a holder.
        self.children = set()

    def cancel(self, signum, frame):
        self.cancelled.set()
        with self.lock:
            children = list(self.children)
        for child in children:
            try:
                if os.name == "nt":
                    child.terminate()
                else:
                    child.send_signal(signal.SIGINT)
            except OSError:
                pass

    def run(self, command):
        if self.cancelled.is_set():
            return None, []
        child = subprocess.Popen(command, cwd=REPOSITORY, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 universal_newlines=True)
        with self.lock:
            self.children.add(child)
        if self.cancelled.is_set():
            self.cancel(None, None)
        lines = []
        for line in child.stdout:
            line = line.rstrip("\n")
            lines.append(line)
            if self.verbose:
                with self.lock:
                    print("  " + line, flush=True)
        code = child.wait()
        with self.lock:
            self.children.discard(child)
        return code, lines


def bake_world(runner, base, world, arguments, threads):
    path = world["path"]
    entry = {"world": path, "kind": world.get("kind", ""), "gi_file": world.get("gi_file")}
    started = time.monotonic()
    code, lines = runner.run(bake_command(base, world, arguments, threads))
    entry["seconds"] = round(time.monotonic() - started, 3)
    if code is None:
        entry.update(status="NOT_RUN")
        return entry
    results = [parse_result(line) for line in lines if line.startswith("GI_RESULT ")]
    fields = results[-1] if results else {}
    entry["exit"] = code
    status = fields.get("status")
    if code == EXIT_CANCELLED or status == "cancelled":
        entry["status"] = "CANCELLED"
    elif code == 0 and status == "baked":
        entry["status"] = "PASS"
    elif code == 0 and status == "current":
        entry["status"] = "SKIP"
    else:
        entry["status"] = "FAIL"
        tail = [line for line in lines if not line.startswith("GI_PROGRESS ")][-5:]
        entry["error"] = fields.get("error") or ("exit %d, no GI_RESULT line" % code if not results else "")
        if not fields.get("error"):
            entry["output"] = tail
    for key in REPORT_FIELDS:
        if key in fields:
            value = fields[key]
            entry[key] = int(value) if isinstance(value, str) and value.isdigit() else value
    if "file" in fields:
        entry["file"] = fields["file"]
    return entry


def describe(entry):
    parts = []
    for key in ("probes", "reflections", "bytes", "ms"):
        if key in entry:
            parts.append("%s=%s" % (key, entry[key]))
    if entry["status"] in ("FAIL", "CANCELLED") and entry.get("error"):
        parts.append("error=%s" % json.dumps(entry["error"]))
    return " ".join(parts)


def write_report(path, report):
    path = os.path.abspath(path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=False)
        stream.write("\n")
    os.replace(temporary, path)


def expected_files(worlds):
    return {world["gi_file"].replace("\\", "/") for world in worlds if world.get("gi_file")}


def prune(out, worlds, dry_run):
    keep = expected_files(worlds)
    removed = []
    if not os.path.isdir(out):
        return removed
    for folder, _, files in os.walk(out):
        for name in sorted(files):
            if not name.lower().endswith(".ob2gi"):
                continue
            full = os.path.join(folder, name)
            relative = os.path.relpath(full, out).replace(os.sep, "/")
            if relative in keep:
                continue
            print("GI_PRUNE %s%s" % (relative, " (dry run)" if dry_run else ""), flush=True)
            if not dry_run:
                os.remove(full)
            removed.append(relative)
    return removed


def list_files(base, worlds, arguments):
    for world in worlds:
        name = world.get("gi_file")
        file = os.path.join(os.path.abspath(arguments.out), name) if name else None
        if not file or not os.path.isfile(file):
            print("GI_LIST %s MISSING%s" % (world["path"], " file=" + name if name else ""), flush=True)
            continue
        command = base + ["--gi-placement-dir", os.path.abspath(arguments.placement_dir), "--gi-verify", file]
        result = subprocess.run(command, cwd=REPOSITORY, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                universal_newlines=True)
        details = dict(line.split(" ", 1) for line in result.stdout.splitlines() if " " in line)
        state = {0: "READY", 4: "STALE" if details.get("status") == "stale" else "INVALID", 5: "FOREIGN"}
        text = state.get(result.returncode, "ERROR")
        extra = " stale=%s" % details["stale"].replace(" ", ",") if "stale" in details else ""
        print("GI_LIST %s %s file=%s%s" % (world["path"], text, name, extra), flush=True)
    return 0


def main(argv):
    arguments = parse_arguments(argv)
    blood2 = find_blood2(arguments)
    if not blood2 or not os.path.isfile(blood2):
        print("bake_all: no game binary (build the release or dev preset, or pass --blood2)", file=sys.stderr)
        return 2
    base = base_command(blood2, arguments)
    try:
        edition, worlds = list_worlds(blood2, arguments)
        selected = select_worlds(worlds, arguments)
    except (OSError, RuntimeError) as exception:
        print("bake_all: %s" % exception, file=sys.stderr)
        return 2
    if not selected:
        print("bake_all: no world selected", file=sys.stderr)
        return 2
    if arguments.list:
        return list_files(base, selected, arguments)
    jobs = min(arguments.jobs, len(selected))
    threads = arguments.threads or max(1, (os.cpu_count() or 1) // jobs)
    if arguments.dry_run:
        for world in selected:
            print(subprocess.list2cmdline(bake_command(base, world, arguments, threads)), flush=True)
        if arguments.prune:
            prune(os.path.abspath(arguments.out), worlds, True)
        return 0

    runner = Runner(arguments.verbose)
    previous = {number: signal.signal(number, runner.cancel)
                for number in (signal.SIGINT, signal.SIGTERM)}
    started_at = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    started = time.monotonic()
    entries = [None] * len(selected)
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = {pool.submit(bake_world, runner, base, world, arguments, threads): index
                       for index, world in enumerate(selected)}
            for future in concurrent.futures.as_completed(futures):
                entry = future.result()
                entries[futures[future]] = entry
                if entry["status"] != "NOT_RUN":
                    with runner.lock:
                        print("GI_BAKE %s %s %s" % (entry["world"], entry["status"], describe(entry)), flush=True)
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)

    totals = {key: 0 for key in ("PASS", "SKIP", "FAIL", "CANCELLED", "NOT_RUN")}
    for entry in entries:
        totals[entry["status"]] += 1
    cancelled = runner.cancelled.is_set() or totals["CANCELLED"] > 0
    pruned = []
    if arguments.prune and not cancelled:
        pruned = prune(os.path.abspath(arguments.out), worlds, False)
    seconds = round(time.monotonic() - started, 3)
    report = {
        "schema": 1,
        "tool": "tools/gi/bake_all.py",
        "started": started_at,
        "seconds": seconds,
        "blood2": blood2,
        "edition": edition,
        "selection": {"worlds": arguments.worlds} if arguments.worlds else {"set": arguments.set},
        "quality": arguments.quality,
        "direct": arguments.direct,
        "bounces": arguments.bounces,
        "rays": arguments.rays,
        "force": arguments.force,
        "jobs": jobs,
        "threads": threads,
        "out": os.path.abspath(arguments.out),
        "cancelled": cancelled,
        "totals": {key.lower(): value for key, value in totals.items()},
        "pruned": pruned,
        "worlds": entries,
    }
    try:
        write_report(arguments.report, report)
    except OSError as exception:
        print("bake_all: cannot write the report: %s" % exception, file=sys.stderr)
        return 2
    print("GI_BAKE_TOTAL worlds=%d pass=%d skip=%d fail=%d cancelled=%d not_run=%d seconds=%.1f report=%s"
          % (len(entries), totals["PASS"], totals["SKIP"], totals["FAIL"], totals["CANCELLED"],
             totals["NOT_RUN"], seconds, os.path.abspath(arguments.report)), flush=True)
    if cancelled:
        return EXIT_CANCELLED
    return 1 if totals["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
