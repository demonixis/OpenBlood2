# Documentation unification — 2026-10-20

One maintained documentation tree: `Blood2/Documentation/`.

- `Blood2/docs/` moved here; root `Documentation/` removed after migration.
- One 38-line roadmap replaces the split active plans, retaining existing completion estimates and M2-GPU-001.
- Build/play, tests, architecture, launcher and diagnostic guides shortened; README files link to them.
- Reports/evidence moved to `implementation/reports/`; earlier studies/plans to `history/`.
- README/AGENTS links and twelve source navigation comments updated. Executable code and reference sources unchanged.

Validation: local Markdown paths/anchors checked, with no active missing targets.
All 6,121 immutable files matched their pre-move SHA-256 hashes. The 411 moved
Markdown references/reports retained their prose and commands; only link targets
changed. Existing missing reference paths remain within historical studies.
`git diff --check` passed in both repositories. No build or gameplay test was
needed for documentation and comment-only changes; no new platform or milestone
acceptance is claimed.
