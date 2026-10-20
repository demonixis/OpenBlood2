# CalebAI and authored SparksObj

This separately requested loading continuation extends
[AI runtime](ai_runtime_contracts.md) and [sparks](spark_contracts.md).

## CalebAI

The original CalebAI class adds constructor defaults, fixed Caleb model/skin and
one shared animation/hit-sphere/sound table, with no state overrides. The existing
AI_Mgr runtime applies those selections: hearing/smell 0, sense 200, seeing 1000,
walk 75, run 180, roll 3.5, mass AI_DEFAULT_MASS, strength 5, flags 0, Cabal false,
HP 30, random HP 0 and armor 0. Its registered singular `AIWeapon` property is
not read by the original AI_Mgr reader, which reads numbered slots. It does not
magically grant a weapon. Authored numbered slots retain the normal checked path.

The table is distinct from player and other NPC tables, shared by CalebAI instances
and reset on unload/candidate rollback. Original AI_Mgr state, damage, script,
movement and inventory behavior remains active. Source casing/slashes are normalized
by resource lookup only; no assets are copied or substituted. Credits now exercises
actual production entry at difficulties 1, 0 and 2, short simulation and cleanup.

## SparksObj

The source callback order, property defaults and explicit .001/.01 scheduling are
preserved. Burst and removal deadlines are strict `>` comparisons. OFF updates the
on-duration clock but leaves the burst clock unchanged; expiry is checked before
that OFF refresh. Case-sensitive SWITCH/ON/OFF prefixes do not reset either clock
or change scheduling. One owned HSTRING read is released per trigger.

The source attempts LongInt lookup of boolean `SparkSwitch`, instead of reading
`Type`. The typed-lookup contract retains type 0 (corroborated by later LithTech
si_GetPropLongInt; not a 1998 executable trace). The burst therefore uses colors
(200,200,200)..(200,200,0), spritetextures/particle1.dtx, radius500, gravity-500,
truncated byte count, authored duration/emission radius and actual forward direction.
The temporary CClientSparksSFX server object only carries a message then removes
itself; typed publication replaces that transport. Its Setup explicitly serializes
**zero position and identity rotation**, so the particles remain at world origin.
This source quirk is retained, not silently corrected to the emitter position.

The sound uses the actual emitter position, radius1000/inner100 and MiscHigh priority2.
It is detached and remains independent of removal. A failed sound start does not
prevent the source's subsequent particle request. Errors remain durable and visible;
RunWorld rejects a live failed emitter. Creation bounds reject nonfinite/negative
or inverted count/delay ranges, count>255, duration outside (0,10], emission radius
outside [0,100], or delay/max lifetime>1e6. These are explicit bounded adaptations.

The supplied Power Station CVBC authors `Sounds\lightpop2.wav`, absent from the
mounted archives. It creates particles successfully but records missing sound;
this remains a separate unresolved opening issue alongside CClientLightningSFX.
The private behavior test uses an explicitly authored, present c4 sound to verify
real decoding independently; production does not substitute that sound.

The Sprites inspector shows emitter clocks, commands/requests, current on state,
particles, actual recipe gravity and failures. Existing GPU billboard rendering
and detached effect ownership are reused. Tests cover strict deadlines, source
origin/type quirks, truncation, switch prefixes, OFF/removal ordering, unchanged
clocks, missing sound, real particles/audio and unload. No renderer/dependency
change, network support or complete campaign traversal is claimed.
