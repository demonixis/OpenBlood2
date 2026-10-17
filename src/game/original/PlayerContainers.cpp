#include "game/game_settings.h"
// Selected PlayerObj::UpdateClientPhysics and UpdateAirLevel source rules.
#include "PlayerObj.h"
#include <algorithm>
#include <cmath>

DDWORD CPlayerObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    // Original PlayerObj.cpp 4266-4280: ignore volume damage during the first
    // two level seconds. The explicit level clock survives delayed player entry
    // without restarting the grace period, and resets on world replacement.
    if (message == OpenBlood2::damage_message && server->GetWorldTime() < 2.0f &&
        server->IsKindOf(server->GetObjectClass(sender), server->GetClass("VolumeBrush")))
    {
        ++protected_volume_hits;
        return 0;
    }
    if (!m_bDead && !m_damage.IsDead() &&
        (message == OpenBlood2::key_query_message || message == OpenBlood2::key_pickup_message))
    {
        OpenBlood2::KeyMessage key;
        if (server->ReadKeyMessage(read, key))
        {
            const auto result = message == OpenBlood2::key_query_message ? m_InventoryMgr.QueryKey(key.item.name)
                                                                         : m_InventoryMgr.AddKey(key.item);
            key.have = result == InventoryResult::Ok;
            auto reply = (message == OpenBlood2::key_query_message || key.have)
                             ? server->StartMessageToObject(this, sender,
                                                            message == OpenBlood2::key_query_message
                                                                ? OpenBlood2::key_response_message
                                                                : OpenBlood2::picked_up_message)
                             : nullptr;
            if (reply)
            {
                server->WriteKeyMessage(reply, key);
                server->EndMessage(reply);
            }
        }
    }
    if (!m_bDead && !m_damage.IsDead() &&
        (message == OpenBlood2::add_ammo_message || message == OpenBlood2::add_powerup_message ||
         message == OpenBlood2::add_weapon_message || message == OpenBlood2::add_item_message))
    {
        OpenBlood2::PickupMessage pickup;
        if (server->ReadPickupMessage(read, pickup))
        {
            bool accepted = false;
            if (message == OpenBlood2::add_ammo_message)
                accepted = m_InventoryMgr.AddAmmo(pickup.type, pickup.amount) == InventoryResult::Ok;
            else if (message == OpenBlood2::add_item_message)
                accepted =
                    m_InventoryMgr.AddItem(pickup.type, static_cast<DBYTE>(pickup.amount)) == InventoryResult::Ok;
            else if (message == OpenBlood2::add_weapon_message)
            {
                if (server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) !=
                    static_cast<float>(GAMETYPE_SINGLE))
                    return CBaseCharacter::ObjectMessageFn(sender, message, read);
                auto result = m_InventoryMgr.ObtainWeapon(pickup.type);
                if (result == InventoryResult::Ok)
                {
                    int chance = 0;
                    // Original optional voice branch: retain its RNG draw. Playback remains unavailable.
                    if (server->RandomInt(0, 99, chance) && chance < 15)
                        ++weapon_pickup_voice_requests;
                }
                const auto* definition = weapon_definition(pickup.type);
                if (result != InventoryResult::Full && pickup.amount > 0 && definition &&
                    m_InventoryMgr.AddAmmo(definition->m_nAmmoType, pickup.amount) == InventoryResult::Ok)
                    result = InventoryResult::Ok;
                accepted = result == InventoryResult::Ok;
            }
            else
                switch (pickup.type)
                {
                    case 1:
                        accepted = m_damage.Heal(pickup.amount);
                        break;
                    case 2:
                        accepted = m_damage.MegaHeal(pickup.amount);
                        break;
                    case 3:
                        accepted = m_damage.AddWard(pickup.amount);
                        break;
                    case 4:
                        accepted = m_damage.AddNecroWard(pickup.amount);
                        break;
                }
            if (accepted)
            {
                auto reply = server->StartMessageToObject(this, sender, OpenBlood2::picked_up_message);
                if (reply)
                {
                    server->WritePickupMessage(reply, pickup);
                    server->EndMessage(reply);
                }
            }
        }
    }
    return CBaseCharacter::ObjectMessageFn(sender, message, read);
}

bool CPlayerObj::UpdateContainerState()
{
    auto* server = GetServerDE();
    DVector head;
    server->GetObjectPos(m_hObject, &head);
    head.y += eye_level;
    return server->GetContainers(m_hObject, head, containers);
}
bool CPlayerObj::UpdateAirLevel(float seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > .1f || !UpdateContainerState())
        return false;
    if (m_damage.IsDead())
        return true;
    if (IsLiquid(containers.head.code) && containers.head.code != CC_FREEFALL)
    {
        m_fAirLevel -= seconds * (100.0f / 15.0f);
        if (m_fAirLevel < 0)
        {
            m_fAirLevel = 0;
            DVector position;
            auto* server = GetServerDE();
            server->GetObjectPos(m_hObject, &position);
            auto message = server->StartMessageToObject(this, m_hObject, OpenBlood2::damage_message);
            if (!message)
                return false;
            if (server->WriteDamage(message,
                                    {{}, position, 5 * seconds, OpenBlood2::DamageType::Suffocate, m_hObject}) != DE_OK)
            {
                server->EndHMessageWrite(message);
                return false;
            }
            if (server->EndMessage(message) != DE_OK)
                return false;
        }
    }
    else
        m_fAirLevel = std::min(100.0f, m_fAirLevel + seconds * (100.0f / 2.5f));
    return UpdateStats();
}
