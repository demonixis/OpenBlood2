// Original CGideonShield::FireVector: visual publication only.
#include "B2BaseClass.h"
#include "Weapon.h"
#include "server_de.h"
FireResult CGideonShield::FireVector()
{
    auto* server = BaseClass::GetServerDE();
    effect = 0;
    effect_error.clear();
    if (!server || !firing || !server->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    effect = server->PublishObjectParticles(m_hOwner, 19, effect_error);
    // Client presentation failure does not undo the accepted server shot.
    // Armor is added by Gideon::MC_Fire_Stand, not by this weapon.
    return last_shot.result = FireResult::Fired;
}
