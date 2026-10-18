# Per-world gaps and implementation batches

Snapshot of what blocks each mounted Blood II world from starting, grouped into
implementation batches. Measured on 2026-10-18 at `b52bb66` against the retail
2.1 data (BLOOD2, BLOOD2P, BLOOD2P2, BLOOD2NM and SOUND archives), difficulty 1,
with `blood2 --inspect-world --world <name>` for every world the inspector Levels
tab lists. The counts match the Levels-tab rejection messages.

This is a backlog, not an acceptance record. It covers only world-entry
validation (class registration and creation). A world that passes here can still
fail later on presentation resources, runtime services or gameplay. Earlier
per-world counts in the contracts are historical.

Update after L0/L1 (same data, `--start-game`): `drop` and `multi/bb_deathcanyon`
now start; 17 equal-radius Sounds now play (434 → 417 failed entries over all
worlds); every precreate rejection names its cause. After L13 `08_subwayb`
also starts; after L3, 147 entries still fail, and after L4 115. Of the seven
worlds that pass entry validation, six start with `--start-game`; `bb_midgard`
fails its first update (L14). The tables below are regenerated from
the latest run.

## How to refresh

```bash
blood2 --data-dir <Blood2> --inspect-world --world <name>
```

The summary lists `Class / Class not ported: N`, and each failed entry is now
logged as `Failed entry <index> (<class>): <detail>`. Since L0 a precreate
refusal reads `Rejected precreate: <class>: <reason>`.

## Status by world

`starts` means the world passes entry validation and `--start-game` (01_SubwayA,
02_CityHub, 05_CDMB, 08_SubwayB, Drop, BB_DeathCanyon). Batches are defined in the next section.

| World | Created | Unported (classes) | Failed | Batches needed |
|---|---:|---:|---:|---|
| 01_subwaya | 204 | 0 (0) | 0 | **starts** |
| 02_cityhub | 453 | 0 (0) | 0 | **starts** |
| 02_cityhubb | 392 | 19 (3) | 0 | L7 |
| 02_cityhubc | 370 | 34 (4) | 2 | L2, L7, L10 |
| 03_museum | 559 | 86 (6) | 6 | L6, L8, L10, L11, L12 |
| 04_steamtunnels | 449 | 55 (6) | 6 | L2, L5, L6, L7, L10 |
| 05_cdm | 534 | 14 (5) | 2 | L6, L7, L10, L11 |
| 05_cdmb | 129 | 0 (0) | 0 | **starts** |
| 06_airship1 | 191 | 0 (0) | 5 | L6, L12 |
| 06_airship2 | 243 | 2 (2) | 17 | L6, L10 |
| 07_cathedral | 207 | 18 (9) | 4 | L2, L6, L7, L8, L10, L11 |
| 08_subwayb | 175 | 0 (0) | 0 | **starts** |
| 09_abandoned_buildings | 641 | 38 (11) | 32 | L2, L4, L5, L6, L7, L8, L10, L11 |
| 10_sewagetreatmentplant | 99 | 25 (5) | 1 | L6, L7, L11 |
| 11_slaughter_house | 467 | 26 (8) | 14 | L6, L7, L8, L10, L12 |
| 21_laboratory | 282 | 38 (9) | 7 | L6, L7, L8, L11 |
| 22_office | 490 | 6 (3) | 9 | L2, L6, L7, L8, L10 |
| 23_rooftop | 136 | 8 (6) | 0 | L8, L9, L10, L11 |
| 24_ancientcity | 255 | 57 (9) | 1 | L6, L7, L8, L10 |
| 24b_arena | 152 | 18 (11) | 0 | L7, L9, L10, L11 |
| 25_ancientone | 123 | 30 (10) | 1 | L2, L8, L9, L10, L11 |
| credits | 21 | 1 (1) | 0 | L9 |
| drop | 5 | 0 (0) | 0 | **starts** |
| end | 20 | 4 (4) | 0 | L9 |
| multi/bb_area15 | 75 | 1 (1) | 0 | L10 |
| multi/bb_bloodarena | 125 | 5 (5) | 0 | L10 |
| multi/bb_bloodkeep | 81 | 2 (2) | 1 | L2, L10 |
| multi/bb_bodies | 66 | 2 (2) | 0 | L10 |
| multi/bb_brownstar | 132 | 2 (2) | 0 | L10 |
| multi/bb_cabana | 65 | 3 (3) | 0 | L10 |
| multi/bb_chosenarena | 123 | 2 (2) | 0 | L10 |
| multi/bb_condemned | 181 | 4 (4) | 3 | L2, L4, L10 |
| multi/bb_crypt | 116 | 6 (4) | 0 | L10, L11 |
| multi/bb_deathcanyon | 44 | 0 (0) | 0 | **starts** |
| multi/bb_drain | 50 | 2 (1) | 0 | L10 |
| multi/bb_fields | 97 | 8 (1) | 0 | L11 |
| multi/bb_jgrave | 135 | 11 (4) | 4 | L2, L10 |
| multi/bb_midgard | 81 | 0 (0) | 0 | L14 |
| multi/bb_power | 140 | 1 (1) | 0 | L10 |
| multi/bb_rooftop | 104 | 4 (4) | 0 | L10, L11 |
| multi/ctf_cabana | 122 | 8 (4) | 0 | L10, L11 |
| multi/ctf_deadcities | 153 | 8 (4) | 0 | L10, L11 |
| multi/ctf_dmx | 99 | 7 (3) | 0 | L10, L11 |
| multi/ctf_jam | 219 | 21 (7) | 0 | L10, L11 |

