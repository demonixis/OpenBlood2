#include "ProximityBomb.h"
#include "AI_Mgr.h"
#include "BombExplosion.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <numbers>
using namespace OpenBlood2;
CProximityBomb::CProximityBomb() : CProjectile(OT_MODEL)
{
    filename = "Models\\Ammo\\c4.abc";
    skin = "Skins\\Ammo\\c4_prox.dtx";
    flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_REMOVEIFOUTSIDE | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW |
            FLAG_MODELGOURAUDSHADE | FLAG_RAYHIT | FLAG_GRAVITY;
    m_damage.m_fDeathDelay = .25f;
    constexpr float circle = 2 * std::numbers::pi_v<float>;
    g_pServerDE->RandomFloat(-circle, circle, pitch_velocity);
    g_pServerDE->RandomFloat(-circle, circle, yaw_velocity);
}
void CProximityBomb::Sound(const char* name)
{
    SoundRequest request;
    request.resource = std::string("Sounds\\Weapons\\proximities\\") + name;
    request.outer_radius = 500;
    request.inner_radius = 50;
    request.priority = 1;
    if (g_pServerDE->PlayDetachedSound(m_hObject, request, last_position, sound_error))
        ++sounds;
}
bool CProximityBomb::AdvanceFlight(double seconds)
{
    if (armed || !configured || m_bExplode || failed || seconds == 0)
        return !failed;
    if (!g_pServerDE->AdvanceModelProjectile(m_hObject, seconds))
        return failed ? false : Fail("Proximity bomb box flight unavailable/invalid");
    return !failed;
}
bool CProximityBomb::Update()
{
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    if (!configured || g_pServerDE->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Proximity bomb setup/position unavailable");
    const auto time = g_pServerDE->GetTime();
    if (m_bExplode || m_damage.IsDead())
    {
        if (!hit_character && time < m_fStartTime + explode_delay && !m_damage.IsDead())
        {
            if (play_detect)
            {
                Sound("detect.wav");
                play_detect = false;
            }
            return true;
        }
        Explode();
        return false;
    }
    if (armed)
    {
        DVector zero{};
        if (g_pServerDE->SetVelocity(m_hObject, &zero) != DE_OK)
            return Fail("Bomb velocity unavailable");
        if (g_pServerDE->IsObjectAlive(m_hFiredFrom) &&
            dynamic_cast<AI_Mgr*>(g_pServerDE->HandleToObject(m_hFiredFrom)))
            get_away = 2;
        if (time < m_fStartTime + get_away)
            return true;
        if (play_arm)
        {
            Sound("arm.wav");
            play_arm = false;
        }
        ++scans;
        // Read-only original destructible list order, no callbacks in this traversal.
        // The shooter is deliberately eligible. No visibility/faction/solid filter.
        auto& head = CDestructable::m_DestructableHead;
        for (auto* link = head.m_pNext; link && link != &head; link = link->m_pNext)
        {
            const auto h = static_cast<CDestructable*>(link->m_pData)->m_hObject;
            if (!h)
                break;
            if (!g_pServerDE->IsObjectAlive(h))
                continue;
            const auto* character = dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(h));
            if (!character || character->m_damage.IsDead())
                continue;
            DVector p;
            if (g_pServerDE->GetObjectPos(h, &p) != DE_OK)
                return Fail("Bomb detection pose unavailable");
            const auto delta = vector_subtract(p, last_position);
            if (vector_dot(delta, delta) <= detect_radius * detect_radius)
            {
                detected = h;
                m_bExplode = true;
                m_fStartTime = time;
                break;
            }
        }
    }
    else if (pitch_velocity != 0 || yaw_velocity != 0)
    {
        pitch += pitch_velocity * g_pServerDE->GetFrameTime();
        yaw += yaw_velocity * g_pServerDE->GetFrameTime();
        auto rotation = rotation_from_euler({pitch, yaw, 0});
        if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Bomb rotation rejected");
    }
    // Original override never calls base Update: no inherited ten-second expiry.
    return true;
}
void CProximityBomb::HandleContact(const SegmentHit& hit)
{
    if (!configured || armed || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    if (hit.object && (hit.object == m_hFiredFrom || !g_pServerDE->IsObjectAlive(hit.object) ||
                       !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID)))
        return;
    if (!hit.object && hit.polygon == UINT32_MAX)
        return;
    if (auto* character = dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(hit.object)))
    {
        if (character->m_damage.IsDead())
            return;
        CProjectile::HandleContact(hit);
        hit_character = true;
        return;
    }
    DRotation rotation;
    if (g_pServerDE->AlignRotation(rotation, hit.normal) != DE_OK)
    {
        Fail("Bomb contact normal unavailable");
        return;
    }
    // Source aligns local forward to the plane normal, then rotates around local right by pi/2.
    rotation = multiply_rotation(rotation, rotation_from_euler({std::numbers::pi_v<float> * .5f, 0, 0}));
    if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
    {
        Fail("Bomb sticking rotation rejected");
        return;
    }
    g_pServerDE->SetObjectFlags(m_hObject, g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
    armed = true;
    m_fStartTime = g_pServerDE->GetTime();
    Sound("stick.wav"); // Source cached last position, not the collision point.
}
void CProximityBomb::AddExplosion(DVector p, DVector n)
{
    PublishBombExplosion(m_hObject, p, n, 1, "Sounds\\Weapons\\proximities\\explode.wav", m_fDamage,
                         static_cast<float>(m_nRadius));
}
FireResult CWeapProximityBomb::FireProjectile()
{
    projectile = nullptr;
    projectile_error.clear();
    if (!last_shot.request.projectile_speed)
    {
        DVector p;
        if (g_pServerDE->GetObjectPos(m_hOwner, &p) != DE_OK)
            return last_shot.result = FireResult::Unavailable;
        PublishBombExplosion(m_hOwner, p, {0, 1, 0}, 0, "Sounds\\Weapons\\time\\explode.wav",
                             last_shot.damage * m_pInventoryMgr->damage_multiplier, 0);
        if (!g_pServerDE->IsObjectAlive(m_hOwner))
            return last_shot.result = FireResult::Fired;
        auto* owner = g_pServerDE->HandleToObject(m_hOwner);
        auto message = g_pServerDE->StartMessageToObject(owner, m_hOwner, damage_message);
        if (!message)
            return last_shot.result = FireResult::DeliveryFailed;
        // Original SendDamageMsg: this weapon's NORMAL damage type, no radius pass.
        auto written = g_pServerDE->WriteDamage(
            message, {{0, 1, 0}, p, last_shot.damage * m_pInventoryMgr->damage_multiplier, damage_type, m_hOwner});
        auto delivered = g_pServerDE->EndMessage(message);
        return last_shot.result =
                   written == DE_OK && delivered == DE_OK ? FireResult::Fired : FireResult::DeliveryFailed;
    }
    auto result = CWeapon::FireProjectile();
    if (!m_pInventoryMgr->GetAmmoCount(GetAmmoType(last_shot.request.alternate)))
    {
        ShowHandModel(false);
        m_pInventoryMgr->SelectNextWeapon();
    }
    return result;
}
