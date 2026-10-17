# Original Player Entry Boundary

> The [compact HUD continuation](hud_contracts.md) now displays actual current ammunition and supports numeric owned-slot commands. Earlier blanket HUD omissions remain historical; original artwork and the M2 gate are still incomplete.

> The [smell-trail continuation](ai_smell_contracts.md) now supplies periodic player hints and the original SearchSmellTarget body. Its source guards do not automatically seed a tracking cursor. Earlier blanket smell omissions are historical; M2 remains incomplete.

> Continued by [player restore contracts](player_restore_contracts.md): supported
> engine state and original restored-client association now execute in the same
> loaded world. Earlier blanket omissions below are historical. Full client/world
> state and safe NEW_LEVEL transfer remain incomplete.

> Extended by [node visibility and player model restoration](character_node_contracts.md):
> original named resets now affect rendering/hit candidates, and the selected Load
> model/flags/velocity tail is available before client association. Earlier blanket
> node-hide/Load-tail omissions are historical. Full engine/client restore, limb/gib
> behavior and the M2 transfer gate remain incomplete.

> Continued by [player state contracts](player_state_contracts.md): selected player
> values and the original savegame initialization early return now execute.
> Earlier blanket player-field/initialization omissions are historical; full
> engine/client/player restoration and level transfer remain unavailable.

> Continued by [damage state contracts](damage_state_contracts.md): original damage
> Save/Load values and death counter/delay/nigh-invulnerability now execute.
> Earlier blanket damage-save omissions are historical. Whole player restoration
> and safe level transfer remain unavailable; M2 stays open.

> Extended by [inventory state contracts](inventory_state_contracts.md): supported
> original inventory Save/Load and exit-time capture now execute. Full player
> preservation/association and safe destination entry remain unavailable; NEW_LEVEL
> is still refused and M2 remains open.

> Extended by [level-exit contracts](exit_contracts.md): original ExitTrigger and
> shell exit/load messages now execute. NEW_LEVEL still explicitly refuses missing
> keep-alive transfer before teardown; this is not a completed level transition.

> Extended by [AI perception contracts](ai_perception_contracts.md): original
> candidate lists and stimulus/target selection now execute. Automatic NPC states
> and complete M2 remain unavailable; earlier acquisition omissions are historical.

> Extended by [character audio contracts](character_audio_contracts.md): original
> voice helpers, player sound keys and material/liquid footsteps now execute.
> Earlier blanket character-audio omissions are historical; AI hearing and M2 remain open.

> Extended by [character animation contracts](character_animation_contracts.md):
> original CAnim_Sound tables now drive locomotion; current model node queries and
> original sphere measurements are inspectable. AI/damage-localization callers
> and complete M2 remain unavailable.

> Extended by [objective contracts](objectives_contracts.md): original mission
> objective broadcasts and external localized client strings are connected.
> Other localization/audio/HUD and full M2 acceptance remain pending.

> Extended by [interaction_contracts.md](interaction_contracts.md): original player USE and RotatingDoor
> are connected with bounded atomic collision. Earlier omissions below describe
> their previous increments; the full M2 gate remains open.

> Key inventory, pickup and query messages are extended by
> [trigger_contracts.md](trigger_contracts.md); prior key/pickup omissions below
> describe their earlier increment. General item/combat coverage remains pending.

This is a **selective port of original single-player entry and lifetime code**,
not the complete original shells, CPlayerObj, CBaseCharacter, InventoryMgr or
Destructable. Their accepted entry branches now execute in the new project.
The [weapon continuation](weapon_contracts.md) now connects Beretta/knife
creation, use and ordinary damage. The [movement continuation](movement_contracts.md)
connects standing dry-land MoveMgr commands, jump/gravity and collision. Complete
movement, combat reactions, audio and autosave still prevent a playable session. See [source selections](../implementation/sources/m2_player_sources.json) and
[validation](../../../Documentation/Implementation/m2_player.md).

## Call sequence

1. `CBloodClientShell::StartNewWorld` supplies the existing bounded load request.
   Only a fresh single-player request is accepted. Failed resource validation
   leaves the current world and client intact.
2. `CBloodServerShell::PreStartWorld` resets the entry subset. After original
   world objects exist, `PostStartWorld` copies PathPoint positions, names and
   actions using the original PathMgr/PathListData traversal. The snapshot owns
   its strings. The [path-manager continuation](path_manager_contracts.md) now
   provides original prefix selection and reverse enumeration order; AI following
   and save methods remain unavailable.
3. Explicit player entry preloads Caleb's ABC and requires an indexed default
   skin. `OnClientEnterWorld` calls `CreatePlayer`, which creates a registered
   `CPlayerObj` through the existing original virtual callbacks. Its ancestry is
   `CBaseCharacter -> B2BaseClass -> BaseClass`; its owned damage/inventory entry
   aggregates remain in the original aggregate dispatch chain.
4. `InitialUpdate` runs **before** client association. It establishes health,
   armor, ammo, dimensions `(12,41,12)`, the original damage list, a linked
   muzzle-light object, an original SmellHint and the owned TRIGGER string.
5. The server associates the client and calls the original alive-player Respawn
   branch, sends the single-player initialization request and executes the
   single-player `GoToStartPoint` selection/trigger/teleport flow.
6. The client enters, receives the initialization request and returns the
   original default Caleb profile: strength **5**, speed **3**, resistance **3**,
   magic **1**, default skin, Beretta in slot 1. Attribute validation, maximum
   health **200**, move speed **280** and a second entry Respawn execute. Actual
   health is **100**, armor **0**, bullet count **50**. The weapon continuation
   obtains the knife and requested Beretta through original inventory methods.
