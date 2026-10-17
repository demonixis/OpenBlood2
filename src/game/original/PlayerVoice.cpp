// Selected original ObjectDLL/PlayerObj.cpp voice message helpers.
// Copyright (c) 1997, 1998 Monolith Productions Inc.
#include "PlayerObj.h"
#include "game/game_settings.h"
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
