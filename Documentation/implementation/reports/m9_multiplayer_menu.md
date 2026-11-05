# M9 — Host/Join menus and LAN discovery

2026-10-31. Baseline: `1a2d44b4`. Delivery branch: `develop-multiplayer`.

## Delivered behavior

Backport note (2026-11-05): the screen labels are now English (Host, Join, Start game,
Join this game, Connect, Back); the French names below are the labels as delivered.

Normal retail desktop launch now offers **BLOODBATH → Héberger / Rejoindre**.
Choose name, one of the four Chosen, team preference, Bloodbath/Team/CTF, a mounted
multiplayer map, gameplay port, match thresholds, team/flag rules and rotation.
The existing defaults are unchanged (25 frags; host timed default 15 minutes,
dedicated 10). All 18 Bloodbath/Team and four CTF maps are offered by mode.

Rejoindre refreshes local servers every two seconds, shows name/address/mode/map
and occupancy, and offers direct IPv4/hostname connection. Discovery broadcasts
on UDP 27889, includes loopback, and advertises the actual gameplay port (default
27888). Full or incompatible-protocol rows remain visible with Join disabled.
Discovery never bypasses the complete mounted-data SHA-256/protocol/edition check.
No downloads or new dependencies are introduced; ENet remains pinned upstream.

The portable screen replaces the original Windows wizard; visual polish is deferred.
It uses existing ImGui rendering/fonts and keyboard/gamepad navigation. The original
Main artwork and demo Bloodbath promotion are retained. It is unavailable for demo,
Nightmares, overlays and XR. CLI operations remain supported.

Starting network play destroys the paused local session before creating a host.
Opening/closing the screen resets local command edges; background releases/focus
remain tracked and network exit releases SDL text input. The shared SDL/GPU/ImGui
runtime survives, and the existing network loop serves
both CLI and menu entry. Leave/End returns to Main, window close exits, and admission
failure returns to an error dialog. Back from setup leaves solo state intact.
Hosted simulation continues without local focus or while its menu is open.

## Protocol and lifetime

Discovery schema 1 is independent of gameplay protocol 23. Explicit little-endian
fields carry a nonce, server instance, protocol, port, mode, counts and bounded
strings. Packets are limited to 256 bytes, polling to 64 datagrams/frame, responses
to 20/second/server and browser entries to 64. A matching nonce has a three-second
response window; rows expire after six seconds. Instance/port deduplication prefers
LAN over loopback. Shared ENet acquisition is reference counted across transports
and raw discovery sockets, including their failure paths. Discovery failure leaves
direct networking usable. No gameplay or rendering algorithm is changed.

## Verification

Raw logs and test-driver sources are in [evidence](evidence/m9_multiplayer_menu/).
The [source ledger](../sources/m9_multiplayer_menu_sources.json) pins references and
implementation. All runs use isolated user directories and external read-only
retail resources; no commercial assets are committed.

- Native menus/config/network selection: **21/21**. New discovery test covers all
  truncated prefixes, 10,000 mutated packets, invalid fields/oversized packets,
  actual UDP request/reply, deduplication, full/incompatible status, expiry and
  cleanup. Setup tests cover host/direct conversion, invalid rules/profile/rotation,
  original demo command and the private 22-map inventory.
- Inspector-free Release: **21/21** selected headless tests; client build and GPU menu traversal succeed.
- Clang ASan/UBSan server-only: **4/4** discovery/setup/admission tests, no sanitizer
  suppression. The dedicated remains independent of renderer/UI/audio devices.
- Native graphical process regressions: dedicated + two clients, host + client,
  and host shutdown all pass (**3/3**). Existing gameplay process tests are separate
  from menu-entry tests.
- Native GPU menu traversal passes with an additional Host/Join open/Back visit
  during the existing exact solo world/audio pause snapshot, then resumes and quits.
- The Linux graphical UI driver links the normal frontend objects and injects SDL
  navigation/ImGui item activation, without host/connect CLI flags or session
  shortcuts. Host opens Area15 from Main; a second process selects its LAN row at
  the machine's LAN IPv4 address, reaches player 2, leaves and returns to Main.
  Host records two admissions/two departures, ends and returns to Main. Both exit 0.
  A direct connection to an absent server times out, displays the refusal dialog,
  dismisses it and returns to Main successfully.
  This is synthetic UI input on the native NVIDIA GPU, not human LAN qualification.
- Solo effects, audio, movement, environment and view oracle output is byte-identical
  to the preceding ObjectFX lot. All **6,961** protected hashes remain identical,
  including the 39 existing untracked PBR files and historical/reference evidence.

Initial failed checks are retained: an overlapping edit/build left different
`LanOffer` layouts in already-built translation units, producing a sanitizer and
Release discovery failure. Rebuilding every dependent unit against the frozen
header passes. A Release command requested a client/server-only process target in
a client-only configuration; the corrected target selection succeeds. Enabling the
Bloodbath row also invalidated three old smoke-test Down counts; the traversal now
accounts for the enabled row and tests its Back path. The first two UI-driver
attempts lacked synthetic focus or used a button ID without its tab scope; their
failures are retained alongside the corrected successful probe.

A separate X11/XTest capture attempt stalled in the installed NVIDIA presentation
path (native stacks retained). It was stopped without changing SDL, GPU code or
M2-GPU-001. Successful graphical tests use the ordinary native display path. No
GPU sanitizer qualification is claimed.

## Remaining boundaries

Human LAN on separate computers, routed/VPN/multi-interface broadcast behavior,
Windows/macOS and mobile/Quest builds are not qualified by this Linux lot. Broadcast
can be filtered; direct address remains the fallback. No IPv6, directory, relay,
NAT traversal, host migration or content download. UI settings are retained within
the running frontend, not persisted to a new preferences file. Visual refinements
and remaining M9 effect/qualification work are separate. CMoveMgr single integration,
server logical audio/RNG, solo behavior, PBR references and independent M2/M3
acceptance boundaries stay unchanged.