7. The selected original first-update code sends physics, position and stats.
   The original 0.001-second scheduling request remains; each application step
   invokes at most one due update. Server/client and animation updates continue
   with the inspector hidden. This does not implement the rest of PlayerObj's
   Update. The MoveMgr continuation now consumes these messages and advances the shared local body.
8. Client exit detaches its inspectable camera state. Server exit clears client
   association and requests deferred player removal. Owned strings, aggregates
   and muzzle light are reclaimed before the client token is disconnected.

## Typed services and lifetime

`HCLIENT` is an opaque, owned stable token. One local client is supported, with a
65,536-token lifetime budget per adapter. Tokens are never reused during that
adapter's lifetime. Unknown/disconnected tokens fail validation. Client user
data is a checked `HOBJECT`, replacing the original untyped C++ pointer; lookup
requires a live CPlayerObj. Pending/dead objects cannot receive client messages.

Local messages are owned variants: initialization request, rotation, position,
physics/model descriptor and stats. Forced positions include the original byte move code; typed client-to-server movement now carries position, velocity, acceleration and an on-ground bit (see movement contracts). The return initialization payload is also
typed. No packet contains object pointers or borrowed strings. A queue accepts
at most 256 messages; the pump permits eight rounds, allowing the original
initialization reply to enqueue further server messages. Nonfinite payloads,
oversized descriptors, duplicate initialization and stale associations fail.
There is no network protocol, reliable transport, client-ref restoration or
multiplayer implementation.

`SetModelFilenames` loads before replacing a model and retains the previous
resource on failure. The skin is a resource reference, not decoded texture data.
`GetObjectDims`, normalized `SetObjectRotation` and case-insensitive string
comparison use typed values. `MoveObjectToGround` follows the original downward
ray/animation-0-dimensions/MoveObject sequence using the primary collision index
and live solid object boxes, ignoring the subject itself. Its ray is bounded by
the supported coordinate range. Missing ground, unsupported models or penetrating
movement fail entry. It does not add moving BSP instances. Stair stepping is now covered by the [locomotion extension](locomotion_contracts.md).

All entry, exit, message pumping and world replacement happen outside object
callbacks. Reentrant requests fail without changing the active session. Creation
failures roll back newly created children and preserve the existing inspection
world. A missing start point fails instead of teleporting to the origin; the
original empty-list leak is fixed. Missing named starts retain the original
last-single-player-start fallback. Start-point triggers still use original
message dispatch; a trigger may remove the player safely during entry.

The damage intrusive list is unlinked on destruction. The inventory aggregate
releases its muzzle light when either end of the ownership link is removed;
merely clearing the handle would orphan it under this engine's link contract.
After successful entry, the scent belongs to the world and expires through its
original 20-second update. Failed entry removes its scent. World unload removes
all instances before destroying service storage. Removing the player from the
object inspector also detaches the local client on the next step.

## Deliberate incomplete branches

| Original behavior | Current boundary |
| --- | --- |
| Fresh Caleb entry, attributes, spawn, live reset, exit | Executed with the adaptations above |
| Dead single-player respawn | Rejected; original death/save restoration is not replaced by a revival shortcut |
| Other characters, multiplayer skins, teams and saved client refs | Rejected/outside the accepted request mode |
| Inventory | Original weapon slots, Beretta/knife and ammo capacity/use are connected; items, other weapons and world pickups remain pending |
| Destructable | Entry state plus ordinary normal/melee resistance, armor/health and dead-state calculation; full reactions, impulses, gibs and triggers remain pending |
| BaseCharacter | Entry flag subset and original aggregate forwarding; movement aggregate, containers and special user flags remain absent |
| Character animation commands | ABC playback and real callback delivery; unhandled character commands retained for inspection, not silently executed |
| Player movement | Original MoveMgr dry-land movement, crouch and body animation selection connected; shared local body replaces prediction/leash. Containers/platforms, full movement states and AI membership remain pending |
| Model flags | STAIRSTEP uses the bounded replacement stair solver; ANIMTRANSITION still does not implement blending |
| Client camera | Player eye plus selected original CameraObj view/gates are inspectable; see [camera contracts](camera_contracts.md). Cinematic rendering/audio remain pending |
| View weapons | Selected original client animation/firing paths, actual ABC resources, diagnostic input; see weapon contracts |
| Voice, HUD/menu, autosave, sound playback | Absent; no successful service stubs |

`enter_player` succeeds for this documented inspection subset, while
`run_world` still fails for incomplete gameplay, even after the subset enters.
`SessionState` remains Empty/Inspection/Blocked, with the client/player entry
state shown separately. The 190 missing SubwayA runtime entries remain missing.
The Models tab continues to be an independent ABC preview; Session and Objects
show the actual CPlayerObj instance.

## Inspection and checks

Use the **Session** tab to enter, select, reset a live player or leave. Commands
execute at frame boundaries. The panel shows both shell states, client/player
association, attributes, health/ammo, source requests, position, camera, animation,
list membership, message/update counts and blockers.

```sh
./build/debug/blood2 --inspect-player
./build/debug/blood2 --player-smoke --capture build/player.png
ctest --preset debug -L player
```

The headless command returns 0 for the entry inspection contract. `--start-game`
still returns 6, with missing gameplay services reported. The GPU test checks
hidden-UI updates, selection invalidation, world unload/re-entry and restored
object/string counts, also in the inspector-free Release build.

The [single-player death continuation](player_death_contracts.md) supersedes the missing selected Caleb death/corpse/gib and client death-camera paths above. Active powers, multiplayer death and whole-world/dead-player restoration remain unavailable. M2 is still incomplete.
