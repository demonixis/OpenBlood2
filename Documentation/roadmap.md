# Roadmap

Reviewed on 2026-10-20 against `39b622c` and
[audit 15](implementation/reports/evidence/all-levels-audit-15.log).
Percentages are task estimates, not acceptance.

## Overall progress

| Scope | Estimate | Basis |
| --- | ---: | --- |
| Solo campaign worlds that open | 36 / 38 | All but 24_ancientcity and 25_ancientone. |
| All mounted worlds that open | 52 / 58 | Multiplayer maps run solo inspection only; the four CTF maps lack FlagStand. |
| Complete desktop game (M0–M6) | **~74%** | Weighted milestones below. |
| Desktop game with LAN multiplayer (M0–M6, M9) | **~63%** | Adds the new M9. |
| Whole roadmap, including mobile and VR (M0–M9) | ~52% | M7 and M8 have not started. |

Weights reflect estimated effort: M0 4, M1 6, M2 30, M3 12, M4 12, M5 12,
M6 8, M9 16, M7 10, M8 10. Optional extensions (Mods, editor, PBR and
post-processing) are not counted.

"Opens" means the world passes the
[all-level audit](implementation/reports/all_level_loading.md): player entry at
three difficulties and three seconds of simulation. That audit does not test
traversal or combat. The remaining classes (UndeadGideon, AncientOne,
AncientOneTentacle, FlagStand) are being ported in separate work.

## Milestones

| Milestone | Completion | Scope |
| --- | --- | --- |
| M0 | 100% | Independent C++20 build, SDL_GPU, configuration, logs and inspector. |
| M1 | 100% | Original-code integration: objects, typed services, callbacks and lifetime. |
| M2 | 100% — closed with reservation | World readers, gameplay services and the selected SubwayA → CityHub session. |
| M3 | ~65% — current | In-game acceptance: the whole solo campaign is played and recorded from new game to credits. |
| M4 | ~55% | Original rendering validated on Vulkan/Linux, Metal/macOS and D3D12/Windows. |
| M5 | ~35% | Complete game: save/load, remaining cinematics, menus, game rules and FX. |
| M6 | ~40% | Desktop packages, measured optimization and sustained-play validation. |
| M7 | 0% | Android/iOS: ARM64, touch/controller input, storage, lifecycle and adaptive UI. |
| M8 | 0% | OpenXR: stereo, tracked controllers, aiming, menus, comfort and session recovery. |
| M9 | ~5% | LAN multiplayer: Bloodbath, CTF and Team on the original multiplayer maps. |

[M2](implementation/reports/m2_acceptance.md) is closed with **M2-GPU-001**, an unresolved
NVIDIA/D-Bus GPU LeakSanitizer failure. Only the user can accept a milestone.
M3–M9 remain unaccepted.

## M3 — in-game acceptance

M3 is reached when the whole solo campaign is played in the game and recorded.
Automated checks do not replace this. The gate: on the inspector-free Release
build, play from a new game through every chapter, boss and transition to the
credits at one difficulty or more. No defect may block progress. Every
remaining defect is recorded.

| # | Task | Status |
| --- | --- | --- |
| 3.1 | All mounted worlds open (all-level audit) | 52 / 58. UndeadGideon, AncientOne/Tentacle and FlagStand are in progress. |
| 3.2 | Campaign chaining: every authored NEW_LEVEL/EndEpisode in order, with KeepAlive inventory and stats | SubwayA → CityHub has a scripted test. The user has played the rest without a recorded result. |
| 3.3 | Per-world in-game record: objectives, scripts, cinematics, encounters, exit reached, difficulty | Not started. Needs a new evidence report. |
| 3.4 | Bosses and endgame: Gideon, EnhancedGideon, UndeadGideon, AncientOne, End script, credits | Gideon and EnhancedGideon work. The others are blocked on 3.1. |
| 3.5 | Input-driven traversal regressions, headless and GPU, one per chapter and boss | Only spawn → CityHub. |
| 3.6 | Retest the bugs players reported: post-transition stutter, idle Cultists, frozen corpses, NPC placement | Partly fixed; must be retested in the game. |
| 3.7 | M3 acceptance record for the user's decision | Not started. |

There is no save/load until M5, so each 3.3 record must be played in one
session or chained through a level transition.

## M9 — LAN multiplayer (evaluation)

