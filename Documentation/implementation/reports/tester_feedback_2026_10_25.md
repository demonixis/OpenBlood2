# Tester feedback, 2026-10-25

Changes on top of `94793720`. The user confirmed that the CityHub door is the one
in the supplied screenshot. Museum's crash location, action and tested build are
still unknown. No commercial assets or reference-source dependencies are added.
M2's reservation and milestone acceptance are unchanged.

## CityHub laundromat doors

The screenshot matches `Textures/Greys/doormetal5.dtx` on `laundrodoor1` and
`laundrodoor2`, at (1538,480,416) and (1538,480,480). These are unlocked,
self-triggered RotatingDoors, not the locked/stationary `moteldoor`.

Before the correction, a player USE from (1480,460,416), facing +X, activates the
first leaf's trigger, but every rotation is blocked by `laundrodoor2` (180 retries
in the three-second probe). The conservative envelope treats the adjacent leaf
as an immovable obstacle at their shared seam.

Two kinematic world models that both carry the original GOTHRUWORLD flag no longer
block each other's rotation. Other brushes, ordinary bodies and free debris remain
in the transaction. The reference runtime's `CollideAgainstWorld` respects the
pushed object's GOTHRUWORLD flag; its MoveObject collision also exempts solid world
models for a world-bypassing mover. This motivates the mutual kinematic exemption,
without claiming that our atomic solver reproduces that later runtime wholesale.

`interaction.city_laundry` opens and closes each authored leaf independently through
player USE while its neighbour stays closed. `interaction.blocking` checks that a
neighbour without GOTHRUWORLD still blocks, alongside the existing blocked-body
rollback, transport and callback tests. No DAT, lock, trigger or angular speed changes.

## Scripted cameras and controls

Both frontend paths now pass the camera ownership gate to MovementControls::look.
Scripted cameras suppress mouse, touch, keyboard and gamepad look, including when
AllowPlayerMovement is true. Suppressed relative deltas are consumed, preventing a
jump when the camera releases. Death camera look is also blocked; Eye and Orb retain
player steering. Movement/use/fire permissions are unchanged.

Functional empty control rows already accept rebinding. The actual ambiguity is the
nine disabled, unported rows being displayed as unassigned: Strafe, RunLock,
TurnAround, DropWeapon, Grab, Message, Taunt, MouseAimToggle and Crosshair. They now
show NOT IMPLEMENTED. The regression clears Jump and binds it again through the real
menu input path. This does not implement those nine actions; another failing action
example from the tester would still need investigation.

## Texture detail

The Beretta skin is loaded at its full 256x256 resolution with all four authored mips;
there is no blanket low-resolution decode or upload. The shared repeat sampler used
nearest texel/mip sampling with no anisotropy. A same-pose, same-size isolated weapon
comparison removes the coarse grazing-angle blocks using trilinear filtering and
8x anisotropy. This is the new repeat sampler for both renderer modes, including
world textures. Clamped sprite/FX textures, HUD and menu art remain unchanged.

The CPU and GPU model paths share the filter. No shader, UV, mip bias, asset rewrite,
or forced mip-zero shortcut is introduced. Earlier reference images intentionally
change in texture sampling. Metal verification does not establish Windows/Vulkan
visual parity or Quest headset performance.

The GPU regression checks interpolated texel boundaries and an 8:1 UV footprint
with green fine mips and red coarse mips: the anisotropic sample must remain green.
It also retains repeat, row orientation, alpha policy, cache replacement and invalid
upload checks. CPU/GPU skinning equivalence and ORIGINAL/ENHANCED/post-process
comparisons pass.

## Museum and class inventory

See [class coverage](../../contracts/class_coverage.md) for all 40 unregistered original
names, split into replacement services, a base class, add-on classes and remaining
absent registrations. This is distinct from save support or missing resources.

Current Museum inspection has 1436 authored objects and 705 original instances.
There is one explicitly omitted CDestructableBrush without a world model, rather
than an unregistered class. Entry plus 30 seconds, `locomotion.museum_duct`, and the
difficulty-1 save audit after entry/combat pass. The audit restores 444 records /
478303 bytes and runs subsequent simulation. The reported crash is **unreproduced**,
not fixed. A failing route/action, tested build or pre-crash save remains necessary.

## Verification

- Release application and affected test targets compile on macOS arm64.
- 39 targeted input/camera/interaction/inventory/Orb/death/Museum/GPU checks pass.
- Menu rendering/traversal, enhanced surface and post-processing GPU tests pass.
- Final camera multi-device checks and color-coded anisotropic mip test pass.
- Museum save audit: `save_audit_tests worlds <scratch> <retail-data> 1 03_museum` passes.
- Quest build produces its APK; no headset, simulator or emulator was run.
- Control-menu capture inspected: disabled labels fit and remain visually distinct.

Local captures and test logs are retained under `build/release/tests/tester-feedback/`
(ignored build output, including the proprietary-texture comparison). The run does
not certify the full campaign, reproduce the Windows crash, or close M2-GPU-001.