Closest to starting:

- **L14 only:** `multi/bb_midgard` (passes entry validation, fails first update).
- **L9 only:** `credits`, `end` (the Chosen as scripted actors).
- **L10 only:** `multi/bb_bloodarena`, `multi/bb_area15`, `bb_brownstar`, `bb_cabana`, `bb_chosenarena`, `bb_drain`, `bb_power`.
- **L11 only:** `multi/bb_fields`.
- **L7 only:** `02_cityhubb`. **L6 + L10:** `06_airship2`.

## Batches

L0, L1, L3 and L13 are done, L4 partly. L2, the rest of L4, L5 and L14 are fixes and policies on code that already exists;
L6–L12 port new original code. Unported-class totals count source entries over all 44 worlds.

### L0 — Diagnostics (done)

Classes call the bounded `ServerDE::RejectPrecreate(reason)` service before
refusing precreate; the engine keeps the first reason of the refusal in progress
and appends it to `Rejected precreate: <class>`. Reasons are recorded for the AI
runtime failures, every CDestructableBrush refusal and each
CDestructableModel property check. A refusal without a reason keeps the bare
message; an accepted precreate with an invalid creation state says so.

### L1 — Quick fixes in ported code (done)

- **Sounds with `InnerRadius == OuterRadius`** (17 entries in 04, 06_airship1,
  21, 23, bb_crypt, bb_rooftop) now play. The mixer gives full gain within the
  inner radius and none beyond the outer one, so equal radii are a hard edge
  without a zero division. This follows the later LithTech `CSoundInstance`
  branch order; inner > outer and a zero outer radius still reject.
- **Worlds authoring no music** (`drop`, `multi/bb_deathcanyon`): when every
  WorldProperties music property is absent or empty, the catalogue is
  `unauthored` and level requests are counted silent no-ops (inspector
  "no authored music"), as the original ignored `PlayList` failure after a failed
  `InitPlayLists`. An authored but invalid or missing plan still fails startup.
- The SpawnObject allow-list part moved to **L13**: those pickups refuse string
  creation until the original bounce is ported.

### L2 — Authored data errors: explicit omission policy (9 worlds)

The retail data contains references that cannot resolve. Decide, per the
`fallengrills` precedent (typed explicit omission, never invented content),
whether each kind is omitted visibly or keeps the world blocked:

- Missing sounds: `sounds\ambients\neonloop1.wav`, `sounds\ambients\amb1.wav`
  (`ambients` directory, 09), `sounds\ambient\chantloop2.wav` (bb_bloodkeep),
  `neonlight1.wavt` / `neonlight2.wavt` (bb_condemned), empty `Filename`
  (4 Sounds, bb_jgrave).
