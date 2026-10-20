# M2: Original SoundFX states and audio

Project commit: `70070b764bb71153033f3b281b54b47659aa806f`.

Original `ObjectDLL/SoundFX` now uses the existing typed audio mixer for
nonstreaming ramp-up, main sound and ramp-down playback. Whole-string ON/OFF/
TOGGLE/TRIGGER commands preserve original restart/interruption behavior. Ambient
ON loops; spatial ON plays once and schedules ramp-down from decoded duration.
InitialUpdate bypasses ramp-up and cancels even a spatial ON deadline. These
source quirks are covered explicitly rather than replaced with a generic loop.

[Contracts](../../contracts/sound_fx_contracts.md),
[source ledger](../sources/m2_sound_fx_sources.json) and
[retained evidence](evidence/m2-sound-fx.json) separate verified source rules,
engine adaptations and remaining failures. No new dependency, mutable game
global, reference edit or commercial resource copy is introduced.

Objects inspects the current state, all three paths, startup/ambient/streaming
flags, radii/byte volume/priority, attempted starts/failures, actual voice cursor,
loop state, fixed position and last error. Generic activity and scheduling remain
visible, including hidden/inactive objects. Rejected source properties and
resource references remain in World. No audible listening or manual whole-level
playthrough is claimed.

The private catalog reads 57 valid worlds (the previously identified malformed
16_bridge still fails), finding **124 SoundFX entries, all rejected by historical
properties: 121 report VoiceType and three report TypeAction**. There are 124 nonempty resource references and
one unavailable resource (`sounds/ambient/ratsscury1.wav`); resource decoding does not mean the emitter runs.
The five CityHub entries all also request FileStream. They are not silently
translated into full-clip playback. CityHub therefore remains **413 created /
541 editor-only / 32 unsupported / 40 failed**; five entries moved from unknown
class to explicit schema failure. Structural preparation remains 417 checked,
with 32 unknown and 36 invalid. SubwayA source accounting is unchanged.

This exposes a content/source generation mismatch that must be investigated
alongside real streaming before these authored controllers can run. The source
SoundFX and available SDK do not define/read VoiceType. This increment deliberately
makes no assertion about the older binary's interpretation of it.

Value-owned paths fix the source destructor's duplicate RampDownSound release;
null trigger text is safe. Source callback/filter ordering, retained voice cleanup,
fixed sampled position and old-deadline behavior remain intact. FileStream,
save/load, precache, region loops, reverb and priority arbitration are unavailable.
Failures stay inspectable; no success-shaped service is introduced.

Debug and inspector-free Release each pass **666/666 headless tests**, including
106 private-data cases, plus **5/5 relevant GPU tests** (lifecycle, objectives,
cinematics, random sounds and world sounds). The complete 59-case GPU suite was
not rerun. Debug takes 94.92s headless / 27.89s GPU; Release 16.02s / 19.20s.
Clang ASan/UBSan with leak detection passes **65/65** object/service/world/audio
checks, including nine private cases, in 64.31s. Final logs have no compiler
warning/error, GPU validation error/VUID or sanitizer diagnostic.

Commands from Blood2: `cmake --build build/debug -j4` and
`cmake --build build/release -j4`, then
`ctest --test-dir build/<configuration> -L headless -j4 --output-on-failure` and
`ctest --test-dir build/<configuration> -R '^gpu\.(lifecycle|sound|randomsound|cinematics|objectives)$' --output-on-failure`.
Sanitizer targets: `sound_fx_tests object_contract_tests services_tests world_tests
sound_tests random_sound_tests`; CTest uses
`-j2 -R '^(objects|services|world|sound|randomsound|soundfx)\.'` with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`.

All ten changed C++ files pass clang-format, 2847 local Markdown links resolve,
and all 10923 inventoried reference files plus six reference repositories remain
unchanged. The Debug world-sound capture was reviewed; it shows the existing audio
panel, not the newly added SoundFX object fields. Those fields are compiled but
have no dedicated visual UI assertion. Verification is Linux/Vulkan only.

The added GPU audio probe is explicitly diagnostic: it creates a SoundFX using
an actual SubwayA WAV, sends original ON/OFF messages, verifies nonzero PCM
submission/cursor and voice removal while the inspector is hidden and after
world reload. It does not normalize CityHub properties or claim those five
emitters succeeded. The inspector-free Release path is exercised as well.

Safe automatic NEW_LEVEL, required remaining services and the actual M2 exit
gate remain open. Registration and diagnostic playback alone do not certify a
complete original session.
