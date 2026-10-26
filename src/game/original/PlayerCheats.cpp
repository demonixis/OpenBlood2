// Selected original CPlayerObj::ProcessCheat (PlayerObj.cpp:3393-3609), reached by the
// Options > CHEATS rows through the typed CMSG_CHEAT. See cheat_contracts.md.
#include "KeyPickup.h"
#include "PlayerObj.h"
#include "game/game_settings.h"
#include "trigger_helpers.h"
#include <vector>
namespace
{
    constexpr int VME_POWERUP = 6, VME_WEAPON = 9;
}
bool CPlayerObj::ProcessCheat(DBYTE nCheatCode, bool bState)
{
    CServerDE* pServerDE = GetServerDE();
    auto refuse = [&]
    {
        ++cheats_refused;
        return false;
    };

    // No cheats in multiplayer: the server shell NetGame type, so Chosen (action) games allow them.
    if (pServerDE->IsMultiplayerGame())
        return refuse();

    switch (nCheatCode)
    {
        case CHEAT_KFA:
            m_InventoryMgr.SetFullAmmo();
            for (DBYTE weapon :
                 {WEAP_BERETTA, WEAP_SUBMACHINEGUN, WEAP_SHOTGUN, WEAP_ASSAULTRIFLE, WEAP_LIFELEECH, WEAP_TESLACANNON,
                  WEAP_ORB, WEAP_VOODOO, WEAP_DEATHRAY, WEAP_MINIGUN, WEAP_NAPALMCANNON, WEAP_SNIPERRIFLE})
                if (!m_InventoryMgr.HasWeapon(weapon))
                    m_InventoryMgr.ObtainWeapon(weapon); // _DEMO refuses the non-demo weapons.
            PlayVoiceGroupEventOnClient(VME_WEAPON, true);
            break;

        case CHEAT_GOD:
            m_damage.m_bGodMode = bState;
            if (bState)
                PlayVoiceGroupEventOnClient(VME_POWERUP, true);
            break;

        case CHEAT_AMMO:
            m_InventoryMgr.SetFullAmmo();
            break;

        case CHEAT_HEALTH:
            // The source also revives a dead player (flags, air, burning); that branch is not
            // ported, so a dead player is refused rather than half revived by Heal.
            if (m_bDead || m_damage.IsDead())
                return refuse();
            m_damage.AddWard(m_damage.m_fMaxArmorPoints); // CDestructable::Reset.
            m_damage.Heal(m_damage.m_fMaxHitPoints);
            break;

        case CHEAT_KILLALLAI:
        {
            HCLASS hClass = pServerDE->GetClass("AI_Mgr");
            HSTRING hMsg = pServerDE->CreateString("KILL");
            if (hClass && hMsg)
            {
                SendTriggerMsgToClass(this, hClass, hMsg);
            }
            // For good measure..
            HSTRING hName = pServerDE->CreateString("mime");
            if (hName && hMsg)
                SendTriggerMsgToObjects(this, hName, hMsg);
            if (hName)
                pServerDE->FreeString(hName);
            if (hMsg)
                pServerDE->FreeString(hMsg);
            break;
        }

        case CHEAT_GIVEALLINV:
            for (DBYTE item : {1 /* INV_MEDKIT */, 2 /* INV_FLASHLIGHT */, 3 /* INV_NIGHTGOGGLES */, 4 /* INV_THEEYE */,
                               5 /* INV_BINOCULARS */})
                m_InventoryMgr.AddItem(item, 100);
            break;

        case CHEAT_ALLKEYS:
        {
            // Replacement (no original key cheat): every key a KeyPickup of this world still
            // offers, without collecting the pickups or running their pickup triggers.
            std::vector<HOBJECT> pickups;
            for (HOBJECT h = pServerDE->GetNextObject(nullptr); h; h = pServerDE->GetNextObject(h))
                pickups.push_back(h);
            for (HOBJECT h = pServerDE->GetNextInactiveObject(nullptr); h; h = pServerDE->GetNextInactiveObject(h))
                pickups.push_back(h);
            cheat_keys_added = 0;
            for (HOBJECT h : pickups)
                if (const auto* key = dynamic_cast<KeyPickup*>(pServerDE->HandleToObject(h)); key && !key->picked_up)
                    if (m_InventoryMgr.AddKey(key->item) == InventoryResult::Ok)
                        ++cheat_keys_added;
            break;
        }

        default:
            // Weapon, clipping, attribute, character, powerup and view cheats are not reached
            // by the menu and remain unported.
            return refuse();
    }
    ++cheats_applied;
    return true;
}
