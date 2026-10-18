# Per-world gaps and implementation batches

Snapshot of what blocks each mounted Blood II world from starting, grouped into
implementation batches. Measured on 2026-10-18 at `b52bb66` against the retail
2.1 data (BLOOD2, BLOOD2P, BLOOD2P2, BLOOD2NM and SOUND archives), difficulty 1,
with `blood2 --inspect-world --world <name>` for every world the inspector Levels
tab lists. The counts match the Levels-tab rejection messages.

This is a backlog, not an acceptance record. It covers only world-entry
validation (class registration and creation). A world that passes here can still
fail later on presentation resources, runtime services or gameplay; `drop` and
`multi/bb_deathcanyon` already pass entry validation and fail on client music.
Earlier per-world counts in the contracts are historical.

## How to refresh

```bash
blood2 --data-dir <Blood2> --inspect-world --world <name>
```

The summary lists `Class / Class not ported: N`, and each failed entry is now
logged as `Failed entry <index> (<class>): <detail>`. A failed entry whose detail
is only `Rejected precreate: <class>` does not say why; batch L0 fixes that. The
causes of those rejections below were found with temporary tracing in the
precreate paths (`CDestructableBrush`, `CDestructableModel::ReadProp`,
`AI_Mgr::RuntimePrecreate`, `ServerAdapter::ValidateSpawnObject`).

## Status by world

`starts` means the world already passes entry validation (01_SubwayA,
02_CityHub, 05_CDMB). Batches are defined in the next section.

| World | Created | Unported (classes) | Failed | Batches needed |
|---|---:|---:|---:|---|
| 01_subwaya | 204 | 0 (0) | 0 | **starts** |
| 02_cityhub | 453 | 0 (0) | 0 | **starts** |
| 02_cityhubb | 390 | 19 (3) | 2 | L1, L7 |
| 02_cityhubc | 370 | 34 (4) | 2 | L2, L7, L10 |
| 03_museum | 554 | 86 (6) | 11 | L1, L4, L6, L8, L10, L11, L12 |
| 04_steamtunnels | 447 | 55 (6) | 9 | L1, L2, L5, L6, L7, L10 |
| 05_cdm | 508 | 14 (5) | 28 | L1, L3, L6, L7, L10, L11 |
| 05_cdmb | 129 | 0 (0) | 0 | **starts** |
| 06_airship1 | 187 | 0 (0) | 11 | L1, L4, L6, L12 |
| 06_airship2 | 243 | 2 (2) | 17 | L6, L10 |
| 07_cathedral | 205 | 18 (9) | 6 | L1, L2, L6, L7, L8, L10, L11 |
| 08_subwayb | 174 | 0 (0) | 1 | L1 |
| 09_abandoned_buildings | 638 | 38 (11) | 35 | L2, L3, L4, L5, L6, L7, L8, L10, L11 |
| 10_sewagetreatmentplant | 99 | 25 (5) | 1 | L6, L7, L11 |
| 11_slaughter_house | 465 | 26 (8) | 16 | L4, L6, L7, L8, L10, L12 |
| 21_laboratory | 280 | 38 (9) | 17 | L1, L4, L6, L7, L8, L11 |
| 22_office | 482 | 6 (3) | 17 | L2, L4, L6, L7, L8, L10 |
| 23_rooftop | 136 | 8 (6) | 2 | L1, L8, L9, L10, L11 |
| 24_ancientcity | 254 | 57 (9) | 2 | L3, L6, L7, L8, L10 |
| 24b_arena | 149 | 18 (11) | 3 | L3, L7, L9, L10, L11 |
| 25_ancientone | 123 | 30 (10) | 1 | L2, L8, L9, L10, L11 |
| credits | 21 | 1 (1) | 0 | L9 |
| drop | 5 | 0 (0) | 0 | L1 |
| end | 20 | 4 (4) | 0 | L9 |
| multi/bb_area15 | 75 | 1 (1) | 0 | L10 |
| multi/bb_bloodarena | 16 | 5 (5) | 109 | L3, L10 |
| multi/bb_bloodkeep | 81 | 2 (2) | 1 | L2, L10 |
| multi/bb_bodies | 12 | 2 (2) | 54 | L3, L10 |
| multi/bb_brownstar | 132 | 2 (2) | 0 | L10 |
| multi/bb_cabana | 65 | 3 (3) | 0 | L10 |
| multi/bb_chosenarena | 123 | 2 (2) | 0 | L10 |
| multi/bb_condemned | 177 | 4 (4) | 7 | L2, L4, L10 |
| multi/bb_crypt | 116 | 6 (4) | 3 | L1, L10, L11 |
| multi/bb_deathcanyon | 44 | 0 (0) | 0 | L1 |
| multi/bb_drain | 50 | 2 (1) | 0 | L10 |
| multi/bb_fields | 97 | 8 (1) | 0 | L11 |
| multi/bb_jgrave | 131 | 11 (4) | 8 | L2, L4, L10 |
| multi/bb_midgard | 13 | 0 (0) | 68 | L3 |
| multi/bb_power | 140 | 1 (1) | 0 | L10 |
| multi/bb_rooftop | 104 | 4 (4) | 1 | L1, L10, L11 |
| multi/ctf_cabana | 122 | 8 (4) | 0 | L10, L11 |
| multi/ctf_deadcities | 153 | 8 (4) | 0 | L10, L11 |
| multi/ctf_dmx | 97 | 7 (3) | 2 | L4, L10, L11 |
| multi/ctf_jam | 219 | 21 (7) | 0 | L10, L11 |

