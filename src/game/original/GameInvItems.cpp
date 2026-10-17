// Selected GameInvItems.cpp CInvMedkit and InvItem.cpp SendActionMessage.
#include "GameInvItems.h"
#include "PlayerObj.h"
#include <algorithm>
#include <cmath>
bool CInvMedkit::AddItem(DBYTE)
{
    if (count >= 100)
        return false;
    count = 100; // Original ignores the incoming quantity.
    return true;
}
int CInvMedkit::Activate()
{
    ++attempts;
    requested_gain = actual_gain = 0;
    activation_result = 0;
    error.clear();
    auto* server = BaseClass::GetServerDE();
    auto* character = server && server->IsObjectAlive(owner_)
                          ? dynamic_cast<CBaseCharacter*>(server->HandleToObject(owner_))
                          : nullptr;
    if (!character)
    {
        error = "Medkit owner is not a live character";
        return 0;
    }
    auto& damage = character->m_damage;
    const float health = damage.m_fHitPoints;
    if (!std::isfinite(health) || std::abs(health) > 1e6f)
    {
        error = "Invalid medkit owner health";
        return 0;
    }
    requested_gain = std::min(100.f - health, float(count));
    if (requested_gain > 0)
    {
        damage.Heal(requested_gain); // Source spends the requested gain even when Heal clamps/refuses it.
        actual_gain = damage.m_fHitPoints - health;
        if (auto* player = dynamic_cast<CPlayerObj*>(character))
        {
            if (server->SendToClient(player->GetClient(), OpenBlood2::InventoryAction{1}) == DE_OK)
                ++action_messages;
            else
                error = "Medkit action could not reach its client";
        }
        count -= static_cast<int>(requested_gain);
    }
    activation_result = static_cast<int>(requested_gain); // Preserve fractional-zero and negative nonzero returns.
    return activation_result;
}
