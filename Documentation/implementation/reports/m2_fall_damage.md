# M2 — original falling damage and stopping contacts

Original player falling damage now receives the actual stopping force and incoming
velocity from ordinary MoveObject collisions. Player engine mass is 250 and
friction zero, including supported live restoration. Both original falling flags
are saved. The inspector shows delivered impulses, incoming velocity, original raw
damage and delivery failures. M2 remains incomplete.

The source rule is speed Y < -1100 and positive (force - 3000) / 25 while either
falling flag is set. Newly queued rotating-door pushes retain their early return.
Self Normal damage goes through existing resistance, armor and death code. Sensors
retain zero force; pending removal and callback velocity changes remain safe.
Primary-world contacts use null HOBJECT rather than inventing a world object.

See [contracts](../../contracts/fall_damage_contracts.md) and
[source ledger](../sources/m2_fall_damage_sources.json).
The force formulas and callback-before-stop order are corroborated by later
LithTech, not verified against the 1998 executable. The small swept solver remains
replacement engine code. Reference sources, pins and commercial assets are
unchanged; the user's workspace README modification is preserved.

## Verification

Implementation commit: `711893eb55c7dcd994413474c6c3e9597c8dfbc0` in the independent Blood2 repository.

| Configuration | Targeted behavior/lifetime checks | NVIDIA GPU checks |
| --- | ---: | ---: |
| GCC Debug, inspector enabled | 195/195 (98.40 s) | 4/4 |
| GCC Release, inspector disabled | 195/195 (11.21 s) | 4/4 |
| Clang Debug, ASan/UBSan, inspector disabled | 195/195 (392.75 s) | Not run |

The four new cases cover the source threshold/door branches, force delivery and
callback lifetime, real player collision/restoration, and external Caleb falling
under MoveMgr. A fall from diagnostic Y=1800 produces one delivered impact at
30/60/120 Hz: force **6008.45**, incoming Y **-2403.38**, raw damage **120.338**.
Each run enters original self-kill death, produces one corpse and activates the
client death camera, then leaves zero object records and audio voices on unload.
These displayed values come from the verbose Debug run, not retail observations.

Broader checks cover objects, collision services, movement/locomotion/containers,
doors/triggers/pushables, AI updates/clearance, destructible models, blood/death FX,
player fields/checkpoints/KeepAlive and continuous SubwayA-to-CityHub input/combat
at 30 and 60 Hz. No acceptance threshold or timeout was weakened.

Four existing GPU regressions pass in both Debug and inspector-free Release on
**NVIDIA GeForce RTX 3060 Laptop GPU / Vulkan**: movement, character, player restore
and death effects. The movement capture was inspected locally; scene, compact HUD
and folded inspector remain visible. The new falling assertions are headless;
GPU checks independently exercise presentation and lifetime regressions.

[Machine-readable evidence](evidence/m2-fall-damage.json) records commands, hashes
and limitations. Logs: [Debug](evidence/m2-fall-damage-debug-tests.log),
[Release](evidence/m2-fall-damage-release-tests.log),
[ASan/UBSan](evidence/m2-fall-damage-sanitize-tests.log),
[fall behavior](evidence/m2-fall-damage-debug-behavior.log),
[NVIDIA Debug](evidence/m2-fall-damage-debug-gpu.log) and
[NVIDIA Release](evidence/m2-fall-damage-release-gpu.log).

Twelve changed C++ files pass formatting; four original selections and four engine
references match their source hashes. All 10,923 inventoried reference files and
six reference repositories remain unchanged. Local documentation links are checked.
The user's workspace README is excluded from both commits. Existing enum/float
comparison deprecation warnings remain outside this change. Sanitizer evidence is
headless; the previously documented SDL/libdbus GPU-LSan residual was not retested.

## Remaining gate

Dedicated dynamic-world/model-body, translating/rotating pusher and projectile
callbacks retain their prior zero-force behavior; no crush/rigid-body parity is
claimed. General SaveObjects/RestoreObjects, interactive IMA music and remaining
required services keep the actual M2 acceptance gate open. Native macOS/Windows
execution and retail falling-force measurements were not performed in this run.
