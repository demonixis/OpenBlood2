# M2: Original conversation triggers and client communication

Original ConversationTrigger sequence callbacks and the numeric-character
CCommLink timing/audio path now execute in Blood2. Read the
[contracts](../../contracts/conversation_contracts.md) and
[source ledger](../sources/m2_conversation_sources.json). M2 remains incomplete.

The complete original filename/text table is retained, including backslash-only
basename lookup. Eight authored waves stop at the first empty field. Original
player/filter/delay rules remain inherited from Trigger; repeat-last overrides
loop-to-first. Unmapped cues do not advance. Successful mappings queue owned
messages, and the source advances even when delivery fails or the client is busy.

Client communication keeps original portrait selection, strict entry/exit clocks,
text-derived duration and busy refusal. It starts real buffered local audio once,
retains the voice independently of the trigger and kills it at the original text
deadline or on player/world exit. Missing audio stays visible and is not retried.
Timing runs in simulation instead of Draw so hidden or absent inspection cannot
freeze the sequence. Original portrait/font HUD rendering and client VoiceMgr
stop/delay arbitration are explicitly unavailable; their requests are inspected,
not reported as completed services.

The Cinematics inspector exposes every source sequence and resource, including
hidden/inactive entries, as well as client text, actual sound handle, phase,
duration, busy drops and failures. Commands use the existing frame-boundary
TRIGGER path. No commercial asset or reference repository is modified.

CityHub now has **434 created / 31 transferred sounds / 541 editor-only /
19 unsupported / one failed**. Structural preparation checks 466 entries, with
19 unknown and zero invalid schemas. SubwayA counts are unchanged. These counts
measure creation, not a completed playable session or successful dialogue.

The private catalog contains seven ConversationTrigger entries across 57 parseable
base worlds. Four use default.wav; three use historical names under sounds/chosen/caleb
(C_e1m8_1.wav, e1m2c3.wav and e1m2c6.wav). None has a matching original text entry
or an installed WAV. They remain silent and diagnostically unresolved, including
CityHub's placeholder. The malformed 16_bridge DAT remains rejected. No guessed
aliases or invented dialogue are introduced.

A separately labelled diagnostic uses the original callbacks with the real
installed 1ACaleb1 voice. This verifies playback and removal/unload behavior
without changing any authored conversation. The GPU check displays that diagnostic
alongside CityHub's untouched placeholder, exercises hidden inspection and reload,
and submits PCM to SDL's dummy output. It is not evidence of an authored CityHub
conversation or retail dialogue/HUD parity.

## Validation

Implementation commit: `db842761f642bd9bb8dcb8fca6c062f208fae7c2`.

Commands, complete test logs, source hashes and capture hashes are in the
[evidence record](evidence/m2-conversations.json). Linux/Vulkan only; Release
has Dear ImGui disabled. No cross-platform or retail playthrough result is claimed.

| Check | Result |
| --- | --- |
| Debug headless | 722/722, including 117 private-data tests |
| Release headless, inspector disabled | 722/722, including 117 private-data tests |
| Debug GPU | 4/4, including three private-data tests |
| Release GPU | 4/4, including three private-data tests |
| Clang ASan/UBSan with leak detection | 71/71, including 11 private-data tests; no finding |
| Original table comparison | All 119 filename/text pairs identical and in original order |
| Formatting/reference integrity | 25 C++ files checked; 10,923 reference files unchanged; six reference repositories clean |

The eight new headless cases cover filename mapping, first-empty waves, repeat/loop
precedence, unsigned character conversion, inherited delays, exhausted sequences,
queue capacity, invalid strings, client busy state, strict phase timing, missing
sound without retry, queued ownership, player/controller removal and two private
CityHub unload/reload cycles. The real diagnostic WAV has 25,930 decoded frames
and nonzero samples. Catalog tests require the seven missing/unmapped authored
cues explicitly. Existing creation totals change only for the newly registered
class; no failure or unknown-class expectation is relaxed into generic success.

GPU checks cover application teardown, original cinematics, destination rehearsal,
and the new communication path. The new test runs with inspection hidden, then
unloads/reloads the world and checks the current player-owned voice and submitted
PCM. Debug and Release captures were visually inspected and remain in ignored
build directories. No GPU validation error was reported.

After the full suites, the catalog test gained an additional assertion that each
of the seven unresolved paths is absent from the resource index, distinguishing
missing assets from decoder failures. That catalog was rebuilt and rerun in all
three configurations; production code did not change. The initial Debug wrapper
terminated after CTest reported 722 passes but before retaining the detailed log;
the complete Debug suite was rerun successfully, and final detailed logs are retained.

## Remaining M2 work

The missing communication HUD and VoiceMgr arbitration remain explicit. CityHub's
19 unsupported source entries, missing fallengrills geometry, remaining required
services and safe live NEW_LEVEL transfer keep M2 open. M3–M8 remain planned.
