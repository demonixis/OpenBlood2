# Transient object-message lifetime

The continuous SubwayA route at 30 Hz exposed a session-long message-token limit:
the exit relay counted its delayed sends, but the player fade and ExitTrigger no
longer received messages. `Objects` retained `Session message token limit` errors.
The 60 Hz route did not reproduce this exhaustion; combat and contact histories
differ across rates. Counting a named dispatch or service call is not proof of
delivery. See [execution evidence](../../../Documentation/Implementation/m2_message_lifetime.md).

## Identity and ownership

`HMESSAGEWRITE` and `HMESSAGEREAD` now name the same small typed value containing
a 64-bit identity. Zero/null is invalid. These are source-level adaptations, not
retail pointers, serialized messages or an ABI. No integer is converted to a pointer.
The process-wide atomic allocator gives every adapter distinct, monotonically
increasing identities, including suspended sources and prepared destinations.
It refuses before wrapping; allocator exhaustion cannot reuse a stale identity.
Only this counter is shared, never message contents or original game state.

An adapter owns only its live message map. Ending a message erases the payload;
there is no retained token per completed message. The existing limit of 65,536
now bounds **simultaneously live** messages. A refusal preserves all live messages,
and releasing one allows another allocation. The map may retain bucket capacity
at its bounded high-water mark. Other object/string/list/client token policies
are unchanged and remain separately bounded.

Routed dispatch is still synchronous and checked, with the same owned bytes and
typed damage/key/pickup payloads. A callback borrows its reader: it cannot release
or write that message, while it may create and release nested messages. Existing
read-cursor reset, 64 KiB packet bound, truncated-field failures, deferred receiver
removal and unavailable owned nested-read behavior remain unchanged. Closing or
writing a stale/foreign identity never changes another live payload. Session
teardown and NEW_LEVEL do not reassign identities from the old adapter.

The original SDK explicitly releases standalone writers and describes the old
send functions as freeing their messages. The value representation and allocator
are replacement engine code; no original game rules or callback order change.

## Inspection and tests

The Objects tab reports live messages, cumulative created/released/refused counts
and peak simultaneous messages. History is for the adapter lifetime, remains after
world unload, and belongs to the new adapter after NEW_LEVEL. Drawing does not
allocate messages or reset counters. The service catalogue still reports attempted
calls independently; the new counters expose actual allocation refusals.

`objects.message_churn` performs 100,000 routed callbacks plus 100,000 nested
messages in each of two independent adapters. It checks borrowed-reader protection,
stale/double-close rejection and foreign identities, with only two simultaneous
messages. `objects.message_limit` fills the live limit, verifies refusal, releases
one and verifies safe recovery without losing the remaining payloads.

`train_session.private_30` and `train_session.private_60` start at the actual spawn,
complete the original intro, traverse the train using production input mapping,
fight authored Cultists through FIRE_KEY, collect items through contact, activate
the actual exit relay, and preserve player state into CityHub. The driver uses
visible model heads for synthetic mouse aiming and fixed aisle navigation; it is
not a human playtest or retail behavior comparison. It changes no NPC, player stats,
trigger, damage rule or position directly. It switches to the original knife if
bullets run out. The 30 Hz driver lets a wounded, alerted Cultist retaliate before
finishing combat, ensuring a real incoming damage impulse. After the exit relay
activates, it continues normal firing until it has exhausted any remaining bullets
and fired the knife, so dropped ammunition cannot skip that check. The route
explicitly requires combat deaths and health pickups, ammunition
collection at 60 Hz and real dry-ammo knife firing at 30 Hz,
all ten relay sends, received fade, no message refusals, real NEW_LEVEL and cleanup.

The NVIDIA variant submits SDL input through the real window/event queue, verifies
native relative mode, renders/checks world and model pixels at three checkpoints,
and checks audio-output ownership through transition. Separate existing fade/HUD
GPU tests cover their composition. Snapshots must not advance simulation. Native
Metal execution and physical input-device automation are not claimed.

M2 remains incomplete pending its consolidated acceptance review. Music remains an
explicit separately observed player-command omission; the route does not certify
other levels, all services, full world saving, campaign completion or retail parity.

The pinned SDL source has a hash-checked one-character stale-property assignment
fix in `cmake/PatchSdlProperties.cmake`. It prevents a late Wayland HDR callback
from dereferencing a missing property group during shutdown. The public-API
`platform.destroyed_properties` regression must remain; review the patch on SDL
updates. This does not change the chosen dependency revisions or game rules.
