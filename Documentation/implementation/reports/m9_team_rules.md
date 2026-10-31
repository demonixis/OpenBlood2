# M9 — Original server Team rules

This lot ports the server rules needed before opening Team admission. It extends
[Bloodbath rotation](m9_rotation.md). **Network admission remains Bloodbath-only,
protocol 16 is unchanged, and this is not yet a playable Team match.** Team wire
profiles/roster, team limits, retained-team rotation and presentation follow next.
CTF capture-score integration stays separate. See the
[source ledger](../sources/m9_team_rules_sources.json).

## Selected source behavior

BloodServerShell assigns membership before creating/spawning the player. A typed
preference is auto/blue/red (0/1/2); auto chooses the least populated team. When
counts tie, TeamMgr replaces the first team only for `rand()%100 > 50`, so the
boundary is 50/51, not 49/50. The port preserves that predicate and one server
random request per tie. It uses the existing portable RandomInt generator rather
than importing C runtime rand state; retail random-bit equivalence is not claimed.
Explicit choices and unequal counts draw nothing. Checked client records replace
TeamMgr's player-ID linked lists; stale clients, repeated assignment and mutation
inside object callbacks are refused. Failed entry removes membership. Departure
removes membership but never subtracts its accumulated team frags. World reset
clears membership/totals, retaining configured rule flags.

Original player team identity is captured before Respawn. Team profile init
forces skin `team + 4`, using the existing four-character retail blue/red tables.
Attributes, weapons, ammo, entry announcement and respawn ordering remain shared
with Bloodbath. Team spawn uses one random ordinal over the full named list,
then scans cyclically for the player's team or neutral TeamID 0. If none exists,
the original fallback is the first marker even if assigned to the other team.
Checked grounding and bounded occupied-start separation stay unchanged; there
is no extra marker draw. Solo marker priority and local CTF inspection do not
change.

Enemy kills add one to player and team. A same-team kill subtracts one only when
both FriendlyFire and NegTeamFrags are enabled; otherwise neither changes.
Suicide always subtracts one, independently of those two settings. Original
name/announcement updates still occur for a zero-delta team kill. Team totals
are stored independently, not recomputed from current players. Both counters
are checked against the existing ±1,000,000 network score bound before mutation.
The default flags are both true, as in host and dedicated NetStart. CTF's
OnlyFlagScores and capture contribution remain unconnected; its admission is
still refused rather than silently treated as Team.

CDestructable preserves original friendly-fire ordering: either sender-self or
owner-self allows damage first. Outside team modes, or with FriendlyFire enabled,
no team lookup is needed. Otherwise only player victims are filtered. A direct
player sender takes precedence; a nonplayer projectile falls back to its player
owner. Same-team damage returns before health, armor, impulse, leech or triggers.
The typed damage service still requires legitimate sender/owner attribution;
this lot does not relax that boundary for tests.

Voodoo needs its separate source gate because its slow/effect handling follows
ordinary damage. A protected nearest primary target produces neither damage nor
slow/effects nor self punishment. Alternate fire treats a protected in-cone target
as a hit **before its visibility ray**, preventing self punishment even behind a
wall. A real miss still punishes its owner. Source PlayerObj also confirms slow
death before forwarding to CDestructable: after the safe interval, a teammate may
finish an already dying player despite disabled friendly fire, while the original
lethal attacker receives the frag. This source quirk is preserved explicitly.

## Verification

**51 native, 30 headless ASan/UBSan and 51 inspector-free Release test cases pass.**
These are distinct cases after corrected reruns. The [check manifest](evidence/m9_team_rules_checks_2026-10-31.json)
records exact build commands, CTest selections and solo hashes; complete attempts
are retained in `evidence/m9_team_rules_*_2026-10-31.*`.

- Synthetic Team tests exercise checked membership, invalid/stale/repeated choices,
  exact 50/51 RNG boundaries, least-populated selection without a draw, failed
  entry cleanup, cyclic spawn/fallback and Voodoo protection behind a solid wall.
  Real retail data checks all eight character/team skin combinations and respawn,
  configured projectile ownership, blocked armor/impulses, enemy/self/environment
  damage, team-kill options, slow-death finishing, departure and atomic score bounds.
- Existing object lifetime/dispatch, Bloodbath capacity/profile/death and retained
  map-rotation checks pass in all three builds. Native and Release also pass ten
  local CTF cases and eight save-session cases; Release includes the real demo
  Voodoo/Focus weapon regression. Server-only sanitizers do not register those
  presentation-dependent cases and are not counted as running them.
- Two independent Bloodbath client processes pass actual dedicated combat in
  native and sanitized builds: 2,400 server ticks, 13 shots, one frag, 13 damage
  impulses, health-100 respawn and clean departure, with no discarded server ticks.
  Native graphical hosting with two remote combatants also passes: 1,200 frames,
  33 shots, one frag, 13 impulses and 3,609 server ticks. Eight ticks were discarded
  under concurrent verification load; this is not a performance qualification.
- Inspector-free Release hosting retains each remote connection through
  Crypt → Fields → Crypt. It finishes 750 frames, 2,288 server ticks, five rotations
  and 12 world admissions/departures, with eight discarded server ticks. Fresh
  replica/collision epochs, reset scores, local bells and clean exit are verified.
  These process scenarios exercise Bloodbath regressions, not Team network play.
- Five fresh solo traces are byte-identical to the prior lot: effects 240, audio
  360, movement 360, environment 720 and camera/HUD 300 frames. All **6,863**
  protected hashes remain unchanged. Independent client process binaries retain
  no server gameplay symbols. Formatting and maintained documentation paths
  pass checks; the sixteen source-ledger hashes match their recorded revisions.

Builds use `cmake --build` in `build/m9-fx-runtime`, `build/m9-fx-runtime-asan`
and `build/release`, followed by verbose CTest selections recorded in the manifest.
GPU regressions run on the installed NVIDIA Vulkan driver with CTest's dummy audio
output; ASan/UBSan results are headless and do not clear the existing GPU sanitizer
reservation. Rebuilds retain pre-existing MainMenus aggregate-initializer warnings;
the external effects trace helper retains its indentation warning. Its first final
invocation had an extra output-path argument and exited 2 before setup; correcting
the caller to redirect stdout produced all five matching final traces.

Initial native tests caught two fixture errors: TeamID was supplied as float
instead of the registered integer type, and a forged direct-player damage owner
was correctly refused by WriteDamage. The fixtures now use typed TeamID, direct
checks for otherwise-unrepresentable source priorities, and actual configured
CGrenade ownership for delivered projectile damage. The failed attempt remains
in the raw evidence. No production attribution or collision check was weakened.

This is a server prerequisite, not Team network/process qualification. Team
admission/profile transport, roster/team display, team-score limits and stable
assignment across map rotation remain next work; CTF and the broader M9 gates
remain pending. No claim of human LAN, Windows/macOS interoperability, 16-player
load, impaired combat or mobile/Quest qualification. Solo, CMoveMgr's single
integration, logical audio, PBR and M2-GPU-001/M3 boundaries remain unchanged.
