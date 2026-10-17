# Mission objectives and client resource strings

This extends [trigger contracts](trigger_contracts.md) and [player contracts](player_contracts.md).
M2 remains incomplete. See [source selections](../implementation/sources/m2_objective_sources.json) and
[implementation evidence](../../../Documentation/Implementation/m2_objectives.md).

## Original behavior

`ObjectivesTrigger` inherits Trigger's dispatch, filtering, activation counter,
locks, active-command guard and scheduling. Its original defaults disable touch,
AI, ordinary-object and trigger-relay activation; players remain enabled. The
original title/text, resource number and sound properties are retained. Both
world and string-property creation read them; unsupported restore still rejects.

Its SendMessages override sends a single objective broadcast only when **both**
authored title and text are nonempty, even for a nonzero ResourceNum. It does not
send inherited target slots. A nonzero inherited message delay still selects the
base Trigger's delayed slot path instead of this override, as in the source.
The sound request follows the broadcast inside the same title/text guard; it is
counted and inspected, not played. Queue failure does not invent a retry or
reactivate the source Trigger.

The client handles the original SMSG_OBJECTIVES branch in single/custom game
modes. ResourceNum=0 uses the owned authored strings. Otherwise, IDs
`5000 + ResourceNum - 1` and `6000 + ResourceNum - 1` replace the authored title
and text. Missing localized data is an explicit unresolved objective, never a
successful fallback to the authored text. The previous objective is replaced;
this increment does not invent an objective-completion list. Original `|` line
breaks are displayed in the inspector; the original status bar/tab is not ported.

## Typed delivery and lifetime

`MissionObjectives` is a value-owned local PlayerEvent. The server's original
null-client broadcast maps to the supported single associated client. No live
client returns DE_NOTFOUND; a full 256-event queue returns DE_ERROR. Counts in
the source object distinguish activation, successfully queued delivery and sound
requests. Queue success is not proof of resolved strings or audible sound.
No callback into the client occurs while original objects are being dispatched.

Authored strings are bounded by MAX_GP_STRING_LEN, reject embedded NULs, and
ResourceNum must fit both resulting 16-bit string IDs (0–59536). Negative/overflow
resource numbers reject precreate instead of wrapping. These checks are deliberate
portability/validation adaptations. Existing deferred destruction and checked
client associations apply: deleting the sender preserves its queued owned event;
deleting the player discards that client's pending queue. Exit/re-entry clears
objective state. World unload also resets the lazy string cache and diagnostics.
An unresolved UI string is reported without tearing down an otherwise valid
inspection session; it is not credited as implemented gameplay or successful
resource resolution.

## Portable PE resource reader

The first localized objective reads external `cres.dll` through the existing REZ
mount precedence. It is parsed as data; no DLL loader, executable code, Windows
header, new dependency or extracted commercial string file is used. The entire
string table is cached once per world and inspection shows the selected archive,
decoded count, language and failures. Other historical FormatString callers are
not implicitly implemented; this narrow service performs lookup without printf
format substitution.

The reader follows Microsoft's [PE resource format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
and [STRINGTABLE layout](https://learn.microsoft.com/en-us/windows/win32/menurc/stringtable-resource).
It accepts PE32/PE32+, maps data RVAs through checked section raw ranges, walks
numeric RT_STRING block/language entries, and decodes the sixteen length-prefixed
UTF-16 strings per block into UTF-8, including checked surrogate pairs. Empty
slots are absent. Invalid signatures, truncation, duplicate numeric IDs, cyclic
or aliased selected directories, ambiguous section mappings, malformed UTF-16,
embedded NULs and budgets reject with no partially published table.

Limits: 32 MiB input, 96 sections, 16 MiB resource directory, 4,096 entries per
directory, 4,096 selected block/language records, block IDs 1–4,096, 4,096 UTF-16
units per string and 8 MiB decoded text. Named resources and other resource types
are outside the selected reader. Default language selection requires a single
language across string blocks; a mixed-language image fails explicitly. The
reader can take an exact nonzero language ID without fallback; no application language
selector or OS locale guessing has been added. Missing IDs in that selected
language remain missing. Input failure clears output, and no data is executed.

## Inspection and remaining scope

The Objectives tab lists hidden/inactive original objective triggers, source
properties, activation/rejection/queue counts and sound requests. It shows the
actual client text and resolution errors separately. Its diagnostic TRIGGER
button uses the associated player and the usual original filters. World retains
failed source entries. `--objectives-smoke` checks delivery while ImGui is hidden,
resolution from retail resources, stale selection and unload/reload; a diagnostic
rearm allows this check if the authored objective already fired at player spawn.
The same simulation runs in the inspector-free build.

CameraObj is extended by [camera contracts](camera_contracts.md). Campaign
transitions/ExitTrigger, KeyFramer/cinematics, remaining actors
and effects, audio playback, original HUD, saves and full M2 acceptance remain
open. No retail executable behavior comparison or non-Linux/XR validation is claimed.
