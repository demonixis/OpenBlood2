# M2 — original NPC ammunition recovery

Status: original FindAmmo and NPC ammo collection connected; **M2 remains incomplete**.
Implementation commit: `44e1f55c06eb2fb174c0459bc1f96e62b8ab7ad1`.

## Corrected behavior

The original `MC_BestWeapon` already selected `StateFindAmmo` for an empty current
ammo pool. The dispatch case was missing, so the next AI_Update latched a runtime
failure and stopped the actor. The original search, path phases, run command,
ledge/arrival decisions and fallback to EscapeRun or Idle are now integrated.

The original MID_ADDAMMO receiver is also connected. Living NPCs without
NOAMMOCOLLECT can accept ammunition up to their attribute-derived limit. Accepted
contact acknowledges the exact pickup, which plays its existing original WAV,
sends its authored trigger and removes itself deferred. Full reserves, dead
actors, disallowed collection and replayed/out-of-contact offers cannot consume it.
No health, item or weapon pickup behavior is invented for NPCs.

A removed searched pickup is recognized by its stable identity even after the
actor changes state. The stale non-owning tracker is cleared while its route and
historical position remain values. It can no longer suspend a later attack or
death by attempting to query a destroyed pickup. Other invalid tracker errors
remain explicit. Inspection exposes searches, requested ammo/class, accepted and
refused offers, removed tracked pickups and the checked last collection source.

## Source fidelity and explicit adaptations

[Source hashes](../sources/m2_ai_ammo_sources.json) identify the original
AI_Mgr, AmmoPickup, PickupObject and Cultist decisions.
[Contracts](../../contracts/ai_ammo_contracts.md) describe the complete boundary.

- Specific+AvoidTarget previously rejected because the original reads an
  uninitialized forward vector. Since its specific-class branch leaves the
  object dot equal to zero, the rejection predicate cannot be true. The port
  skips that unused undefined arithmetic and retains all defined selection rules.
- The original `RocketAmmoPU` spelling is preserved. Its absent class yields an
  empty search; no HowitzerAmmoPU alias is introduced.
- Ordinary CanSee ray-hit requirements remain unchanged. Normal nonsolid pickups
  may be invisible to this search. Positive route coverage deliberately adds
  RAYHIT to a labelled synthetic fixture; it does not change commercial content
  or claim autonomous retail pickup pursuit.
- A blocked path discards partial points and reaches the source empty-route
  escape. Invalid services and bounded-search failures remain failures. Animation
  and velocity commands use the existing original path and single integration.

This establishes a specific missing-state fix, not the cause of every idle Cultist
reported during play. FindHealth/AssistAlly, remaining services and the complete
observable M2 acceptance review remain open. Source-entry counts, shader artifacts,
stack pins, external assets and README milestone estimates are unchanged.

## Verification

The selected suite contains **94 headless checks per configuration**, including
15 private-data checks: object lifetime/messages, AI runtime/perception/fire/
recovery, pickups and pickup audio. Debug, Release without the inspector and Clang
ASan/UBSan with leak detection are checked. All 94 checks pass again on the final source revision after extending tracker
safety across state changes; see the
[evidence manifest](evidence/m2-ai-ammo.json) for exact commands and logs.

New behavioral coverage includes:

- Empty-weapon and empty-pool source decisions, normal nonsolid no-match escape,
  explicit ray-hittable route/face/run/arrival, hidden actor updates and a pickup
  removed during pursuit or after a state change.
- Dead/NOAMMOCOLLECT/full-pool refusal, partial cap, typed offer replay refusal,
  non-ammo refusal and deferred pickup-list/message/string cleanup.
- Real original Cultist/player initialization and retail ABC/WAV resources in a
  labelled synthetic collision arena. Two unload/re-entry cycles collect through
  actual physics contact, restore 100 bullets and retain a detached voice. This
  is not an authored campaign encounter or a complete playthrough.
- NVIDIA `gpu.pickups` and `gpu.character` in Debug and Release. The pickup test
  now includes original NPC collection through MoveObject contact while the UI is
  hidden, then removes both participants and verifies the audio tail. Existing
  rendering/toggle/resize/re-entry checks remain. Logs identify the NVIDIA GeForce
  RTX 3060 Laptop GPU. The Debug pickup capture was visually reviewed; it shows
  the pickup inspector, not every new NPC counter.

A negative control recompiles the pre-increment AIState.cpp against the current
headers/test executable. `ammo_search` fails at the original missing-dispatch
boundary (exit 1); the current implementation passes. This isolates the old
state dispatch, rather than claiming a complete historical build.

Initial fixture failures are retained in the evidence: a deliberately solid
pickup blocked the path it was intended to test; the positive branch fixture now
uses RAYHIT without SOLID. The private arena initially lacked real player entry,
then attempted to initialize an already initialized session on reload. These
fixture errors were corrected without relaxing production checks. An initial
smoke compilation used a return value from the void RemoveObject API; it was
corrected to the checked Objects removal call.

The 10,923 inventoried reference files and six reference repository states are
verified unchanged. Commercial captures remain in ignored build directories.
Linux NVIDIA execution does not add new native macOS/mobile/VR validation.
