# M2 — Original ordinary NPC death and animated corpses

Cultist/Civilian ordinary lethal damage now reaches original Dying/MC_Dead and
transfers the model to a real independent CCorpse. The NPC disappears only after
successful setup. Its corpse plays the original animation and WAV model-key sounds,
uses the shared gravity/collision service and remains inspectable after source
removal. The existing keyboard controls and collapsible inspector remain available.

This is a bounded continuation, not complete death/combat support. Burning, gibs,
corpse damage/limb loss and blood-pool FX remain explicit unavailable branches.
Optional player kill-voice requests are counted, not played. **M2 remains open**;
SubwayA's 580 entries still comprise 202 created, four transferred sounds,
372 editor-only, two unsupported (Gideon, ExitTrigger), zero creation failures.

See [contracts](../../contracts/corpse_contracts.md),
[source selections](../sources/m2_corpse_sources.json) and
[execution ledger](evidence/m2-corpses.json).

## Preserved behavior and adaptations

The source selects death clips on one update and transfers on the next. Region,
front/back side, nightmare draw, fallback clip, cached movement pose, raw clip
collision dimensions versus visual scale, and the small Y placement offset are
preserved. Original damage fractions now accumulate/reset alongside damage amount,
including DEATH's unusual 100+incoming/maxHP fraction. Unsupported gib/fire paths
remain failures rather than silently choosing an ordinary corpse.

Typed owned strings replace a source pointer cast to 32 bits. Failure rolls back
the incomplete child and keeps the actor visible; stale handles cannot become a new
owner. Corpse sound keys omit GETHANDLE in the original, so playback self-releases
and can overlap. The inspection ID does not introduce a new busy-voice rule.
Measurements reuse original hit-sphere formulas with checked/fallback node reads.
The physics/teleport integration is the existing replacement solver, not verified
retail behavior. Only a real downward primary-world hit records an unavailable
blood-pool FX request. SP persistence and the MP-specific 30-second expiry rule are
separate; the latter does not claim multiplayer support.

## Verification

GCC Debug and inspector-free GCC Release build successfully. Both pass **499/499**
checks (Debug 197.66s; Release 104.24s):
**472 headless checks (70 with external private data) and 27 GPU checks**.
Targeted Clang ASan/UBSan/leak checks pass **93/93** (216.56s).
`--inspect-player` exits 0; `--start-game` still exits 6 because M2 is incomplete.
Changed C++ passes clang-format 23.1.3 and Git whitespace checks; Markdown links
resolve. All 10,923 inventory reference files and six reference repositories are
unchanged. Only Linux/Vulkan has been exercised.

The initial new tests exposed fixture issues: null damage senders were rejected by
the existing typed protocol, and the GPU fixture's seven-second torso clip could
not finish during its two-second hidden-UI window. Fixtures now use checked live
senders and the real 900 ms neck clip; the private test still completes the full
seven-second torso animation. No production timing or assertions were relaxed.
Initial failure logs are retained.

Commands from Blood2:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug --output-on-failure -j4
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure -j4
cmake --build build/sanitize -j2 --target corpse_tests weapon_drop_tests pickup_tests attachment_tests ai_hit_tests ai_runtime_tests object_contract_tests services_tests player_tests weapon_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(corpses|weapon_drops|pickups|attachments|ai_hit|ai_runtime|objects|services|player|weapons)\.' --output-on-failure -j1
```

Captured [corpse inspection panel](evidence/m2-corpses-inspector.png).

Eight dedicated cases cover clip/side/nightmare mapping; two-call transfer;
cached transform and unscaled dimensions; rollback and invalid resources;
independent source/child lifetime; SP/MP scheduling; auto-released overlapping
voices and malformed keys; unavailable damage/fire/gib branches; frame damage
accumulation; and external real NPC/model/audio execution.

The private catalog runs two cycles for each original class. One uses an explicitly
diagnostic NEVERGIB forced-DEATH message; the other uses an ordinary Cultist
headshot and an ordinary low-health Civilian torso kill. Both flow through actual
production AI_Update and deferred NPC removal. The real seven-second Cultist torso
clip also completes in the headless catalog. No commercial asset files are copied.
GPU character smoke adds an isolated ordinary death-action donor during the
authored camera (the real 900 ms neck clip fits its hidden-UI window), verifies
the textured corpse's model submission with UI hidden, then invalidates/recreates
its identity across world unload/reload. It does
not alter authored NPCs or bypass the original intro gate for gameplay.

Inspect a corpse through **Skeletons**; the model list includes hidden/inactive
instances. **Original NPC death** shows selection/transfer state while the actor
still exists; recent spawn reports retain transfer outcomes afterwards. Blood-pool,
fire, gibs and corpse damage limitations stay visible. Pink train glass remains a
separate deferred observation; this continuation does not change materials.