- Misspelled spawn classes: `DieBugDieAmmo` (07, 2×), `FareAmmoPU` (25).
- Missing resources: `models\worldobjects\urinal2.abc` (22_office);
  `sprites\bloodsplat1-3.dtx` DetailSprites (04); a `Sprite` with empty filename
  (07); a CDestructableModel with empty `InitFilename` (02_cityhubc,
  "clip-zero dimensions unavailable").

### L3 — Legacy property schemas (done)

- **`TypeAction`** (231 entries, `multi/bb_bloodarena`, `bb_bodies`, `bb_midgard`,
  plus three SoundFX): no original class declares or reads it, and LithTech's
  `GetPropGeneric` never looks at a name nobody asks for. BaseClass now declares it
  as a retired, non-defaulted bool: kept, never read, wrong types still reject.
  Undeclared names keep rejecting so incomplete schemas stay visible.
- **World-file CGib** (27 entries, 05_cdm 24, 09 3): the original BEGIN_CLASS
  schema and ReadProp are ported (plastic surface, InitAnim, kickable cube, pylon
  bounce sound, PostPropRead overrides; see the death-effect contracts).
- **Spawner `SpawnObject`** (4 entries, 24_ancientcity, 24b_arena): the original
  bound is MAX_GP_STRING_LEN (200 bytes), not 100; the 112–141-byte requests are
  accepted. They name unported classes (UndeadGideon, the Revenant Chosen,
  SoulDrudge), which still fail when triggered (L7–L9).

Accepting these rows surfaced two more ambiguous world-model names (bb_bodies
`window3`, bb_midgard `rotate01`), now listed under L4.

### L4 — World-model name resolution and shells (duplicate names done)

- **Duplicate BSP names** (done): creation binds the first same-named model in
  file order, as later LithTech `FindWorldModel` does; drawing reuses that
  binding and later candidates stay unbound (object contracts). This cleared
  every ambiguous failure, including the two surfaced by L3.
- **Still open — open/non-manifold door shell** `Buildings` (bb_condemned): one
  component has 4 unpaired edges. **Off-plane container face** `helloyou3` (09):
  a quad deviates 0.985 units from its plane (tolerance 0.1), a real warp rather
  than rounding. Both need a collision policy for shells the replacement solver
  cannot represent today; the original BSP collision never required closure.

### L5 — Polygon dynamics (2 worlds)

CDestructableBrush with `Gravity` but neither `BoxPhysics` nor `Pushable`
(28 entries: 26 in 09_abandoned_buildings, 2 in 04_steamtunnels). The bounded
body solver refuses them. Needs falling non-box world models.

### L6 — Weapons and weapon pickups (12 worlds)

Fanatic/Prophet are rejected when an authored `AIWeapon` names a weapon family
that is not ported (34 NPCs): SHOTGUN (20), TESLA/TESLACANNON (4), NAPALMCANNON
(3), SNIPERRIFLE (3), SINGULARITY (2), MINIGUN, HOWITZER. The same families appear
as pickups/spawns: `ShotgunPU`, `HowitzerPU`, `RocketAmmoPU`, `TeslaCannonPU`,
`SingularityPU`, `DeathRayPU`. Suggested order by reach: Shotgun (06_airship2,
03, 04, 07, 11, 24), then Sniper, Tesla, Napalm, Singularity, Minigun, Howitzer.
Each family ports the player weapon, the AI branch of `WeaponStrToInt` and the
pickup together.

### L7 — Cabal creatures (12 worlds)

| Class | Entries | Worlds |
|---|---:|---:|
| SoulDrudge | 119 | 11 |
| ShikariAI | 58 | 10 |
| BoneLeech (also SpawnObject `BoneLeech`/`boneleech`, 12 containers) | 44 | 6 |
| Thief | 24 | 3 |
| DrudgeLord | 19 | 7 |
| Gremlin | 10 | 2 |
| DrudgePriest | 3 | 1 |

