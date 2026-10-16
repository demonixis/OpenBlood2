# M1 Object Boundary

Later [production NPC lifecycle contracts](AI_RUNTIME_CONTRACTS.md) supersede
this snapshot's absence of Cultist/Civilian initialization, AI_Update and activation.
The existing action/service limitations remain unless explicitly superseded.

The later [player entry contracts](PLAYER_CONTRACTS.md) supersede this increment's statements that shell/player entry is entirely absent. Complete gameplay remains blocked; earlier limits below remain unless explicitly superseded.

The authoritative source interface is in [src/game/legacy](../src/game/legacy/server_de.h). It is a **bounded source-level subset**, not an implementation of the entire 1998 SDK or its DLL ABI. Game and engine compile against the same `BaseClass`, callback signatures and scalar definitions. Read-only inputs use `const char*`; Windows-sized integers use fixed-width types. `ServerAdapter` is a normally constructed C++ object implementing virtual message methods and assigning compiler-checked callback thunks.

The original `g_pServerDE` requires one bound session on one simulation thread. A second session is rejected. The server stays bound while original destructors run; session shutdown then clears the global. Class descriptions are immutable and must outlive the registry; the original registrations use static storage. No client shell is linked yet, so client/server symbol collisions have not been introduced.

## Creation, Time and Ownership

1. Resolve a registered class, inherit its defaults and validate overrides against the property schema.
2. Allocate an engine record and original C++ instance, give it a stable handle, then enter a nested property context.
3. Call original virtual `MID_PRECREATE`; only then consume the modified `ObjectCreateStruct`.
4. Call `MID_INITIALUPDATE` with the corresponding reason: runtime **0**, world **1**, string properties **2**, restore **3**. The restore reason is tested; save-file restoration is not implemented.
5. Pop the context, restoring the parent creation context. Rejected creation becomes pending destruction and never receives initial update.

The engine owns each instance through `unique_ptr<BaseClass>`. Aggregates are owned by their original containing object; `BaseClass` only forwards messages through the original typed aggregate callbacks. It never recursively redispatches an object's own virtual message method.

`SetNextUpdate` arms a one-shot delay relative to simulation time; zero disables updates. The creation structure supplies the initial delay, and initial update can override it. The callback return is not converted into time. A scheduled callback may rearm itself. Active-state changes do not discard its deadline; an overdue inactive object runs after explicit activation. Automatic deactivation, touch physics and reference-game timing comparisons remain later work.

Each step snapshots the worklist, advances a double-precision clock by the supplied finite nonnegative delta, invokes due active objects at most once, then drains pending destruction. Objects spawned during an update first become eligible in a later step. The app uses elapsed SDL time; the GPU smoke uses deterministic 1/60-second steps. No fixed-rate gameplay equivalence is claimed.

Removal is idempotent while pending. The object remains resolvable until end of frame, but cannot receive another update/message. Link notifications run before reclamation; callbacks can remove other objects safely. Teardown rejects new creation. Nested stepping, destruction or clearing from a callback is rejected.

Internal IDs contain slot and generation. Legacy `HOBJECT` values point to stable tokens; IDs are never cast to pointers. Tokens are not reused within a session, and lookups validate ownership without dereferencing arbitrary handles. Lists retain safe tokens rather than owning objects. Editor selection uses an ID and clears after its generation disappears. M1 bounds each object's/string's/message's/list's token arena at 65,536 allocations per session; payloads are reclaimed immediately when released. These are explicit harness limits, to revisit with actual session traffic before campaign use.

## Services

| Surface | Implemented behavior | Evidence |
| --- | --- | --- |
| Classes, handles, creation | Four registrations: `BaseClass`, `B2BaseClass`, `CameraSpot`, `Key`; inherited schemas; factory/virtual dispatch; generation validation | `original_objects`, `nested_creation`, `aggregate_dispatch`, `creation_failures` |
| Properties | Case-insensitive property names, typed overrides, `GenericProp`, string/real/bool getters; scope includes precreate and initial update | `reasons_properties`, `nested_creation` |
| Scheduling/activity | Explicit deadlines, zero disable, active/inactive states, end-of-frame removal | `scheduling`, `deferred_removal` |
| Object inspection | Name/class, creation reason, transform, flags, activity, schedules, update count, properties, outgoing link count and actual `Key` fields | Headless selection test and GPU smoke/capture |
| Names/lists | Exact case-sensitive name matching, caller-owned snapshots, add/remove/relinquish | `links_lists`; retail name-case behavior still needs comparison |
| Links | Unique directed links, no self-links; `MID_LINKBROKEN` when either endpoint is removed; explicit break sends no notification | `links_lists` |
| Strings | Create/copy/free reference ownership, stable data until final free, safe stale-handle lookup | `original_objects`, `messages_strings` |
| Object messages | Real virtual `CSBase`/`ServerDE` methods; synchronous `EndMessage`, owned bounded buffer, borrowed callback reader, nested calls bounded at 128 | `messages_strings`, `aggregate_dispatch` |
| Message fields | Little-endian 32-bit integers and copied nullable strings; reads return caller-owned `HSTRING`; rewind at end as specified by original `CSBase` | `messages_strings`; no pointer serialization or network/save compatibility claim |
| Game variables | `GAME_DIFFICULTY`, `GAME_GAMETYPE`, initialized to medium/single; explicit harness setter | `original_objects`, `unsupported_teardown` |
| Deferred services | `MoveObject`, `SaveObjects`, `RestoreObjects` return `DE_ERROR` and record the service name; owned nested readers are not exposed | `unsupported_teardown` |
| Other original APIs | Not declared in this subset; importing a caller produces a compiler error until the service is deliberately added | No generic successful stub or null vtable |

