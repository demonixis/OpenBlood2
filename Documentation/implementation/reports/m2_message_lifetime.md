# M2 — Continuous train traversal and message lifetime

The new regression drives the player from SubwayA's real spawn through the train
and into CityHub. Earlier tests covered initial encounters and the exit separately,
with relocation before the exit. Connecting them exposed a real lifetime defect:
the 30 Hz route exhausted 65,536 retained message tokens. The authored relay still
reported its sends, but its fade and level-exit messages were no longer delivered.

The engine now uses typed, non-reused 64-bit message identities and retains only
live payloads. The 65,536 simultaneous-message limit remains enforced, alongside
borrowed callback ownership and stale/foreign validation. This is a replacement
engine lifetime correction, not a change to original gameplay or message ordering.
The in-game Objects tab displays allocation/release/refusal counts and peak live
messages. See [contracts](../../contracts/message_lifetime_contracts.md).

## Scenario and acceptance boundary

The scripted driver completes the original intro and uses production SDL input
mapping for walking, mouse aiming, primary fire, USE and numeric weapon selection.
It follows the aisle, approaches visible enemies and falls back to the knife when
out of bullets. NPC state, stats, pickups, damage, positions and trigger schedules
are not edited. There is no teleport, direct damage, direct exit or invulnerability.
Synthetic input and automated aiming are not a human walkthrough.

The test requires player-attributed damage and deaths, real pickup observations,
contact activation of Trigger2, every authored delayed send, received fade/black
hold, NEW_LEVEL to `01_egressa`, preserved health/ammunition, reset input/fade state,
destination updates and complete teardown. It retains all 174 service rows for
SubwayA, CityHub and teardown, independently of implementation status, plus actual
message-allocation outcomes. Music remains a separately recorded unsupported
player command; service counts alone must not conceal it.

The new route initially required an ammo pickup at both rates. The 30 Hz route
uses twenty original knife FIRE_KEY shots after running dry and takes a different
combat/drop path. Its final assertion checks those shots and health collection;
the 60 Hz route still requires ammunition collection. No existing gameplay test,
stat or damage rule was relaxed.

The first 60 Hz probe passed. The initial 30 Hz test failed the new scenario and
exposed the token exhaustion; it is retained as evidence. An existing CLI test
also expected an obsolete error sentence. Its check now matches the current
acceptance-gate diagnostic while still requiring exit status 6 and zero missing
world instances; it does not declare startup accepted.

M2 remains open. This continuous route replaces the relocation limitation for this
scenario; it does not certify complete CityHub traversal, every original service,
retail frame-rate parity, rendering fidelity or the later campaign/save gates.

## Measurements and validation

| Full route | 30 Hz | 60 Hz |
| --- | ---: | ---: |
| Damaged / killed authored Cultists | 10 / 10 | 10 / 10 |
| Source messages created / released | 91,139 / 91,139 | 65,534 / 65,534 |
| Peak simultaneous messages | 3 | 3 |
| Allocation refusals | 0 | 0 |
| Knife FIRE_KEY shots | 20 | 0 |
| Health retained into CityHub | 146.948 | 175 |
| Bullets retained into CityHub | 0 | 11 |

Debug, inspector-free Release and Clang ASan/UBSan each passed the 56 selected
object, world, restoration, service-coverage, fade, input and route tests. Leak
checking was enabled for the sanitizer suite. This is a targeted regression set,
not a rerun of every registered project test.

The final NVIDIA/Vulkan checks pass 5/5 in Debug and 5/5 in Release: GPU
lifecycle, weapons, repeated level changes, continuous train and screen fade.
The three train snapshots contain textured world/model pixels without fallback
markers; gameplay clocks are unchanged by capture.

## SDL shutdown defect exposed by the GPU check

The initial Debug NVIDIA route reached CityHub, rendered all three checkpoints
and completed game teardown, then crashed in SDL window destruction. The retained
core identifies a late Wayland HDR callback assigning to a destroyed property
group. In the pinned SDL revision, `SDL_SetupPropertyAssignment` checked the
output-pointer argument rather than the lookup result (`!properties` instead of
`!*properties`), then dereferenced null.

`cmake/PatchSdlProperties.cmake` applies that one-character correction to the
fetched source. It checks exact original/patched SHA-256 values, is idempotent,
and fails on unexpected source changes. SDL's revision and archive hash stay
unchanged. Reassess this local patch at the next dependency update. The public-API
regression `platform.destroyed_properties` reproduces exit 139 before the patch;
afterward it checks rejected stale float/bool/string assignments and an unaffected
live property group in Debug, Release and ASan/UBSan.

An additional exploratory sanitizer GPU run completed all gameplay/render checks
but LeakSanitizer reported 44,056 bytes in 816 allocations, with libdbus and
unresolved dynamically loaded module stacks. That run is retained as a failure;
no suppression or disabled leak check is presented as success, and complete
attribution remains open. It is separate from the clean headless sanitizer suite
and the diagnosed null-pointer shutdown defect. No native Metal test was run.

Validation and implementation commit are recorded in the
[evidence ledger](evidence/m2-message-lifetime.json).
