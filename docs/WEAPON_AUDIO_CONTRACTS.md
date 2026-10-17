# Original weapon fire audio and hearing

> Extended by [weapon impact contracts](WEAPON_IMPACT_CONTRACTS.md): original multi-hit
> material rules, selected impact audio and automatic impact hearing now execute.
> Earlier impact-audio omissions are historical; visuals and the M2 gate remain open.

Selected `CWeapon::PlayFireSound`, the sound loop in `Update`, and `KillSounds`
now execute for the original Beretta and Chosen Knife. See [source ledger](M2_WEAPON_AUDIO_SOURCES.json)
and [validation](../../Documentation/Implementation/M2_WEAPON_AUDIO.md).
This extends [weapon contracts](WEAPON_CONTRACTS.md), [character audio](CHARACTER_AUDIO_CONTRACTS.md)
and [AI trigger contracts](AI_TRIGGER_CONTRACTS.md); other weapons and M2 remain incomplete.

## Original behavior

After accepting ammunition, `Fire` attempts the selected primary/alternate WAV,
then calls original `SendSoundTrigger(SOUND_GUNFIRE)` before vector impact/damage.
An absent sound file does not suppress the hearing alert or invalidate the shot.
An empty filename skips both sound and alert. The two supported definitions have
empty dry-fire filenames; dry attempts produce neither a fabricated click nor an alert.

Playback is attached to the weapon owner, spatial, non-looping, volume 100%, with
outer/inner radii 2000/200. Player priority is 6 and NPC priority 4. Existing typed
WAV decoding and SDL mixing implement playback; priority is retained intent, not
voice arbitration, and requested reverb remains dry. The alert uses the original
shot position, distinct from the attached voice's current owner position. Existing
faction order, duplicate recipients, hearing boundaries and class guards are unchanged.

Each weapon owns four retained voice IDs. A successful new voice takes the first
empty slot, otherwise the earliest expected end (`GetTime + decoded duration`);
strict comparison selects the first slot on ties. Failure to create the new voice
leaves all old slots untouched. Duration failure kills only the new voice and is
reported. Completion is checked by `IsSoundDone`, not the estimated end deadline;
`UpdateSounds` kills completed voices and clears IDs while retaining historical
end times, as in the source. Original inventory updates visit every right/left slot,
including unselected weapons. Weapon destruction kills all its retained voices.

## Checked lifetime adaptations

The existing collision preflight still rejects unavailable queries before consuming
ammo. After an eligible hearing callback, final rays are refreshed before damage,
even when that callback reports a message failure: it may already have moved or
removed an object. No random values are resampled. Post-callback query failure is
explicit and cannot undo already accepted ammo/audio/hearing. If a callback removes
the shooter, the accepted shot records `interrupted`, clears its hit, skips impact,
and releases the newly created voice. Existing deferred destruction and firing
reentrancy guards keep the inventory/weapon alive throughout dispatch.

IDs are monotonic checked identities. Inspection resolves them each frame, including
completed, empty and unresolved slots; a historical ID is never dereferenced as a
pointer. Counters distinguish attempts, created voices, failures, replacement and
release requests. Audio and hearing errors are separate. Missing assets remain
failures, never successful placeholder sounds. All added state belongs to its
weapon/adapter; no mutable original global is introduced. Temporary destination
rehearsal preserves source voice IDs, cursors and weapon slots; copied destination
inventories start with no source-session fire voices.

## Presentation and limits

The Weapons inspector and selected AI weapon panel expose paths, counters, hearing
results, four slots, completion, expected end times, cursors and priorities. They
never advance audio. Sound and NPC hearing continue with the UI hidden and in the
inspector-free build. Existing client `SOUND_KEY` paths remain indexed references;
this increment starts one authoritative server voice per accepted local shot and
does not add duplicate client playback or implement multiplayer prediction.

Other weapon definitions, surface impact sound/FX, full reverb, priority arbitration
and retail audio/backend parity are not implemented by this change. A diagnostic
direct `PlayFireSound` call bypasses shot input by definition and must not be counted
as gameplay through an active cinematic. Required world services, Gideon, safe
NEW_LEVEL and actual M2 acceptance remain open.

The [shield continuation](SHIELD_WEAPON_CONTRACTS.md) reuses retained original WAV/hearing for both modes. Its visual-only FireVector has no damage ray or collision preflight; armor remains a separate boss action. All checked hearing/voice lifetime rules remain in force.