SoulDrudge + ShikariAI + BoneLeech are the minimum for the early campaign
(02_cityhubb, 02_cityhubc, 04, 05_cdm).

### L8 — Bosses and unique enemies (9 worlds)

ZealotAI (5), DeathShroud (5), MadScientistAI (3), TheHandAI (2), AncientOne (2)
+ AncientOneTentacle (16), EnhancedGideon (2), UndeadGideon (1), Naga (1),
Behemoth (1), Tchernotronic (1).

### L9 — The Chosen as NPCs (5 worlds)

CalebAI (4), OpheliaAI (3), IshmaelAI (2), GabriellaAI (2), and the Revenant
variants OpheliaREV, IshmaelREV, GabriellaREV (1 each). Unblocks `credits` and
`end` alone; also needed by 23, 24b, 25.

### L10 — Power-ups and inventory items (30 worlds, most multiplayer maps)

| Class | Entries | Worlds |
|---|---:|---:|
| AngerPU (also SpawnObject `TheAngerPU`) | 22 | 18 |
| InvulnerabilityPU | 12 | 12 |
| StealthPU | 12 | 10 |
| TheEyePU | 12 | 8 |
| NightGogglesPU | 8 | 6 |
| TimeBombPU | 28 | 5 |
| RevenantPU | 3 | 2 |
| BinocularsPU | 3 | 2 |
| VoodooDollPU, LifeLeechPU, BugSprayPU | 2 each | 2 each |

Also SpawnObject `LifeEssencePU` (2). The timed powers need the timer
preservation that player restoration currently refuses.

### L11 — Level mechanics and effects (16 worlds)

| Class | Entries | Worlds |
|---|---:|---:|
| Container | 53 | 7 |
| TripLaser | 45 | 2 |
| CClientLightningSFX | 9 | 3 |
| FlagStand (CTF) | 8 | 4 |
| Minefield | 8 | 1 |
| CClientWarpGateSFX | 7 | 2 |
| RotatingModel | 1 | 1 |
| ParticleSystem | 1 | 1 |

### L12 — NPC ground placement (3 worlds)

CultistAI (6) and CivilianAI (1) fail "NPC world ground placement failed" in
03_museum, 06_airship1 and 11_slaughter_house. Investigate the placement probe
against those spawn positions (no relocation shortcut).

### L13 — String-created pickups (done)

Destructible containers whose `SpawnObject` names an already ported pickup are
rejected: `MedKitPU` (5), `FlashlightPU`/`FlashLightPU` (3), `FlareGunPU` (2),
`SubMachineGunPU`, `AssaultRiflePU`. `ValidateSpawnObject` does not admit them
because `ItemPickup` and the weapon pickups refuse `PRECREATE_STRINGPROP`: the
original string-spawn path (PickupObject.cpp, `m_bBounce` / rest-position
spring) was not ported. It now is (see the pickup contracts): SpawnItem raises
the pickup 10 units and leaves it bouncing with no update scheduled, so the
source spring is never reached. The allow-list admits the ported item and weapon
pickups; 02_cityhubb, 03, 04, 05_cdm, 06_airship1, 07 and 08_subwayb lost those
failures.

### L14 — Runtime start placement (1 world)

`multi/bb_midgard` passes entry validation now, but its first update fails:
`NecroWardPU0` lies 1.29 units inside world polygon 449 after its initial ground
placement, beyond the 0.01-unit contact overlap tolerance. Investigate the
pickup's MoveObjectToGround result against that slope; no depenetration
shortcut.

## Details by world

