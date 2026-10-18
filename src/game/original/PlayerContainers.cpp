#include "game/game_settings.h"
// Selected PlayerObj::UpdateClientPhysics and UpdateAirLevel source rules.
#include "Music.h"
#include "PlayerObj.h"
#include "game/key_messages.h"
#include <algorithm>
#include <cmath>

// Original ProcessTriggerMsg uses only a space-delimited first token. Fade
// suffixes are ignored, and dispatch has no dead-player guard. Other command
// families remain explicitly inspectable rather than pretending they ran.
void CPlayerObj::ProcessTriggerMsg(HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    auto text = server->ReadFromMessageHString(read);
    const auto* raw = server->GetStringData(text);
    last_player_trigger = raw ? raw : "";
    server->FreeString(text);
    ++trigger_messages;
    player_trigger_error.clear();
    const auto begin = last_player_trigger.find_first_not_of(' ');
    if (begin == std::string::npos)
        return;
    const auto end = last_player_trigger.find(' ', begin);
    const auto token = last_player_trigger.substr(begin, end - begin);
    const bool in = OpenBlood2::same_name(token, "fadein");
    if (in || OpenBlood2::same_name(token, "fadeout"))
    {
        if (server->SendToClient(m_hClient, OpenBlood2::ScreenFade{in}) == DE_OK)
            ++fade_messages;
        else
            player_trigger_error = "Screen fade client delivery failed";
    }
    else if (OpenBlood2::same_name(token, "sound"))
    {
        // strtok(NULL, "") preserves the suffix after exactly one consumed space.
        if (server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) != GAMETYPE_SINGLE ||
            end == std::string::npos || end + 1 == last_player_trigger.size())
            return;
        ++trigger_sound_requests;
        if (server->SendToClient(m_hClient, OpenBlood2::VoiceStop{}) == DE_OK)
            ++trigger_voice_stops;
        else
            player_trigger_error = "Voice stop client delivery failed";
        OpenBlood2::SoundRequest request;
        request.resource = trigger_sound_resource = last_player_trigger.substr(end + 1);
        request.space = OpenBlood2::SoundSpace::Local;
        request.priority = 6; // PLAYER_HIGH, buffered local one-shot, no reverb.
        request.reverb_requested = false;
        trigger_sound_error.clear();
        trigger_sound = server->PlayLocalSound(m_hObject, request, trigger_sound_error);
        if (trigger_sound)
            ++trigger_sound_starts;
    }
    else if (OpenBlood2::same_name(token, "music"))
    {
        OpenBlood2::MusicCommand command;
        const auto suffix =
            end == std::string::npos ? std::string_view{} : std::string_view(last_player_trigger).substr(end + 1);
        if (CMusic::ParseTrigger(suffix, command, player_trigger_error))
        {
            if (server->SendToClient(m_hClient, command) == DE_OK)
                ++music_messages;
            else
                player_trigger_error = "Music client delivery failed";
        }
    }
    else if (OpenBlood2::same_name(token, "flaggrab") || OpenBlood2::same_name(token, "flaggive") ||
             OpenBlood2::same_name(token, "flagdrop"))
    {
        ++unsupported_triggers;
        last_unsupported_player_trigger = last_player_trigger;
        player_trigger_error = "Player trigger command is not integrated";
    }
    else
        ++ignored_triggers; // Original ProcessTriggerMsg has no branch for this token.
}

DDWORD CPlayerObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    if (message == 1000 /* MID_TRIGGER */)
        ProcessTriggerMsg(read);
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
                accepted = ((pickup.type == 7 || pickup.type == 8)
                                ? m_InventoryMgr.AddInventoryWeapon(pickup.type, static_cast<DBYTE>(pickup.amount))
                                : m_InventoryMgr.AddItem(pickup.type, static_cast<DBYTE>(pickup.amount))) ==
                           InventoryResult::Ok;
            else if (message == OpenBlood2::add_weapon_message)
            {
                if (server->GetVarValueFloat(server->GetGameConVar("GAME_GAMETYPE")) !=
                    static_cast<float>(GAMETYPE_SINGLE))
                    return CBaseCharacter::ObjectMessageFn(sender, message, read);
                auto result = m_InventoryMgr.ObtainWeapon(pickup.type);
                if (result == InventoryResult::Ok)
                {
                    int chance = 0;
                    // Original 15% branch, then unique reply and an independent 20% group draw.
                    if (server->RandomInt(0, 99, chance) && chance < 15)
                    {
                        ++weapon_pickup_voice_requests;
                        PlayWeaponVoiceSoundOnClient(pickup.type);
                    }
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
