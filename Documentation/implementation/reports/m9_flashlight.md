# M9 — client flashlight replica

Implemented against `8fc5bd63`, continuing [moving effects](m9_effect_services.md)
under the [M9 contract](../../contracts/multiplayer_contracts.md).

## Delivered boundary

CFlashlightFX now lives in the independently linked client effect library. Its
source and owner are world-qualified generational identities. Updates read the
source pose/visibility from ClientWorld and cast into the owned client collision
scene; the local player's body is still excluded. Current presentation checks
only replica membership and visibility, including immediate hide/removal, without
reading original objects or running another ray. Hit metadata uses ObjectId.

The server CInvFlashlight code is unchanged: battery, rounded count, toggle/actions,
child creation/removal and authoritative body/pitch pose remain gameplay rules.
Keep the source body origin, minimum distance 10, radius distance * .4, intensity
formula and failed point-shade fallback 132/255. Misses, stale owners and hidden
children remain invisible. Invalid/unavailable queries fail and hide the light.
World or slot reuse cannot revive an old effect. A mismatched collision epoch is
unavailable, never a fallback to the live server.

The local session publishes owner objects after authoritative completion when a
flashlight child exists. An active light requests the stable collision copy,
which the later moving FX phase reuses. This does not advance CMoveMgr, physics,
contacts, battery, audio or RNG. Other presentation paths and PBR files are unchanged.
No new saved payload or network message is introduced.

Direct legacy fixtures now publish owner/collision snapshots explicitly before
calling client light services. The trusted local ClientWorld frame does not yet
validate every pose field; malformed fixture coordinates still reach the bounded
client ray and are refused there, preserving the original invalid-ray behavior.
An initial test incorrectly assumed publication rejected those fields; its failed
output is retained. Untrusted wire-frame validation remains a later requirement.
The [source ledger](../sources/m9_flashlight_sources.json) records provenance.

## Verification

Linux native checks use the isolated GCC runtime and Clang ASan/UBSan trees from
[the runtime lot](m9_effect_queries.md). The new standalone `network.flashlight`
links no game/server registry. It checks body exclusion, primary and object hits,
source distance/intensity math, miss metadata, hide/removal, object/world reuse,
unavailable geometry and epoch mismatch. Existing tests cover original battery,
input, saved inventory restore, teardown and actual retail rendering.

The [validation record](evidence/m9_flashlight_validation_2026-10-30.json) records
**12/12 distinct headless cases after the fixture correction**, **2/2 GPU cases**
and **3/3 ASan/UBSan cases**. All five preservation traces remain byte-identical
(240 FX / 360 audio / 360 movement / 720 environment / 300 camera-HUD frames).
Sanitized dedicated Crypt completes 60 ticks with unavailable audio/video drivers,
zero clients and zero discarded ticks. All 6,265 protected hashes are unchanged,
and the standalone test has no server/game registry symbols. Synthetic,
private-data and hardware tests remain separate; no emulator or simulator is used.

## Remaining work

The extracted cosmetic pools and flashlight can now run using client values,
geometry and output services. Immediate original-game creation still uses the local
read-only query bridge where necessary. Wire events, asset lookup/admission,
remaining client-shell services and playable network sessions remain subsequent
M9 work. This is not LAN acceptance. Windows/macOS/mobile/Quest and human play
remain unqualified. Earlier PolyGrid pixel evidence, M2-GPU-001 and M3 acceptance
are unchanged; protected PBR/reference files are excluded.
