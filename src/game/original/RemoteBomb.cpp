#include "RemoteBomb.h"
#include "BombExplosion.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <numbers>
using namespace OpenBlood2;
CRemoteBomb::CRemoteBomb() : CProjectile(OT_MODEL)
{
    filename = "Models\\Ammo\\c4.abc";
    skin = "Skins\\Ammo\\c4_remote.dtx";
    flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_REMOVEIFOUTSIDE | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW |
            FLAG_MODELGOURAUDSHADE | FLAG_RAYHIT | FLAG_GRAVITY;
    m_damage.m_fDeathDelay = .25f;
    constexpr float circle = 2 * std::numbers::pi_v<float>;
    g_pServerDE->RandomFloat(-circle, circle, pitch_velocity);
    g_pServerDE->RandomFloat(-circle, circle, yaw_velocity);
}
bool CRemoteBomb::Detonate()
{
    if (!configured || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return false;
    m_bExplode = true; // Original command only latches; Update does the damage later.
    return true;
}
bool CRemoteBomb::AdvanceFlight(double seconds)
{
    if (armed || !configured || m_bExplode || failed || seconds == 0)
        return !failed;
    if (!g_pServerDE->AdvanceModelProjectile(m_hObject, seconds))
        return failed ? false : Fail("Remote bomb box flight unavailable/invalid");
    return !failed;
}
bool CRemoteBomb::Update()
{
    g_pServerDE->SetNextUpdate(m_hObject, .001f);
    if (!configured || g_pServerDE->GetObjectPos(m_hObject, &last_position) != DE_OK)
        return Fail("Remote bomb setup/position unavailable");
    if (m_bExplode || m_damage.IsDead())
    {
        if (g_pServerDE->IsObjectAlive(m_hFiredFrom))
            if (auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(m_hFiredFrom)))
                player->RemoveRemoteBomb(m_hObject);
        Explode();
        return false;
    }
    if (armed)
    {
        DVector zero{};
        if (g_pServerDE->SetVelocity(m_hObject, &zero) != DE_OK)
            return Fail("Remote bomb velocity unavailable");
    }
    else if (pitch_velocity != 0 || yaw_velocity != 0)
    {
        pitch += pitch_velocity * g_pServerDE->GetFrameTime();
        yaw += yaw_velocity * g_pServerDE->GetFrameTime();
        auto rotation = rotation_from_euler({pitch, yaw, 0});
        if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return Fail("Remote bomb rotation rejected");
    }
    return true; // Original override has neither a fuse nor automatic proximity detection.
}
void CRemoteBomb::HandleContact(const SegmentHit& hit)
{
    if (!configured || armed || failed || !g_pServerDE->IsObjectAlive(m_hObject))
        return;
    if (hit.object && (hit.object == m_hFiredFrom || !g_pServerDE->IsObjectAlive(hit.object) ||
                       !(g_pServerDE->GetObjectFlags(hit.object) & FLAG_SOLID)))
        return;
    if (!hit.object && hit.polygon == UINT32_MAX)
        return;
    if (dynamic_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(hit.object)))
    {
        if (falling)
            return;
        DVector zero{};
        if (g_pServerDE->SetVelocity(m_hObject, &zero) != DE_OK)
        {
            Fail("Remote bomb character contact velocity unavailable");
            return;
        }
        falling = true; // Dead characters are included. Retain gravity and spin.
        return;
    }
    DRotation rotation;
    if (g_pServerDE->AlignRotation(rotation, hit.normal) != DE_OK)
    {
        Fail("Remote bomb contact normal unavailable");
        return;
    }
    rotation = multiply_rotation(rotation, rotation_from_euler({std::numbers::pi_v<float> * .5f, 0, 0}));
    if (g_pServerDE->SetObjectRotation(m_hObject, &rotation) != DE_OK)
    {
        Fail("Remote bomb sticking rotation rejected");
        return;
    }
    g_pServerDE->SetObjectFlags(m_hObject, g_pServerDE->GetObjectFlags(m_hObject) & ~FLAG_GRAVITY);
    armed = true; // Original has no sticking/arming sound or arming delay.
}
void CRemoteBomb::AddExplosion(DVector position, DVector normal)
{
    PublishBombExplosion(m_hObject, position, normal, 0, "Sounds\\Weapons\\time\\explode.wav", m_fDamage,
                         static_cast<float>(m_nRadius));
}
FireResult CWeapRemoteBomb::FireProjectile()
{
    const auto result = CWeapon::FireProjectile();
    if (result != FireResult::Fired)
        return result;
    if (auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(m_hOwner)))
        if (!player->AddRemoteBomb(projectile))
        {
            projectile_error = player->remote_error;
            g_pServerDE->RemoveObject(projectile);
            projectile = nullptr;
            return last_shot.result = FireResult::Unavailable;
        }
    if (!m_pInventoryMgr->GetAmmoCount(GetAmmoType(last_shot.request.alternate)))
    {
        ShowHandModel(false);
        m_pInventoryMgr->SelectNextWeapon();
    }
    return result;
}
