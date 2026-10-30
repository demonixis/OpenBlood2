# M9 — cosmetic impact audio on the client

Implemented against `70379d4a`, following [owner-bound effects](m9_effect_owners.md)
under the [M9 contract](../../contracts/multiplayer_contracts.md).

## Delivered boundary

Flesh/destruction fragment, particle blood-drop and ballistic casing impacts now
start buffered spatial one-shots in ClientAudio. The existing local adapter forwards
the original request, position and resource failure; it no longer creates a server
logical voice. No original random draw, impact choice, attenuation, sample interpolation
or source error/fatality rule changes. No server sound service used by game rules
is redirected. CMoveMgr, save formats and PBR files are unchanged.

Cosmetics use a distinct upper-bit SoundId namespace and have no original owner or
speaker handle. Their IDs are monotonic across world resets. Incoming logical frames
cannot claim this namespace. Logical reconciliation preserves local cosmetic voices
and cursors; ClientAudio removes completed cosmetics after playback. Consequently
local playback cannot consume a server sound identity, voice slot or logical clock.
The existing 128-voice mixer cap remains. When a valid authoritative frame needs
capacity, the receiver displaces the oldest local one-shots, never refusing logical
audio because of a cosmetic burst. Invalid frames leave voices and displacement
counts unchanged. Inspection exposes started, failed and displaced cosmetic counts.

The dedicated ServerSession explicitly disconnects its cosmetic receiver before
loading. This covers immediate effect-creation updates as well as later simulation;
requests without a receiver report an inspectable cosmetic error and create no voice.
The dedicated still validates, advances and retires all authoritative logical audio.

This changes cosmetic voice ownership and allocation order deliberately. The mixer
visits logical voices before cosmetic voices, instead of arbitrary interleaving by
server allocation. Its sample arithmetic is unchanged. A new standalone test compares
exact PCM for one logical and two cosmetic voices against the existing mixer path;
this is not a claim that every differently ordered floating-point sum is bit-identical.

The [source ledger](../sources/m9_effect_audio_sources.json) records the adapted
implementation. No new source gameplay code is imported.

## Verification

Linux native execution in the isolated `build/m9-fx-runtime` and
`build/m9-fx-runtime-asan` trees from the [runtime lot](m9_effect_queries.md).
Final results and commands are recorded in the
[validation record](evidence/m9_effect_audio_validation_2026-10-30.json).

- **344/344 distinct selected headless cases pass**, including 22 dedicated maps
  at 180 ticks, solo/save-load and original impact behavior. The targeted selection
  ran 28 cases; the remainder ran 317, with `network.audio_local` repeated after
  adding the dedicated receiver check.
- **8/8 ASan/UBSan cases pass** after building the initially absent receiver test.
- All five prior preservation traces are byte-identical: 240 FX, 360 audio,
  360 movement, 720 environment and 300 camera/HUD frames.
- Native scene-explosions smoke passes. Sanitized dedicated Crypt completes 60
  ticks with unavailable audio/video drivers, zero clients and zero discarded ticks.
- All **6,243 protected hashes** remain unchanged, including pre-existing PBR
  files and earlier evidence; the independent test contains no server/game symbols.

The new `network.audio_effects` executable links engine_core only. It checks
independent IDs/cursors, completion/removal, world reset, missing resources, rejected
loop/stream/local requests, descriptor namespace injection, bounded saturation,
logical priority and PCM equality. Existing collision tests now check the real
client voice and assert absence of a logical impact voice. The 22-map dedicated
simulation additionally asserts that no cosmetic receiver or voice exists.

The initial sanitizer selection included one unbuilt executable
(`network.effects_receiver`), reported as **Not Run**. After building that target,
its separate rerun passes; the initial output is retained. This was not a sanitizer
finding. Test data is synthetic except cases explicitly marked private-data.
No emulator or simulator is used. Native Vulkan smoke uses the Linux NVIDIA GPU;
this does not qualify another platform or human LAN play.

## Remaining work

Moving effects still use legacy query and child-publication entry points. Their
extraction, flashlight separation, effect network events, remaining shell services,
player admission and network gameplay continue in subsequent lots. This delivery
is a local boundary, not multiplayer acceptance. Windows/macOS/mobile/Quest and
human LAN qualification remain pending. The prior `gpu.polygrid` pixel failure,
M2-GPU-001 and M3 acceptance remain separate; no reference capture is regenerated.
