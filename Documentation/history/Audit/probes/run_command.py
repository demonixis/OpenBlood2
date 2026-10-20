#!/usr/bin/env python3
"""Record an audit command, exit status, duration, and complete output."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("name")
parser.add_argument("--cwd", default=str(Path(__file__).resolve().parents[3]))
parser.add_argument("--seconds", type=int, default=180)
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command[1:] if args.command[:1] == ["--"] else args.command
if not command:
    parser.error("a command is required")
output = Path(__file__).resolve().parents[1] / "evidence"
output.mkdir(exist_ok=True)
started = datetime.now(timezone.utc).isoformat()
clock = time.monotonic()
with (output / f"{args.name}.log").open("wb") as log:
    result = subprocess.run(
        ["timeout", "--signal=TERM", "--kill-after=10", str(args.seconds), *command],
        cwd=args.cwd, stdout=log, stderr=subprocess.STDOUT, check=False,
    )
record = {
    "name": args.name, "command": command, "cwd": args.cwd,
    "started_utc": started, "elapsed_seconds": round(time.monotonic() - clock, 3),
    "timeout_seconds": args.seconds, "exit_code": result.returncode,
    "log": f"{args.name}.log",
}
(output / f"{args.name}.json").write_text(json.dumps(record, indent=2) + "\n")
print(json.dumps(record))
print((output / f"{args.name}.log").read_text(errors="replace")[-5000:])
raise SystemExit(result.returncode)
