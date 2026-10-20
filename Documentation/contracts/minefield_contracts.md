# Original Minefield

This extends [specialized volumes](specialized_volume_contracts.md) for the eight
Fields volumes. The selected source is ObjectDLL/VolumeBrushTypes.cpp:387–510
and VolumeBrushTypes.h. The original constructor's CC_MINEFIELD (10) is overridden
by its class registration and the actual map's Code=CC_DAMAGE (8). Generic
unsupported container codes remain rejected.

The original physics override accepts players regardless of support and other
BaseCharacters only with fresh standing support. Ordinary objects are ignored.
It intentionally bypasses base viscosity, current, volume damage and Hidden.
MID_INCONTAINER (3510) damages the sending BaseCharacter without testing support.
A dead character does not explode again; a living invulnerable character can.

The victim is both sender and responsible object for the 500-point Death message,
with upward direction and actual victim position. This bypasses the player's
first-two-seconds protection against VolumeBrush senders, exactly as the source.
The explosion has no radius-damage call. Its EXP_GRENADE (3) client components
and c4/explosion_1.wav detached voice use the existing checked effect services;
the sound radius is 1000 and priority MiscMedium. Client effects and voices have
independent lifetime. Missing resources stay explicit in retained effect reports.

Checked handles, owned typed messages and fresh triangle support replace SDK
pointers/engine calls; support against primary-world triangles is not a fabricated
world handle. Deferred removal and the base/aggregate callback chain remain.
Damage-service failures are durable and reject subsequent RunWorld validation.
The Containers inspector shows mine calls, delivered messages, explosions and errors;
the explosion inspector retains component/resource results.

Synthetic tests distinguish air/player/character/ordinary-object behavior, hidden
volumes, untouched velocity, self damage, dead guards, lack of adjacent damage,
explicit MID_INCONTAINER and cleanup. Private tests execute the grenade model,
sprite, ring, light and actual decoded sound with external resources. Fields is
also opened at all three difficulties with three seconds of simulation and a
runtime recheck. These are loading/behavior probes, not multiplayer certification.
