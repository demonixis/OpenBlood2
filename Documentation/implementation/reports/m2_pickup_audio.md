# M2 — Original pickup collection sounds and playtest follow-up

Successful ammunition, weapon, health/armor, inventory-item and key collection
now starts the original spatial WAV. The sound continues after the pickup is
removed. Missing sounds and budget failures remain inspectable without preventing
collection. **M2 remains incomplete.** Implementation commit: `bbfc2f4`.

Read the [contracts](../../contracts/pickup_audio_contracts.md),
[source hashes](../sources/m2_pickup_audio_sources.json) and
[validation ledger](evidence/m2-pickup-audio.json).

## What changed

The previously diagnostic-only `PickupObject` acknowledgement now calls the
existing detached one-shot audio service, preserving its original fixed position,
300-unit outer / 30-unit inner radius, full volume and MISC_HIGH priority.
Constructor-selected WAVs are used unchanged. Checking `ItemPickups.h` corrected
the port's misleading old key comment: `KeyPickup` overrides the base sound with
`inventory1.wav`, not `powerup4.wav`. The original sources remain untouched.

No sound plays for a refused full pool, dead player, forged acknowledgement or
repeated touch. Successful collection hides/marks the item before sound and trigger
callbacks. Even if a trigger removes the source or player, the voice remains
world-owned until completion or unload. Audio failures do not undo the item grant.
The latest 32 owned reports include keys, resource paths, positions, voice IDs and
start errors. The Pickups inspector distinguishes live decoded cursors/gains from
retired voices; it does not advance simulation or audio.

Reverb and priority arbitration, pickup character quips, general pickup FX and
other required M2 services remain separate. No dependency, shader or original
balance rule changes in this increment.

## Verification

The targeted suite passes **47/47 headless tests** in Debug with inspector,
Release without inspector, and Clang ASan/UBSan with leak detection. Six cases use
the external retail assets. This includes the new four-case audio suite, existing
pickup/key acknowledgement tests, weapons, object lifetime and NEW_LEVEL round
trips. No malformed-resource or previously expected-failure assertion is relaxed.

Synthetic fixtures exercise accepted/refused/dead/repeated collection, missing,
malformed and empty sounds, the 128-voice limit, bounded 32-entry history,
completion, unload and reentrant trigger removal of both pickup and player.
The private scenario diagnostically collects **21 supported classes**, using
external models and **eight distinct original WAVs**, and checks nonzero mixed PCM
and sample-clock progression. This is not an authored playthrough.

Five GPU scenarios pass in both graphical builds on **NVIDIA GeForce RTX 3060
Laptop GPU**: lifecycle, pickups, triggers, weapons and the authored level-exit
transition. The Debug pickup scenario initially exposed that its old silent harness
did not open an audio output; enabling the same SDL output requirement as other
audio scenarios made the strengthened check pass. The other four scenarios already
passed. There are no Vulkan validation errors. The hidden-UI pickup test checks
three original WAVs (ammo, weapon, medkit), fixed radii, source removal, advancing
voices and submitted SDL frames; reload/teardown remain exercised. The inspector
capture was visually checked. Captures and commercial resources remain outside Git.

All 10,923 inventoried reference file hashes remain unchanged and six reference
repositories remain clean. Native Metal and human listening have not been tested
for this increment. The pinned dependencies and bundled backend support are unchanged.

## Other playtest findings and remaining work

An isolated diagnostic on baseline `cc329c6` used the real SubwayA intro and original client weapon
update/animation keys, then committed NEW_LEVEL to CityHub. Holding primary for
two seconds fired six Beretta shots; alternate fired ten. Both sequences repeated
successfully in CityHub: ammunition went 50 → 44 → 34 → 28 → 18, and player movement
and cinematic weapon gates were clear. Default difficulty was **1 (normal)** in
both worlds. This rules out a universal transfer lock in that scenario, not the
reported intermittent failure. The diagnostic explicitly disabled player damage
and aimed upward; it did not reproduce a normal encounter, native mouse events or
all inventory states. The [source probe](evidence/m2-pickup-audio-playtest-probe.cpp) and output are retained in the evidence.
Its destination health of 1,000,000 follows the existing diagnostic invulnerability
restore semantics and is not normal gameplay health.

The same diagnostic subsequently relocated the player near collision-clear
24 of 27 CityHub NPC positions (three primary-world blocked probes were skipped)
and sampled five seconds of original AI updates at each. No latched runtime
failure was recorded in this sample. It does not prove every Cultist activates
correctly or that the reported idle enemies are fixed. No enemy health, armor,
resistance, hit-region damage or difficulty balancing has been changed.

The remaining user reports stay queued for focused reproduction:

- Cultists that never activate, and unexpectedly high numbers of hits to kill.
- Possible firing failure after level transfer in the user's actual encounter.
- An invisible water volume in CityHub; identify the authored volume and surface.
- **Severe slowdown and large frame stutters after changing levels**, newly reported
  during this run. Compare steady frame times before/after NEW_LEVEL, then isolate
  simulation, AI/physics, resource/GPU work and audio; distinguish a one-time load
  pause from sustained stutter before selecting a fix.

The earlier silent-collection report is addressed by this increment's playback
path. Actual listening and the other playtest symptoms still need their own
validation. The M2 gate and autonomous continuation remain open.
