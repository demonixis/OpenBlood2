# View weapons on the controllers with arm IK on Meta Quest 3 (M8 lot 8.9)

2026-10-21, on top of `926912d4`. Contract: [OpenXR runtime](../../contracts/xr_runtime_contracts.md#arms-and-view-weapons-lot-89).
Plan: [OpenXR plan](../openxr_plan.md).

## Rig audit (retail data)

All 26 installed view models (`Models\Weapons\*_pv.abc`, `lh_*_pv.abc`) were dumped
(hierarchy, rest poses, per-node vertex bounds) and rendered as side views.

| Finding | Consequence |
| --- | --- |
| Arm chains follow one rule: `lu_arm*` → `ll_arm*` → `d_null*` (wrist) → hand mesh; numbers vary per model | Chains are discovered by name; no per-model chain data |
| The models are about 1/8 scale: Beretta hand 0.9 units, gun 1.3, forearm 1.71, upper arm 1.44; the flash offset (28 units) is ten times the geometry | 0.17 m per model unit. Lot 8.7 had assumed life-size models from the flash offset, which left the arm tiny and behind the controller |
| One-handed weapons hang the gun under the hand | The held hand is the grip; recoil and draw play relative to it |
| Eleven two-handed models have no trigger hand (M16, sniper, minigun, Tesla, death ray, howitzer, napalm, Singularity, `lh_` M16/napalm/Tesla) | Audited rest grips from the side views |
| Some parts are hidden only by the original framing: the Mac-10's and knife's spare left arms, the Singularity's lowered support arm, the M16's grenade | Masked when wholly outside the original view cone widened by 1.5 units; stocks and grips cut by the frame or near plane stay |
| Death ray, howitzer, napalm, Singularity and Tesla upper arms have no geometry | The forearm starts at the elbow (kept) |

## Results

| Check | Result |
| --- | --- |
| `arm_ik.*` (8 new headless) and `xr.player_shoulders` (new) | Pass |
| `arm_ik.catalog` (private): 26 models rigged, posed and solved at their definitions' offsets | Pass: 15 hand grips, 11 audited, none on the bounds fallback |
| `xr.*`, `model_view.*` and related view tests | 108/108 |
| Desktop GPU suite | 126/127; `gpu.entry_wait` fails as before the XR work |
| Desktop headless suite | 1356/1363; the same 7 private-data catalogue failures as at `c09a9d1f` |
| Quest build | No warnings in the changed files |
| Quest 3, not worn (controllers asleep) | SubwayA at 72/72 FPS, no view-weapon error in the status log; the untracked fallback now draws a life-size Beretta and arm in both eyes (it was at the nose, one eye only, before the fallback scale) |

## Not verified

With the headset worn and the controllers tracked: the grip placement and scale of every
weapon, the elbow and shoulder look, the dead-zone body yaw, two-handed grips, dual
weapons, and the framed-out masks in motion. The status log names each view weapon and any
IK error (`Invalid arm IK chain`) every 360 frames.
