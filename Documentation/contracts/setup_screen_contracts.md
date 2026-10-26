# Game data setup screen

The setup screen replaces the standalone launcher (now [archived](../history/launcher/launcher.md)).
It is replacement UI (Dear ImGui with the embedded Droid Sans, no original artwork, which may
not exist yet) in `app/setup_screen.{h,cpp}`, over the headless `app/game_setup.{h,cpp}` and
`app/demo_install.{h,cpp}`.

## When it shows

- A data folder from `--data-dir`, `BLOOD2_DATA_DIR` or `blood2.local.ini` skips it
  entirely (developer and test runs are unchanged). Tools and smokes without data still exit
  with code 2.
- Otherwise a portable copy plays the game installed around it (desktop): the first playable
  installation in the executable's folder or its `Game` folder (an extracted demo package),
  the full game before the demo, with its enabled mods from `setup.cfg`. It wins over the
  stored choice, which is not rewritten; a choice made in Options > SETUP lasts for the session.
- Otherwise the stored choice in `setup.cfg` is used when `inspect_data` still accepts it; the
  screen opens only the first time or while that choice is missing or unplayable.
- A choice the game cannot mount (missing archives, a refused mod) returns to the screen with
  the refusal shown.
- Options > SETUP (a replacement row after the original Options rows, and after VR) opens it
  over the paused menu. The running installation is listed and preselected; Back, Esc or
  the gamepad's B return to the game unchanged, Play with an unchanged choice does too, and
  Play with a new one ends the game and restarts it in the same process (window, GPU device
  and OpenXR session kept). Mobile platforms cannot relaunch a process.
- The demo's 41-pixel menu font fits six rows; pages with more rows (Options with SETUP, and
  VR) share the list height evenly instead of failing.

## Installations

An installation is a folder and an edition: retail with `BLOOD2.REZ`, demo with `B2DEMO.REZ`
(case-insensitive; a folder with both lists both). The list merges, without duplicates:

- folders the player added (desktop native folder picker, `SDL_DIALOG` on desktop only);
- the running game's folder (from the menu);
- detected folders: downloaded demos (`<user dir>/content/<install>/Game`), the executable's
  folder and its `Game` folder (desktop), the app's Documents folder and its folders (iOS: Files app, Finder file
  sharing), `/sdcard/openblood2` and its folders (Android phones and Quest; all-files access,
  requested by the "Allow file access" button).

Each entry is checked with `inspect_data`; unplayable entries show the refusal and cannot be
selected. With no stored choice, a single playable entry is preselected. Choosing between a
demo and the full game is choosing an entry.

## setup.cfg

`<user dir>/setup.cfg`, UTF-8 `key=value`, at most 64 KiB, written atomically:
`version=1`, `selected.{edition,path}`, `added.N.{edition,path}` (at most 32) and
`mod.N.{edition,root,path,enabled}` (at most 24, in load order). Paths are absolute.
Unknown, duplicate, incomplete or out-of-order records refuse the whole file; an unreadable
file is reset (with a message) rather than locking the player out. Changes are saved as they
are made; CTest runs use an isolated user directory.

## Mods

Data-only REZ overlays per installation, mounted after the stock archives in list order
(later entries win), with `Resources::mount_game`'s existing checks (no code, no Nightmares,
no duplicates). A root `cres.dll` is accepted (0.5.0): it is only read as string/bitmap
resources by the bounded PE reader and never executed, and a mod's copy replaces the
stock strings. Any other `.dll`, `.exe` or `.lto` (`object.lto`) still refuses the mod. Up, Down, Remove and the checkbox edit the list. `.rez` files in the
installation's `mods` folder (any case) are listed automatically, disabled; desktop also adds
any file through a native picker.

## Demo installation

The qualified Archive.org package of the archived launcher, unchanged: `B2Demo.zip`,
37,695,395 bytes, SHA-256 `2f22bed1…9b7bc1`, extracted `Game/B2Demo.rez` SHA-256
`e410ce29…7218c`. Only on the player's request: download (or a local ZIP on desktop) into a
staging folder under `<user dir>/content/`, size and SHA-256 checks (the engine's SHA-256),
bounded miniz extraction of `Game/` (entry, size, name, collision and CRC checks), REZ
checksum, `inspect_data` and `mount_game`, then an atomic rename to
`content/Blood2Demo-1.0-<id>/`, selected automatically. Any failure or cancel removes the
staging folder; an existing installation is never replaced.

HTTPS goes through the platform's own TLS and trust store, one GET at a time with redirects
returned, never followed: each hop must be `https` on port 443 of `archive.org` or a
subdomain, without user information (at most five). Transports: libcurl (Linux with
OpenSSL, Windows with Schannel; 15 s connect, 600 s total, 1 KiB/s low-speed limit),
`NSURLSession` (macOS, iOS; 30 s request, 600 s resource), `HttpsURLConnection` through
`Blood2Activity.httpsGet` (Android; 15 s connect, 30 s read, progress and cancel through
JNI). Every transport stops past the package size.

## Platforms and verification

`setup.*` tests cover persistence, detection, mods, the URL policy and the installer with
synthetic packages. The macOS NSURLSession download of the real package passed manually
(`setup_tests download`). The retail menu traversal smoke opens Options > SETUP and returns.
The libcurl transport (Linux, Windows) is built and tested by CI only; iOS and Android are
build-only, and the Quest panel path is not checked in the headset.