Closest to starting:

- **L1 only:** `08_subwayb`, `drop`, `multi/bb_deathcanyon`.
- **L3 only:** `multi/bb_midgard`.
- **L9 only:** `credits`, `end` (the Chosen as scripted actors).
- **L10 only:** `multi/bb_area15`, `bb_brownstar`, `bb_cabana`, `bb_chosenarena`, `bb_drain`, `bb_power`.
- **L11 only:** `multi/bb_fields`.
- **L1 + L7:** `02_cityhubb`. **L6 + L10:** `06_airship2`.

## Batches

Batches L0–L5 are fixes and policies on code that already exists; L6–L12 port
new original code. Unported-class totals count source entries over all 44 worlds.

### L0 — Diagnostics

Keep the reason of a precreate rejection in the entry detail (today the object is
removed and only `Rejected precreate: <class>` survives). The causes exist
already: `AI_Mgr::runtime_error`, `CDestructableModel::error`, the
`CDestructableBrush` early returns and the `ValidateSpawnObject` result. No world
is unblocked, but every later batch becomes measurable from the inspector.

### L1 — Quick fixes in ported code (13 worlds)

- **Sound with `InnerRadius == OuterRadius`** (17 entries: 04, 06_airship1, 21,
  23, bb_crypt, bb_rooftop). `AudioMixer::play` requires `outer > inner`;
  authored ambient loops use equal radii (7000/7000, 600/600, 50/50…). Confirm
  the original attenuation for equal radii and accept it.
- **SpawnObject allow-list** (`ServerAdapter::ValidateSpawnObject`) misses
  classes that are already registered: `MedKitPU` (5), `FlashlightPU` /
  `FlashLightPU` (3), `FlareGunPU` (2), `SubMachineGunPU`, `AssaultRiflePU`.
  Affects CDestructableBrush/Model containers in 02_cityhubb, 03, 04, 05_cdm,
  06_airship1, 07, 08_subwayb.
- **Empty music lists** (`drop`, `multi/bb_deathcanyon`): WorldProperties declare
  no CD track, directory or lists; "Finite playlist is empty" should mean no
  music, not a failed level start.

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

### L3 — Legacy property schemas (7 worlds)

- **`TypeAction`** instead of `TypeActionMode` (231 entries, only
  `multi/bb_bloodarena`, `bb_bodies`, `bb_midgard`): Door, Ladder, RotatingBrush,
  BigTrigger, Water, WorldProperties and all pickups. The contracts forbid a
  speculative alias; establish what the original does with an unknown world
  property (most likely ignored by `GetProp*`) and apply that rule generally.
- **CGib placed in world files** (27 entries, 05_cdm 24, 09 3): `CGib` is
  registered without properties, but the DAT rows carry the CDestructable schema
  (`DamageTriggerMessage`, `InitFilename`, `Kickable`, `ObjectMass`…). Port the
  world-file CGib properties.
- **Spawner `SpawnObject` rejected by the schema** (4 entries, 24_ancientcity,
  24b_arena): probably an over-length spawn string with properties; confirm and
  raise the bound to the original's.

### L4 — World-model name resolution and shells (9 worlds)

- **Ambiguous named world models** (28 entries): Door, CDestructableBrush,
  RotatingDoor and Ladder (container) whose name matches several BSP models
  (03, 06_airship1, 11, 21, 22, bb_condemned, bb_jgrave, ctf_dmx). Establish the
  original engine's choice for duplicate names (first match?) and replace the
  refusal by that rule.
