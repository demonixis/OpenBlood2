# M2 — Original burning deaths and dismemberment

Original lethal fire, explosive/large-hit gibbing and postmortem limb loss now
connect to visible corpse, limb and client-fragment state. The earlier fire/gib
service refusal no longer suspends these lethal NPC paths. **M2 remains incomplete.**

The original AI decisions, corpse burn-to-smoke sequence, node masks, launch
formulas and random thresholds are adapted from the Blood II reference sources.
Burning corpses use the real burned skin, torso attachments, flame/smoke textures
and looping fire WAV. Detached limbs use the original character model's limb clips;
flesh fragments use the original gib models, skins and impact WAVs. Nonsolid limbs
and client-only fragments do not obstruct the player. The existing bounded corpse
placement and independent death animation remain in force.

A separate postmortem hit query preserves CCorpse's rules. Living-NPC limb loss
remains disabled because that branch is commented out in the original source.
Fire precedes gibbing unless ALWAYSGIB is set; NEVERGIB does not disable burning.
Fragment creation failure is reported without cancelling original actor removal.
Cleanup covers deletion before transfer, deletion during fire/smoke, corpse gibbing
and world unload. The original erroneous destructor sound cleanup is replaced by
checked ownership. Read the [contracts](../../contracts/death_effect_contracts.md)
and [source ledger](../sources/m2_death_effects_sources.json) for exact rules,
source quirks and replacement physics boundaries.

The Skeleton inspector now shows burning deadlines, source/attachment/effect/voice
state, postmortem hit node, health, detached limbs and retained fragment reports.
The same model and sprite passes render the effects when the inspector is hidden.
This increment changes no dependency pins, shader artifacts, reference repositories
or commercial assets. Authored SubwayA/CityHub object accounting is unchanged.

## Verification

Private tests use original Cultist/Civilian ABC, DTX and WAV resources over an
explicit diagnostic floor. They verify lethal messages and original AI_Update,
source removal, independent corpse animation, fire and smoke timing, real particle
emission, node-mask transfer, actual postmortem hit-driven limb loss, no repeated
hidden limbs, client fragment collision/rest and repeated unload/reload. These are
isolated service/behavior diagnostics, not a complete retail encounter playthrough.

Synthetic corpse tests retain ordinary animation, placement failure, lifetime and
audio coverage. Their old blanket unsupported-fire/gib assertions are replaced
with new source-rule assertions; missing torso still fails, and a missing cosmetic
resource cannot retain the dead NPC. Fault probes found and fixed an unchecked null
source name before fragment validation and a zero-elapsed fragment update that
otherwise attempted a zero-length ray. The sustained retail Flare regression also exposed a detached limb starting inside
the train floor. Limb creation now uses the existing bounded nonsolid placement
service before its first physics step, with requested/resolved positions in the
inspector. No collision-service validation was relaxed. Limb lifetime tests track
the actual child identities, accounting for independent cleanup of the NPC's held weapon.

NVIDIA readback tests require non-background pixels from external flame and flesh
textures, rendered detached-limb triangles, identical repeated rendering without
clock advancement, and successful unload/re-entry. Regression checks also cover
existing combat/corpse clearance, character inspection and safe level transitions.
Debug and inspector-free Release each pass 96 headless tests (22 with external
retail data). The same 96 tests pass under Clang AddressSanitizer/UndefinedBehaviorSanitizer
with leak detection. Seven NVIDIA GPU tests pass in each configuration, covering the new effects and
existing lifecycle, character, combat, clearance, objectives and level-change paths.
The final Debug character run lost desktop window focus at frame 50 and correctly
failed its crouch/run checks; an unchanged targeted rerun passed. Both logs are retained.
Commands, logs and exact source/patch hashes are recorded in
[validation evidence](evidence/m2-death-effects.json).

## Remaining work

Blood-spurt particles, impact splats/pools, optional VoiceMgr kill groups, generic
CGib templates/recursive material debris, save/restore of these objects and general
client SFX eviction/fade remain unavailable. The selected client fragment solver
uses existing world rays and does not reproduce retail outside-world classification.
No complete M2 acceptance or macOS execution of this increment is claimed; Metal
continues to use the existing shared model/particle pipelines.
