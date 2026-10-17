# Original AI path snapshots and selection

> Extended by [CMovement contracts](ai_navigation_contracts.md): original cached
> transforms and obstacle-path calculation are available for explicitly initialized
> actors. Current-transform adaptations below remain; full AI following is still absent.

M2 remains incomplete. Selected PathMgr::BuildPathList/GetPath and
PathListData::Copy now replace the shell's earlier ad hoc snapshot. They provide
original path selection for future AI callers, not path following, navigation or
AI states. See [source ledger](../implementation/sources/m2_path_manager_sources.json),
[execution evidence](../../../Documentation/Implementation/m2_path_manager.md),
[object contracts](object_contracts.md) and [shell contracts](player_contracts.md).

## Preserved rules

PostStartWorld builds the path snapshot after world object initialization.
PreStartWorld/unload clears it. Every enumerated PathPoint descendant is copied:
name, position, optional action target and optional action message. The source's
comments mention skipping SMELL points and removing PathPoints, but neither action
is executed. SMELL points are included if present at build time; they may later
expire independently. Building paths never removes or renames source objects.
Later movement, rename, expiry or removal does not update copied values. Rebuilding
explicitly replaces the snapshot.

GetPath matches an ASCII case-insensitive prefix followed immediately by a digit.
It only checks that first suffix character: route2suffix matches route, while
route, routeX1 and route-1 do not. Duplicate names remain distinct entries. There
is no numeric parsing, deduplication, whole-suffix validation or invented alias.

Despite its name, original AddToSortedList does **not sort**. It begins at the
list sentinel, whose data is null, breaks immediately and inserts after that
sentinel. The resulting query order is the reverse of enumeration. The port
iterates the stored array backwards to preserve that behavior without allocating
an intermediate linked list. Original DLink initialization/insertion was checked
to establish this result; the misleading sorting comment is not the specification.
The engine's stable slot enumeration is not claimed to reproduce retail load order.

Invalid/empty prefix and an empty manager preserve the caller's previous result,
as in the source's preflight returns. With a nonempty manager and valid prefix,
the previous result is cleared even if nothing matches. Success returns owned
copies in source order. Error text distinguishes invalid input, empty snapshot
and no match. Querying has no object, message, audio or scheduling side effects.

## Active enumeration and safe ownership

GetNextObject now skips inactive objects as well as pending/destroying objects.
The original SDK exposes a separate GetNextInactiveObject; later engine source
confirms the active-list boundary. Only this active filter is adopted, without
importing the later engine. Existing editor snapshots continue to include hidden,
inactive and pending records. GetNextInactiveObject itself remains unavailable.
Hidden active PathPoints are included. An already copied point remains in the
snapshot if it later becomes inactive. This corrects the previous adapter's
all-alive enumeration; it is not an AI-driven activity/deactivation system.

PathListData replaces HSTRING ownership with strings and optional strings. A null
action and a present empty action remain distinct. Copy validates a live typed
PathPoint and readable strings, and replaces its output only after success.
Invalid/stale/wrong-class copies preserve the previous value. No new string-pool
reference, object link or ownership of the source is acquired. The source handle
is retained solely for checked inspection, never for executing an action.

PathList is an ordinary vector of owned values instead of the original array of
borrowed pointers. Returned results survive manager rebuild/clear and source
removal safely. Future gameplay callers must still discard their current route
on level transitions; owned values do not imply a current live-world binding.
Copying a result and modifying it cannot change the manager. Prefix text may
alias an output entry: it is saved before clearing that output.

Prefixes are bounded to 100 bytes and reject embedded NUL. Snapshots allow at most
4096 PathPoints; exceeding this bound fails, clears the old snapshot and publishes
no partial new list. The shell remains not-started on failure and load errors
retain the manager diagnostic before rollback. This replaces the source's partial
allocation/failure behavior. Existing engine object/property bounds apply. No
unchecked linked-list ownership, persistent pointers to vector elements or manual
DLL layout is introduced. Save/load and path action delivery are not implemented.

## Inspection and validation

The Session panel exposes a prefix query, selected order, copied positions/actions
and all copied entries. A source is separately shown as hidden, inactive, missing
its model or removed, and may be selected only while alive. These are snapshots,
not a moving AI route. Queries are rebuilt from current manager state when drawn,
so unload/reload cannot retain a borrowed entry or stale engine selection.

Synthetic tests cover typed copy and string ownership, case/suffix/duplicate rules,
reverse order, preflight versus no-match output behavior, aliased prefixes, removal,
SMELL expiry, rebuild/unload, active/inactive enumeration and the exact 4096/4097
boundary. The private test performs two SubwayA entry/unload cycles, checking the
seven real points and these exact selected orders:

| Prefix | Selected names |
| --- | --- |
| spank | spank1, spank0 |
| dog | dog3, dog2, dog1, dog0 |
| cat | cat1 |

All seven have absent action targets/messages in the supplied installation.
The existing player GPU scenarios on SubwayA check copied values and path queries
while the inspector is hidden, then compare fresh source identities and unchanged
snapshot values after reload. No extra runtime object or authored class is added.
Debug and inspector-free builds execute the same checks.

SubwayA remains 187 live-created source entries, 4 transferred sounds, 372 editor-only,
17 unsupported and zero failed, plus 7 SelfTriggers. AI state/perception/movement,
script FOLLOWPATH/action delivery, ExitTrigger, attachments and required FX/services
keep M2 incomplete and --start-game blocked with exit 6. No retail executable,
other-platform or XR parity is claimed.

Selected FOLLOWPATH consumption and arrival actions are now connected through
[typed AI script helpers](ai_follow_contracts.md). Full SCRIPT dispatch and
production AI following remain unavailable; the snapshot and lifetime contracts
above are unchanged.