- **Open/non-manifold door shell** `Buildings` (bb_condemned) and **container
  face not on its plane** `helloyou1` (09): compound-shell/plane tolerance cases.

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

## Details by world

### 02_cityhubb

390 created instances · 19 unported entries (3 classes) · 2 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| SoulDrudge | 10 | L7 |
| BoneLeech | 7 | L7 |
| ShikariAI | 2 | L7 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableBrush | SpawnObject class not accepted: MedKitPU | 1 | L1 |
| CDestructableBrush | SpawnObject class not accepted: FlashlightPU | 1 | L1 |

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

554 created instances · 86 unported entries (6 classes) · 11 failed entries

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
| CDestructableBrush | SpawnObject class not accepted: MedKitPU | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: InvulnerabilityPU | 1 | L10 |
| CDestructableBrush | SpawnObject class not accepted: BinocularsPU | 1 | L10 |
| CultistAI | NPC world ground placement failed | 1 | L12 |
| CDestructableBrush | Ambiguous named world model | 4 | L4 |
| CDestructableModel | SpawnObject class not accepted: RocketAmmoPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: SHOTGUN | 1 | L6 |
| Prophet | authored AI weapon unavailable: SINGULARITY | 1 | L6 |

### 04_steamtunnels

447 created instances · 55 unported entries (6 classes) · 9 failed entries

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
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: FlareGunPU | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: SubMachineGunPU | 1 | L1 |
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat1.dtx | 1 | L2 |
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat2.dtx | 1 | L2 |
| DetailSprite | Missing, oversized or non-SPR sprite resource: sprites\bloodsplat3.dtx | 1 | L2 |
| CDestructableBrush | Gravity without BoxPhysics (polygon dynamics) | 2 | L5 |
| Fanatic | authored AI weapon unavailable: SHOTGU | 1 | L6 |

### 05_cdm

508 created instances · 14 unported entries (5 classes) · 28 failed entries

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
| CDestructableModel | SpawnObject class not accepted: MedKitPU | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: FlashlightPU | 1 | L1 |
| CGib | CGib: invalid property DamageTriggerMessage | 24 | L3 |
| CDestructableModel | SpawnObject class not accepted: TeslaCannonPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: TESLA | 1 | L6 |

### 06_airship1

187 created instances · 0 unported entries (0 classes) · 11 failed entries

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 2 | L1 |
| CDestructableBrush | SpawnObject class not accepted: MedKitPU | 1 | L1 |
| CDestructableBrush | SpawnObject class not accepted: FlashLightPU | 1 | L1 |
| CultistAI | NPC world ground placement failed | 3 | L12 |
| Door | Ambiguous named world model | 2 | L4 |
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

205 created instances · 18 unported entries (9 classes) · 6 failed entries

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
| CDestructableModel | SpawnObject class not accepted: FlareGunPU | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: AssaultRiflePU | 1 | L1 |
| CDestructableModel | SpawnObject class not accepted: DieBugDieAmmo | 2 | L2 |
| Sprite | Missing, oversized or non-SPR sprite resource:  | 1 | L2 |
| CDestructableModel | SpawnObject class not accepted: ShotgunPU | 1 | L6 |

### 08_subwayb

174 created instances · 0 unported entries (0 classes) · 1 failed entries

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableBrush | SpawnObject class not accepted: MedKitPU | 1 | L1 |

### 09_abandoned_buildings

638 created instances · 38 unported entries (11 classes) · 35 failed entries

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
| CGib | CGib: invalid property DamageTriggerMessage | 3 | L3 |
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

465 created instances · 26 unported entries (8 classes) · 16 failed entries

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
| CDestructableBrush | Ambiguous named world model | 2 | L4 |
| CDestructableModel | SpawnObject class not accepted: HowitzerPU | 1 | L6 |
| CDestructableModel | SpawnObject class not accepted: ShotgunPU | 1 | L6 |
| Prophet | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| Fanatic | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| CDestructableModel | SpawnObject class not accepted: BoneLeech | 5 | L7 |

### 21_laboratory

280 created instances · 38 unported entries (9 classes) · 17 failed entries

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
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 8 | L1 |
| CDestructableBrush | Ambiguous named world model | 2 | L4 |
| CDestructableModel | SpawnObject class not accepted: boneleech | 7 | L7 |

