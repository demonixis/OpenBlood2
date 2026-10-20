# M2 — Compact player HUD and numeric weapon commands

A compact HUD now displays delivered player health, armor, equipped weapon and
its ammunition above the 3D view. Physical **1..9 / 0** and the keypad select the
ten normal original inventory slots, initially **1 knife / 2 Beretta**. It works
with the inspector folded, hidden, or excluded from the build. **M2 remains incomplete.** Implementation commit: `cc329c6`.

Read the [contracts](../../contracts/hud_contracts.md),
[source hashes](../sources/m2_hud_sources.json) and
[validation ledger](evidence/m2-hud.json).

## What changed

The HUD uses the existing typed client snapshot, expanded with the current primary
and alternate ammo pools and owned weapon slots. It does not assume that every
weapon uses bullets. M16 alternate grenades are separate, melee displays `--`,
and empty ammunition is explicit. Health/armor truncate to bounded word values; ammunition retains the original
999 display cap while raw counts remain inspectable.
The original external-camera rule hides the HUD during cinematic views.

Numeric commands retain original slot ownership and dead/movement/camera guards.
Empty slots do not change weapons, while an owned ordinary weapon may be selected
with no ammunition. Key repeats do not repeat selection; focus loss, disabled
controls, text editing and player replacement consume/discard pending commands.
Physical keys work on QWERTY and AZERTY. The existing safe view-model replacement
and rollback remain in use, and the HUD itself takes no input focus.

One application-owned ImGui context now serves both the optional inspector and
HUD. Its SDL_GPU resources are destroyed before the device. Inspector-free builds
use the same pinned dependency; no dependency revision or shader changed. The
Session inspector exposes the exact delivered HUD fields and external-camera gate.

The panel is new compact presentation. Original `CNewStatusBar` PCX artwork/fonts,
air/focus/item bars, communication overlays and rebinding remain separate work.
The numeric mapping and stat selection come from the original sources; this is
not a claim that the full original graphical interface has been ported.

## Validation

The targeted suite passes **50/50 headless tests** in Debug with inspector,
Release without inspector, and Clang ASan/UBSan with leak detection. Ten cases use
the external retail assets. Debug and Release each pass **10/10 GPU tests** on
**NVIDIA GeForce RTX 3060 Laptop GPU**, with no Vulkan validation errors. Final
HUD-only reruns cover the display cap and event-time input guards in all three
configurations, plus NVIDIA readback in both graphical builds.

New cases verify finite/bounded values, knife/no-ammo presentation, distinct M16
ammo pools using real assets, original initial slots, empty-slot refusal,
dead/movement/camera gates, release of typed snapshots, all ten row/keypad keys,
repeat suppression, focus loss and same-frame inspector/control changes.
Selecting the already-equipped type leaves held fire input and inventory revision
unchanged, matching the original no-op.
Existing player, weapon, mouse combat, corpse clearance, save/restore and object
lifetime checks stay enabled. The real SubwayA–CityHub transfer test now also
asserts delivered health, armor, current ammo and owned slots across three round
trips. It does not assert the user's reported later firing sequence is fixed.

GPU readback creates/destroys the shared UI context three times and renders widths
1200, 600 and 320, with ordinary stats, low health/empty ammo, knife selection and
player exit. Pixels change when stats change, stay in the bottom panel, and clear
entirely after exit; the HUD never captures keyboard or mouse. The saved captures
were visually inspected. They remain local ignored build artifacts. Reference
integrity checks retain all 10,923 inventory file hashes and six clean reference
repositories. No commercial content is copied or changed. Native Metal execution
of this increment has not been tested here; existing bundled backends are retained.

## Next playtest checks requested by the user

These are reports from a two-level playtest, queued **after this HUD increment**.
They have not yet been independently reproduced or diagnosed here:

- Some Cultists remain standing and never activate. Inspect original activation,
  scripted state, perception and retained service errors before changing AI rules.
- Enemies appear unusually durable (roughly a magazine per enemy). Check the actual
  selected difficulty, health, armor, resistance and delivered hit damage; do not
  assume hard mode or rebalance the original rules.
- Shooting may stop after entering level two, but ammunition exhaustion is also
  plausible. Use the new ammo display and inspect retained inventory, client weapon
  animation/fire state and movement/camera restrictions across NEW_LEVEL. Existing
  transfer tests alone do not prove the reported firing sequence works.
- A water volume is visually absent in level two. Identify the authored volume and
  surface/rendering path; do not assume a missing brush or claim a visual fix from
  existing liquid movement tests.
- Picking up an item produces no audible feedback. Trace the original pickup sound
  request, asset resolution and actual playback; inventory acknowledgement is not
  proof that a sound was played.

These reports remain actionable follow-up work. No difficulty, AI, liquid-rendering
or pickup-audio change is included in this HUD increment. The M2 acceptance criteria
are unchanged and the autonomous continuation remains active.

Follow-up: [original collection sounds and diagnostic findings](m2_pickup_audio.md)
addresses pickup playback and records tested post-transfer firing without closing
the remaining playtest reports, including newly reported transition stutter.
