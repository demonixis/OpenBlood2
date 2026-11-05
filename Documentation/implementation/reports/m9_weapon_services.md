# M9 — independent client weapon runtime

2026-10-30, following `40d4a626`. CViewWeapon now runs through a client-facing
WeaponServices interface and links without the server runtime. The existing solo
entrypoint is a small synchronous compatibility bridge. Network weapon-state,
commands, fire validation/acknowledgements and graphical weapon control are the
next delivery; this extraction alone does not enable remote firing.
See the [contract](../../contracts/multiplayer_contracts.md) and
[provenance](../sources/m9_weapon_services_sources.json).

## Boundary

WeaponServices provides the current clock, checked owner/liveness and selected
hand, mode-specific ammunition, typed WeaponFire delivery and local animation
sound delivery. It exposes no ServerDE, CPlayerObj, CWeapon or server handle.
CViewWeapon's runtime has its own `blood2_client_weapons` static target, depending
only on engine_core. Its existing visual base and resource ownership are unchanged.
FireResult moves to a shared vocabulary header with the same values; it is not
implicitly serialized as a protocol enum.

The runtime retains its draw/rest/start/fire/end/holster transitions, model keys,
reload conditions, semiautomatic/looping behavior, shotgun barrel alternation,
melee/Voodoo choices, bomb charge/overhold, Orb spin, flash clocks and local RNG.
The original data reference is still the immutable weapon_definition entry.
Selection and owner checks are repeated at delivery boundaries. Only the service
calls replace direct server reads/calls; weapon rules remain original-port code.

`local_weapon.cpp` keeps the old Update signature for the existing shell/tests.
It borrows the current checked player/weapon for that call, obtains ammo through
CWeapon::CheckAmmo (including alternate ammo overrides), delivers fire through
CBloodServerShell::OnMessage and sends the shotgun reload SOUND_KEY through the
existing PlayLocalSound path. Deferred removal remains owned by the existing
GameSession dispatch. Empty views do not query a player. A changed player identity
cannot satisfy the borrowed owner's checks. No solo clock, movement, audio, damage,
server randomness, rendering algorithm or PBR file is replaced.

A future remote implementation will predict cosmetic acceptance and send checked
commands for authoritative processing. It must not interpret the synchronous solo
result as a network guarantee. Local ammo/cadence checks are presentation behavior;
the required server validation and shot deduplication still remain to implement.

## Verification

**70/70 native tests and 5/5 server-only ASan/UBSan tests pass.** Native coverage
includes the established weapon inventory/fire/damage/animation/lifetime/rollback
cases, private retail weapons, Mac-10, Flare, M16/grenade, shotgun player fire,
proximity/remote bomb fire, weapon sprites, movement and the GPU weapon smoke.
The sanitizer subset covers the new independent consumer plus reused solo
animation/lifetime/rollback cases. Native client/server and sanitized dedicated
builds succeed.

The new independent executable links only blood2_client_weapons/engine_core.
`nm -C` finds no GameSession, ServerAdapter, CBloodServerShell, Objects, CWeapon or
CInventoryMgr methods. Synthetic cases exercise delivered origins/seeds/hand,
ammo refusal, binocular/dead/selection gates, Dry/Blocked versus invalid delivery,
owner removal between two FIRE_KEY commands, and original shotgun SOUND_KEY path
normalization/local sound. Its retail case loads twenty weapons/bombs, executes
39 firing modes (sniper alternate is zoom), records 595 cosmetic fire requests,
and tests four character skins with both Beretta hands. Those requests go to a
test receiver; they are not network shots or server damage qualification.

The initial retail fixture failed on Singularity because its three-second warmup
plus 1.5-second held input never crossed the original five-second reload gate.
The fixture now holds input for six seconds. Production timing was not changed;
the failed run and diagnostic output remain in the evidence.

All five solo traces match the pre-extraction binaries byte for byte: 240 effect
frames, 360 audio frames, 360 movement frames, 720 environment frames and 300 view
frames. All **6,615** protected PBR/capture/earlier-evidence hashes remain unchanged.
No immutable GPU reference was regenerated. Linux verification only; no new
Windows/macOS, mobile/XR, human LAN or complete combat claim. M2-GPU-001 and M3
remain independent.

Commands and full outputs: `evidence/m9_weapon_services_*`.

```sh
cmake --build build/m9-fx-runtime --target blood2_app blood2_server movement_tests weapon_tests weapon_sprite_tests mac10_tests shotgun_tests assault_tests flare_tests proximity_tests remote_tests network_weapon_services_tests network_weapon_local_tests -j6
ctest --test-dir build/m9-fx-runtime --output-on-failure -R '^network\.weapon_(services|local_)|^weapons\.|^weapon_sprites\.|^mac10\.|^shotgun\.(weapon|player)$|^assault\.|^flare\.|^movement\.|^(remote|proximity)\.private_fire$|^gpu\.weapons$' -j3
cmake --build build/m9-fx-runtime-asan --target blood2_server network_weapon_services_tests network_weapon_local_tests -j4
ctest --test-dir build/m9-fx-runtime-asan --output-on-failure -R '^network\.weapon_(services|local_)' -j2
```
