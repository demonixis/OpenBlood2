// Selected original ObjectDLL/PlayerObj.cpp voice message helpers.
// Copyright (c) 1997, 1998 Monolith Productions Inc.
#include "AI_Mgr.h"
#include "PlayerObj.h"
#include "game/game_settings.h"
#include <cmath>
#include <limits>
bool CPlayerObj::PlayVoiceGroupEventOnClient(int event, bool single_player_only)
{
    auto* server = GetServerDE();
    if (single_player_only && server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) != GAMETYPE_SINGLE)
        return false;
    const bool sent = server->SendToClient(m_hClient, OpenBlood2::VoiceEvent{true, m_nCharacter, event}) == DE_OK;
    if (sent)
        ++voice_messages;
    else
        ++voice_delivery_failures;
    return sent;
}
bool CPlayerObj::PlayVoiceUniqueEventOnClient(int event)
{
    auto* server = GetServerDE();
    if (server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) != GAMETYPE_SINGLE)
        return false;
    const bool sent = server->SendToClient(m_hClient, OpenBlood2::VoiceEvent{false, m_nCharacter, event}) == DE_OK;
    if (sent)
        ++voice_messages;
    else
        ++voice_delivery_failures;
    return sent;
}
void CPlayerObj::PlayWeaponVoiceSoundOnClient(int weapon)
{
    auto* server = GetServerDE();
    if (server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) != GAMETYPE_SINGLE)
        return;
    int unique = -1;
    switch (weapon)
    {
        case 6:
            unique = 0;
            break; // Howitzer
        case 13:
            unique = 1;
            break; // Tesla
        case 5:
            unique = 2;
            break; // Sniper rifle
        case 14:
            unique = 4;
            break; // Voodoo
        case 9:
            unique = 5;
            break; // Assault rifle
        case 7:
            unique = 6;
            break; // Napalm
        case 8:
            unique = 7;
            break; // Singularity
        case 4:
            unique = 8;
            break; // Shotgun
    }
    if (unique != -1)
        PlayVoiceUniqueEventOnClient(unique);
    int chance = 0;
    if (server->RandomInt(0, 99, chance) && chance < 20)
        PlayVoiceGroupEventOnClient(9, true);
}

CPlayerObj* CPlayerObj::LocalVoicePlayer()
{
    auto* server = GetServerDE();
    if (!server || !AI_Mgr::m_dwNumCabal)
        return nullptr;
    // Both active and inactive characters remain in this original list. Resolve
    // each identity through the owning adapter; pending/foreign players cannot speak.
    CPlayerObj* result = nullptr;
    unsigned visited = 0;
    auto* head = &AI_Mgr::m_CabalHead;
    for (auto* link = head->m_pNext; link != head; link = link->m_pNext)
    {
        if (!link || !link->m_pData || ++visited > AI_Mgr::m_dwNumCabal)
            return nullptr;
        auto handle = server->ObjectToHandle(static_cast<BaseClass*>(link->m_pData));
        if (!server->IsObjectAlive(handle))
            continue;
        auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(handle));
        if (player && player->GetClient() && server->GetClientUserData(player->GetClient()) == handle)
        {
            if (result)
                return nullptr; // The supported session has exactly one associated player.
            result = player;
        }
    }
    return result;
}
void CPlayerObj::UpdateDamageVoice()
{
    // Original UpdateStats compares truncated integer health, not damage packets.
    const double health = m_damage.m_fHitPoints;
    damage_voice_error.clear();
    if (!std::isfinite(health) || health < 0 || health > std::numeric_limits<std::uint32_t>::max())
    {
        damage_voice_error = "Player health outside original unsigned stat range";
        return;
    }
    const auto current = static_cast<std::uint32_t>(health);
    if (m_nLastHitPoints > current)
    {
        m_nLastDamage = m_nLastHitPoints - current;
        ++damage_voice_checks;
        damage_voice_event = -1;
        damage_voice_roll = -1;
        // Consume the source draw even when the chance is at least 100 percent,
        // before the single-player and client busy/camera/cooldown gates.
        if (!GetServerDE()->RandomInt(0, 99, damage_voice_roll))
            damage_voice_error = "Player damage voice draw unavailable";
        else if (std::uint64_t(damage_voice_roll) < 69ull + m_nLastDamage)
        {
            damage_voice_event = m_damage.reaction_damage_type == OpenBlood2::DamageType::Fire ? 5 : 2;
            ++damage_voice_requests;
            PlayVoiceGroupEventOnClient(damage_voice_event, true);
        }
    }
    m_nLastHitPoints = current;
}
