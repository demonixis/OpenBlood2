# Isolated destination initialization rehearsal

> Extended by [weapon fire-audio contracts](WEAPON_AUDIO_CONTRACTS.md): Beretta/knife
> retained playback and automatic gunfire hearing now execute; source weapon voices
> survive disposable destination rehearsal. Earlier fire-audio omissions are historical.
> Required services and safe NEW_LEVEL still keep M2 incomplete.

This extends [structural destination preparation](DESTINATION_CONTRACTS.md) with
actual original object initialization and [named keep-alive player entry](PLAYER_KEEPALIVE_CONTRACTS.md)
in a temporary session. It then destroys that session and resumes the source.
**It does not execute, accept or commit a level transfer. M2 remains incomplete.**

## Scope and sequence

`GameSession::rehearse_destination` is a synchronous, explicit diagnostic. It
requires the currently bound session, an idle callback dispatcher and a capturable
supported living player. A foreign or callback invocation fails without replacing
the last report. Request parsing and start-name limits match the existing APIs.
No user-controlled filename reaches a shell or bypasses the resource index.

The current difficulty is validated and retained. The temporary session copies the
mounted resource index, constructs its own caches/objects/clients/audio, and uses
normal fresh-world construction for the destination geometry and original world
objects. It then restores the captured player through KeepAlive entry, including
original key removal, named/fallback start selection, start triggers, ground query
and final placement. The active source player is never respawned or replaced.
No destination simulation tick or `RunWorld` occurs. A partial inspection world's
successful player entry remains observable even when other source entries fail;
that boolean is never a readiness or acceptance decision.

All returns after suspension are confined to an inner scope: temporary original
objects, shared tables, voices and adapter are destroyed before source binding
returns. No old source handle is passed to the destination, and no destination
handle survives in the owned report. Source object/client IDs, simulation time,
world revision and current state remain intact. Resource index metadata is copied;
commercial payloads stay external, although normal destination reads/decode allocate
separate temporary CPU caches. There is no staging GPU scene or SDL audio output.

## Original globals and lifetime

The old game still uses process-global intrusive sentinels and tables. During this
single-threaded diagnostic only, suspend and exchange exactly these runtime globals:

- Cabal and Monster faction heads/counts;
- destructible, smell, exit-hint, ammo-pickup and camera heads/counts;
- active camera handle;
- the five ported class/gender AI animation/sound table owners (including Gideon);
- active server adapter binding and `g_pServerDE`.

Sentinel addresses do not move. Source nodes keep their original endpoint addresses
and are never traversed while another world is bound. Destination constructors see
empty lists and tie off the same fixed sentinels. After destination destruction,
restore the saved head values/counts; this reconnects the untouched source chains.
Retain shared animation tables, including voice ownership, until source restoration.
Destination table destructors must run under the destination adapter. Ordinary
second-session initialization remains forbidden; this is not concurrent-session or
thread-safe support. Every newly imported mutable game global must join this audit
before it can run during a rehearsal. Immutable class registration/property strings
are shared and are not exchanged.

The process-wide audio ID allocator deliberately advances and is never rewound;
resetting it would reuse IDs. Existing source voice identities/cursors are retained.
Source gameplay clocks do not advance, but an already queued hardware audio buffer
may continue playing while synchronous preparation runs. Queries may populate source
capture diagnostics/caches; the contract is not a bitwise rollback of all counters.

Cleanup checks binding release, all seven empty list counts and active-camera reset.
Lifetime tests additionally compare every source intrusive node's address, previous,
next and data pointers, all shared table owners, and live shared Cultist/Gideon voices.
No destination failure reloads the source DAT or reconstructs its NPCs.

## Inspection and evidence

The Session panel provides **Rehearse destination entry; retain current session**.
It queues execution outside UI drawing. Session and Objectives display owned source/
destination revisions, requested/selected start, loaded/entered/cleanup observations,
creation outcomes and details, restored stats, runtime object/attachment/voice counts
and active-camera observation. Creation rows are captured before entry triggers;
runtime counts follow entry. Reports remain historical after source unload/reload.
The existing automatic blocked-exit action remains the cheaper structural scan.

`world.destination_trials` covers successful entry and start-camera activation,
missing marker/ground/resource/data, source key preservation with destination key
removal, repeat attempts, foreign-session rejection and report lifetime. Existing
`world.destination_lifetime` also rejects callback rehearsal. Private tests repeat
SubwayA -> temporary CityHub/SubwayA/malformed Bridge -> unchanged SubwayA three
times, with 16 production NPCs and diagnostic live shared Cultist/Gideon voices. The GPU
objectives scenario executes CityHub entry and cleanup with hidden UI and reload.
See [source ledger](M2_DESTINATION_TRIAL_SOURCES.json) and
[implementation evidence](../../Documentation/Implementation/M2_DESTINATION_TRIAL.md).

A rehearsal cannot promote the staged runtime, preserve all client presentation
state, guarantee subsequent updates or make an incomplete destination playable.
Automatic NEW_LEVEL, complete boss encounter/death behavior, required remaining FX/illumination and other required
services still block M2. This isolation is new engine adaptation, not recovered
retail transition code or proof of retail behavior equivalence.

The fifth-table extension is documented in [Gideon runtime contracts](GIDEON_RUNTIME_CONTRACTS.md).
