# M2 — Checked original-session startup

The public startup check no longer unconditionally refuses a working session.
It validates current source dispositions, player/client association and actual
resources, using the strict CPU checks already used by NEW_LEVEL. Direct 3D entry
now goes through this boundary. The finite `--start-game` harness enters the player
before its first update, checks the resulting state and cleans up normally.

The World inspector shows the last accepted/rejected checkpoint, simulation time,
decoded images, unknown/failed/omitted source entries and the concrete failure.
This is a snapshot, not a promise that all later requests succeed. M2 remains
incomplete pending the consolidated acceptance review and the retained GPU
sanitizer lifetime investigation.

## Behavior and ownership

- Unknown classes, failed creation, incomplete source accounting, recorded
  unavailable services and existing NPC runtime failures refuse startup.
- Original server/client entry supplies the player; an existing player is checked
  without respawning it. Stale, inconsistent or dead associations fail.
- World and hidden model/sprite resources are actually decoded. Existing missing
  BSP omissions retain their separate source disposition; they do not bypass
  image, model, audio, schema or service errors.
- A new player created by a failed attempt is removed through original shell
  cleanup. An existing player is retained. Start-trigger changes to the world
  are not rolled back or disguised as an atomic world replacement.
- Reentrant/foreign calls preserve the previous report. Rechecks do not advance
  clocks, duplicate the player or reset game state. A pending exit is retained
  and rejected as a startup request, while an exit newly queued by failed entry
  is cleared with that player's cleanup.
- Music failure remains an explicit rejection. Synthetic fixtures without music
  cannot pass a full startup check simply because player construction succeeded.

No original gameplay rule, dependency pin, source reference or commercial file
changed. The extracted validation helpers also serve NEW_LEVEL; its existing
transaction and failure tests remain intact. See the
[contract](../../contracts/session_start_contracts.md) and
[source ledger](../sources/m2_session_start_sources.json).

## Verification

Implementation commit: `c1004fc1e98e0644f98b9df04dcd947c5090b8c0`.

The 81 selected session/resource/behavior tests pass in each build, plus 17
object lifetime/snapshot checks per build: **98 distinct headless cases** in each
configuration. Clang ASan/UBSan runs with leak detection and halt-on-error enabled.
This is a targeted regression run, not the full registered test suite.

| Configuration | Session suite | Object checks | Session-suite time |
| --- | --- | --- | --- |
| Debug | 81/81 | 17/17 | 127.35 s |
| Release without inspector | 81/81 | 17/17 | 18.99 s |
| Clang ASan/UBSan | 81/81 | 17/17 | 662.65 s |

NVIDIA checks pass **5/5 in Debug and 5/5 in Release**, plus direct graphical
startup for both SubwayA and CityHub in both builds (four additional checks).
All fourteen confirm the NVIDIA RTX 3060 Laptop Vulkan device. The final builds
use `cmake --build build/<configuration> --target ... --parallel 3`; exact targets,
CTest filters, per-case results, environments and failed attempts are retained in
[the evidence ledger](evidence/m2-session-start.json).

All 16 changed C++ files pass formatting; Markdown links and whitespace checks
on changed implementation and maintained Markdown pass. Raw logs, XML and patch
context retain their original trailing whitespace; the unfiltered evidence check
reports it, and those bytes are preserved. All 10,923 inventoried reference files and six reference repositories remain
unchanged. The user-edited workspace README is byte-identical.


The first new private regression failed because it incorrectly expected the
engine object's clock to reset to zero across unload/reload. The clock is
monotone by design. The corrected assertion records its value after loading and
requires startup/rechecks to leave it unchanged; no production clock changed.
The initial failed log is retained. All existing behavioral assertions remain;
only expectations of the historical unconditional refusal are replaced by actual
acceptance or specific rejection checks.

Fresh SubwayA startup decodes 68 images; CityHub decodes 275 and explicitly
accounts for one absent named BSP. Three load/entry/update/leave/unload cycles
check both levels, repeated admission, association and NPC failure probes and
resource/client cleanup. Synthetic cases cover unknown classes, creation failure,
incomplete accounting, missing music, malformed images, unavailable services,
recursive entry exits and foreign-session guards. Existing callback, removal,
NEW_LEVEL rollback and continuous 30/60 Hz train scenarios are retained.

NVIDIA Vulkan captures from actual `--view-world` startup show the original train
intro and the CityHub station with textured characters and health/armor/ammunition
HUD. Both Debug and inspector-free Release are exercised. Images remain outside
Git under the build directories. The existing NVIDIA lifecycle, cinematic,
player-preservation, level-transition and full train scenarios run separately.
Dummy audio is not a listening comparison. No native Metal test is claimed.

## Remaining M2 decision

The [original M2 criteria](../../history/Roadmap/03_milestones.md) are unchanged. This increment
resolves the obsolete runtime-start guard; it does not turn one successful entry
into full acceptance. The consolidated review still needs to connect the selected
SubwayA -> CityHub scenario to the service matrix, resource/callback evidence and
lifetime checks. Unsupported general world saves remain outside that selected
path and belong to later persistence work, not an invented M2 expansion.

The earlier [GPU sanitizer experiment](m2_message_lifetime.md) reported 44,056 leaked
bytes in 816 allocations after completing gameplay/render checks. Its stacks
include libdbus and unresolved dynamically loaded modules; complete attribution
remains open. Passing headless ASan/UBSan or ordinary NVIDIA tests does not
supersede that failure. The next acceptance increment should isolate this GPU
shutdown report and finish the evidence review without suppressing unexplained
failures or extending M2 into whole-campaign/retail-soundtrack parity.
