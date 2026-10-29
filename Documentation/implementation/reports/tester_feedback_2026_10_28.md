# Tester feedback, 2026-10-28 (0.5.1)

GitHub issues #5, #6 and #7 and six Windows session logs, on top of `72a4b10d`.
No commercial assets or reference-source dependencies are added. M2's reservation
and milestone acceptance are unchanged.

## Logs

Six logs were supplied. Two are 0.3.1 and one 0.4.0; only the 0.5.0 logs are
current evidence.

- `session-1793213411514294600` and `session-1793214159557762800` (0.5.0): both end
  in 02_CityHubB at `time=100.0` with `CDestructableBrush / startcrate at
  640,-9998930,2624 result=60` (the second after loading a CityHubB save). This is
  issue #6, already fixed by the [2026-10-27 correction](tester_feedback_2026_10_27.md)
  (`72a4b10d`): a restored crate below the world is held like a falling one.
- The first of them also logs two `Music: silent note, No DLS instrument for this MIDI
  patch/key (bank 0/0 program 0 key 64/66)` warnings in 03_Museum. These are
  reported notes without an installed instrument, not failures; they are not changed.
- `session-1793213398305695100` (0.5.0) and `session-1793212555396839300` (0.4.0) stop
  after the saved-games line with no `Session finished` line, and the next session
  starts 13 s later. Windows crash capture is inactive in these builds, so nothing
  identifies a crash, a hang or a killed process. They remain unexplained.

## Issue #6: crash in the sewer pipe after the Museum (fixed in `72a4b10d`)

The reported failure is the CityHubB `startcrate` fall above. No further change.

## Issue #7: frozen NPCs refuse quick saves (fixed)

A save is refused while any NPC carries a runtime failure; the same failure
suspends the NPC (it stops moving and no longer dies). The three reported failures
have three causes.

- `Target link acquisition failed; ... Target assignment requires distinct live
  objects` (Steam Tunnels, after the three valves and the Shikari): the steam valves
  are damage volumes, and an NPC's own explosion damages it too. `ComputeStimuli`
  turned a non-character last damager into an explicit perception failure and a self
  damager into a refused self link. The original clears a non-character damager and
  then casts it to a character anyway (undefined), and would target itself. Both now
  resolve as no stimulus, like the original dead-attacker branch, as does an
  own-list enemy fallback that selects the actor itself. `AI_Mgr::ignored_damagers`
  counts them.
- `Movement infinite-ray service failed` (Steam Tunnels): `CastRay` clips an infinite
  ray at the ±10⁷ coordinate domain. `sweep_object` translates a segment into a moved
  world model's or container's local frame, where that endpoint can leave the domain,
  and the whole query was refused as invalid. Any door, platform or pushed brush
  displaced against the ray direction did this. Now only the part inside the shape's
  padded bounds is swept and the fraction remapped; in-domain queries are unchanged.
- `Obstacle normal/up is degenerate` (Center for Disease Management): an obstruction
  hit without a usable normal (the side ray starting inside it) failed the plan as
  invalid. It is now an ordinary `Blocked` refusal, which callers already handle
  (the NPC changes state and replans). A coincident clearance destination is also
  clear rather than invalid, matching the source's zero-length side segments.

The tester also saw green teleport effects without enemies in CDM. Without a log this
is not reproduced or diagnosed; it may be original WarpGate presentation and remains
open.

## Issue #5: weapon sidebar (added)

The original `CNewStatusBar` weapon tabs were not ported. They are now, toggled by F1
without Shift as in the original shell (see the
[HUD contracts](../../contracts/original_hud_contracts.md#weapon-tabs-f1-051)).

## Verification

- Headless: `ai_runtime.damagers`, updated `ai_perception.retaliation`,
  `ai_navigation.clearance`/`parallel_axes`, `compound_shell.lifetime` (domain-edge
  sweeps against a displaced shell) and `input_action.menu` (F1 routing).
- Private data: `hud.private` (real weapon art, highlight, slide timing, low detail,
  reset).
- `ai_fire.script_private` failed since `72a4b10d`: its scripted actors stood at
  y = 10000, outside SubwayA's padded extents, so the outside-world hold stopped their
  walk. The fixture now uses free air at y = 600 (geometry ends at 512, the hold
  starts at 612); the assertions are unchanged.
- The tester's exact Steam Tunnels and CDM encounters were not replayed.
