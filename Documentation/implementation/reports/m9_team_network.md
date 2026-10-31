# M9 — Team network admission, scores and retained teams

This lot connects the [original server Team rules](m9_team_rules.md) to the
independent network client and the graphical host, using protocol **17**. Team
uses the retail `bb_*` maps and the same checked movement, weapons, death/respawn,
audio and fixed-tick simulation as Bloodbath. CTF admission remains closed until
capture scoring and flag consumers are connected. See the
[source ledger](../sources/m9_team_network_sources.json).

## Admission and match rules

`--mode team` selects Team on the dedicated or playable host. A client or host
uses `--team auto|blue|red`; auto is default. The bounded JoinProfile adds this
0/1/2 preference, with the original checked assignment before player creation.
Team skin remains the source `team + 4`, while character/attributes/weapons stay
in the shared profile. A Bloodbath server ignores the team preference.
`--friendly-fire on|off` and `--negative-team-frags on|off` configure the shared
server rules; both default on. They and `--mode` are host-only on the client
executable. Team choice is rejected on non-network launch paths.

The server retains the **assigned** team beside the original peer profile.
Ready must still match that original profile at every map. The server overrides
its preferred team with the retained assignment when creating the fresh player;
clients cannot change assignment by sending a different Ready. New worlds still
have fresh player aliases/objects and reset scores/inventory. Disconnect removes
the peer's assignment; reconnect, even under the same name, is a new admission.
No handle or team-manager pointer crosses the protocol. Auto balancing considers
currently admitted members, as in the source, not reserved future admissions.

Team end rules read the two accumulated team totals instead of individual scores.
A departed member's contribution remains; a team can win while every connected
player is below the frag limit. Empty teams exist in original TeamMgr, so a zero
Team frag limit ends even an empty server, unlike Bloodbath's player-based zero
threshold. Strict 450/451-tick intermission, neutral commands, source weapon-loop
stop and checked same-peer world replacement are unchanged.

## Replica and presentation

Reliable roster replacements carry mode, immutable player teams and absolute
team totals. Frag events carry absolute team totals too; clients never derive
those totals by summing current players or counting duplicated notifications.
The inbox checks its admitted mode and refuses changing an existing player's
team. Ordered lifetime/world checks, bounded queues and atomic decoding remain.
Sixteen maximum-length names now occupy **2,041 of 2,048 bytes**, including the
private envelope; every prefix and malformed team/score is rejected before
replacement. The global version changes consistently across all packet families.

Original SMSG_ADDPLAYER connects the recipient's team to the existing original
HUD team icon. New arrivals use IDS_ADDPLAYERTEAM and IDS_TEAM1NAME/IDS_TEAM2NAME
from external cres.dll; percent-bearing player names remain literal. Missing
team names retain the source's second-team `Team Blue` fallback typo with an
explicit diagnostic. Departure and Bloodbath notices retain their existing path.

CFragInfo's Team table uses the real frag bar, Voice portrait, character portraits
and font. The two columns stay in blue/red order, 32 pixels apart, with independent
team totals. Each team's players use the source stable descending draw-time sort,
retaining ties across hidden score updates. Source header spacing, strict
`nCount < nMax`, DrawClip behavior and sub-350-pixel scrunch arithmetic remain.
Below 500 pixels wide, Tab/gamepad Back opening advances the selected column;
automatic death/intermission display does not advance it. The original initial
column zero therefore shows no narrow table until the first manual opening.
This is retained source behavior, not a missing network roster.

CPU surfaces crop unused transparent bottoms and bound storage by 16 players and
headers. Tiny viewports avoid unsigned subtraction underflow. The overlay waits
for submitted GPU work before replacing its owned texture when column width or
required height changes. World rendering, PBR paths and reference captures are
untouched. Measured pings still await their own lot; `(-)` remains truthful.

## Verification

Linux validation completed on 2026-10-31 with external retail data:

- **56 distinct native tests**, **46 headless ASan/UBSan tests** and **55 Release
  tests** with the inspector disabled. The matrix covers bounded codecs, Team
  rules/state/admission, immutable teams, total scores, private events, source
  notices/tables, rule parsing and existing object/configuration contracts.
