# M2: production Gideon lifecycle and boss health

Project commit: `365e3b5c74978859eec190d985286cd189dbf98e`.

Original Gideon is now registered with his fixed model/skin, original profile,
four weapons, shared animation table, gravity exclusion and AI_Update driver.
Typed boss-health delivery reaches the client and in-game inspection. The damage
chain applies each accepted packet once, then the subclass health/threshold branch,
then the base AI reaction; retail exhausted-message behavior remains unverified.

See [contracts](../../contracts/gideon_runtime_contracts.md),
[source ledger](../sources/m2_gideon_runtime_sources.json) and
[retained evidence](evidence/m2-gideon-runtime.json).

The source captures full health before base initialization, initially 1, and keeps
publishing in its nonzero branch. Ratios above 1 are retained and hide the client
bar exactly like the original client condition. The inspector labels the captured
value and delivery status. No normalization to an invented maximum is performed.

SubwayA now has **204 created source entries, four transferred sounds, 372 editor-only
entries, zero unsupported classes and zero creation failures**. Its 16 production
NPCs are ten Cultists, five Civilians and Gideon. Runtime counts are 227 before
player entry, 230 after entry and 232 after the first tick (11 attachments).
The extra two runtime objects are Gideon and his inventory muzzle-flash light.
These counts account for original source entries; they do not prove all services.
Gideon's opposite-faction presence also removes the three self-target failures
in the existing no-player first-tick diagnostic. Interactive playback still waits
for player entry; this does not establish general pre-player simulation safety.

The actual intro script requests TALKING_02, while the ABC stores talking_02.
Animation lookup now uses ASCII case-insensitive first-match comparison, supported
by the available later-engine reference and this authored DAT/ABC pair. Unknown
names still fail; no script, asset or alias was substituted.

The destination rehearsal now creates 244 CityHub source entries, with 206 unknown
and 35 failed entries (schema scan: 248 checked, 206 unknown, 31 invalid). These
remain a partial destination, even though original player entry can be rehearsed.
Both the source Cultist and Gideon shared voices/table identities survive disposable
SubwayA/CityHub initialization; source client health state is retained.

Integration also exposed two bounded issues. Inventory strength now retains Gideon's
original 10 while all ammo columns stay bounded. A penetrating ordinary corpse is
rejected during setup, with deferred rollback and a visible source error, instead
of aborting the next unrelated player-input tick. The input test retains its
original firing actions and normal player health. This does not solve corpse fit
or claim successful death transfer where geometry prevents it.

Debug and inspector-free Release each pass **699/699 checks**: 642 headless and
57 serial GPU, including 144 private-data cases (103 headless). Debug takes
102.96s headless / 182.66s GPU;
Release takes 16.01s / 122.16s.
Clang ASan/UBSan with leak detection passes **105/105** in 261.59s.
Build logs contain no compiler warning/error; retained runtime logs contain no
sanitizer report or GPU validation error/VUID.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure`
and serial `ctest --test-dir build/<configuration> -L gpu --output-on-failure`.
Sanitizer build uses `-j2` for gideon_runtime_tests, gideon_attack_tests,
gideon_combat_tests, gideon_teleport_tests, ai_hit_tests, ai_runtime_tests,
object_contract_tests, world_tests, services_tests, session_playback_tests,
weapon_tests, energy_tests and corpse_tests. Sanitizer CTest uses `-j2`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1` and
`^(gideon_runtime|gideon_attack|gideon_combat|gideon_teleport|ai_hit|ai_runtime|objects|services|session_playback|weapons|energy|corpses)\.`
plus `^world.destination_(trials|lifetime|trial_private)$`.

All 48 changed/new C++ files pass clang-format 23.1.3; all 2750 local Markdown
links resolve. All 10923 reference file hashes and six reference repositories
remain unchanged. The actual production-model GPU capture was visually checked;
commercial images/assets remain ignored and external. Only Linux/Vulkan was tested.

Eight new headless cases cover original defaults, missing/restore rejection,
repeated/zero/nonfinite health, client bounds/queue capacity/reset, accepted/refused
and nested damage, removal callbacks, three difficulty profiles, shared table
reuse/unload, three SubwayA cycles and autonomous arena fire. Existing tests extend
fifth-table/voice isolation, case-insensitive clip lookup, high NPC strength,
corpse placement rollback and production-boss GPU lifetime. Exact world/model/
faction counts were updated for the real added actor and its inventory light;
no runtime-failure or unsupported-service assertion was relaxed.

M2 stays incomplete: creation accounting is not a playable-session acceptance test.
Safe NEW_LEVEL, required remaining services/FX, original HUD presentation and broader
boss encounter/death parity remain open. The destination remains a disposable
initialization rehearsal and does not replace the source world.

The three ten-second SubwayA runs prove initialization, the authored dialogue clip,
health publication and cleanup, not a complete cinematic or boss encounter. The
boss receives two AI updates and one client health message, then is automatically
deactivated with zero visibility pings; its selected clip is 19 (`talking_02`).
The replacement ray-based activation therefore needs further authored-scene
validation before claiming that the complete intro/script runs correctly. The
bounded arena independently proves autonomous target acquisition and a real
animation-key weapon shot (122 updates, 99 attack updates, one shot). Continue by
inspecting the remaining authored script/activation/death path and required services before accepting
M2; do not infer completion from zero unknown source classes.