Diagnostics retain the latest 32 failures; `Objects::error()` is the most recent failure, not an implicit success status. Successful operations must be judged by their return value. Null/missing getters return the declared sentinel; property getters return `DE_NOTFOUND` outside a creation context.

## Runtime Property Strings

The implemented grammar is semicolon-separated `Name value` or `Name=value`, with optional double quotes around a value. For example:

```text
Name "camera marker"; Pos "32 16 -64"; Visible true
```

Property strings are bounded to 16 KiB; individual strings to 100 bytes; names resolve against inherited schemas. Duplicates, unknown properties, nonfinite numbers, malformed values and incomplete quotes fail creation. Vectors use three numbers and rotations use quaternion x/y/z/w, in quoted values. Backslashes are literal, preserving resource paths; embedded quotes/escape syntax and retail Euler-string compatibility are not implemented. This grammar is a tested integration contract, not a claim that all original spawn strings are already compatible. M2 must validate actual content before extending it.

Original behavior is retained: `Key` reads its custom fields only on world creation, while `B2BaseClass` reads options for world and string creation. `CameraSpot` sets a 0.001-second precreate delay then disables updates during initial update. `B2BaseClass`'s original cooperative-mode `OPT_FLAG_CTF` check is left intact and recorded in provenance rather than silently repaired.

## M2 Additions

The original M1 contracts above remain the baseline. [World/session contracts](WORLD_CONTRACTS.md) describe the additive Euler property representation (retaining source angles), original GameStartPoint/PathPoint/trigger helpers, enumeration, stored dimensions/user flags, source-object accounting and bounded resource readers. These additions do not implement the missing collision, animation or complete original session.

## M2 model and motion continuation

See [MODEL_AND_COLLISION_CONTRACTS.md](MODEL_AND_COLLISION_CONTRACTS.md) for the
implemented animation, continuous primary-world collision, motion and console
services. `MoveObject` is no longer an unconditional unsupported service; it
requires a loaded collision index and retains explicit unsupported cases. ABC
loading occurs between precreate and initial update; missing models remain
explicit diagnostics on inspection instances. Model/touch callbacks use the
existing deferred lifecycle and original BaseClass/aggregate chain.

## String creation transform precedence

`parse_properties` validates the existing bounded property grammar without creating
an object. `create_props` preserves the supplied position/rotation and nonempty name
unless the string explicitly overrides them (only for schema-declared properties).
This is needed by the original SpawnObject helper; see [pickup contracts](PICKUP_CONTRACTS.md).
Unknown/duplicate properties still reject; world-file creation defaults are unchanged.

The [AI path-manager continuation](PATH_MANAGER_CONTRACTS.md) corrects GetNextObject
to enumerate only active Alive objects. Editor snapshots remain inclusive; pending
and stale handles are excluded. Stable slot order is not claimed retail-equivalent.

The [held-model/attachment continuation](ATTACHMENT_CONTRACTS.md) adds original
Beretta/knife held children and typed visual attachments. Earlier blanket hand-model
and attachment omissions are superseded for this bounded subset. Dropping, physical
attachments, client visibility SFX and complete M2 remain unavailable.

The [explicit target/facing continuation](AI_TARGET_CONTRACTS.md) adds original
AI_Mgr named target/link commands and deferred target clearing. GetObjectPos now
returns a typed status; target sampling and CanSee reject unresolved poses.
Autonomous acquisition/retention and full AI behavior remain unavailable.

The [destination preparation continuation](DESTINATION_CONTRACTS.md) shares const property validation and structural CPU preparation with fresh loading.
Blocked exits now perform read-only destination lookup after refusing transfer; earlier blanket resource-lookup omissions are historical.
No destination callbacks run and no active session is discarded; safe automatic NEW_LEVEL and M2 remain incomplete.
