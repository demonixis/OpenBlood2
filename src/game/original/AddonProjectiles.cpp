// Original ObjectDLL/GameProjectiles.cpp _ADD_ON projectiles (Monolith). See
// nightmares_edition_contracts.md.
#include "AddonProjectiles.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "game/weapon_messages.h"
#include <numbers>
using namespace OpenBlood2;

CRockProjectile::CRockProjectile() : CProjectile(OT_MODEL)
{
    m_fLifeTime = 6.5f;
    filename = "Models_ao\\Powerups_ao\\rock1_pu.abc";
    skin = "Skins_ao\\Powerups_ao\\rock1_pu.dtx";
    damage_type = DamageType::Normal;
    flags |= FLAG_GRAVITY;
}
bool CRockProjectile::AdvanceFlight(double seconds)
{
    if (!configured || failed || seconds == 0)
        return !failed;
    if (!g_pServerDE->AdvanceModelProjectile(m_hObject, seconds))
        return failed ? false : Fail("Rock box flight unavailable/invalid (including embedded starts)");
    return !failed;
}
// Source Update: no base CProjectile::Update. Expiry removes the rock; the last fade_time seconds
// ramp its alpha down; below 5 units/s it stops spinning (pitch cleared, yaw kept).
bool CRockProjectile::Update()
{
    auto* s = g_pServerDE;
    if (s->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Rock position unavailable");
    s->SetNextUpdate(m_hObject, .001f);
    if (!configured)
        return Fail("Projectile never received Setup");
    if (m_fLifeTime > 0)
    {
        const float now = s->GetTime();
        if (now > m_fStartTime + m_fLifeTime)
            return false;
        if (now > m_fStartTime + m_fLifeTime - fade_time)
        {
            DVector color;
            float alpha = 0;
            if (!s->GetObjectColor(m_hObject, color, alpha) ||
                !s->SetObjectColor(m_hObject, color, 1 - (now - (m_fStartTime + m_fLifeTime - fade_time)) / fade_time))
                return Fail("Rock fade color unavailable");
        }
    }
    DVector velocity;
    if (s->GetVelocity(m_hObject, &velocity) != DE_OK)
        return Fail("Rock velocity unavailable");
    if (std::sqrt(vector_dot(velocity, velocity)) < 5)
    {
        pitch_velocity = yaw_velocity = 0;
        const auto rotation = rotation_from_euler({0, yaw, 0});
        if (s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Rock rotation rejected");
    }
    else if (pitch_velocity != 0 || yaw_velocity != 0)
    {
        pitch += pitch_velocity * s->GetFrameTime();
        yaw += yaw_velocity * s->GetFrameTime();
        const auto rotation = rotation_from_euler({pitch, yaw, 0});
        if (s->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Rock rotation rejected");
    }
    return true;
}
// Source HandleTouch, replacing the base contact: no explosion. A touched character takes
// m_fDamage * bounces_left / 5 (DAMAGE_TYPE_NORMAL, the shooter responsible); a world polygon
// reflects 0.3 of the normal velocity, halves it and draws new spins; anything else stops it.
// Every counted contact plays the basketball bounce.
void CRockProjectile::HandleContact(const SegmentHit& hit)
{
    auto* s = g_pServerDE;
    if (!configured || failed || !s->IsObjectAlive(m_hObject))
        return;
    if (hit.object && (hit.object == m_hFiredFrom || !s->IsObjectAlive(hit.object)))
        return;
    if (bounces_left <= 0)
        return;
    if ((hit.object && !(s->GetObjectFlags(hit.object) & FLAG_SOLID)) || (!hit.object && hit.polygon == UINT32_MAX))
        return;
    last_contact = hit.object;
    DVector velocity, position;
    if (s->GetVelocity(m_hObject, &velocity) != DE_OK || s->GetObjectPos(m_hObject, &position) != DE_OK)
    {
        Fail("Rock contact state unavailable");
        return;
    }
    if (hit.object && dynamic_cast<CBaseCharacter*>(s->HandleToObject(hit.object)))
    {
        const auto message = s->StartMessageToObject(this, hit.object, damage_message);
        if (!message ||
            s->WriteDamage(message, {velocity, position, m_fDamage * (float(bounces_left) / 5), DamageType::Normal,
                                     m_hFiredFrom}) != DE_OK ||
            s->EndMessage(message) != DE_OK)
        {
            Fail("Rock damage delivery failed");
            return;
        }
        ++damage_deliveries;
        if (!s->IsObjectAlive(m_hObject))
            return;
        if (s->GetVelocity(m_hObject, &velocity) != DE_OK)
        {
            Fail("Rock bounce velocity unavailable");
            return;
        }
    }
    if (hit.polygon != UINT32_MAX)
    {
        velocity = vector_subtract(velocity, vector_scale(hit.normal, vector_dot(velocity, hit.normal) * .3f));
        constexpr float circle = 2 * std::numbers::pi_v<float>;
        if (!s->RandomFloat(-circle, circle, pitch_velocity) || !s->RandomFloat(-circle, circle, yaw_velocity))
        {
            Fail("Rock spin randomness unavailable");
            return;
        }
        velocity = vector_scale(velocity, .5f);
    }
    else
        velocity = {};
    SoundRequest request;
    request.resource = "Sounds\\Bounce\\basketball.wav";
    request.outer_radius = 500;
    request.inner_radius = 50;
    request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
    request.speaker = m_hObject;
    request.space = SoundSpace::Spatial;
    if (s->PlayCharacterSound(m_hObject, request, false, sound_error))
        ++bounce_sounds;
    if (s->SetVelocity(m_hObject, &velocity) != DE_OK)
    {
        Fail("Rock bounce velocity rejected");
        return;
    }
    --bounces_left;
}