### 22_office

482 created instances · 6 unported entries (3 classes) · 17 failed entries

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
| CDestructableBrush | Ambiguous named world model | 6 | L4 |
| RotatingDoor | Ambiguous named world model | 1 | L4 |
| Door | Ambiguous named world model | 1 | L4 |
| Fanatic | authored AI weapon unavailable: TESLACANNON | 3 | L6 |
| CDestructableBrush | SpawnObject class not accepted: RocketAmmoPU | 1 | L6 |
| Fanatic | authored AI weapon unavailable: SNIPERRIFLE | 1 | L6 |
| Fanatic | authored AI weapon unavailable: NAPALMCANNON | 1 | L6 |
| Prophet | authored AI weapon unavailable: MINIGUN | 1 | L6 |

### 23_rooftop

136 created instances · 8 unported entries (6 classes) · 2 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 2 | L10 |
| Container | 2 | L11 |
| CalebAI | 1 | L9 |
| EnhancedGideon | 1 | L8 |
| LifeLeechPU | 1 | L10 |
| OpheliaAI | 1 | L9 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 2 | L1 |

### 24_ancientcity

254 created instances · 57 unported entries (9 classes) · 2 failed entries

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
| Spawner | Spawner: invalid property SpawnObject | 1 | L3 |
| Fanatic | authored AI weapon unavailable: SHOTGUN | 1 | L6 |

### 24b_arena

149 created instances · 18 unported entries (11 classes) · 3 failed entries

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

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Spawner | Spawner: invalid property SpawnObject | 3 | L3 |

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

### drop

5 created instances · 0 unported entries (0 classes) · 0 failed entries · client music refused

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| WorldProperties | No music lists authored; client music start refuses the empty playlist | – | L1 |

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

16 created instances · 5 unported entries (5 classes) · 109 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| BinocularsPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |
| StealthPU | 1 | L10 |
| TheEyePU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Door | Door: invalid property TypeAction | 24 | L3 |
| Ladder | Ladder: invalid property TypeAction | 14 | L3 |
| BulletAmmoPU | BulletAmmoPU: invalid property TypeAction | 12 | L3 |
| ProximitiesPU | ProximitiesPU: invalid property TypeAction | 8 | L3 |
| RotatingBrush | RotatingBrush: invalid property TypeAction | 8 | L3 |
| HealthPU | HealthPU: invalid property TypeAction | 7 | L3 |
| ShellAmmoPU | ShellAmmoPU: invalid property TypeAction | 6 | L3 |
| WardPU | WardPU: invalid property TypeAction | 4 | L3 |
| FuelAmmoPU | FuelAmmoPU: invalid property TypeAction | 4 | L3 |
| HowitzerAmmoPU | HowitzerAmmoPU: invalid property TypeAction | 4 | L3 |
| DieBugDieAmmoPU | DieBugDieAmmoPU: invalid property TypeAction | 4 | L3 |
| BatteryAmmoPU | BatteryAmmoPU: invalid property TypeAction | 4 | L3 |
| RemotesPU | RemotesPU: invalid property TypeAction | 2 | L3 |
| FlareAmmoPU | FlareAmmoPU: invalid property TypeAction | 2 | L3 |
| BMGAmmoPU | BMGAmmoPU: invalid property TypeAction | 2 | L3 |
| WorldProperties | WorldProperties: invalid property TypeAction | 1 | L3 |
| MegaHealthPU | MegaHealthPU: invalid property TypeAction | 1 | L3 |
| NecroWardPU | NecroWardPU: invalid property TypeAction | 1 | L3 |
| MedKitPU | MedKitPU: invalid property TypeAction | 1 | L3 |

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

