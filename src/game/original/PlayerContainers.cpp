#include "game/game_settings.h"
// Selected PlayerObj::UpdateClientPhysics and UpdateAirLevel source rules.
#include "KeyPickup.h"
#include "Music.h"
#include "PickupObject.h"
#include "PlayerObj.h"
#include "game/key_messages.h"
#include <algorithm>
#include <cmath>

// Original ProcessTriggerMsg uses only a space-delimited first token. Fade
// suffixes are ignored, and dispatch has no dead-player guard. Other command
// families remain explicitly inspectable rather than pretending they ran.
void CPlayerObj::ProcessTriggerMsg(HMESSAGEREAD read, HOBJECT sender)
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
        if (server->IsMultiplayerGame() || end == std::string::npos || end + 1 == last_player_trigger.size())
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
    else if (OpenBlood2::same_name(token, "flaggrab"))
        GrabFlag(sender);
    else if (OpenBlood2::same_name(token, "flaggive"))
        GiveFlag(sender);
    else if (OpenBlood2::same_name(token, "flagdrop"))
        DropFlag();
    else
        ++ignored_triggers; // Original ProcessTriggerMsg has no branch for this token.
}

DDWORD CPlayerObj::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    auto* server = GetServerDE();
    if (message == OpenBlood2::immobilize_message && !m_bImprisoned && !m_bDead)
    {
        float seconds;
        if (!server->ReadImmobilize(read, seconds) || !Imprison(seconds))
            return 0;
    }
    if (message == 1000 /* MID_TRIGGER */)
        ProcessTriggerMsg(read, sender);
    // Original PlayerObj.cpp 4266-4280: ignore volume damage during the first
    // two level seconds. The explicit level clock survives delayed player entry
    // without restarting the grace period, and resets on world replacement.
    if (message == OpenBlood2::damage_message && server->GetWorldTime() < 2.0f &&
        server->IsKindOf(server->GetObjectClass(sender), server->GetClass("VolumeBrush")))
    {
        ++protected_volume_hits;
        return 0;
    }
    // Source PlayerObj handles this before its damage aggregate: a finishing
    // hit confirms the original lethal attacker's frag, even for a different sender.
    if (message == OpenBlood2::damage_message && m_bInSlowDeath && m_fSlowDeathSafeTimer <= 0)
        DoDeath(true);
    if (!m_bDead && !m_damage.IsDead() &&
        (message == OpenBlood2::key_query_message || message == OpenBlood2::key_pickup_message))
    {
        OpenBlood2::KeyMessage key;
        if (server->ReadKeyMessage(read, key))
        {
            const auto result = message == OpenBlood2::key_query_message ? m_InventoryMgr.QueryKey(key.item.name)
                                                                         : m_InventoryMgr.AddKey(key.item);
            key.have = result == InventoryResult::Ok;
            const int picked = result == InventoryResult::Ok            ? CHWEAP_OK
                               : result == InventoryResult::AlreadyHave ? CHWEAP_ALREADYHAVE
                                                                        : CHWEAP_NOMESSAGE;
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
            if (message == OpenBlood2::key_pickup_message)
                SendPickedUpMessage(sender, picked);
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
            // Original iRet for SendPickedUpMessage (powerups send only an accepted CHWEAP_OK).
            int picked = CHWEAP_NOMESSAGE;
            const auto original_result = [](InventoryResult result)
            {
                switch (result)
                {
                    case InventoryResult::Ok:
                        return int(CHWEAP_OK);
                    case InventoryResult::NotAvailable:
                    case InventoryResult::Invalid:
                        return int(CHWEAP_NOTAVAIL);
                    case InventoryResult::Current:
                        return int(CHWEAP_CURRENT);
                    case InventoryResult::Full:
                        return int(CHWEAP_NOAVAILSLOTS);
                    case InventoryResult::AlreadyHave:
                        return int(CHWEAP_ALREADYHAVE);
                    case InventoryResult::Busy:
                        return int(CHWEAP_WEAPONBUSY);
                }
                return int(CHWEAP_NOMESSAGE);
            };
            if (message == OpenBlood2::add_ammo_message)
            {
                // Original AddAmmo returns CHWEAP_OK or CHWEAP_NOMESSAGE.
                accepted = m_InventoryMgr.AddAmmo(pickup.type, pickup.amount) == InventoryResult::Ok;
                picked = accepted ? CHWEAP_OK : CHWEAP_NOMESSAGE;
            }
            else if (message == OpenBlood2::add_item_message)
            {
                const bool weapon = pickup.type == 7 || pickup.type == 8 || pickup.type == 9;
                const auto result =
                    weapon ? m_InventoryMgr.AddInventoryWeapon(pickup.type, static_cast<DBYTE>(pickup.amount))
                           : m_InventoryMgr.AddItem(pickup.type, static_cast<DBYTE>(pickup.amount));
                accepted = result == InventoryResult::Ok;
                // An owned bomb returns AddAmmo's result, whose refusal is CHWEAP_NOMESSAGE.
                picked = weapon && result == InventoryResult::Full ? CHWEAP_NOMESSAGE : original_result(result);
            }
            else if (message == OpenBlood2::add_weapon_message)
            {
                if (server->IsMultiplayerGame()) // Server shell GetGameType() != GAMETYPE_SINGLE.
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
                picked = original_result(result);
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
                    case 5: // POWERUP_INVULNERABILITY
                    case 7: // POWERUP_ANGER
                    case 6: // POWERUP_STEALTH
                        accepted = AddPowerup(static_cast<DBYTE>(pickup.type), pickup.amount);
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
            if (message != OpenBlood2::add_powerup_message)
                SendPickedUpMessage(sender, picked);
            else if (accepted) // AddPowerup: if (hSender && bPickedUp) ... CHWEAP_OK.
                SendPickedUpMessage(sender, CHWEAP_OK);
        }
    }
    return CBaseCharacter::ObjectMessageFn(sender, message, read);
}

