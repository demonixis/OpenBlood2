# Safe NEW_LEVEL ownership transfer

The [CityHub entry continuation](cityhub_entry_contracts.md) supersedes earlier
missing-named-world-model and CityHub refusal statements: absent named BSPs are
explicitly omitted before initial update, with no runtime instance. All other
preparation failures retain their checks. Later simulation and the M2 gate remain
separate from successful NEW_LEVEL initialization.

`GameSession::queue_level_change` and `process_level_change` connect original exit
messages to the existing player snapshot and KeepAlive Load paths. This is a
replacement engine transaction, not retail Keep.sav serialization. M2 remains open.

## Boundary and sequence

The original client dead-player guard remains in effect. The message pump copies
NEW_LEVEL and the server's start name into one owned pending request. Later requests
coalesce to the final intent. No HSTRING, object pointer or saved player reference
is queued. The application processes the request after simulation and before
rendering; object callbacks and foreign session bindings cannot commit it.
The in-place `load_world` API still rejects NEW_LEVEL because it cannot replace the
owning session. Inspector commands use the same queue as original exit messages.

1. Consume the intent once. Validate syntax/start name, capture the supported living
   player, retain current difficulty (the NEW_LEVEL packet's difficulty does not
   replace it), and check world-revision exhaustion.
2. Suspend the source original globals using the same boundary as
   [destination rehearsal](destination_trial_contracts.md): seven fixed-address
   intrusive heads/counts, active camera, five class/gender AI table owners and the
   server binding. The source runtime and all its object addresses remain alive.
3. Allocate a separate session at a stable address and copy only the mounted resource
   index. Run normal DAT/collision/schema loading and original initialization.
   Unsupported classes, failed entries and calls to generic unavailable services
   reject the candidate. No destination simulation tick runs during preparation.
4. Restore player engine state and original aggregate/player fields using
   [KeepAlive entry](player_keepalive_contracts.md). Preserve raw stats, attributes,
   caps, weapons, ammo and supported items. Original type-1 inventory Load removes
   keys. Reassociate a new client, run named-start selection/fallback and entry
   triggers, then validate destination placement. Never call fresh Respawn.
5. Reject recursive level requests during entry, failed creation/services, missing
   models, or unavailable world/model/view-weapon/SPR-frame images. Decode actual
   DTX data; indexing alone is insufficient. The existing world mesh bound and a
   combined 4,096-image / 128 MiB decoded-image limit bound preparation. Hidden
   runtime models/sprites are checked too. A failed candidate is destroyed under
   its own adapter before restoring the source globals and binding.
6. Suspend the complete candidate, resume the source, flush the old queued PCM and
   move SDL audio-output ownership. Voices remain with their worlds: old sounds
   end, candidate sounds survive. Output transfer is the last fallible preparation.
   Destroy the source under its own adapter, activate the candidate and replace the
   `unique_ptr`. A session itself is never moved, reconstructed or replayed.
7. Advance the world revision monotonically. The application clears old selections,
   queued object commands and input ownership; the existing revision boundary
   rebuilds world/model/sprite/FX presentation. New independent ObjectId namespaces
   must never validate a stale UI selection by coincidence. Preparation and first
   presentation upload time are excluded from the next interactive simulation delta.

Rejection retains the source owner, player/client identity, objects/NPCs, clocks,
list links, shared tables, voices and current revision. Capture/inspection counters
and monotonic audio ID allocation are not rewound. Source queued hardware audio may
continue during synchronous preparation. Successful transfer intentionally discards
source objectives, cameras, transient effects and world audio; original client
world-exit cleanup also clears these. This is not cross-level world-state saving.

## Inspection

Session offers **Change level safely with current player** beside destination/start
fields. Session and Objectives show pending intent, cumulative attempts/commits/
rejections, source/destination revisions, requested/selected start, fallback, capture/
load/entry/cleanup observations, decoded-image count, restored stats, creation rows
and unavailable services. Reports own their strings and remain historical after
unload; drawing never executes a transition. A partial destination remains available
through explicit diagnostic load/rehearsal, not through a forced successful exit.

## Verification and limits

`world.level_change` covers callback refusal without consuming intent, malformed
names, coalescing, missing DAT/start/ground/texture/model/audio, unsupported classes,
recursive entry exit, repeated ownership promotion, difficulty retention, raw level-6
attributes/caps, 37 HP, 23 armor, 1,234 bullets, 43 medkit charges, key removal, active
arrival camera, post-entry update and teardown. Synthetic resources are generated
fixtures, not commercial content or accepted substitutes in production.

`world.level_change_private` repeats real SubwayA -> rejected CityHub -> new SubwayA
three times, checking exact retained lists, source identities, voice cursors, clocks,
restored stats and revision changes. `gpu.level_change` sends original typed exit
messages, renders world/models/alpha before and after promotion, reads back pixels
and checks actual SDL output transfer and resumed PCM submission. Existing application
objectives smoke covers hidden-inspector authored CityHub exit/refusal and reload.

CityHub currently has 435 created source entries, 31 transferred world sounds,
541 editor-only entries, 18 unsupported and one failed. Its authored exit remains
blocked; no stub or class omission is counted as success. Same-world SubwayA transfer
proves lifecycle promotion with retail resources, not campaign progression.
Initialization/resource acceptance does not certify later AI actions, missing FX,
all future resource requests, autosave, whole-world restoration, multiplayer or
complete M2. GPU allocation/driver failure remains an application error after CPU
promotion; the transaction does not claim rollback of device loss. Synchronous
loading briefly holds both worlds in memory; asynchronous loading is not added.

See [evidence](../../../Documentation/Implementation/m2_level_change.md) and
[source ledger](../implementation/sources/m2_level_change_sources.json).

The [remote-bomb continuation](remote_bomb_contracts.md) adds weapon50 in reserved
slot11, ammo11/item8, RemotesPU, R selection and the separate B detonation command.
Charged/secondary throws share the source sequence without proximity overhold
explosion. Listed remotes belong to player teardown/Respawn; already signalled
charges have left that list. Restore/KeepAlive preserve weapon/ammo, never old bomb
handles. The shared model-projectile solver now admits CGrenade, CProximityBomb
and CRemoteBomb. Earlier remote omissions are historical; time bombs and the
complete M2 gate remain open.

The [player VoiceMgr continuation](player_voice_contracts.md) preserves unique
reply flags and client/group random history only after successful preparation.
Old voice handles/deadlines are never transferred; rollback leaves them intact.
