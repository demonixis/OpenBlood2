# Selected M2 acceptance and platform qualification

The selected acceptance path is original SubwayA startup, intro, player combat,
contacts, authored exit and preserved entry into CityHub, followed by teardown.
CityHub's initial objects/resources and ten stationary simulated seconds have
separate coverage. This is not complete CityHub traversal, campaign support,
retail behavior equivalence or general world persistence.

The [final acceptance review](../../../Documentation/Implementation/m2_acceptance.md)
is the current decision record. Historical increment notes do not override it.
Keep functional session results separate from process-wide GPU sanitizer results.
Do not label a process that exits for LeakSanitizer as passing merely because its
last gameplay assertion succeeded. Do not invent additional gameplay requirements
to repair a leak reproducible without the game.

## Route evidence

`train_session.private_30`, `train_session.private_60` and the NVIDIA variant use
`GameSession::run_world` before the original intro. They recheck the restored
CityHub player through the same startup boundary. Input, damage, contacts,
FIRE_KEY events, delayed relay delivery, fade, original NEW_LEVEL and cleanup keep
their existing assertions. No teleport, direct damage or stat grants are added.
The synthetic mouse driver is not a human playtest or a retail comparison.

The route now checks selected live actor/weapon errors and the client/FX/audio
reports in addition to the complete service-attempt catalogue. It samples those
reports after each step, before their bounded histories can normally retire, and
at transition/teardown checkpoints. This does not make the service catalogue an
exhaustive error log: source callbacks can remove objects within a step, and
histories remain bounded. Existing focused destruction and lifetime tests cover
those callback boundaries. Failed render preparation and actual GPU checks remain
separate from CPU-side effect admission.

## Original missing sound

Original `ClientShellDLL/ParticleExplosionFX.cpp` requests
`sounds/gibs/flesh/gib_impact%d.wav` with `GetRandom(0,NRES(5))`; the selected
B2_LITE profile maps `NRES(5)` to 3. The installed game lacks variant zero.
The port must preserve this source choice and its actual audio refusal, without
renaming another sound or returning a fake successful voice.

The requested path now accompanies the error in the live emitter, retained report
and Sprites inspector, including after the emitter expires. It uses the existing
64-report bound, clears on world unload and changes no draw, collision, random
sequence or damage behavior. Missing impact audio does not cancel the independent
blood splat or turn a cosmetic emitter into a failed game update.

The selected route distinguishes this one original content anomaly only when the
exact path is requested, the resource is absent and the audio result is the
existing missing-resource refusal. Other paths and decode errors are rejected.
The report prints the number of distinct observed affected events separately;
these are failed requests, never decoded starts. This exception exists only in
the acceptance test, not the audio service or world-admission rules.

`blood_spurts.missing_sound` verifies missing-zero selection alongside successful
variants, actual voice counts, independent splats, retained expiry history and
cleanup. `train_session.audit` is a negative control: an unrelated missing sound,
a decode error, an FX failure counter and a voice failure must all reject the
route audit. Its intentional rejection diagnostics are not production failures.

## External GPU result

Keep [GPU lifetime contracts](gpu_lifetime_contracts.md) and their unsuppressed
NVIDIA/D-Bus reproduction attached to the acceptance decision. No graphics pin,
shader, skinning strategy or renderer lifetime policy changes in this review.
Graphics optimization is being handled separately; merge and revalidate only
after the user reports that the work has been pushed. No native macOS/mobile/XR
validation is implied by Linux tests or the user's successful Mac run.

The selected Beretta/knife asset inventory contains only FIRE_KEY events. Original view-model SOUND_KEY sends a real CMSG_WEAPON_SOUND request; the port's indexed reference does not implement that path. Other view-key commands and assets remain outside the selected route, even though their future implementation must preserve original message/audio rules.
