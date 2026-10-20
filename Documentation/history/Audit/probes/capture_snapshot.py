#!/usr/bin/env python3
"""Verify the phase-1 indexed text and record local audit build dependencies."""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Documentation/Audit/evidence"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build_root", type=Path)
args = parser.parse_args()


def run(command, cwd=ROOT):
    result = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return {"command": command, "exit_code": result.returncode, "output": result.stdout.strip()}


inventory = json.loads((ROOT / "Documentation/Inventory/workspace.json").read_text())
repositories = []
for repo in inventory["repositories"]:
    directory = ROOT / repo["path"]
    repositories.append({"path": repo["path"], "inventory_commit": repo["commit"],
                         "head": run(["git", "rev-parse", "HEAD"], directory),
                         "status": run(["git", "status", "--porcelain=v1"], directory)})

changed, missing = [], []
records = [json.loads(line) for line in (ROOT / "Documentation/Inventory/files.jsonl").read_text().splitlines()]
for record in records:
    path = ROOT / record["path"]
    if not path.is_file():
        missing.append(record["path"])
    elif hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
        changed.append(record["path"])

dependencies = []
for build in ("openblood2", "nomadengine"):
    for directory in sorted((args.build_root / build / "_deps").glob("*-src")):
        if (directory / ".git").exists():
            dependencies.append({"build": build, "dependency": directory.name,
                                 "commit": run(["git", "rev-parse", "HEAD"], directory)})

tools = [run(command) for command in (
    ["cmake", "--version"], ["ninja", "--version"], ["c++", "--version"],
    ["clang++", "--version"], ["dotnet", "--info"], ["uname", "-a"])]
snapshot = {"date": "2026-10-15", "host": platform.platform(), "build_root": str(args.build_root),
            "tools": tools, "repositories": repositories, "dependencies": dependencies,
            "inventory_verification": {"text_files_checked": len(records), "changed": changed, "missing": missing}}
(OUT / "snapshot.json").write_text(json.dumps(snapshot, indent=2) + "\n")
print(json.dumps(snapshot["inventory_verification"]))
for repo in repositories:
    print(repo["path"], "same commit:", repo["head"]["output"] == repo["inventory_commit"], "status:", repo["status"]["output"] or "clean")

lines = ["# Audit Execution Evidence", "", "Snapshot: 15 October 2026. Logs are complete command outputs. Nonzero results include expected defect reproductions and explicitly documented environment/setup failures; they are not all project build failures.", "", "[Environment, repository integrity and fetched dependency commits](snapshot.json). [Original API lexical candidates](original_api_candidates.json).", "", "| Run | Exit | Seconds | Exact command and context | Output |", "| --- | ---: | ---: | --- | --- |"]
for path in sorted(OUT.glob("*.json")):
    record = json.loads(path.read_text())
    if "command" not in record or "elapsed_seconds" not in record:
        continue
    lines.append(f"| {record['name']} | {record['exit_code']} | {record['elapsed_seconds']} | [Metadata]({path.name}) | [Log]({record['log']}) |")
(OUT / "README.md").write_text("\n".join(lines) + "\n")
