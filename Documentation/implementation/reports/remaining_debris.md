# Remaining destruction materials — VFX lot 9

2026-10-25. [Contract](../../contracts/destruction_debris_contracts.md#remaining-materials--lot-9-2026-10-25).

Restores CDebris stone, terrain, plastic and flesh, plus the original metal fallback
for liquid/energy. Original `gibfx.cpp`, `SFXMgr.cpp`, `Debris.cpp` and `ClientGibFX.cpp`
remain the references; [source hashes](../sources/remaining_debris_sources.json).

Verification includes material scale/bounce rules, no unsolicited blood from CDebris,
LOW/MEDIUM/HIGH quantity rules, eviction, simulation with audio advancement, real
CityHub destruction and all nine surface requests against retail resources. GPU captures
cover all nine surfaces with actual model/skin resources and independent fragment motion;
the terrain capture was inspected. The initial expanded simulation probe exhausted audio
voices because it advanced fragments without the mixer; it now advances both. The old
retail test's expected energy-material failure is replaced by the source metal fallback.

Rain and generic legacy emitters stay P3 pending an evidenced live campaign caller, as
specified by the inventory. This is material implementation and isolated rendering
coverage, not an original-executable A/B comparison or campaign acceptance.

Release: 6/6 material/destruction/GPU checks pass. ASan/UBSan's new remaining-material
case passes (macOS LeakSanitizer disabled). The application and Quest APK build passed
with these shared-pool changes; no simulator or device run. [Release evidence](evidence/vfx9-release.txt).
The Quest log is shared with [lot 8](evidence/vfx8-quest.txt).
