#include "InventoryMgr.h"
#include "LaserBeamFX.h"
#include "LightningFX.h"
#include "OrbProjectile.h"
#include "Projectile.h"
#include "Weapon.h"
#include "engine/collision.h"
using namespace OpenBlood2;
FireResult CWeapon::FireProjectile()
{
    projectile = nullptr;
    projectile_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !m_pInventoryMgr || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    const bool alt = last_shot.request.alternate;
    const auto* name =
        alt ? (alt_projectile_class ? alt_projectile_class : data.m_szAltProjectileClass) : data.m_szProjectileClass;
    if (!name)
        return last_shot.result = FireResult::Unavailable;
    ObjectCreateStruct state;
    state.m_Pos = last_shot.request.position;
    legacy_copy(state.m_Name, name);
    auto* instance = dynamic_cast<CProjectile*>(server->CreateObject(server->GetClass(name), &state));
    if (!instance)
    {
        projectile_error = "Original projectile creation/resource binding failed";
        return last_shot.result = FireResult::Unavailable;
    }
    if (!instance->Setup(last_shot.direction, data.m_nType, last_shot.damage * m_pInventoryMgr->damage_multiplier,
                         inventory_bomb(data.m_nType)
                             ? (alt ? data.m_fAltProjVelocity : last_shot.request.projectile_speed)
                             : (alt ? data.m_fAltProjVelocity : data.m_fProjVelocity),
                         alt ? data.m_nAltDamageRadius : data.m_nDamageRadius, m_hOwner))
    {
        projectile_error = instance->error;
        return last_shot.result = FireResult::Unavailable;
    }
    projectile = instance->m_hObject;
    return last_shot.result = FireResult::Fired;
}
void CTeslaCannon::BeginFire(bool alternate)
{
    if (!alternate)
        return;
    ++numAltFires;
    alt_projectile_class = numAltFires == 2 ? "CTeslaBallProjectile" : "CTeslaBoltProjectile";
}
FireResult CTeslaCannon::FireProjectile()
{
    lightning = 0;
    lightning_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    if (last_shot.request.alternate)
    {
        const auto from = last_shot.request.position;
        SegmentHit hit;
        const auto query = server->IntersectSegment(from, last_shot.end, nullptr, hit, false, SegmentObjects::SolidOnly,
                                                    SegmentFilter::None, SegmentStart::SkipInterior);
        if (query == SegmentResult::Hit || query == SegmentResult::Miss)
        {
            lightning_end = query == SegmentResult::Hit ? hit.point : last_shot.end;
            lightning = server->PublishLightning(m_hOwner, {from, lightning_end, 2, 3, 2}, lightning_error);
        }
        else
            lightning_error = "Tesla alternate visual ray unavailable";
    }
    return CWeapon::FireProjectile();
}
FireResult CSingularity::FireProjectile()
{
    beam = 0;
    beam_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    // INTERSECT_OBJECTS | IGNORE_NONSOLID without a filter, from the fire position along
    // the range; the visual beam never suppresses the projectile.
    const auto from = last_shot.request.position;
    const auto to = vector_add(from, vector_scale(last_shot.direction, data.m_fRange));
    SegmentHit hit;
    const auto query = server->IntersectSegment(from, to, nullptr, hit, false, SegmentObjects::SolidOnly,
                                                SegmentFilter::None, SegmentStart::SkipInterior);
    if (query == SegmentResult::Hit || query == SegmentResult::Miss)
    {
        beam_end = query == SegmentResult::Hit ? hit.point : to;
        beam = server->PublishLaserBeam(m_hOwner, from, beam_end, beam_error, LASER_BLUE_LARGE);
    }
    else
        beam_error = "Singularity beam ray unavailable";
    return CWeapon::FireProjectile();
}

FireResult COrb::FireProjectile()
{
    const auto result = CWeapon::FireProjectile();
    if (result != FireResult::Fired || !last_shot.request.alternate)
        return result;
    auto* orb = dynamic_cast<COrbAltProjectile*>(BaseClass::GetServerDE()->HandleToObject(projectile));
    if (!orb || !orb->StartCamera())
    {
        projectile_error = orb ? orb->camera_error : "Orb camera projectile unavailable";
        return last_shot.result = FireResult::Unavailable;
    }
    return result;
}
