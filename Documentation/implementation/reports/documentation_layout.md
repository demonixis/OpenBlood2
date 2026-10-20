# Developer documentation layout

`Blood2/docs/` now keeps four general developer guides at its root:
[index](../../readme.md),
[getting started](../../getting_started.md),
[architecture](../../architecture.md) and
[contributing](../../contributing.md).

The 125 subsystem contracts moved into
[contracts/](../../contracts/readme.md), with an index grouped by
responsibility. The long provenance history moved into
[implementation/](../readme.md), with its 121 JSON
source ledgers in `implementation/sources/`. In total, 247 existing files moved.

Progress reports and measured results already have their dedicated home in this
workspace implementation directory and [evidence/](evidence). The new implementation
index links these locations; no duplicate report archive was copied into the
independent application repository. READMEs remain concise and link to the guides.

Maintained Markdown links and ten source-comment references were updated in both
repositories. Existing JSON ledgers retain their exact bytes and their original
repository/workspace path semantics. Historical logs, patches and hash snapshots
remain untouched. The [move and verification record](evidence/documentation-layout.json)
records the relocation and checks.

This is a documentation-only change. No executable C++ text, build configuration,
dependency, game behavior or milestone acceptance changed; no build was needed.
The preceding [door-audio report](m2_door_audio.md) remains the latest execution
evidence, including the retained sanitizer timeout and exploratory GPU leak failure.
M2 remains incomplete.
