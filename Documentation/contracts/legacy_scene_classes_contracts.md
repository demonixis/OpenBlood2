# Legacy scene objects

`Explosion` follows ObjectDLL/Explosion.cpp. A `TRIGGER` schedules its authored
delay. The first update creates the configured explosion model, then the light,
shockwave and sound. Model growth uses the original exponent .4. Light radius
rises for three quarters of the duration and falls for the last quarter while
the model and shockwave fade. The optional scorch marks use independent
world-only LightFX cycles. The source's DamageRadius and MaxDamage have no
effect, and its smoke and sparks code is disabled. The authored `Duration` read
writes MinScale, as in the source. Runtime `Setup` supplies the actual duration.
The retail installation lacks the source's default `Sounds\exp_tnt.wav`.
The light, model and shockwave still run, and the sound failure is recorded.

`FireFX` follows ObjectDLL/FireFX.cpp. It finds the first named character or binds
the explicitly supplied runtime target. A bounded ring of 20 flame sprites
refreshes one slot per update. Each sprite keeps its target's velocity at creation.
The light follows the character, whose red channel increases with saturation.
Death removes the flames and retains the light for up to five seconds. Removal
of the target clears the remaining owned effects. The source's BurnTime does
not stop the fire, and this class applies no damage or smoke.

`Script` is a selected port of the original ObjectDLL scene-script interpreter.
`ScriptName` resolves only to bounded mounted data, and the complete script is
validated before any command runs. The subset covers `define`, `START`, `END`,
`WAIT`, `OBJECTASSIGN`, `OBJECTCREATE`, `OBJECTCREATENEAR`, `OBJECTCREATEMODEL`,
`OBJECTCREATEMODELNEAR`, `OBJECTCREATEFIRE`, `OBJECTCREATE_EXPLOSIONAT`,
`OBJECTCREATE_EXPLOSIONNEAR`, `OBJECTMOVE`, `OBJECTMOVETOOBJ`, `OBJECTTELEPORT`,
`OBJECTFLAGS`, `OBJECTFLAGVISIBLE`, `OBJECTFLAGSOLID`, `OBJECTFLAGGRAVITY`,
`OBJECTDIMS`, `OBJECTSCALE`, `OBJECTDESTROY`, `OBJECTGIB`, `PLAYANIM`, `PLAYSOUNDPOS`,
`PLAYSOUNDOBJ`, `KILLSOUND`, `SENDTRIGGER`, `SENDAISCRIPT` and `SENDTRIGGERNAMED`.
Cameras and cutscene/subtitle control are refused with a file and line
diagnostic. The authored StartTime starts a single pass and uses .001 seconds
for nonpositive values. The source's trigger-based start-time checker is disabled.
`END` or end of file stops the scheduler. Failed runtime commands are recorded
by line while later commands continue. Object slots keep checked handles, and
links clear on object removal.

`OBJECTMOVETOOBJ` reads object slot, duration and target slot. It accepts the
original parser's six-argument form, whose last three fields are ignored, and
the handler's three-argument form. Up to ten moves interpolate toward captured
poses every .01 seconds during `WAIT`. Rotation is skipped for cameras tracking
an object. A skipped final tick clamps to the destination, and zero duration
applies the destination immediately. The two near effect commands use the
original random offsets from target dimensions and rotation. Sound commands
replace an existing loop in the same numbered slot. One-shots expire on their
own and `KILLSOUND` stops the named loop.

`OBJECTGIB` uses the existing typed client flesh-gib effect and removes the
target even if the effect fails. The original script requested three to eight
large human gibs with upward velocity, but supplied no material flag or model
for the server `CGib` constructor. The port uses the supported client flesh
models and retains the original count and launch-speed ranges in the request.

`fire_effects.explosion` and `fire_effects.fire` use the installed game's model,
sprite and sound resources. The synthetic archive used by `script.run`,
`script.motion`, `script.effects` and `script.refuses` exercises the timer, object
mutations and triggers, interpolation and sound replacement during waits, original
model and explosion commands, and rejection of unsupported or malformed scripts.
