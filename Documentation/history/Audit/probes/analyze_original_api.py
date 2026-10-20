#!/usr/bin/env python3
"""Lexical audit aid, not an AST, execution trace, or completeness score."""
from collections import Counter, defaultdict
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Documentation/Audit/evidence"
HEADERS = ["server_de.h", "client_de.h", "CSBase.h", "common_de.h", "physics_lt.h"]


def clean(text):
    # Keep line numbers; mask comments and literals before candidate matching.
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: "\n" * m[0].count("\n") + " " * (len(m[0]) - m[0].count("\n")), text)


declarations = defaultdict(list)
for name in HEADERS:
    path = ROOT / "sources/blood2/AppHeaders" / name
    source = clean(path.read_text(errors="replace"))
    patterns = [r'\(\s*\*\s*(\w+)\s*\)\s*\(', r'\bvirtual\s+[^;{}\n]*?\b(\w+)\s*\(']
    for pattern in patterns:
        for match in re.finditer(pattern, source):
            declarations[match[1]].append({"path": str(path.relative_to(ROOT)), "line": source.count("\n", 0, match.start()) + 1})

calls = defaultdict(list)
for path in sorted((ROOT / "sources/blood2").rglob("*")):
    if path.suffix.lower() not in {".cpp", ".c", ".h"} or "AppHeaders" in path.parts:
        continue
    source = clean(path.read_text(errors="replace"))
    for match in re.finditer(r'->\s*(\w+)\s*\(', source):
        if match[1] in declarations:
            calls[match[1]].append({"path": str(path.relative_to(ROOT)), "line": source.count("\n", 0, match.start()) + 1})

rows = [{"name": name, "candidate_calls": len(calls[name]), "declarations": declarations[name], "calls": calls[name]}
        for name in sorted(declarations, key=lambda n: (-len(calls[n]), n))]
report = {"method": "Comment/literal-masked regex for function-pointer and virtual declarations in five original API headers; arrow call names in original C/C++ outside AppHeaders. No type resolution, macro expansion, preprocessing, or reachability analysis. Same-name game methods can be false positives; inherited APIs and non-arrow calls can be missed.",
          "headers": HEADERS, "declared_names": len(rows), "names_with_candidates": sum(bool(row["candidate_calls"]) for row in rows),
          "candidate_calls": sum(map(len, calls.values())), "apis": rows}
(OUT / "original_api_candidates.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({k: v for k, v in report.items() if k != "apis"}, indent=2))
print("\nMost frequent lexical candidates:")
for row in rows[:25]:
    print(f"{row['name']}: {row['candidate_calls']}")