12 created instances · 2 unported entries (2 classes) · 54 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| InvulnerabilityPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Door | Door: invalid property TypeAction | 8 | L3 |
| RotatingBrush | RotatingBrush: invalid property TypeAction | 8 | L3 |
| ShellAmmoPU | ShellAmmoPU: invalid property TypeAction | 6 | L3 |
| BulletAmmoPU | BulletAmmoPU: invalid property TypeAction | 4 | L3 |
| HealthPU | HealthPU: invalid property TypeAction | 4 | L3 |
| BatteryAmmoPU | BatteryAmmoPU: invalid property TypeAction | 3 | L3 |
| FlareAmmoPU | FlareAmmoPU: invalid property TypeAction | 3 | L3 |
| FuelAmmoPU | FuelAmmoPU: invalid property TypeAction | 3 | L3 |
| ProximitiesPU | ProximitiesPU: invalid property TypeAction | 3 | L3 |
| Water | Water: invalid property TypeAction | 2 | L3 |
| DieBugDieAmmoPU | DieBugDieAmmoPU: invalid property TypeAction | 2 | L3 |
| HowitzerAmmoPU | HowitzerAmmoPU: invalid property TypeAction | 2 | L3 |
| WorldProperties | WorldProperties: invalid property TypeAction | 1 | L3 |
| BMGAmmoPU | BMGAmmoPU: invalid property TypeAction | 1 | L3 |
| MegaHealthPU | MegaHealthPU: invalid property TypeAction | 1 | L3 |
| NecroWardPU | NecroWardPU: invalid property TypeAction | 1 | L3 |
| WardPU | WardPU: invalid property TypeAction | 1 | L3 |
| Ladder | Ladder: invalid property TypeAction | 1 | L3 |

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

177 created instances · 4 unported entries (4 classes) · 7 failed entries

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
| Door | Ambiguous named world model | 2 | L4 |
| Ladder | Ambiguous named container model | 2 | L4 |
| Door | Open or non-manifold container shell: 4 edges / balance 0 in component 3 | 1 | L4 |

### multi/bb_crypt

116 created instances · 6 unported entries (4 classes) · 3 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| Container | 3 | L11 |
| InvulnerabilityPU | 1 | L10 |
| ParticleSystem | 1 | L11 |
| StealthPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 3 | L1 |

### multi/bb_deathcanyon

44 created instances · 0 unported entries (0 classes) · 0 failed entries · client music refused

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| WorldProperties | No music lists authored; client music start refuses the empty playlist | – | L1 |

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

131 created instances · 11 unported entries (4 classes) · 8 failed entries

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
| Door | Ambiguous named world model | 4 | L4 |

### multi/bb_midgard

13 created instances · 0 unported entries (0 classes) · 68 failed entries

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Door | Door: invalid property TypeAction | 12 | L3 |
| HealthPU | HealthPU: invalid property TypeAction | 10 | L3 |
| RotatingBrush | RotatingBrush: invalid property TypeAction | 9 | L3 |
| BulletAmmoPU | BulletAmmoPU: invalid property TypeAction | 6 | L3 |
| FlareAmmoPU | FlareAmmoPU: invalid property TypeAction | 5 | L3 |
| BigTrigger | BigTrigger: invalid property TypeAction | 4 | L3 |
| ProximitiesPU | ProximitiesPU: invalid property TypeAction | 4 | L3 |
| ShellAmmoPU | ShellAmmoPU: invalid property TypeAction | 4 | L3 |
| FuelAmmoPU | FuelAmmoPU: invalid property TypeAction | 3 | L3 |
| BatteryAmmoPU | BatteryAmmoPU: invalid property TypeAction | 2 | L3 |
| DieBugDieAmmoPU | DieBugDieAmmoPU: invalid property TypeAction | 2 | L3 |
| HowitzerAmmoPU | HowitzerAmmoPU: invalid property TypeAction | 2 | L3 |
| WorldProperties | WorldProperties: invalid property TypeAction | 1 | L3 |
| BMGAmmoPU | BMGAmmoPU: invalid property TypeAction | 1 | L3 |
| MedKitPU | MedKitPU: invalid property TypeAction | 1 | L3 |
| MegaHealthPU | MegaHealthPU: invalid property TypeAction | 1 | L3 |
| NecroWardPU | NecroWardPU: invalid property TypeAction | 1 | L3 |

### multi/bb_power

140 created instances · 1 unported entries (1 classes) · 0 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |

### multi/bb_rooftop

104 created instances · 4 unported entries (4 classes) · 1 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| AngerPU | 1 | L10 |
| Container | 1 | L11 |
| InvulnerabilityPU | 1 | L10 |
| StealthPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| Sound | Invalid or over-limit sound request (InnerRadius == OuterRadius) | 1 | L1 |

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

97 created instances · 7 unported entries (3 classes) · 2 failed entries

**Unported classes**

| Class | Entries | Batch |
|---|---:|---|
| TimeBombPU | 4 | L10 |
| FlagStand | 2 | L11 |
| AngerPU | 1 | L10 |

**Failed entries**

| Class | Cause | Entries | Batch |
|---|---|---:|---|
| CDestructableBrush | Ambiguous named world model | 2 | L4 |

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
