# Original Spawner

`Spawner` is the selected original `ObjectDLL/Spawner.cpp` class. The global
`SpawnObject` helper it calls was ported earlier for destruction spawns; see the
[pickup contracts](pickup_contracts.md). Read the [object](object_contracts.md)
and [trigger](trigger_contracts.md) contracts before extending it.

## Properties and initialization

The parent is B2BaseClass (OT_NORMAL), so the original difficulty and game-type
filters apply. Registration defaults: SpawnObject "", MinVelocity/MaxVelocity
(0,500,0), Sound "", SoundRadius 500, UseTriggerObjPos true, StartActive false,
RespawnCount -1, RespawnRate 1, CreateRiftEffect false.

Properties are read for **world files only**. String and runtime creation keep
the constructor defaults, which differ from the registration: velocities
(-100,400,-100)..(100,600,100), UseTriggerObjPos false, no spawn string. The
count is a source `DDWORD`: -1 becomes 0xFFFFFFFF.

Initial update schedules `RespawnRate` when StartActive, otherwise disables
updates, and sets USRFLG_SAVEABLE. Save/Load follow the original block ([save/load lot 7](save_load_contracts.md#implemented-world-logic-lot-7));
a running spawner keeps its schedule after a load (decision-4 departure). Nonfinite or out-of-range velocities (existing physics vector bounds),
SoundRadius or RespawnRate outside 0..1e6 reject creation; the source schedules
and draws from unchecked values.

## Commands and updates

MID_TRIGGER compares the whole string case-insensitively (`_mbsicmp`):

- `START` schedules `RespawnRate`; `STOP` disables updates. Neither spawns.
- `SINGLE` decrements and spawns at the spawner's own pose only when the
  unsigned count is above zero, even with UseTriggerObjPos.
- Any other payload spawns, at the sender's pose when UseTriggerObjPos and a
  sender exist, otherwise at the spawner's pose.

The read HSTRING is released exactly once. An update decrements a count of at
least one, spawns, then reschedules unless the count is now zero. A zero count is
not decremented, so an active spawner with count 0 spawns once and stops. -1
effectively repeats (0xFFFFFFFE after the first spawn). START rearms an exhausted
spawner for one more spawn.

## Spawning

An empty spawn string makes no attempt, no sound and no report. Otherwise the
velocity is drawn x, y, z in that order as `min + (max - min) * unit`, the linear
interval formula already used for reversed endpoints in the
[AI runtime](ai_runtime_contracts.md). The existing helper validates the class
and the property string (pickups plus Fanatic and Prophet), creates
the object at the chosen pose and applies the velocity. Children are independent:
removing the spawner never removes them.

After creation the source sends a rift explosion (`SFX_EXPLOSIONFX_ID`,
`EXP_RIFT_1`) when CreateRiftEffect is set. No client rift effect exists yet: the
request is counted and named in the spawn report, never presented as success.
150 retail spawners set it. Then the source plays Sound with
`PlaySoundFromPos` (3D, reverb requested, inner radius = radius * 0.1,
SOUNDPRIORITY_MISC_LOW) at the spawn position, even when creation failed; the
port plays a detached voice there and appends any playback failure to the report.
A missing spawn pose skips creation and sound with an explicit outcome.

Every attempt records a bounded `SpawnReport` (source name, request, position,
velocity, child token, outcome) shown in the Pickups inspector. The spawner keeps
trigger/attempt/success/failure/rift counters, the last child token and voice.

## Evidence and limits

Retail 05_CDMB has two Spawners (`spawn1`, `spawn2`) spawning
`Fanatic` at (0,500,0), count -1, inactive. Both create, and a trigger now creates a
ready [Fanatic](fanatic_prophet_contracts.md): `Fanatic` and `Prophet` joined the
supported spawn classes. Tests:
`spawner.properties`, `commands`, `respawn`, `spawn` (synthetic pickups and WAV),
and `spawner.private` (05_CDMB).
The rift FX, save/restore and retail velocity-random parity remain unavailable.
See the [source ledger](../implementation/sources/cdmb_world_sources.json).

SpawnObject strings follow the original MAX_GP_STRING_LEN (200 bytes) of
GenericProp and the `szSpawn` buffer, for Spawner and the destructable aggregate.
24_AncientCity and 24B_Arena author 112–141-byte requests; their Spawners are now
created. Requests naming unported classes (UndeadGideon, the Revenant Chosen)
still fail when triggered.
