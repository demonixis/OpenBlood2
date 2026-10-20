# M2 — Original player damage impulses

The player previously accepted weapon damage without the original physical
response because its damage-physics flag remained disabled. Fresh player entry
now enables that flag. The shared damage handler preserves the original
single-player self-hit exclusion: falling damage and self explosions can reduce
health without propelling the player.

The existing normalized `damage * 2500 / mass` rule adds velocity after resistance
and before armor. The player and MoveMgr share one body, so the server write is
already the client-visible impulse. Replaying the original separate-body network
delta would apply it twice. No extra integrator, dependency or message queue was
introduced. Movement inspection exposes actual accepted impulse counts and the
last attempted delta/result. See [contracts](../../contracts/player_impulse_contracts.md)
and [source ledger](../sources/m2_player_impulse_sources.json).

Implementation commit: `3d15e6ea4b892108ce3da6c8ce90dde157a36c2d`.

## Verification

A generated fixture receives two hits before movement, adding velocity (100,0,50).
The generic engine integrator leaves the player untouched; the real MoveMgr then
produces displacement (6.24472,0,3.12236) over half a second at **30/60/120 Hz**,
with exactly 60 substeps at every rate. A nearby solid wall stops the same path;
a pending door push adds once. Checkpoint restoration preserves the physics flag
but clears motion, with no delayed replay.

On diagnostic geometry with external retail resources, the production Cultist
acquires the player and fires through its real AI/animation/weapon path: **25
shots, 25 accepted impulses**, followed by original player death and clean teardown.
No direct damage, actor-state edit or scripted player relocation supplies those hits.
This is a diagnostic encounter, not an authored campaign scenario.

The full spawn-to-CityHub route still passes unchanged gameplay acceptance:

| Route | Authored Cultists killed | Player impulses | Health into CityHub | Bullets |
| --- | ---: | ---: | ---: | ---: |
| 30 Hz | 10 | 5 | 129.974 | 9 |
| 60 Hz | 10 | 0 | 175 | 11 |

The automated aim at 60 Hz avoids enemy hits; zero impulses there is expected.
The 30 Hz trajectory and subsequent combat/drop sequence change with real
knockback. All existing contact, delayed relay, fade, inventory continuity and
teardown assertions remain. The destination retains enabled physics with fresh
inspection history and no queued impulse.

The initial new train assertion incorrectly demanded a hit at both rates; the
retained log shows the 60 Hz player never loses health. It now requires impulses
on the 30 Hz route, while the dedicated NPC encounter always requires real hits.
The original death regression also assumed no initial momentum. Original DoDeath
does not reset velocity: its test now requires passive decay, zero control flags,
then continued immobility with forward/fire held. It still checks no repeated
death, unchanged ammunition, corpse lifecycle and all five restoration paths.
Instant Death damage must remain stationary with zero impulses. The failed runs
are retained rather than presented as successful checks. A preliminary generated
test setup also omitted its start point; it was corrected without changing game
entry behavior.

All **214 distinct selected headless cases** are now validated in GCC Debug,
inspector-free GCC Release and inspector-free Clang ASan/UBSan, with leak detection
enabled. This is the initial broad suite plus final targeted rechecks, not a claim
that the initial suites were entirely green. The two expectation failures above
were corrected and rechecked. The sanitizer's existing NPC-clearance case also
hit its 90-second timeout during the parallel suite; its isolated rerun passed in
**86.90 seconds**, with unchanged timeout and sanitizer options. Its timing margin
remains narrow. No sanitizer report was suppressed.

NVIDIA/Vulkan passes **3/3 in Debug and 3/3 in Release**: movement, player restoration
and the complete train/CityHub route. The actual device is the NVIDIA GeForce
RTX 3060 Laptop GPU. The CityHub capture was reviewed locally; captures are not
committed. These GPU checks are separate from headless sanitizer validation.

All six changed C++ files pass the formatter check; source ledger hashes match,
10,923 reference files and six reference repositories are unchanged, and 3,770
local Markdown links resolve. The user's root README remains byte-identical to
its initial working-tree modification and is excluded from these commits.

Exact commands, configurations, final checks and retained failures are recorded
in [the evidence ledger](evidence/m2-player-impulses.json).

## Remaining boundary

This connects the admitted single-player profile. Production NPC damage impulses,
trapped-character behavior, already-dead damage ordering, dedicated pusher/body
forces, whole-world restoration and interactive IMA music remain separate gaps.
The source dead check occurs after impulse calculation; the current shared
aggregate still rejects already-dead owners earlier. No retail trajectory,
multiplayer, native Metal or Windows verification is claimed. M2 remains incomplete.
