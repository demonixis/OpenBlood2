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
