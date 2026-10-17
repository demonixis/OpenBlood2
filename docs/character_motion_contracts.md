# Original character motion keys

> The later [AI model-key continuation](ai_model_key_contracts.md) adds selected
> AI_Mgr sound dispatch. Earlier blanket AI-key audio omissions are superseded;
> full AI behavior, hearing, fire/extra and M2 acceptance remain open.

M2 remains incomplete. This imports the motion branch of original
CBaseCharacter::OnStringKey and a bounded Sparam_Get lookup. It is a prerequisite
for character integration, not a new AI implementation. CMovement path steering,
AI_Mgr states, hearing, fire/extra keys, attachments and ExitTrigger remain absent.
See [source ledger](m2_character_motion_sources.json) and
[execution evidence](../../Documentation/Implementation/m2_character_motion.md).
The [object](object_contracts.md), [model/collision](model_and_collision_contracts.md)
and [character audio](character_audio_contracts.md) contracts still apply.

## Source rules

Only argv[0] is examined. Sparam looks for the first exact, case-sensitive
`[name:value]` substring and takes the bytes through the first closing bracket.
Repeated names retain the first value; lowercase aliases are not invented.
Existing model command tokenization remains unchanged: semicolons separate
callbacks, quotes preserve a word with spaces, and unquoted extra words are not
merged into argv[0]. The source's six independent branches execute in this order,
regardless of their textual order:

| Branch | Observable action |
| --- | --- |
| RotY | Rotate around the character's current local up axis, in radians |
| ForwVel | Replace velocity with current forward times the value |
| RightVel | Replace velocity with current right times the value |
| UpVel | Replace velocity with current up times the value |
| AddX, AddY, AddZ | Add a world-space position delta through checked MoveObject |
| DimsX, DimsY, DimsZ | Set half dimensions, preserving the object center |

Velocity branches overwrite rather than add. Rotation precedes velocity selection.
The existing engine quaternion/vector helpers replace the old math API calls;
later LithTech EulerRotateY/RotateAroundAxis and matrix math confirm local-up
composition and positive-angle convention. This is a source comparison, not a
Blood II retail executable comparison. No second integrator is introduced.
The established session order remains physics, then animation callbacks: a velocity
key affects subsequent physics, while AddXYZ invokes collision immediately.

AddXYZ preserves the existing swept/slide solver. A successful clipped movement
counts as an applied action; actual position/contact shows how far it moved.
DimsXYZ uses the existing raw SetObjectDims operation, not player ResizeObject:
it neither anchors the feet nor checks new-shape clearance. Zero dimensions are
allowed; negative/nonfinite/out-of-range dimensions fail. Collision response and
raw resize behavior are not claimed retail-equivalent.

CPlayerObj's override continues to handle its own play_sound/footstep keys and
**does not forward to the base motion handler**, matching the commented-out base
call in the original player source. Existing MoveMgr movement remains unchanged.
Unknown commands remain bounded inspection records, never successful services.

## Bounded adaptations and failures

Sparam_Get now returns Missing, Found or Invalid and a borrowed string_view on
success. Output remains unchanged on failure. Input is limited to 4096 bytes,
IDs to 252, values to 255; embedded NULs, missing/empty closing values and oversized
inputs fail. A fixed stack marker replaces Windows formatting/substring functions
and unbounded destination copies. Unused Sparam_Add/Remove are not imported.

Motion numbers have a 31-byte bound matching the old 32-byte scalar buffer.
Locale-independent parsing accepts finite signed decimal/scientific values,
including a leading plus and whitespace within a quoted word. It rejects junk
suffixes, NaN/infinity, overflow and magnitudes beyond ten million original units.
This deliberately replaces atof's permissive fallback and the source's buffer
writes. AddXYZ and DimsXYZ require all three components; the source could otherwise
read uninitialized Y/Z buffers. All present motion parameters are validated before
any action, including nonnegative dimensions. Malformed bundles apply nothing.

Successful actions retain source order. A service failure stops subsequent actions
and leaves earlier successful actions in place; there is no rollback across touch
callbacks. SetObjectDims now returns a checked DRESULT and rejects pending/stale,
container/world-model and invalid dimension requests. Existing callers may ignore
that return; creation initialization still succeeds on an Alive object. If AddXYZ
triggers removal, dimensions and later animation commands cannot mutate the dying
object. Destruction remains deferred until the callback chain finishes.

The inspector retains the last bounded key, applied bit mask (bits 0–5 in the
above order), total keys/actions/failures, explicit failure text, current position,
velocity and dimensions. A recognized failed key is visible as a failure; it is
not counted as an unknown command. Inspection has no execution side effects.
Hidden/inactive character records remain selectable; inactive objects do not
advance model clocks. No UI is needed for dispatch or cleanup.

## Verified content and tests

The seven required-character assets examined contain 326 clips and 193 nonempty
keys: 46 plain Caleb footstep keys and 147 bracket keys in civilians, cultists,
labtech and Gideon. **None contains a motion key.** Private tests dispatch every
actual marker through the base character callback, retain each unavailable key,
and verify zero motion mutations and clean destruction. This does not make the
AI sound/fire/extra handlers available or replace the player's sound path.

Synthetic tests cover duplicate/case lookup, bounded malformed values, order and
velocity replacement, tilted local-yaw rotation, collision clipping and overlap,
centered dimension changes, partial service failure, touch-driven self-removal,
subsequent-key cancellation, hidden/inactive lifetime, immutable model data and
the original player override exclusion.

`--skeleton-smoke` retains the existing real player locomotion, node, audio and
unload/re-entry assertions. It also creates one explicitly labelled **Synthetic
motion probe (not AI)**, using a private in-memory model copy with synthetic keys
far from world actors. No archive or cached model changes. Restarting that probe
at UI hide must deliver a second motion key while hidden; reload must invalidate
its old identity and run a newly created probe. Its extra runtime object is only
in this smoke scenario, not source-entry accounting or normal startup.

The SubwayA gate is unchanged: 187 created, 4 transferred sounds, 372 editor-only,
17 unsupported, zero failed, plus 7 SelfTriggers. The missing source instances
remain 5 CivilianAI, 10 CultistAI, Gideon and ExitTrigger. Required client FX and
other services remain unavailable. --start-game must still exit 6. Other platforms,
XR and retail gameplay/physics parity are not validated by this increment.
