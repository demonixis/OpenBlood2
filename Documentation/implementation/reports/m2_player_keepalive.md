# M2 — Named keep-alive player entry

Captured supported players can now enter an already loaded different world through
the original shells' new-level start branch. Inventory Load uses keep-alive type 1
and discards keys. Health, armor, weapons, raw ammo, attributes/caps and selected
player state survive without Respawn or fresh profile grants.

See [contracts](../../contracts/player_keepalive_contracts.md),
[source selections](../sources/m2_player_keepalive_sources.json) and
[execution evidence](evidence/m2-player-keepalive.json).

## Behavior and boundaries

Start selection follows the original case-insensitive solo-name match and last-solo
fallback. Empty input uses `start`. The selected trigger runs before teleport;
marker/player deletion is checked. Destination grounding and final body clearance
must succeed. Client physics/stats are published before MoveMgr resumes as the only
player integrator. Snapshot strings/resources contain no old object/client handles.

In Session: capture the player, explicitly load the destination, supply its start
name, then use **Enter loaded world with captured player**. The usual restore button
retains same-world saved-position semantics. Inspection shows successful mode,
destination, requested/selected marker, fallback and separate failure diagnostics.

Automatic ExitTrigger/NEW_LEVEL remains refused before teardown. This increment
provides destination entry, not safe world replacement or a retail Keep.sav service.
Failed entry removes staged owners/children/client state; it does not undo destination
start-trigger effects or restore a discarded source world. Full client state,
missing saved player fields, Gideon and required FX/services still keep M2 open.

## Verification

Debug passes **511/511 headless checks** (123.19s)
and **29/29 GPU checks** (154.26s). Inspector-free Release passes
**540/540** (108.63s). Each complete configuration covers 77 private
headless cases and 103 private cases overall. Clang ASan/UBSan/leak checks pass
**100/100** (97.45s). After adding an exact private arrival-coordinate
assertion, all four keep-alive cases pass again in Debug and with sanitizers.
The existing fresh/saved-position and exit-refusal cases remain unchanged.

The new fallback fixture initially assumed creation order; FindNamedObjects actually
prepends stable-slot entries. The corrected fixture checks its explicit list order
before asserting the original last-solo fallback. No gameplay-selection rule or
existing acceptance assertion was weakened.

`--inspect-player` exits 0 and `--start-game` still exits 6 for incomplete gameplay.
13 changed C++ files pass clang-format 23.1.3. All 10,923 reference files and six
reference repositories remain unchanged; local Markdown links resolve.
The [inspector capture](evidence/m2-player-keepalive-inspector.png) was visually reviewed.

Commands from Blood2:

```sh
cmake --build build/debug -j4
ctest --test-dir build/debug -L headless --output-on-failure -j2
ctest --test-dir build/debug -L gpu --output-on-failure -j1
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure -j4
cmake --build build/sanitize -j2 --target player_keepalive_tests player_restore_tests character_nodes_tests model_view_tests ai_hit_tests player_state_tests player_tests inventory_state_tests destructable_state_tests object_contract_tests exit_tests services_tests attachment_tests weapon_tests ai_perception_tests
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitize -R '^(player_keepalive|player_restore|character_nodes|model_view|ai_hit|player_state|player|inventory_state|destructable_state|objects|exits|services|attachments|ai_perception)\.|^weapons\.(inventory|lifetime|rollback)$' --output-on-failure -j1
```


Synthetic checks exercise preserved nondefault stats/raw attributes, removed keys,
trigger-before-teleport ordering, marker deletion, player deletion during triggers,
no-ground/embedded-body failures, unknown modes/names, missing solo markers,
multiplayer exclusions and four repeated fallback/default entry cycles. Existing
same-world restoration and explicit exit-refusal tests remain enabled.

Private tests capture SubwayA then explicitly load CityHub and enter its authored
`01_egressa` marker three times, preserve 73 HP/17 bullets, discard a labelled
synthetic key and rebuild Caleb's 941 triangles. CityHub accounts for **243 created / 541 editor-only / 207 unsupported / 35 failed**
source entries in this harness. Failures comprise 31 legacy Sound `VoiceType`
schemas, two destructible brushes (missing model / non-manifold shell), one Door
(non-manifold shell), and one rejected CultistAI precreate. None is silently treated
as successful creation or difficulty filtering. This is not an authored-playthrough or a claim that CityHub works fully. GPU smoke repeats this entry
with UI hidden and after destination reload, in Debug and inspector-free Release.

Only Linux/Vulkan is tested. Original executable parity and other platforms remain
unverified. Reference repositories and external commercial files are preserved.
