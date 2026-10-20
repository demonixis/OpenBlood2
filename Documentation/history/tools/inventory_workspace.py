#!/usr/bin/env python3
"""Inventory tracked workspace files without building or opening game assets."""

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
from urllib.parse import quote


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Documentation" / "Inventory"
REPOSITORIES = (
    "OpenBlood2", "NoMadEngine", "nomad", "sources/blood2",
    "sources/Shogo_Aegis", "sources/lithtech",
)
CODE = {
    ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx", ".inl", ".ipp",
    ".cs", ".gd", ".py", ".m", ".mm", ".asm", ".s", ".java",
    ".vert", ".frag", ".comp", ".glsl", ".hlsl", ".vsh", ".psh",
    ".fx", ".fxh", ".rc", ".rc2", ".cp", ".l", ".y", ".idl",
    ".hairy", ".simple",
}
BUILD = {
    ".cmake", ".csproj", ".sln", ".dsp", ".dsw", ".vcproj", ".vcxproj",
    ".props", ".targets", ".mk", ".sh", ".bat", ".cmd", ".bpr",
}
TEXT = {".md", ".txt", ".rst", ".html", ".htm"}
CONFIG = {".json", ".cfg", ".ini", ".toml", ".yaml", ".yml", ".godot", ".tscn", ".xml", ".def", ".dfm"}
ASSET_DIRS = {"assets", "game_data", "rez", "scenes/imports", ".godot", ".nomad_cache"}


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(ROOT / repo), *args])


def paths(data):
    return [p for p in data.decode("utf-8").split("\0") if p]


def category(relative):
    path = Path(relative)
    suffix = path.suffix.lower()
    lower_parts = [p.lower() for p in path.parts]
    # Keep source code even if a project places it in an asset directory.
    if suffix in CODE:
        return "code"
    if suffix in {".bak", ".backup", ".old"} and Path(path.stem).suffix.lower() in CODE | BUILD:
        return "archived_text"
    if path.name == "makefile.x64":
        return "build"
    if suffix in BUILD or path.name in {"CMakeLists.txt", "CMakePresets.json", "Makefile", "makefile", "GNUmakefile"}:
        return "build"
    if suffix == ".md":
        return "documentation"
    if any(p in ASSET_DIRS for p in lower_parts) or "scenes/imports/" in relative.lower():
        return "asset_or_local_data"
    if suffix in TEXT or path.name.upper() in {"README", "LICENSE", "COPYING", "COPYRIGHT", "AUTHORS", "CHANGELOG"}:
        return "documentation"
    if suffix in CONFIG or path.name in {".gitignore", ".gitattributes", ".editorconfig", ".gitmodules"}:
        return "configuration"
    if suffix in {".uid", ".import"}:
        return "editor_metadata"
    return "other_or_binary"


def module(repo, relative):
    parts = Path(relative).parts
    if len(parts) == 1:
        return "(root)"
    if repo == "OpenBlood2" and parts[0] == "OpenLithTech":
        if len(parts) == 2:
            return "OpenLithTech"
        return "/".join(parts[:3] if len(parts) > 3 and parts[1] == "src" else parts[:2])
    if repo == "NoMadEngine" and parts[0] in {"nomadengine", "nomadeditor"}:
        return "/".join(parts[:3] if len(parts) > 3 else parts[:1])
    if repo == "nomad" or (repo == "sources/lithtech" and parts[0] in {"runtime", "libs", "tools"}):
        return "/".join(parts[:2]) if len(parts) > 2 else parts[0]
    if parts[0] in {"blood2", "docs", "tools"} and len(parts) > 2:
        return "/".join(parts[:2])
    return parts[0]


