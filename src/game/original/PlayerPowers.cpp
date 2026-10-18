// Selected original CPlayerObj AddPowerup/CheckPowerups/ResetPowerups and the
// powerup sound stack (PlayerObj.cpp:3670-4046), Stealth only. See powerup_contracts.md.
#include "PlayerObj.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    constexpr float alpha_normal = 1, alpha_invisible = .08f;
    constexpr const char* start_sound = "sounds\\powerups\\specialpowerupstart1.wav";
    constexpr const char* end_sound = "sounds\\powerups\\specialpowerupend1.wav";
    // Indexed by PowerupType; the source's second entry is the Stealth loop.
    constexpr const char* looping_sounds[] = {"sounds\\powerups\\invulnerabilityloop1.wav",
                                              "sounds\\powerups\\revenantloop1b.wav",
                                              "sounds\\powerups\\tripledamageloop3.wav", "", ""};
    SoundId play(HOBJECT owner, const char* sound, DBYTE priority, bool loop, std::string& error)
    {
        // PlaySoundFromObject(.., 1000, priority, loop, handle): attached, inner 10%, retained.
        SoundRequest request;
        request.resource = sound;
        request.speaker = owner;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 1000;
        request.inner_radius = 100;
        request.priority = priority;
        request.reverb_requested = true;
        request.loop = loop;
        auto* s = BaseClass::GetServerDE();
        return loop ? s->PlayObjectLoopSound(owner, request, error) : s->PlayCharacterSound(owner, request, true, error);
    }
} // namespace
bool CPlayerObj::AddPowerup(DBYTE type, float value)
{
    auto* s = GetServerDE();
    if (type != 6 /* POWERUP_STEALTH */ || !std::isfinite(value))
        return false;
    m_fInvisibleTime = s->GetTime() + value;
    s->SetObjectColor(m_hObject, {}, alpha_invisible);
    s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) & ~FLAG_SHADOW);
    m_bPowerupActivated[PU_INVISIBLE] = true;
    ++powerup_pickups;
    StartPowerupSound(PU_INVISIBLE);
    return true;
}
bool CPlayerObj::CheckPowerups()
{
    auto* s = GetServerDE();
    float time = s->GetTime();
    if (m_bPowerupActivated[PU_INVISIBLE])
    {
        bool visible = false;
        if (time > m_fInvisibleTime)
        {
            visible = true;
            m_bPowerupActivated[PU_INVISIBLE] = false;
            ++powerup_expiries;
            ++powerup_console_unavailable; // IDS_GENERAL_INVISIBILITY_EXP console text is not delivered.
            EndPowerupSound(PU_INVISIBLE);
        }
        else if (time > m_fInvisibleTime - 5)
        {
            // Blink while running out: visible in the second half of each half second.
            time *= 2;
            time -= std::floor(time);
            if (time * 2 > 1)
                visible = true;
        }
        const auto flags = s->GetObjectFlags(m_hObject);
        if (!visible)
        {
            s->SetObjectFlags(m_hObject, flags & ~FLAG_SHADOW);
            s->SetObjectColor(m_hObject, {}, alpha_invisible);
        }
        else
        {
            s->SetObjectFlags(m_hObject, flags | FLAG_SHADOW);
            s->SetObjectColor(m_hObject, {}, alpha_normal);
        }
    }
    bool new_loop = false, done = false;
    if (m_hPowerupStartSound && s->IsSoundDone(m_hPowerupStartSound, done) && done)
    {
        s->KillSound(m_hPowerupStartSound);
        m_hPowerupStartSound = 0;
        new_loop = true;
    }
    if (m_hPowerupEndSound && s->IsSoundDone(m_hPowerupEndSound, done) && done)
    {
        s->KillSound(m_hPowerupEndSound);
        m_hPowerupEndSound = 0;
    }
    if (new_loop || (!m_hPowerupStartSound && m_nPowerupSoundStackPos && !m_hPowerupLoopSound))
        while (m_nPowerupSoundStackPos)
        {
            const auto power = m_ePowerupSoundStack[m_nPowerupSoundStackPos - 1];
            if (m_bPowerupActivated[power] && looping_sounds[power][0])
            {
                if (m_hPowerupLoopSound)
                {
                    s->KillSound(m_hPowerupLoopSound);
                    m_hPowerupLoopSound = 0;
                }
                m_hPowerupLoopSound = play(m_hObject, looping_sounds[power], 5 /* PLAYER_MEDIUM */, true,
                                           powerup_error);
                if (m_hPowerupLoopSound)
                    break;
            }
            else
                --m_nPowerupSoundStackPos;
        }
    return true;
}
void CPlayerObj::ResetPowerups()
{
    auto* s = GetServerDE();
    if (m_bPowerupActivated[PU_INVISIBLE])
    {
        m_bPowerupActivated[PU_INVISIBLE] = false;
        EndPowerupSound(PU_INVISIBLE);
        s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_SHADOW);
        s->SetObjectColor(m_hObject, {}, alpha_normal);
    }
}
void CPlayerObj::StartPowerupSound(PowerupType power)
{
    if (m_nPowerupSoundStackPos < int(m_ePowerupSoundStack.size()))
        m_ePowerupSoundStack[m_nPowerupSoundStackPos++] = power;
    if (!m_hPowerupStartSound)
        m_hPowerupStartSound = play(m_hObject, start_sound, 4 /* PLAYER_LOW */, false, powerup_error);
}
void CPlayerObj::EndPowerupSound(PowerupType power)
{
    // The source reads the top of the stack without checking for an empty stack.
    if (m_nPowerupSoundStackPos > 0 && m_ePowerupSoundStack[m_nPowerupSoundStackPos - 1] == power)
    {
        --m_nPowerupSoundStackPos;
        if (m_hPowerupLoopSound)
        {
            GetServerDE()->KillSound(m_hPowerupLoopSound);
            m_hPowerupLoopSound = 0;
        }
    }
    if (!m_hPowerupEndSound)
        m_hPowerupEndSound = play(m_hObject, end_sound, 4 /* PLAYER_LOW */, false, powerup_error);
}
