# Original player screen fades

The authored SubwayA exit relay (`Trigger2`) sends `player FADEOUT` after seven
seconds, then `exit2 TRIGGER` after twelve. ExitTrigger adds its original two-second
delay. Previously the player ignored MID_TRIGGER entirely; the stationary service
trace and direct-exit diagnostics did not exercise this command. This continuation
connects the original fade branches and tests the actual relay through contact.

## Original rules and typed ownership

`CPlayerObj::ProcessTriggerMsg` reads and releases its message string once. As in
the source, only literal spaces delimit the first token; comparisons ignore ASCII
case, and trailing fade arguments are ignored. Tabs are not space separators.
Dispatch has no dead-player guard and continues to the original base callback.
An owned ScreenFade event replaces SMSG_SCREENFADE's byte for this local boundary.
The existing checked client queue rejects stale recipients and drops queued events
on disconnect; no sender/string pointer survives delivery.

Receipt starts fade-in at zero or fade-out at 255, replacing any previous fade.
Each simulation update changes the value by `100 * float(frame_seconds)`. Despite
the source's “4 second” comments, full travel takes 2.55 seconds. Fade-in clears its
flag only above 255, clamps to 255 and returns to normal scale. Fade-out clamps at
zero but retains its flag, holding black. Zero-time updates preserve the source's
strict endpoints. Invalid/nonfinite deltas fail without mutation. Direct diagnostic
steps retain the session’s existing 60-second validation bound;
interactive input remains capped at 100 ms by the frontend. Fades use the accepted
delta without introducing another clamp. Inspection/drawing never advances time.
Fresh/restored world entry and client exit clear fade state. KeepAlive does not
transfer a source fade into the destination; this is not whole-world save support.

The inspector shows mode, value, scale, received fades and player command/delivery
counts, including the last unsupported-command reason. Music and multiplayer flag commands remain **unimplemented**. The later
[player VoiceMgr continuation](player_voice_contracts.md) connects SOUND. They now increment
an explicit unsupported-player-command counter and retain their last command; receiving
them is not successful execution. Unknown tokens take the original no-op branch
and have a separate ignored counter. They are separate from the server-interface
service catalogue, whose
scope does not cover every game command. Empty commands do nothing.

## Presentation boundary

The original client applies the value through SetGlobalLightScale. The current
unlit diagnostic renderer instead multiplies the composed scene by drawing a black
SDL_GPU/ImGui background layer with alpha `1 - scale`, before the HUD and inspector.
This supplies a visible fade in Vulkan and bundled Metal-capable UI builds without
new shaders/dependencies. It also darkens sky/FX as a composed image; it does **not**
claim original per-material lighting, additive-effect, blindness/night-vision or
seeing-eye precedence. Those lighting modes remain separate. HUD and inspection
stay legible and usable; inspector-free builds use the same UI runtime.

## Verification scope

Synthetic tests cover strict endpoints, hold/restart/reversal, invalid time,
message parsing, unsupported commands, dead-player dispatch, single string release,
queued-message disposal and reentry. A private diagnostic completes the actual
intro, relocates the player to the final carriage, then uses normal movement/USE
and contact to activate Trigger2. It edits no trigger/NPC, grants no health/ammo,
and uses neither direct exit injection nor invulnerability. A diagnostic aiming
loop fights visible Cultists through original view-weapon
FIRE_KEY rather than making this uncleared carriage harmless. The delayed slots
run, including cinematic/explosion requests and the original ExitTrigger. It checks
fade timing/hold, actual NEW_LEVEL, retained health/ammo, fresh destination fade and
teardown across two runs. The relocation means this is **not** a complete level
playthrough. Existing independent mouse-combat/corpse tests cover earlier encounters.

GPU readback checks full brightness, half brightness, black and world-reset output,
UI ordering and two runtime sizes. Rendering must leave simulation state untouched.
Linux NVIDIA results do not certify a new native Metal run. M2 remains incomplete;
this resolves a reached command and exposes other command omissions rather than
assuming the earlier server-service trace was exhaustive.

## Reached smoke-budget failure

The full staggered exit initially failed at 6.783 seconds with one trail at 2,024
live particles before its next burst (3,919 total). The 2,048 per-system budget
could not retain the original emission of a continuing falling fragment. Advancing
the same authored sequence with unchanged fragment motion, emission/RNG, lifetimes
and contacts measures 10,240 live particles at peak at 60 Hz. The previous simultaneous
emitter probes did not reach this case.

The smoke-only pool now admits at most 16,384 live particles per system and 16,384
across explosion trails. The particle integrator takes an explicit bounded capacity;
ordinary callers retain their existing 2,048 default, and capacities above 16,384
fail before mutation. Per-burst sample limits (2,048), 128-system limits, endpoint
bounds, atomic validation, RNG order and visible capacity failures remain intact.
No particles are silently dropped and no effect failure is reclassified as success.
The shared sprite snapshot limit becomes 32,768 quads (196,608 vertices), allowing
bounded smoke plus other existing effects. These are replacement engine budgets,
not limits found in the original game. They allocate on demand, not at world entry.
Higher-rate or longer untested paths can still exhaust explicit limits; this is not
unbounded resource support or a general particle-performance claim.
