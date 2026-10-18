# Original TripLaser

Selected original `ObjectDLL/TripLaser.cpp` (10/23/97). Retail 05_CDM authors 7
beams and 03_Museum 38.

## Creation

The class is B2BaseClass/OT_MODEL with the source BEGIN_CLASS schema (Dims,
Color, Alpha, StartOn, TouchTriggerTarget/Message, Sound, TouchSound, Damage and
the hidden Filename/Skin). Precreate reads the properties for world files and
string creation, then forces `Models\default.abc` and `Skins\ammo\beamred.dtx`.
Non-finite or out-of-range values reject precreate; savegame precreate rejects
(save/load are not ported).

InitialUpdate marks the object savable, scales Color by 1/255, doubles Dims and
uses them as the model scale, and sets the object color/alpha. The beam runs
along the longest doubled dimension (ties prefer x, then y, as the source
branch order) using the rotation's right/up/forward vectors; its endpoints are
`position ± half` and the object dims are `|half|` with each axis at least 1.
StartOn beams get VISIBLE | TOUCH_NOTIFY | NOLIGHT and start the attached Sound
loop (outer 400, inner 40, low priority, retained handle). The beam is never solid.

## Contact and damage

Every touch counts; damage is applied at most every 0.1 s
(`now <= m_fDamageTime` returns, which also skips a contact at time zero, as the
source does). The beam segment between the endpoints must hit the toucher first
(solid objects and world); it then receives `Damage * 0.1` Normal damage from the
beam (05_CDM: 1000 → 100 per contact), with the beam direction and hit point, and
TouchSound plays attached (outer 800, high priority, not retained).
TouchTriggerTarget/Message are read but never sent, as in the source.

## Commands and flicker

Commands are whole-string and case-insensitive. TRIGGER sets VISIBLE |
TOUCH_NOTIFY | NOLIGHT and starts the loop if none is retained. TRIGGEROFF and KILL
clear the flags, stop the loop and schedule an update 0.25–0.5 s later; KILL also
marks removal. Each update while off toggles: a hidden beam shows for 0.2–0.3 s,
a visible one hides again (and is removed after KILL). Removal kills the loop.

Inspection retains touches, beam hits, damage messages, flickers and the last
sound result. Save/load and client SFX beyond the model are not ported.
