// Selected original PlayerObj.cpp HandleWeaponSoundMessage and ObjectUtilities
// attached-sound defaults. Copyright (c) 1998 Monolith Productions Inc.
#include "PlayerObj.h"
using namespace OpenBlood2;

void CPlayerObj::StopWeaponSound()
{
    if (!m_hWeapSound)
        return;
    if (auto* server = GetServerDE())
        server->KillSound(m_hWeapSound);
    m_hWeapSound = 0;
    ++weapon_sound_stops;
}
bool CPlayerObj::HandleWeaponSound(WeaponSoundAction action, std::string_view resource)
{
    ++weapon_sound_requests;
    weapon_sound_error.clear();
    auto* server = GetServerDE();
    if (!server || !server->IsMultiplayerGame() || !server->IsObjectAlive(m_hObject) ||
        (action != WeaponSoundAction::OneShot && action != WeaponSoundAction::Loop &&
         action != WeaponSoundAction::Stop))
    {
        weapon_sound_error = "Weapon sound requires a live multiplayer owner and a known action";
        ++weapon_sound_failures;
        return false;
    }
    if (action == WeaponSoundAction::Stop)
    {
        StopWeaponSound();
        return true;
    }
    // Replacement stops the old loop even when the new resource is unavailable.
    if (action == WeaponSoundAction::Loop)
        StopWeaponSound();
    weapon_sound_resource = resource;
    SoundRequest sound;
    sound.resource = resource;
    sound.speaker = m_hObject;
    sound.space = SoundSpace::Spatial;
    sound.outer_radius = 1000;
    sound.inner_radius = 100;
    sound.priority = 2; // SOUNDPRIORITY_MISC_HIGH, including player-owned keys.
    sound.loop = action == WeaponSoundAction::Loop;
    const auto voice = sound.loop ? server->PlayObjectLoopSound(m_hObject, sound, weapon_sound_error)
                                  : server->PlayCharacterSound(m_hObject, sound, false, weapon_sound_error);
    if (sound.loop)
        m_hWeapSound = voice;
    if (!voice)
    {
        ++weapon_sound_failures;
        return false;
    }
    ++weapon_sound_starts;
    return true;
}