Original model: the authoritative server runs the ObjectDLL objects. The client
shell runs its own movement (CMoveMgr) and effects. The engine copies object
state to every client. OpenBlood2 runs one session in one process with one
client. Presentation, HUD and audio read server objects directly. Typed client
messages exist as in-process values only, and there is no network code.

Original references are available. These include the multiplayer branches in
PlayerObj, BloodServerShell, the pickups and B2BaseClass, as well as FlagObjects,
TeamMgr, FragInfo, MessageMgr and NetStart. NetStart is a Win32 dialog wizard
that must be replaced. The later LithTech network layer, about 9k lines, is a
reference only. The source names Co-op, but only Bloodbath, CTF and Team maps
ship, so Co-op is assessed separately.

| # | Task | Size | Depends on |
| --- | --- | --- | --- |
| 9.0 | Contracts and audit: every direct server-state read by the app, every original multiplayer branch, the choice of transport library (for example a pinned ENet) | S | — |
| 9.1 | Client replica: presentation, HUD, audio and inspector read a client world that server snapshots feed (objects, models and animation, sprites, lights, world models, effects). Loopback solo must match current behavior, checked by the GPU readback tests. | XL | 9.0 |
| 9.2 | Wire protocol and transport: versioned, bounded encoding of every typed message; UDP with reliable and unreliable channels; fuzzed decoders | M | 9.0 |
| 9.3 | Sessions: listen and headless dedicated server, LAN discovery, handshake (protocol, data and mod fingerprint), join/leave/timeout, map rotation | M | 9.2 |
| 9.4 | Multi-player server: N players and clients, the original multiplayer branches, respawn, slow death and humiliation, frags, item respawn, game-type filtering, start points, frag and time limits | L | 9.1 |
| 9.5 | Game modes: Bloodbath; CTF with FlagStand, FlagObjects and TeamMgr; Team | M | 9.4, 3.1 |
| 9.6 | Network movement and fire: the original client-side movement upload with server bounds, remote interpolation, server-side hits, loss and latency | M–L | 9.2, 9.4 |
| 9.7 | Multiplayer UI: host and join screens, name, character and team, FragInfo scoreboard, MessageMgr chat | M | 9.3 |
| 9.8 | Validation: headless multi-client loopback on every bb_ and ctf_ map, simulated loss and latency, sanitizers, Windows/Linux/macOS interoperability, a LAN playtest | M | all |

Sizes: S ≈ 1 lot, M ≈ 2–3, L ≈ 4–5, XL ≈ 6–8, where a lot is one
continuation-sized change. Total: about 25–35 lots, roughly the remaining M3
and M5 work combined. Task 9.1 carries most of the risk because it changes how
presentation reads state. Coordinate it with the graphics work on the other
computer. Task 9.0 and the loopback part of 9.1 can start after M3; network
play needs the stable rules from M5.

## Next work

| Area | Planned work |
| --- | --- |
| Delivery order | M3, then M5/M4 → M6 → M9 (LAN); mobile and VR follow the desktop baseline. |
| Input | Rebindable actions, gamepad, hotplug and saved profiles; touch for M7, tracked controllers for M8. |
| Materials / PBR | External texture packs, material definitions, normal/roughness/metalness maps and an optional PBR path; retain original rendering. |
| Post-processing | Bloom, tonemapping, GTAO and SSR, with per-effect quality settings. |
| Lighting | Baked GI/probes, reflection probes and blending for moving objects. |
| Upscaling | Render scaling and FSR1; assess the GSR SDK and device support for mobile. |
| Mobile — M7 | Android/iOS packaging, content import, touch UI, audio focus, resume and real-device thermal tests. |
| VR — M8 | SDL/OpenXR, stereo cameras, controller actions, comfort locomotion and arm IK on the original rig; standalone headsets also need Android. |
| LAN — M9 | See the evaluation above. Internet play, a master server and NAT traversal are out of scope. |
| Mods — M10 | Versioned packages, load order/conflicts, replacements/custom levels and save compatibility. Existing REZ overlays are a first subset. |
| In-game editor — M11 | Hierarchy, asset browser, gizmos, undo/redo, edit/play modes, materials/probes and mod export. |
| Later | Reassess Shogo after Blood II's campaign and engine contracts are established. |

Graphics work starts from the reviewed baseline after the other computer's push.
