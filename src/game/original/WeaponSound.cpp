// Selected original Weapon.cpp PlayFireSound, Update sound loop and KillSounds.
#include "PlayerObj.h"
#include "Weapon.h"
#include <cmath>
using namespace OpenBlood2;
const char* CWeapon::FireSound(bool alternate) const
{
    return alternate ? data.m_szAltFireSound : data.m_szFireSound;
}
void CWeapon::PlayFireSound(bool alternate, DVector position)
{
    auto* server = BaseClass::GetServerDE();
    const auto* path = FireSound(alternate);
    fire_sound_resource = path ? path : "";
    fire_sound_error.clear();
    fire_sound_alert = {};
    last_fire_sound = 0;
    if (fire_sound_resource.empty())
        return; // Original empty filename also skips SendSoundTrigger.
    ++sound_attempts;
    if (!server || !server->IsObjectAlive(m_hOwner))
    {
        fire_sound_error = "Fire sound requires a live weapon owner";
        ++sound_failures;
        return;
    }
    SoundRequest request;
    request.resource = fire_sound_resource;
    request.speaker = m_hOwner;
    request.outer_radius = 2000;
    request.inner_radius = 200;
    request.priority = dynamic_cast<CPlayerObj*>(server->HandleToObject(m_hOwner)) ? 6 : 4;
    const auto sound = server->PlayCharacterSound(m_hOwner, request, true, fire_sound_error);
    last_fire_sound = sound;
    if (sound)
        ++sounds_played;
    else
        ++sound_failures;
    // Original order: alert after attempting playback, including missing files.
    // The checked broadcast can remove the owner; retain no new voice in that case.
    if (!SendSoundTrigger(m_hOwner, 0, position, 2000, fire_sound_alert))
        ++sound_alert_failures;
    if (!sound)
        return;
    if (!server->IsObjectAlive(m_hOwner))
    {
        server->KillSound(sound);
        ++sounds_released;
        return;
    }
    unsigned slot = 0;
    for (unsigned i = 0; i < fire_sounds.size(); ++i)
    {
        if (!fire_sounds[i])
        {
            slot = i;
            break;
        }
        if (fire_sound_ends[i] < fire_sound_ends[slot])
            slot = i; // Strict comparison preserves the first slot on ties.
    }
    float duration = 0;
    if (!server->GetSoundDuration(sound, duration) || !std::isfinite(duration) || duration <= 0)
    {
        server->KillSound(sound);
        ++sounds_released;
        ++sound_failures;
        fire_sound_error = "New fire voice has no valid decoded duration";
        return;
    }
    if (fire_sounds[slot])
    {
        server->KillSound(fire_sounds[slot]);
        ++sound_evictions;
        ++sounds_released;
    }
    fire_sounds[slot] = sound;
    fire_sound_ends[slot] = server->GetTime() + duration;
}
void CWeapon::UpdateSounds()
{
    auto* server = BaseClass::GetServerDE();
    if (!server)
        return;
    for (auto& sound : fire_sounds)
    {
        bool done = false;
        if (sound && server->IsSoundDone(sound, done) && done)
        {
            server->KillSound(sound);
            sound = 0;
            ++sounds_released;
        }
    }
}
void CWeapon::KillSounds()
{
    auto* server = BaseClass::GetServerDE();
    if (!server)
        return;
    for (auto& sound : fire_sounds)
        if (sound)
        {
            server->KillSound(sound);
            sound = 0;
            ++sounds_released;
        }
}
void CSoulWeapon::BeginFire(bool)
{
    int sound = 0;
    if (!BaseClass::GetServerDE()->RandomInt(0, 4, sound))
        return;
    alt_sound = sound < 1   ? "Sounds\\Slash1.wav"
                : sound < 2 ? "Sounds\\Slash2.wav"
                : sound < 3 ? "Sounds\\Slash3.wav"
                            : "Sounds\\Slash4.wav";
}
void CShikariClaw::BeginFire(bool alternate)
{
    int sound = 0;
    if (!BaseClass::GetServerDE()->RandomInt(0, 4, sound))
        return;
    const char* slash = sound < 1   ? "Sounds\\Slash1.wav"
                        : sound < 2 ? "Sounds\\Slash2.wav"
                        : sound < 3 ? "Sounds\\Slash3.wav"
                                    : "Sounds\\Slash4.wav";
    (alternate ? fire_sound : alt_sound) = slash;
}

void CSoulHook::BeginFire(bool)
{
    int sound = 0;
    if (!BaseClass::GetServerDE()->RandomInt(0, 4, sound))
        return;
    fire_sound = sound < 1   ? "Sounds\\Slash1.wav"
                 : sound < 2 ? "Sounds\\Slash2.wav"
                 : sound < 3 ? "Sounds\\Slash3.wav"
                             : "Sounds\\Slash4.wav";
}

void CBoneleechSuck::BeginFire(bool)
{
    int sound = 0;
    if (!BaseClass::GetServerDE()->RandomInt(0, 4, sound))
        return;
    fire_sound = sound < 1   ? "Sounds\\Slash1.wav"
                 : sound < 2 ? "Sounds\\Slash2.wav"
                 : sound < 3 ? "Sounds\\Slash3.wav"
                             : "Sounds\\Slash4.wav";
}