def link(label, path):
    return f"[{label}](../../{quote(path, safe='/._-')})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--date", help="Snapshot date label, YYYY-MM-DD; defaults to UTC date")
    args = parser.parse_args()
    date = args.date or datetime.now(timezone.utc).date().isoformat()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    records = []
    repositories = []
    groups = defaultdict(Counter)
    for repo in REPOSITORIES:
        tracked = paths(git(repo, "ls-files", "-z"))
        counts = Counter()
        extensions = Counter()
        line_counts = Counter()
        missing = []
        for relative in tracked:
            path = ROOT / repo / relative
            kind = category(relative)
            extensions[path.suffix.lower() or "(none)"] += 1
            if not path.exists():
                missing.append(relative)
                counts["missing"] += 1
                continue
            if path.is_symlink():
                counts["symlink_not_read"] += 1
                continue
            if not path.is_file():
                counts["non_regular_not_read"] += 1
                continue
            if kind not in {"code", "build", "documentation", "configuration", "archived_text"}:
                counts[kind] += 1
                continue
            data = path.read_bytes()
            if b"\0" in data:
                counts["binary_with_text_extension"] += 1
                continue
            lines = data.count(b"\n") + int(bool(data) and not data.endswith(b"\n"))
            group = module(repo, relative)
            record = {
                "path": f"{repo}/{relative}", "repository": repo,
                "module": group, "category": kind,
                "bytes": len(data), "physical_lines": lines,
                "sha256": hashlib.sha256(data).hexdigest(),
            }
            records.append(record)
            counts[kind] += 1
            line_counts[kind] += lines
            groups[(repo, group)][kind] += 1
            groups[(repo, group)][f"{kind}_lines"] += lines
        repositories.append({
            "path": repo,
            "commit": git(repo, "rev-parse", "HEAD").decode().strip(),
            "commit_date": git(repo, "show", "-s", "--format=%cs", "HEAD").decode().strip(),
            "branch": git(repo, "branch", "--show-current").decode().strip(),
            "tracked_files": len(tracked), "categories": dict(sorted(counts.items())),
            "physical_lines": dict(sorted(line_counts.items())),
            "extensions": dict(sorted(extensions.items())),
            "missing_tracked_files": missing,
            "tracked_changes": len(paths(git(repo, "diff", "HEAD", "--name-only", "-z"))),
            "untracked_nonignored_files": len(paths(git(repo, "ls-files", "--others", "--exclude-standard", "-z"))),
        })
    snapshot = {
        "date": date,
        "scope": "Current working-tree bytes of Git-tracked files in six repositories; root documentation excluded",
        "line_count_definition": "Physical lines including blanks and comments; copies, third-party code and generated code are not deduplicated",
        "classification": {
            "code_extensions": sorted(CODE), "build_extensions": sorted(BUILD),
            "documentation_extensions": sorted(TEXT), "configuration_extensions": sorted(CONFIG),
            "exclusions": "Asset/local-data directories except source and Markdown, binary files, editor metadata, symlinks; no archive contents inspected",
        },
        "repositories": repositories,
        "totals": {
            "tracked_files": sum(r["tracked_files"] for r in repositories),
            "indexed_text_files": len(records),
            "code_files": sum(r["categories"].get("code", 0) for r in repositories),
            "code_physical_lines": sum(r["physical_lines"].get("code", 0) for r in repositories),
        },
    }
    (OUTPUT / "workspace.json").write_text(json.dumps(snapshot, indent=2) + "\n")
    # One record per line keeps this large index searchable without a huge indented file.
    (OUTPUT / "files.jsonl").write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in sorted(records, key=lambda r: r["path"])))
    rows = ["# Repository and Module Counts", "", f"Snapshot date: {date}.", "",
            "Generated by `Documentation/tools/inventory_workspace.py`. Counts describe files present, not verified functionality. Code includes headers, resource scripts, shaders, and tooling; line counts include blanks, comments, vendored code, and repeated source copies. See [workspace.json](workspace.json) for classifications, exclusions, Git state, and extension counts.", "",
            "| Repository | Commit | Tracked files | Indexed text files | Code files | Code physical lines |", "| --- | --- | ---: | ---: | ---: | ---: |"]
    for r in repositories:
        c = r["categories"]
        rows.append(f"| {link(r['path'], r['path'])} | `{r['commit'][:12]}` | {r['tracked_files']:,} | {sum(c.get(k, 0) for k in ('code','build','documentation','configuration','archived_text')):,} | {c.get('code',0):,} | {r['physical_lines'].get('code',0):,} |")
    rows += ["", "## Modules", "", "Only indexed text files contribute to the module table; empty and asset-only directories are omitted.", "",
             "| Repository | Module | Code files | Code physical lines | Build files | Documentation files | Configuration files | Archived text files |", "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for (repo, group), c in sorted(groups.items()):
        rows.append(f"| `{repo}` | `{group}` | {c['code']:,} | {c['code_lines']:,} | {c['build']} | {c['documentation']} | {c['configuration']} | {c['archived_text']} |")
    (OUTPUT / "counts.md").write_text("\n".join(rows) + "\n")
    docs = ["# Existing Documentation Index", "", f"Snapshot date: {date}.", "",
            "This is a file catalog, not an endorsement of historical plans or a claim that every document has been read. Titles are taken from the first Markdown heading where available; filenames are used otherwise. Source license and notice files are cataloged as references, without assigning a workspace-wide license. Game asset directories are excluded except Markdown documentation.", ""]
    for repo in REPOSITORIES:
        docs += [f"## {repo}", "", "| Document | Title or filename |", "| --- | --- |"]
        for r in sorted(records, key=lambda r: r["path"]):
            if r["repository"] != repo or r["category"] != "documentation":
                continue
            path = ROOT / r["path"]
            title = path.name
            if path.suffix.lower() == ".md":
                headings = [line.lstrip("# ") for line in path.read_text(errors="replace").splitlines() if line.startswith("#")]
                if headings:
                    title = headings[0]
            title = title.replace("|", "\\|").replace("<", "&lt;").replace(">", "&gt;")
            docs.append(f"| {link(r['path'][len(repo)+1:], r['path'])} | {title} |")
        docs.append("")
    (OUTPUT / "documentation.md").write_text("\n".join(docs) + "\n")
    copies = []
    for left, right in (
        ("sources/blood2/", "sources/lithtech/Blood2/"),
        ("sources/blood2/", "OpenBlood2/blood2/"),
        ("OpenBlood2/tools/", "NoMadEngine/tools/"),
    ):
        a = {r["path"][len(left):]: r for r in records if r["path"].startswith(left) and r["category"] == "code"}
        b = {r["path"][len(right):]: r for r in records if r["path"].startswith(right) and r["category"] == "code"}
        common = sorted(a.keys() & b.keys())
        byte_equal = [p for p in common if a[p]["sha256"] == b[p]["sha256"]]
        newline_equal = [p for p in common if (ROOT / left / p).read_bytes().replace(b"\r\n", b"\n") == (ROOT / right / p).read_bytes().replace(b"\r\n", b"\n")]
        copies.append({
            "left": left, "right": right, "matching_paths": len(common),
            "byte_identical": len(byte_equal), "identical_after_crlf_normalization": len(newline_equal),
            "different_after_crlf_normalization": len(common) - len(newline_equal),
            "left_only_paths": sorted(a.keys() - b.keys()),
            "right_only_paths": sorted(b.keys() - a.keys()),
        })
    (OUTPUT / "source_copies.json").write_text(json.dumps({
        "date": date,
        "method": "Code-category files matched by exact relative path; compare raw bytes and bytes with CRLF replaced by LF. No case folding, rename detection, or semantic comparison.",
        "comparisons": copies,
    }, indent=2) + "\n")
    print(json.dumps(snapshot["totals"]))
    for repo in repositories:
        print(repo["path"], repo["tracked_files"], repo["categories"], repo["physical_lines"].get("code", 0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