void CPlayerObj::SendPickedUpMessage(HOBJECT hObject, int iRet)
{
    auto* pServerDE = GetServerDE();
    // Display an appropriate client message
    if (!m_hClient || !pServerDE || !hObject)
        return;

    // IsKindOf(PickupObject): the port's KeyPickup is not a PickupObject subclass, but the
    // original's was, with the same ResourceNum-or-ObjectName display name.
    const auto* pickup = dynamic_cast<PickupObject*>(pServerDE->HandleToObject(hObject));
    const auto* key = pickup ? nullptr : dynamic_cast<KeyPickup*>(pServerDE->HandleToObject(hObject));
    if (!pickup && !key)
        return;

    OpenBlood2::ConsoleTextSource source;
    // PickupObject InitialUpdate: FormatString(m_nNameID), otherwise the object name.
    const auto display_name = [&]
    {
        const auto id = pickup ? pickup->resource_id : key->resource_id;
        std::string name;
        if (id && pServerDE->FindClientResourceString(std::uint32_t(id), name))
        {
            source.argument_resource = std::uint32_t(id);
            return name;
        }
        if (id)
            pickup_message_error = "Missing pickup name string " + std::to_string(id) + "; object name shown";
        return pickup ? pickup->display_name : key->item.name;
    };
    const auto format = [&](std::uint32_t id)
    {
        // FormatString(id, szObject): cres.dll uses FormatMessage's numbered insertion.
        source.resource = id;
        std::string text;
        if (!pServerDE->FindClientResourceString(id, text))
        {
            pickup_message_error = "Missing pickup message string " + std::to_string(id);
            return std::string();
        }
        if (const auto marker = text.find("%1!s!"); marker != text.npos)
        {
            source.argument = display_name();
            text.replace(marker, 5, source.argument);
        }
        else if (const auto plain = text.find("%1"); plain != text.npos)
        {
            source.argument = display_name();
            text.replace(plain, 2, source.argument);
        }
        return text;
    };
    std::string msgbuf;
    switch (iRet)
    {
        case CHWEAP_OK:
            msgbuf = format(260); // IDS_GENERAL_PICKUP_1
            break;
        case CHWEAP_NOTAVAIL:
            if (hObject != m_hLastPickupObject && iRet != m_nLastPickupResult)
                msgbuf = format(261); // IDS_GENERAL_PICKUP_2
            break;
        case CHWEAP_NOAVAILSLOTS:
            if (hObject != m_hLastPickupObject && iRet != m_nLastPickupResult)
                msgbuf = format(262); // IDS_GENERAL_PICKUP_3
            break;
    }

    if (!msgbuf.empty() && SendConsoleMessage(msgbuf, std::move(source)))
        ++pickup_messages;

    m_hLastPickupObject = hObject;
    m_nLastPickupResult = iRet;
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
