# M9 — Bloodbath limits and map rotation

Bloodbath now ends on the original frag/time rules and cycles maps on dedicated
and playable-host sessions without disconnecting their ENet peers. This extends
the [scoreboard lot](m9_scoreboard.md), using protocol **16**. Team/CTF admission
and team-score rules remain separate. See the [contract](../../contracts/multiplayer_contracts.md#bloodbath-match-limits-and-map-rotation)
and [source ledger](../sources/m9_rotation_sources.json).

## Source behavior and adaptations

`BloodServerShell::UpdateMultiplayer` checks time first, then player frags. Limits
use `>=`; FRAGSANDTIME means **either**, not both. NGE_NEVER disables both.
The common CLI accepts `--end frags|time|either|never`, `--frag-limit N`,
`--time-limit N` (minutes), and repeated `--map NAME`. The original defaults are
25 frags with Frags selected, 15 minutes for a host and 10 for the dedicated.
Zero is an immediate threshold, not a disable switch; frag zero still needs a
present player whose score is nonnegative. The first `--world`/`--host` plus
following maps form a cyclic list of at most 50, matching MAX_GAME_LEVELS.
One entry reloads itself. Frag limits are bounded to 1,000,000, matching current
score transport; unsigned minutes use checked parsing and 64-bit tick arithmetic.
Match flags are refused on the standalone/client-only launch path.

The limit latches before object updates. Departure or subsequent scores cannot
restart/cancel it. Original StartNextMultiplayerLevel sends one changing-level
event, keeps current scores for the intermission and changes after **strictly
more than 7.5 seconds**. The replacement fixed 60 Hz clock expresses that as
more than 450 ticks. The owning network pump commits rotation between ticks;
existing six-tick catch-up limits still apply. The server keeps running with
zero clients, including timed rotations. Diagnostics retain cumulative ticks
and count completed rotations separately from the reset world clock.

Clients show IDS_MP_CHANGINGLEVELS, play the original local bellding1.wav at
SOUNDPRIORITY_MISC_MEDIUM, reveal scores and select third person. Zoom, existing
wonky movement restrictions and inventory tabs reset. Commands and look become
neutral; both CViewWeapon hands cancel firing. Original CancelFiringState sends
SOUNDSTOP: the server stops the retained weapon loop and reliably publishes its
end. Server command/fire validation independently blocks new actions during the
wait, even for delayed held-fire frames. Passive movement, existing projectiles
and server rules continue. CMoveMgr receives no second integration. Unimplemented
Wonky visual effects are not claimed as newly supported.

Rotation destroys original players through checked cleanup, ends old logical
voices and reloads the same GameSession owner through its existing fresh-world
boundary. Maps must have the correct multiplayer prefix and exist in mounted
resources at startup; actual initialization is validated when loaded. A failed
destination stops the session instead of continuing a partially destroyed world.
There is no inventory/score transfer or multiplayer save. Each loaded world gets
a new epoch, clock and snapshot sequence. Connected peers retain their checked
profile but receive fresh public player aliases, unlike the original stable
engine client number. Ready/profile and baseline acknowledgements are required
again, with the existing loading deadline; content is mounted/fingerprinted once.

NetworkClient clears voices, speech, models, sprites, input, private queues,
camera/weapon state and prediction history at the new offer. It reloads its own
DAT/collision and only presents a complete new baseline. Device output and mounted
resources remain owned by the client. Existing renderer epoch checks rebuild
world-owned GPU resources; no renderer/PBR file is changed. Valid retired-world
state, baseline chunks and movement reports are ignored across channel ordering.
Private/audio payloads also reject stale ownership; old baseline envelopes are
bounded before their epoch is inspected. Reliable ACK order stays on Control.
Hello admission remains strict, and old server-bound packets do not refresh a
peer's deadline. No pointer or GPU object enters the protocol.

## Verification

**52 native, 42 headless ASan/UBSan and 50 inspector-free Release tests pass**
after the corrections below. The counts are distinct test cases, with corrected
reruns replacing earlier failures; complete attempts remain in
`evidence/m9_rotation_*_2026-10-31.*`.

- Synthetic checks cover all four end rules, exact boundaries, zero/large values,
  shared CLI refusal/bounds, protocol golden bytes, truncations and mutation
  suites. The chunk-epoch parser adds every truncated prefix and 10,000 mutations.
  Object lifetime/dispatch and existing configuration suites pass.
- Private session checks use original UpdateFragStatus, one end event, a retained
  weapon loop stopped exactly once, refused intermission USE, the 450/451-tick
  boundary, winner departure, empty-server rotation, fresh scores/life and stale
  commands. Two endpoints complete Crypt → Fields → Crypt, with new replica and
  collision epochs and three notices/bells. A deliberately delayed old-world
  baseline patch, State, MovementSetup and PlayerDead cannot poison new loading.
- Actual dedicated and graphical-host tests launch two independent client
  processes. Each keeps one connection through three world entries and leaves
  cleanly. The second arrives during intermission. Dedicated native/sanitized
  runs finish 2,100 ticks and four rotations, including empty-server rotations
  after both clients leave (zero discarded ticks native, ten sanitized). Release
  hosting finishes 750 frames, 2,289 total
  ticks, five rotations and 12 world admissions/departures; nine server ticks
  were discarded under concurrent verification load. This is not a performance
  qualification. Native hosting also passes. GPU captures in both builds show
  Fields, third-person player, original scores, end notice and HUD; inspected
  images and their hashes are recorded separately from headless sanitizer results.
- Existing actual-process PvP still verifies shots, damage, frag, impulses and
  health-100 respawn: dedicated native/sanitized 2,400 ticks, 13 shots, one frag,
  13 impulses; native host 1,200 frames, 33 shots, one frag and 13 impulses.
- Five fresh solo traces match the prior lot byte-for-byte: effects 240, audio
  360, movement 360, environment 720 and camera/HUD 300 frames. All 6,857 protected
  hashes remain unchanged. Client process binaries in all three builds contain
  no server gameplay symbols. Solo and logical-audio update ordering are unchanged.

The first delayed-packet fixture failed compilation because ObjectState has no
`name`; changing its metadata mutation to `user_flags` fixed it. An initial test
then ran an old binary without the new selector and failed; that attempt is
retained. Expanded wire checks found two old version-13 golden expectations,
now updated to 16. The first Release selection included an unbuilt private-world
test; it was built and passed. None of these failed/not-run attempts is counted
as a pass. Rebuilds expose existing aggregate-initializer warnings in unchanged
MainMenus/game_setup code; the external effects helper retains its indentation
warning. Source review also caught and corrected the missing intermission loop
stop before commit, with a dedicated assertion and native/sanitized/Release reruns.

This is Linux scripted retail verification, not human LAN or Windows/macOS
interoperability. Full 22-map combat, 16 players, impairment, gamepad/XR hardware,
mobile/Quest and long-session qualification remain pending. Team/CTF, remote
music/FX, communication portraits, measured pings/chat and Host/Join/LAN screens
remain M9 work. M2-GPU-001 and M3 acceptance remain independent.
