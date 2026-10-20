# BoneLeech attachment and usable inventory

This continuation supersedes the attachment and offer-only inventory boundaries in
[CityHubC/SteamTunnels](cityhubc_steamtunnels_contracts.md). It does not reopen M2,
resolve M2-GPU-001 or accept complete campaign traversal. Commercial models,
textures and audio remain external. [Source ledger](../implementation/sources/inventory_use_sources.json).

## BoneLeech

The original jump/target touch and standing-player branches enter EnemyAttach.
A checked local message carries the leech and player identities; the client
acknowledges only its own live player/creature pair. A second leech is refused
and detached while the first remains attached. The view uses boneleech_pv.abc and
its skin, retained source scale, REALLYCLOSE projection and ViewCreature offsets
(right 0, up -2.5, forward 0.6). It follows player pitch/yaw. The server waits for
the acknowledgement before damage: a random 5–10 melee damage times the inventory
multiplier, at most once per update and no more often than once per second, with
one of six bl_burrow sounds. The source's attached-damage immunity is retained.
Client light scale decreases by 0.9 once per second.

Each fresh USE press increments the total counter. Presses less than 0.5 seconds
apart increment the consecutive counter; a later press resets it to zero. Five
consecutive increments (normally six presses) or 25 total presses detach. Holding
E is not repeated input. Detach restores the world model/flags and the source
four-second retry delay. Player death, object removal, exit and unload also clear
the checked client identity; queued messages cannot resurrect a removed object.

Two explicit engine adaptations avoid failures in the replacement box solver:
attached creatures are nonsolid, have zero motion and cannot auto-deactivate during
the handshake/attachment/cooldown. Detach tries five bounded positions around/above
the player, checking the path and body clearance before restoring solidity. If
all are blocked it remains nonsolid and retries, rather than overlapping and
blocking the player. These placements are not claimed to match retail physics.
The established no-head BoneLeech body-damage policy remains unchanged.

## Bug Spray Canister

Weapon 10 uses the complete source WeaponDefs entry: DieBugDie ammo (5), one
unit per shot in either mode, 40–60 primary or 60–80 alternate damage, 1500 speed,
0.25/1 second reload and zero area radius. Pickup acknowledgement, owned slots,
held/view models, FIRE_KEY animation, fire audio, ammo, drop mapping and the existing
owned inventory save/transfer path all recognize it. Secondary animation loops as
in the source. A shot with no ammunition does not create a projectile.

CBugSprayProjectile and CBugSprayAltProjectile retain the invisible (scale-zero)
napalmprime sprite, gravity, ten-second lifetime, point contact and respective
explode/fire damage types. Flight uses bounded 120 Hz swept point segments with
the existing shooter/class filters; direct contact delivers the damage. This
replacement integrator does not claim retail trajectory equivalence or general
container buoyancy. Recipes 31/32 draw the original colored particle streams.
Impact 26 emits smoke; impact 27 emits the two delayed fire rings and both light
requests, including the source duplicate Radius1 assignment. Original impact sounds
use radius 750. Effects and owned voices are cleaned up by existing world lifetimes;
errors remain recorded rather than successful placeholder effects.

## Night goggles

NightGogglesPU now supplies item 3. N toggles it after collection, with the existing
run-key restriction. A full item refuses replacement. A partially depleted item
recharges to 100/30 seconds, ignoring the pickup quantity. While active, remaining
time decreases by simulation frame time and displayed charge rounds to the nearest
integer. Exhaustion sends the off action. Restore/KeepAlive retain charge/time and
clear activation, as InvItem::Load does; death removes the item.

The client owns and stops nightgogglesloop1.wav. Original global light scale
(0.1,1,0.1) applies to the scene and view models. Original NIGHTGOGGLESGLOW characters
and pickups render without their texture, with green light add. This is a scene
uniform added to the existing shared render pass, not a replacement rendering
pipeline. Default uniforms remain white; per-pixel light, shadows, lightmaps and
GPU skinning are preserved. HUD/menu colors remain separate. The fragment constant
buffer is 288 bytes and always pushed in full; all bundled shader variants were
regenerated with the pinned toolchain.

## TheEye

Item 4 is unique. I first throws a SeeingEye at owner position plus half the body
half-height minus one, oriented to player pitch/yaw. Dropit's forward velocity 300
overrides the randomized initial toss in the original call order. Later I presses
toggle the camera and eyeloop1.wav. The remote view has original (1,0,0.5) light
scale, uses the eye as listener, shows the player's body and hides view weapons
and the crosshair. Look rotates the eye without rotating the player's model.
Movement, USE and fire are blocked; the item can still be toggled off. The server
also blocks an already committed FIRE_KEY before the queued camera message arrives.
The eye itself is hidden from its camera to avoid drawing its interior with our
model rendering policy.

SeeingEye retains 10 health, zero armor, mass 30 and half-dimensions (7,7,7).
It uses the shared projectile model-flight solver as a checked implementation
mechanism, with no weapon/explosion behavior. The owner is ignored while the
throw leaves its body. First solid contact stops it; an object contact follows
the source world-axis back offset and a checked host link. Host removal retrieves
it. A landed eye touching/overlapping its owner returns to inventory, stops the
view and plays inventory1.wav. It pings nearby AI through the existing visibility
service once per second. This checked host following replaces the original
untyped MID_ATTACH bookkeeping; whole-world attachment serialization is not added.

Destruction while viewed blinds the scene for ten seconds, stops the camera/loop,
and consumes the inventory item. This follows the executable Remove(true) branch,
not the contradictory introductory source comment claiming automatic return.
Normal retrieval keeps the item and does not blind. Owner removal and item teardown
remove the child through deferred callbacks without deleting the active inventory
method. Inventory snapshots retain ownership only: on cross-world restoration the
eye is back in hand, inactive, with no old-world handle. A saved game restores a
deployed eye ([save/load lot 9](save_load_contracts.md#implemented-pickups-and-projectiles-lot-9)): the inventory binds the restored SeeingEye, which keeps its
owner, stuck object and dropped flag; the item loads inactive, and a later removal turns the
view off on the owner's current client.

## Verification

`inventory_use.goggles`, `.eye`, `.leech` and `.bugspray` exercise actual private
models/audio, input edges, client messages, both FIRE_KEY modes, direct damage,
charge/recharge, restoration, camera/fire gates, competing attachments, USE release,
retrieval/destruction and owner/unload cleanup. `campaign_entry.actors` now checks
successful pickup acknowledgements. The two reported worlds retain their startup
checks. `gpu.inventory_use` renders real CityHub frames and checks green vision,
textureless tagged models, remote-eye tint, blindness and close leech geometry.
Captures are written under the build's tests/inventory-use-gpu directory. Headless,
private-data, GPU and sanitizer results are distinct; these are bounded interaction
checks, not full level completion or a new milestone acceptance.

[Save/load lot 12](save_load_contracts.md#implemented-bosses-and-special-enemies-lot-12) saves an attached BoneLeech, Thief or TheHand: the
attached player in AI_Mgr's enemy-attachment slot, the raw damage and detach clocks and a port tail
with the tracked player and the world flags and dimensions the detach restores. A load replays the
original attach handshake with the restored client; the client's darkening and USE counters restart.