### 02_cityhubb
392 created instances · 19 unported entries (3 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 10 | L7 |
| BoneLeech | 7 | L7 |
| ShikariAI | 2 | L7 |

### 02_cityhubc
370 created instances · 34 unported entries (4 classes) · 2 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| BoneLeech | 17 | L7 |
| SoulDrudge | 10 | L7 |
| ShikariAI | 6 | L7 |
| BugSprayPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: TheEyePU | 1 | L10 |
| CDestructableModel | Model clip-zero dimensions unavailable | 1 | L2 |

### 03_museum
559 created instances · 86 unported entries (6 classes) · 6 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| Container | 44 | L11 |
| TripLaser | 38 | L11 |
| InvulnerabilityPU | 1 | L10 |
| MadScientistAI | 1 | L8 |
| ShotgunPU | 1 | L6 |
| Tchernotronic | 1 | L8 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: InvulnerabilityPU | 1 | L10 |
| CDestructableBrush | SpawnObject class not accepted: BinocularsPU | 1 | L10 |
| CultistAI | NPC world ground placement failed | 1 | L12 |
| CDestructableModel | SpawnObject class not accepted: RocketAmmoPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: SHOTGUN | 1 | L6 |
| Prophet | authored AI weapon unavailable: SINGULARITY | 1 | L6 |

### 04_steamtunnels
449 created instances · 55 unported entries (6 classes) · 6 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 32 | L7 |
| ShikariAI | 13 | L7 |
| Gremlin | 5 | L7 |
| RocketAmmoPU | 3 | L6 |
| DrudgeLord | 1 | L7 |
| NightGogglesPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat1.dtx | 1 | L2 |
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat2.dtx | 1 | L2 |
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat3.dtx | 1 | L2 |
| CDestructableBrush | Gravity without BoxPhysics (polygon dynamics) | 2 | L5 |
| Fanatic | authored AI weapon unavailable: SHOTGU | 1 | L6 |

### 05_cdm
534 created instances · 14 unported entries (5 classes) · 2 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| TripLaser | 7 | L11 |
| SoulDrudge | 4 | L7 |
| Container | 1 | L11 |
| ShikariAI | 1 | L7 |
| StealthPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: TeslaCannonPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: TESLA | 1 | L6 |

### 06_airship1
191 created instances · 0 unported entries (0 classes) · 5 failed entries

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CultistAI | NPC world ground placement failed | 3 | L12 |
| Fanatic | authored AI weapon unavailable: SNIPE | 2 | L6 |

### 06_airship2
243 created instances · 2 unported entries (2 classes) · 17 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| BugSprayPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Fanatic | authored AI weapon unavailable: SHOTGUN | 17 | L6 |

### 07_cathedral
207 created instances · 18 unported entries (9 classes) · 4 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 6 | L7 |
| ShikariAI | 4 | L7 |
| BoneLeech | 2 | L7 |
| AngerPU | 1 | L10 |
| Container | 1 | L11 |
| DrudgeLord | 1 | L7 |
| Naga | 1 | L8 |
| NightGogglesPU | 1 | L10 |
| VoodooDollPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: DieBugDieAmmo | 2 | L2 |
| Sprite | Missing, oversized or non-SPR sprite resource:  | 1 | L2 |
| CDestructableModel | SpawnObject class not accepted: ShotgunPU | 1 | L6 |

### 09_abandoned_buildings
641 created instances · 38 unported entries (11 classes) · 32 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 10 | L7 |
| Thief | 7 | L7 |
| ShikariAI | 6 | L7 |
| TimeBombPU | 6 | L10 |
| ZealotAI | 3 | L8 |
| CClientWarpGateSFX | 1 | L11 |
| DrudgeLord | 1 | L7 |
| HowitzerPU | 1 | L6 |
| MadScientistAI | 1 | L8 |
| NightGogglesPU | 1 | L10 |
| ShotgunPU | 1 | L6 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Missing or oversized sound resource | 4 | L2 |
| CDestructableBrush | Container face is not on its plane | 1 | L4 |
| CDestructableBrush | Gravity without BoxPhysics (polygon dynamics) | 26 | L5 |
| Prophet | authored AI weapon unavailable: SINGULARITY | 1 | L6 |

### 10_sewagetreatmentplant
99 created instances · 25 unported entries (5 classes) · 1 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| Thief | 12 | L7 |
| SoulDrudge | 9 | L7 |
| ShikariAI | 2 | L7 |
| Container | 1 | L11 |
| DrudgeLord | 1 | L7 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Fanatic | authored AI weapon unavailable: HOWITZ | 1 | L6 |

### 11_slaughter_house
467 created instances · 26 unported entries (8 classes) · 14 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| BoneLeech | 9 | L7 |
| SoulDrudge | 5 | L7 |
| Thief | 5 | L7 |
| TimeBombPU | 2 | L10 |
| ZealotAI | 2 | L8 |
| DeathRayPU | 1 | L6 |
| DrudgeLord | 1 | L7 |
| TheEyePU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: BinocularsPU | 1 | L10 |
| CDestructableBrush | SpawnObject class not accepted: InvulnerabilityPU | 1 | L10 |
| CultistAI | NPC world ground placement failed | 2 | L12 |
| CivilianAI | NPC world ground placement failed | 1 | L12 |
| CDestructableModel | SpawnObject class not accepted: HowitzerPU | 1 | L6 |
| CDestructableModel | SpawnObject class not accepted: ShotgunPU | 1 | L6 |
| Prophet | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| Fanatic | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| CDestructableModel | SpawnObject class not accepted: BoneLeech | 5 | L7 |

### 21_laboratory
282 created instances · 38 unported entries (9 classes) · 7 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| ShikariAI | 12 | L7 |
| SoulDrudge | 9 | L7 |
| BoneLeech | 6 | L7 |
| Gremlin | 5 | L7 |
| TheHandAI | 2 | L8 |
| CClientLightningSFX | 1 | L11 |
| MadScientistAI | 1 | L8 |
| RotatingModel | 1 | L11 |
| SingularityPU | 1 | L6 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: boneleech | 7 | L7 |

### 22_office
490 created instances · 6 unported entries (3 classes) · 9 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| BoneLeech | 3 | L7 |
| ShikariAI | 2 | L7 |
| DeathShroud | 1 | L8 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableBrush | SpawnObject class not accepted: TheAngerPU | 1 | L10 |
| CDestructableModel | Model clip-zero dimensions unavailable: Model resource not mounted: modelsworldobjects\urinal2.abc | 1 | L2 |
| Fanatic | authored AI weapon unavailable: TESLACANNON | 3 | L6 |
| CDestructableBrush | SpawnObject class not accepted: RocketAmmoPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: SNIPERRIFLE | 1 | L6 |
| Fanatic | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| Prophet | authored AI weapon unavailable: MINIGUN | 1 | L6 |

### 23_rooftop
136 created instances · 8 unported entries (6 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 2 | L10 |
| Container | 2 | L11 |
| CalebAI | 1 | L9 |
| EnhancedGideon | 1 | L8 |
| LifeLeechPU | 1 | L10 |
| OpheliaAI | 1 | L9 |

### 24_ancientcity
255 created instances · 57 unported entries (9 classes) · 1 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 23 | L7 |
| DrudgeLord | 13 | L7 |
| ShikariAI | 10 | L7 |
| DeathShroud | 4 | L8 |
| DrudgePriest | 3 | L7 |
| Behemoth | 1 | L8 |
| EnhancedGideon | 1 | L8 |
| InvulnerabilityPU | 1 | L10 |
| UndeadGideon | 1 | L8 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Fanatic | authored AI weapon unavailable: SHOTGUN | 1 | L6 |

### 24b_arena
152 created instances · 18 unported entries (11 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| CClientWarpGateSFX | 6 | L11 |
| CClientLightningSFX | 3 | L11 |
| AngerPU | 1 | L10 |
| DrudgeLord | 1 | L7 |
| GabriellaREV | 1 | L9 |
| InvulnerabilityPU | 1 | L10 |
| IshmaelREV | 1 | L9 |
| OpheliaREV | 1 | L9 |
| SoulDrudge | 1 | L7 |
| StealthPU | 1 | L10 |
| VoodooDollPU | 1 | L10 |

### 25_ancientone
123 created instances · 30 unported entries (10 classes) · 1 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AncientOneTentacle | 16 | L8 |
| CClientLightningSFX | 5 | L11 |
| AncientOne | 2 | L8 |
| AngerPU | 1 | L10 |
| CalebAI | 1 | L9 |
| GabriellaAI | 1 | L9 |
| InvulnerabilityPU | 1 | L10 |
| IshmaelAI | 1 | L9 |
| LifeLeechPU | 1 | L10 |
| OpheliaAI | 1 | L9 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableModel | SpawnObject class not accepted: FareAmmoPU | 1 | L2 |

### credits
21 created instances · 1 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| CalebAI | 1 | L9 |

### end
20 created instances · 4 unported entries (4 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| CalebAI | 1 | L9 |
| GabriellaAI | 1 | L9 |
| IshmaelAI | 1 | L9 |
| OpheliaAI | 1 | L9 |

### multi/bb_area15
75 created instances · 1 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| StealthPU | 1 | L10 |

### multi/bb_bloodarena
125 created instances · 5 unported entries (5 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| BinocularsPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |
| StealthPU | 1 | L10 |
| TheEyePU | 1 | L10 |

### multi/bb_bloodkeep
81 created instances · 2 unported entries (2 classes) · 1 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| TheEyePU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Missing or oversized sound resource | 1 | L2 |

### multi/bb_bodies
66 created instances · 2 unported entries (2 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |

### multi/bb_brownstar
132 created instances · 2 unported entries (2 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| InvulnerabilityPU | 1 | L10 |
| TheEyePU | 1 | L10 |

### multi/bb_cabana
65 created instances · 3 unported entries (3 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| RevenantPU | 1 | L10 |
| StealthPU | 1 | L10 |

### multi/bb_chosenarena
123 created instances · 2 unported entries (2 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |

### multi/bb_condemned
181 created instances · 4 unported entries (4 classes) · 3 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |
| StealthPU | 1 | L10 |
| TheEyePU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Missing or oversized sound resource | 2 | L2 |
| Door | Open or non-manifold container shell: 4 edges / balance 0 in component 3 | 1 | L4 |

### multi/bb_crypt
116 created instances · 6 unported entries (4 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| Container | 3 | L11 |
| InvulnerabilityPU | 1 | L10 |
| ParticleSystem | 1 | L11 |
| StealthPU | 1 | L10 |

### multi/bb_drain
50 created instances · 2 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| StealthPU | 2 | L10 |

### multi/bb_fields
97 created instances · 8 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| Minefield | 8 | L11 |

### multi/bb_jgrave
135 created instances · 11 unported entries (4 classes) · 4 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| TimeBombPU | 8 | L10 |
| AngerPU | 1 | L10 |
| NightGogglesPU | 1 | L10 |
| TheEyePU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Invalid sound resource path | 4 | L2 |

### multi/bb_power
140 created instances · 1 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |

### multi/bb_rooftop
104 created instances · 4 unported entries (4 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| Container | 1 | L11 |
| InvulnerabilityPU | 1 | L10 |
| StealthPU | 1 | L10 |

### multi/ctf_cabana
122 created instances · 8 unported entries (4 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 2 | L10 |
| FlagStand | 2 | L11 |
| RevenantPU | 2 | L10 |
| StealthPU | 2 | L10 |

### multi/ctf_deadcities
153 created instances · 8 unported entries (4 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 2 | L10 |
| FlagStand | 2 | L11 |
| NightGogglesPU | 2 | L10 |
| TheEyePU | 2 | L10 |

### multi/ctf_dmx
99 created instances · 7 unported entries (3 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| TimeBombPU | 4 | L10 |
| FlagStand | 2 | L11 |
| AngerPU | 1 | L10 |

### multi/ctf_jam
219 created instances · 21 unported entries (7 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| TimeBombPU | 8 | L10 |
| TheEyePU | 4 | L10 |
| AngerPU | 2 | L10 |
| BinocularsPU | 2 | L10 |
| FlagStand | 2 | L11 |
| NightGogglesPU | 2 | L10 |
| InvulnerabilityPU | 1 | L10 |

### multi/bb_midgard

Passes entry validation but `--start-game` fails on the first update:
`NecroWardPU0` starts 1.29 units inside world polygon 449 (MoveObject refuses
start penetration deeper than 0.01; depenetration is not implemented). Batch L14.

