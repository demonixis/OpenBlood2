# Original character voices and player animation sounds

> Extended by [weapon fire-audio contracts](weapon_audio_contracts.md): Beretta/knife
> retained playback and automatic gunfire hearing now execute; source weapon voices
> survive disposable destination rehearsal. Earlier fire-audio omissions are historical.
> Required services and safe NEW_LEVEL still keep M2 incomplete.

> Extended by [AI command/hearing contracts](ai_trigger_contracts.md): player sound
> alerts, original hearing reactions and selected trigger commands now execute.
> Complete production NPCs and M2 remain open.

> The later [AI model-key continuation](ai_model_key_contracts.md) adds selected
> AI_Mgr sound dispatch. Earlier blanket AI-key audio omissions are superseded;
> full AI behavior, hearing, fire/extra and M2 acceptance remain open.

M2 remains incomplete. This extends [character animation](character_animation_contracts.md),
[audio](cinematic_contracts.md), [movement](movement_contracts.md) and
[container](container_contracts.md) contracts. Read the
[source ledger](../implementation/sources/m2_character_audio_sources.json) and
[execution evidence](../../../Documentation/Implementation/m2_character_audio.md).

## Original voice rules

CAnim_Sound retains sound roots, indexed path construction, PlaySound and PlayVoice.
A nonzero index appends the signed decimal index and .wav; zero appends neither.
The root, separator and supplied name remain literal until resource normalization.
Original 256-byte storage becomes owned strings with a 255-byte payload limit;
embedded NUL and overlong paths reject without modifying the previous root/output.
The mixer's existing 100-byte playback-path bound remains unchanged.

PlayVoice refuses another voice while the retained handle reports unfinished.
A completed or missing handle is killed/cleared before attempting the replacement.
The destructor releases a retained voice. Instances cannot copy their owning handle.
The original Silent flag remains stored and inspectable; these source helper methods
never consult it. Do not infer automatic muting from its name. PlaySound uses
attached spatial one-shots, AI_MEDIUM priority 3, caller volume, outer radius R,
inner radius R*0.1 and requested reverb. Without GETHANDLE it returns zero even on
successful playback, matching the original distinction; explicit starts/failures
and the actual mixer voice distinguish that case. Such unretained voices release
at sample completion. Retained voices stay queryable until their caller kills them.

PlayVoice and PlayPlayerSound deliberately return actual playback success instead
of the source's unconditional true after an attempt. Missing resources, invalid
radius/volume, stale owners and capacity failures allocate no successful placeholder.
This return-value adaptation must be considered when importing further AI callers.
The voice helpers are ready for those callers, but no AI is registered by this work.
Original CivilianAI/CultistAI/Gideon use class/gender-static CAnim_Sound tables
through AI_Mgr::InitStatics; their voice handle can therefore be shared. Preserve
that caller ownership and the AI-specific Sparam/camera/script gates when importing
them; the current player sound-key path does not implement those AI branches.

## Player key and footstep flow

The base character callback now dispatches virtual OnStringKey before recording
unhandled commands. CPlayerObj preserves case-insensitive play_sound recognition.
Ordinary tokens receive .wav unconditionally, then the Caleb sound root established
by SetCharacter. Extra arguments do not change the source request. The player does
not forward other keys to the base character's RotY/velocity/dimension branch;
those commands remain explicitly unhandled and inspectable.

A player sound first kills the current player sound, even when its replacement
fails. Volume clamps to 0–100; priority is PLAYER base 4 plus the caller modifier.
Animation sounds request 1000/100 outer/inner radii at full volume; footsteps use
400/40 at 70%. They remain attached, with a retained handle. Original update-time
completion polling and destruction release it. FileStream requests fail explicitly.
The original post-attempt SendSoundTrigger operation still lacks AI lists/receivers:
an unavailable-operation counter records each attempt; it is never reported as
successful hearing or message delivery. Voice-manager groups, pain/death events,
landing sounds, weapon audio and full combat reactions remain pending.

