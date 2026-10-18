# Original Tchernotronic

This extends the [trigger](trigger_contracts.md), [scene explosion](scene_explosion_contracts.md)
and [explosion](explosion_contracts.md) contracts. Source: `ObjectDLL/Tchernotronic.cpp/.h`
(5/11/98; 03_Museum). Retail parity is not claimed.

## Creation

`BEGIN_CLASS` declares hidden `Filename`/`Skin` defaults (`Models\WorldObjects\Technoberg.abc`
and its skin), read by the base precreate like any model, and `DestroyTriggerTarget` /
`DestroyTriggerMessage`, read for world-file and string creation. Save restoration is
refused at precreate; Save/Load are unavailable.

InitialUpdate (not savegame) adds `USRFLG_SAVEABLE`, sets exactly `FLAG_VISIBLE | FLAG_SOLID`
(B2BaseClass then adds `FLAG_DONTFOLLOWSTANDING`), looks up the `static_model` and `animate`
clips, disables looping, takes the rest clip's authored dimensions and the nonpushable
blocking priority (100), and schedules no update. The source ignores a failed dimension
query, leaving its vector uninitialized; the port keeps the current dimensions instead.
The solid box blocks movement through the ordinary replacement sweep.

## Trigger and explosion

`MID_TRIGGER` from a player is ignored. A message equal to `TRIGGER` (case-insensitive,
`CompareStringsUpper`) plays `animate` when it is not already the finished current clip
and schedules an update after 0.1 s; an invalid clip index plays nothing, so the finished
rest clip explodes at once. Each later update (every 0.05 s) waits for `MS_PLAYDONE`, then
adds the explosion at the object position: the `SFX_EXPLOSIONFX_ID` grenade recipe
(`EXP_GRENADE`, world up) through the existing model/sprite/ring/fragment/flame/light
publishers, the C4 `explosion_1.wav` from the position (radius 1000, inner 100, MISC_MEDIUM),
then `DamageObjectsInRadius` with 100 explode damage over 200 units, the machine itself as
damager. 0.5 s later (`TIME_REMOVE`) it sends its destroy trigger when both target and
message are set, then the failed update removes it (deferred).

Inspection keeps trigger, ignored-player, explosion and destroy-message counters, the last
scene-explosion report and the radius-damage report. Illumination and the complete
composite explosion remain the documented scene-explosion limits.
