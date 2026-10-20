# Documentation cleanup — 17 October 2026

The workspace and implementation READMEs now contain only a project description,
the estimated milestone table/status, build and contribution guidance, an AI-use
note and MIT licensing information. Estimates are unchanged and do not certify
acceptance; M2 remains incomplete. Operational controls, configuration, platform
limits and test commands now live in
[getting started](../../getting_started.md).

Owned documentation filenames are lowercase: 390 files were renamed across
`Documentation/` and `Blood2/docs/`, including provenance manifests. Directory
names and the conventional repository `README.md`, `AGENTS.md` and `LICENSE`
entrypoints are retained. Maintained links and source-comment references were
updated. The [rename map](evidence/documentation-renames.json) records each move.
The M2 heartbeat now points to `Documentation/Roadmap/03_milestones.md`; its scope,
schedule, active state and notification behavior are unchanged.

Both repositories include the MIT license for original reconstruction work.
Imported original-game code, dependencies, reference repositories and commercial
assets keep their own rights/notices. No reference repository or commercial asset
was changed. This is not a relicense of the original Blood II source or assets.

## Verification

- All local Markdown link targets resolve after renaming.
- The 52 changed C++ files differ only in comments; executable text is unchanged.
- Renamed JSON provenance manifests retain their exact bytes.
- Historical implementation logs, patches and hash ledgers retain their exact
  filenames/content, including the paths recorded at their historical commits.
- The reference inventory still matches 10,923 files in six clean repositories.

No new build was required for this documentation-only change. The preceding
[message-lifetime increment](m2_message_lifetime.md) records the Debug/Release,
ASan/UBSan and NVIDIA checks, including the separate exploratory GPU leak-check
failure. Do not infer additional platform or M2 acceptance from this cleanup.