The source disables footsteps below **50 units/second of full 3D velocity**, not
horizontal speed alone. Exactly 50 is enabled. Unknown materials do not play a
fallback sound, interrupt an existing sound, or alternate the foot. Supported
materials are wood, flesh, glass, metal, plastic, stone, terrain, liquid/water and
cloth/carpet. Successful name selection alternates left/right before playback,
including when playback subsequently fails. Caleb uses leftm.wav/rightm.wav;
other character profiles still reject entry. There is no random numbered variant.
The supplied archives contain 34 of the 36 male/female material/side paths;
carpet/leftm.wav and carpet/rightm.wav are absent and remain playback failures.

At each footstep, the player queries **position.y - (half-height - 1)**. The first
point container overrides the last ground material if IsLiquid accepts its code
(Water/Blood/Acid/FreeFall). This original branch does not inspect Hidden; therefore
a hidden first liquid still produces water footsteps and masks later containers.
The fresh bounded point query uses existing exact closed shapes and stable slot
order, excludes pending/stale objects, and never substitutes body/head membership.
The retail overlap order remains unverified.

## Surface metadata and ownership

The v56 DAT surface's final uint16 is now retained as texture_flags, separately
from its render/collision flags. Reference readers label this field TextureFlags;
the later engine confirms GetPolyTextureFlags returns baked surface material flags.
Private validation compares this field to external DTX user flags: 2,163 of
2,723 SubwayA surfaces match; the other 560 have baked zero despite nonzero DTX
metadata. All 1,853 nonzero baked values match the corresponding DTX flags.
Baked flags remain authoritative; they are never backfilled. No texture
pixels, header service, DTX decoder or guessed texture-name classification is added.
The original field is carried through collision triangles and their transformations.
Support queries take nonzero polygon material first; an ordinary object's high
UserFlags byte is the original client fallback when polygon material is zero.
Box-only support cannot invent a polygon material. Unknown numeric values remain
unknown; the player wire-equivalent field remains a byte as in the source.

MoveMgr updates last surface when it observes support, before its steep-slope
rejection, and retains the value in air. WritePositionInfo sends that byte through
the typed local movement message. The player commits it only after the existing
association, move-code, finite-data and collision checks succeed. Unlike the
original legacy handler's early byte assignment, a stale rejected message cannot
change surface state. Movement still integrates once; this addition does not
change collision geometry, friction, damage materials or the replacement solver.

PlayCharacterSound requires a live owner equal to the attached speaker, a spatial
nonlooping request and the existing real WAV decoder/budgets. Hidden/inactive live
objects can play; pending/stale owners cannot. Attached audio follows later motion;
removal releases it on the next audio sweep, with retained character handles also
released by their destructor. Unload destroys game objects before clearing audio.
Handles remain globally monotonic, so old destructors cannot kill a new voice.
Reverb, priority arbitration, occlusion and retail acoustic parity remain absent.

## Inspection and verification

The Skeleton tab displays root, Silent metadata, voice/player attempts, starts,
failures, busy refusals, actual resource/cursor/attachment position, source and foot
materials, point-container code, next foot, speed-suppressed/unmapped keys and
unavailable AI alerts. Counts never replace decoded voice evidence. Unknown keys
remain in the Session panel. State updates independently of ImGui.

Synthetic checks exercise path bounds, busy/completed/missing handles, helper
ownership, hidden/inactive/pending lifetime, capacity reuse, real model callbacks,
interrupt-before-failure, source suffix behavior, volume/priority/radii, the speed
threshold, all footstep mappings, liquid precedence, surface metadata transport,
object fallback and stale movement rejection. Private checks inventory Caleb's
actual model keys and original footstep WAV paths, compare baked materials, and
walk the real SubwayA/player through two entry/unload cycles. The GPU Skeleton
scenario requires actual attached footstep advancement and SDL sample submission
while the inspector is hidden and after reload (dummy audio driver).

SubwayA remains 187 live-created source entries, 4 transferred world sounds,
372 editor-only, 17 unsupported and zero failed, plus 7 SelfTriggers. AI classes,
ExitTrigger, actual client FX, attachments and required services keep M2 open.
--start-game must still fail. Linux-only testing does not establish retail binary,
other-platform, XR or audible-device equivalence.
