#include "InventoryMgr.h"
#include "Projectile.h"
#include "Weapon.h"
using namespace OpenBlood2;
FireResult CWeapon::FireProjectile()
{
    projectile = nullptr;
    projectile_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !m_pInventoryMgr || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    const bool alt = last_shot.request.alternate;
    const auto* name = alt ? (alt_projectile_class ? alt_projectile_class : data.m_szAltProjectileClass)
                           : data.m_szProjectileClass;
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
    // The alternate shot's SFX_LIGHTNING_ID beam to the first solid hit is client-only.
    if (last_shot.request.alternate)
        ++lightning_unavailable;
    return CWeapon::FireProjectile();
}
