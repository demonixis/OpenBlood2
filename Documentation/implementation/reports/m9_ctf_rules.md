# M9 — CTF server capture and match rules

This lot connects the original CTF objects to checked multiplayer players and
match scores. It follows [Team network play](m9_team_network.md), preserving the
[local flag contracts](../../contracts/ctf_contracts.md).
**ENet CTF admission remains closed, protocol 17 is unchanged.** The internal
ServerSession can create CTF players for server verification; the transport and
graphical frontend remain Bloodbath/Team until capture consumers are connected.
See the [source ledger](../sources/m9_ctf_rules_sources.json).

## Original rules and bounded adaptation

CTF uses the shared original multiplayer profile, assigned team skin, ammo,
spawn selection and respawn. A capture adds its points to both the capturing
player's `m_Frags` and the independent team match total. Capture-only inspection
counters remain available separately. Departing players never subtract prior
contributions. CTF end conditions read accumulated team totals, and the existing
strict intermission/rotation path resets scores, flags and notification clocks.

Default capture value is five, OnlyFlagScores and UseTeamSize both default false,
as in original host and dedicated NetStart. Shared rules accept
`--flag-value 0..1000000`, `--only-flag-scores on|off` and `--use-team-size on|off`.
The client executable accepts rule configuration only with `--host`; it still
refuses CTF hosting until its presentation is connected. The dedicated can apply
these settings during zero-client CTF inspection. World load resets the original
CTF globals, so ServerSession reapplies configured values after every map load.

UseTeamSize multiplies capture value by the opposing team's member count, with a
minimum of one. Checked connection membership replaces the original TeamMgr
lookup and includes inactive/dead members. It does not count only active objects
or infer teams from skins. Local single-player CTF inspection retains its existing
independent selection/counting path. Capture broadcasts now also reach inactive
connected players, matching original StartMessage(NULL).

OnlyFlagScores suppresses **enemy-kill points only**. Suicide still subtracts one;
same-team kills still subtract one when both FriendlyFire and NegTeamFrags are on.
Kill attribution, names, frag notifications and the single announcement RNG draw
remain intact even for a zero-point kill. Captures are independent of that option.

The capture product and individual/team match totals are bounded before mutation.
An invalid member or exhausted score cannot partially update counters, detach or
return the flag, publish a capture, or play its sound. Zero-value captures remain
valid: they still return the flag and publish/play the source effects. Existing
capture-only counter overflow checks stay intact. This is a checked adaptation of
the original unbounded arithmetic, not a new scoring formula.

Flag touch/stand commands, attachment offset, missing-friendly-flag gates, strict
four-second server/twenty-second local notice clocks and original 30-second
return deadline remain unchanged. Death drops before the null-killer guard;
normal departure drops before removing the player, while unexpected destruction
retains the existing checked lifetime handling. Capture/return sounds continue
through logical server audio without a device or a cosmetic RNG draw.

## Verification

Linux verification on 2026-10-31 finishes with **61 distinct native cases,
35 headless ASan/UBSan cases and 61 inspector-free Release cases passing**, after
the corrected attempts recorded below. The [check manifest](evidence/m9_ctf_rules_checks_2026-10-31.json)
contains the exact case matrix, selectors, build targets, hashes and process
metrics. Complete [native](evidence/m9_ctf_rules_native_2026-10-31.txt),
[sanitized](evidence/m9_ctf_rules_asan_2026-10-31.txt),
[Release](evidence/m9_ctf_rules_release_2026-10-31.txt),
[build](evidence/m9_ctf_rules_builds_2026-10-31.txt) and
[oracle](evidence/m9_ctf_rules_oracles_2026-10-31.txt) logs retain failures and reruns.

- Private retail server tests create two CTF players on each of Cabana, Dead
  Cities, DMX and Jam, covering all eight character/team skins and respawn ammo.
  Capture tests check missing friendly flags, the inactive/dead member multiplier,
  notification to every associated player, actual capture audio, no capture RNG
  draw, minimum-one multiplier and retained totals after departures.
- Enemy-only score suppression, unchanged suicide/friendly-kill penalties,
  combined kill/capture totals, zero-point captures and atomic individual/team/
  multiplied-score bounds pass. Death and departure detach/drop the flag; its
  timed return executes after 30 seconds with no clients. A capture reaches the
  team limit and rotates Cabana → Jam with reset flags/scores/clocks and retained
  configured options. These are internal server tests, not CTF ENet matches.
- Existing Team, Bloodbath profile/death, object and match tests pass. Native and
  Release also pass ten local CTF and eight save-session cases plus configuration
  checks; these client-dependent tests are not registered in the server-only
  sanitizer tree and are not counted there.
- Two actual client processes pass native Bloodbath and sanitized Team combat,
  frag/impulses/respawn and clean departure. Each dedicated run executes 2,400
  ticks with zero discarded ticks. Release Team hosting retains connections and
  teams through Crypt → Fields → Crypt, finishing 2,289 ticks and five rotations;
  its bounded catch-up discards eight ticks. The [host capture](evidence/m9_ctf_rules_team_host_regression_2026-10-31.png)
  was visually inspected. This is regression evidence for existing network modes,
  not CTF, impaired-network or performance qualification.
- Five fresh solo traces match the prior lot byte-for-byte: effects 240 frames,
  audio 360, movement 360, environment 720, camera/HUD 300. The effects trace uses
  external retail resources; the other four use isolated synthetic fixtures.
  The actual independent process binaries have no server gameplay symbols.
- All **6,883 protected file hashes** remain unchanged, including PBR, historical
  evidence and GPU references. Fifteen source hashes match their recorded
  revisions. Formatting and source/documentation diff checks pass; raw CTest logs
  keep original whitespace. Build warnings are existing MainMenus/game_setup
  aggregate initialization and the external effects helper's indentation warning.

Builds use `cmake --build` in `build/m9-fx-runtime`, `build/m9-fx-runtime-asan`
and `build/release`, followed by the selected verbose CTest cases. Graphical tests
use native NVIDIA Vulkan and dummy audio output; they do not qualify physical
sound/gamepad/XR or clear M2-GPU-001.

Initial attempts are retained as failures. The capture fixture first called
`take_audio_events` without `begin_audio`; the existing recipient/history guard
correctly refused it. The fixture now opens the audio stream before capture. A subsequent assertion
incorrectly assumed lower-case sound names; it now compares a canonical resource
path while preserving the source request spelling and checking priority/radius.

A separate oracle invocation incorrectly passed the retail directory to four
synthetic helpers. They created six fixture files and four subdirectories;
`sound.rez` conflicted with the original uppercase
`SOUND.REZ`, causing subsequent private-data tests to refuse mounting. The six
files were compared byte-for-byte with newly generated isolated fixtures before
removal; only those files and their four empty directories were removed. No
commercial archive was a helper output path. Corrected synthetic runs use separate
`/tmp` directories; the retail effects helper remains read-only. Their traces and
the restored full-content fingerprint match the preceding lot. Raw failed runs and the
[recovery manifest](evidence/m9_ctf_rules_fixture_recovery_2026-10-31.json) remain evidence; none is relabeled as passing.

These server checks do not qualify CTF network gameplay. Absolute capture-score
publication, independent carried-flag HUD/notices/audio, live CTF admission and
actual client-process capture/disconnect scenarios remain next. Broad M9 combat,
16-player load, impaired networking, human LAN, Windows/macOS interoperability
and mobile/Quest qualification remain pending. Solo, CMoveMgr's single integration,
PBR/reference files and independent M2-GPU-001/M3 gates remain unchanged.
