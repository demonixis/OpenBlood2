#include "InventoryMgr.h"
#include "Projectile.h"
#include "Weapon.h"
#include "engine/objects.h"
using namespace OpenBlood2;
const char* CEnergyBlast::FireSound(bool alternate) const
{
    auto* server = BaseClass::GetServerDE();
    // Original override changes the primary string only. Class ancestry is
    // inspected without requesting registration of the still-unported boss.
    if (!alternate && server)
        for (auto type = server->GetObjectClass(m_hOwner); type; type = type->parent)
            if (type->name == "Gideon")
                return "sounds\\enemies\\gideon\\distfire.wav";
    return CWeapon::FireSound(alternate);
}
FireResult CEnergyBlast::FireProjectile()
{
    projectile = nullptr;
    projectile_error.clear();
    auto* server = BaseClass::GetServerDE();
    if (!server || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    ObjectCreateStruct state;
    state.m_Pos = last_shot.request.position;
    legacy_copy(state.m_Name, "EnergyBlastProjectile");
    auto* instance =
        dynamic_cast<CEnergyBlastProjectile*>(server->CreateObject(server->GetClass("CEnergyBlastProjectile"), &state));
    if (!instance)
    {
        projectile_error = "Original energy projectile creation/resource binding failed";
        return last_shot.result = FireResult::Unavailable;
    }
    const bool alt = last_shot.request.alternate;
    if (!instance->Setup(last_shot.direction, data.m_nType, last_shot.damage * m_pInventoryMgr->damage_multiplier,
                         alt ? data.m_fAltProjVelocity : data.m_fProjVelocity,
                         alt ? data.m_nAltDamageRadius : data.m_nDamageRadius, m_hOwner))
    {
        projectile_error = instance->error;
        return last_shot.result = FireResult::Unavailable;
    }
    projectile = instance->m_hObject;
    return last_shot.result = FireResult::Fired;
}