- Real dedicated processes with two independent clients pass Team lethal combat,
  one blue-team frag, impulses and respawn. Dedicated and graphical-host processes
  pass Crypt → Fields → Crypt while retaining assigned teams and resetting scores,
  player aliases, collision and replicas. The graphical host also passes combat.
  Native Bloodbath process combat remains successful. These are automated local
  processes with private data, not human LAN qualification.
- Empty-server Team zero-frag rotation and a changed Ready team during a retained
  Baseline-phase connection are checked explicitly. The latter is rejected before
  another player is admitted. A departed player's contribution can complete the
  team limit even though every connected individual remains below it.
- Five fresh solo traces remain byte-identical to the preceding lot: effects
  (240 frames), logical audio (360), movement (360), environment (720), camera/HUD
  (300). Independent client-process binaries contain no server adapter, game
  session, original player/weapon/inventory or logical-server-voice symbols.
- Corrected Team GPU captures agree byte-for-byte between native and Release.
  Both Bloodbath scoreboard captures remain identical to their immutable prior
  references. Visual inspection confirms the team HUD icon, two-column table,
  compact blue/red tables and live rotation view.
- All **6,869 protected hashes** remain unchanged, including existing PBR files,
  immutable evidence and GPU references; all 18 source-ledger hashes match.
  Source/documentation formatting and diff checks pass; raw CTest logs retain their
  original whitespace. Build warnings remain the existing
  MainMenus/game_setup aggregate-initializer warnings and the external oracle's
  existing indentation warning.

Builds use `cmake --build build/m9-fx-runtime`, `build/m9-fx-runtime-asan` and
`build/release`, with the relevant application/server and network test targets.
The exact target lists, CTest selectors, capture hashes and distinct case matrix
are retained in the [check manifest](evidence/m9_team_network_checks_2026-10-31.json).
Raw [native](evidence/m9_team_network_native_2026-10-31.txt),
[ASan/UBSan](evidence/m9_team_network_asan_2026-10-31.txt),
[Release](evidence/m9_team_network_release_2026-10-31.txt),
[build](evidence/m9_team_network_builds_2026-10-31.txt) and
[solo oracle](evidence/m9_team_network_oracles_2026-10-31.txt) logs retain every
recorded attempt. GPU runs use native NVIDIA Vulkan and the dummy audio output;
they do not establish physical audio/gamepad/XR behavior. The Release Team host
rotation records **eight discarded server ticks** under the bounded catch-up;
its client records zero. This observed overload is retained, not suppressed.

Corrected visual evidence: [wide Team table](evidence/m9_team_network_corrected_team-640-1_2026-10-31.png),
[compact blue](evidence/m9_team_network_corrected_team-320-1_2026-10-31.png),
[compact red](evidence/m9_team_network_corrected_team-320-2_2026-10-31.png),
[host](evidence/m9_team_network_team-pvp-host_2026-10-31.png) and
[rotation](evidence/m9_team_network_team-rotation-host_2026-10-31.png).

Visual review of the first GPU capture caught a clipped final glyph row in the
320×240 Team table: its cropped surface ended at the next compact row start.
The corrected bound retains the complete final row while preserving source
spacing and the original table clipping boundary. Initial captures are retained
separately from the corrected visual evidence.

The first ASan build request named presentation-only config_tests, which is not
registered in the server-only tree. The first Release build exposed that the
independent PvP client target was incorrectly inside BLOOD2_BUILD_SERVER although
graphical-host tests also use it. The target now builds whenever private-data
network tests are enabled; only actual dedicated process tests depend on the
server target. Both initial failed attempts remain in evidence.

This lot does not qualify all 22 maps in combat, 16 players, impaired combat,
human LAN, Windows/macOS interoperability or mobile/Quest hardware. CTF, remote
music/FX, communication portraits, chat/ping and original Host/Join/LAN screens
remain M9 work. Solo and CMoveMgr's single integration, logical server audio,
PBR files, immutable references and independent M2-GPU-001/M3 gates remain intact.
