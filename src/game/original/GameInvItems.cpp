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

// GameInvItems.cpp CInvBinoculars and InvItem.cpp SendActionMessage.
void CInvBinoculars::SendActionMessage()
{
    auto* player = dynamic_cast<CPlayerObj*>(BaseClass::GetServerDE()->HandleToObject(owner_));
    if (!player)
        return;
    if (BaseClass::GetServerDE()->SendToClient(player->GetClient(), OpenBlood2::InventoryAction{5, active}) == DE_OK)
        ++actions;
    else
        error = "Binoculars action could not reach its client";
}
int CInvBinoculars::Activate()
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    auto* player = server->IsObjectAlive(owner_) ? dynamic_cast<CPlayerObj*>(server->HandleToObject(owner_)) : nullptr;
    if (player)
    {
        // Already zoomed by something else (the sniper alt-fire in the source): ignore.
        if (player->GetZoomMode())
            return 0;
        active = true;
        SendActionMessage();
        player->SetZoomMode(true, 98); // Original cursor type 98.
    }
    else
        active = true;
    return active;
}
int CInvBinoculars::Deactivate()
{
    error.clear();
    auto* server = BaseClass::GetServerDE();
    auto* player = server->IsObjectAlive(owner_) ? dynamic_cast<CPlayerObj*>(server->HandleToObject(owner_)) : nullptr;
    active = false;
    if (player)
    {
        SendActionMessage();
        player->SetZoomMode(false);
    }
    return active;
}
bool CInvBinoculars::ActivateItem()
{
    if (!active)
        return Activate() != 0;
    Deactivate(); // Source ignores the deactivation result and reports success.
    return true;
}

bool CInvNightGoggles::AddItem(DBYTE)
{
    if (count >= 100)
        return false;
    count = 100;
    remaining = 30;
    return true;
}
bool CInvNightGoggles::SetActive(bool on)
{
    error.clear();
    auto* s = BaseClass::GetServerDE();
    if (!s->IsObjectAlive(owner) || (on && remaining <= 0))
        return false;
    if (auto* p = dynamic_cast<CPlayerObj*>(s->HandleToObject(owner)))
        if (s->SendToClient(p->GetClient(), OpenBlood2::InventoryAction{3, on}) != DE_OK)
        {
            error = "Night goggles action could not reach its client";
            return false;
        }
    active = on;
    return true;
}
bool CInvNightGoggles::ActivateItem()
{
    return SetActive(!active);
}
bool CInvNightGoggles::Update()
{
    if (!active)
        return true;
    remaining -= BaseClass::GetServerDE()->GetFrameTime();
    count = remaining <= 0 ? 0 : static_cast<DBYTE>(100 * remaining / 30 + .5f);
    return remaining > 0 || SetActive(false);
}
