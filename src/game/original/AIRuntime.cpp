// Selected original AI_Mgr construction/ReadProp/InitialUpdate/AI_Update and
// CultistAI/CivilianAI/Fanatic/Prophet construction/model/table initialization.
// See ai_runtime_contracts.md and fanatic_prophet_contracts.md.
#include "AI_Mgr.h"
#include "CameraObj.h"
#include "CivilianAI.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
using namespace OpenBlood2;
namespace
{
    AI_Mgr::RuntimeTables tables; // Reset after world objects are destroyed.
    struct WeaponPrefix
    {
        std::string_view prefix; // Compared bytes: the source _mbsncmp length.
        DBYTE id;                // 0 for a family not integrated here (except NONE).
        bool supported;
    };
    // Original AI_Mgr::WeaponStrToInt order (AI_Mgr.cpp:377-663). The _ADD_ON
    // COMBAT/FLAYER/ROCK/NIGHTMARE_FIREBALL branches are absent: the patched retail
    // object.lto contains no FLAYER literal (only BLOOD2NM does).
    constexpr WeaponPrefix weapon_prefixes[]{{"BERET", WEAP_BERETTA, true},
                                             {"SHOTG", WEAP_SHOTGUN, true}, // "SHOTGU" compared on 5 bytes.
                                             {"SNIPE", WEAP_SNIPERRIFLE, true},
                                             {"ASSAULTR", WEAP_ASSAULTRIFLE, true},
                                             {"SUBMA", WEAP_SUBMACHINEGUN, true},
                                             {"FLARE", WEAP_FLAREGUN, true},
                                             {"HOWITZ", WEAP_HOWITZER, true},
                                             {"FLAME", 0, false},
                                             {"NAPALM", WEAP_NAPALMCANNON, true},
                                             {"MINIG", WEAP_MINIGUN, true},
                                             {"VOODO", 0, false},
                                             {"ORB", WEAP_ORB, true},
                                             {"LASER", 0, false},
                                             {"LIFEL", WEAP_LIFELEECH, true},
                                             {"TESLA", WEAP_TESLACANNON, true},
                                             {"MELEE", WEAP_MELEE, true},
                                             {"SINGULAR", WEAP_SINGULARITY, true},
                                             {"NONE", 0, true},
                                             {"PROX_BOMB", 0, false},
                                             {"SHIKARI_CLAW", WEAP_SHIKARI_CLAW, true},
                                             {"SHIKARI_SPIT", WEAP_SHIKARI_SPIT, true},
                                             {"SOUL_CROWBAR", WEAP_SOUL_CROWBAR, true},
                                             {"SOUL_AXE", WEAP_SOUL_AXE, true},
                                             {"SOUL_PIPE", WEAP_SOUL_PIPE, true},
                                             {"SOUL_HOOK", WEAP_SOUL_HOOK, true},
                                             {"BEHEMOTH_CLAW", WEAP_BEHEMOTH_CLAW, true},
                                             {"HEAL", WEAP_ZEALOT_HEAL, true},
                                             {"SHIELD", WEAP_ZEALOT_SHIELD, true},
                                             {"ENERGY_BLAST", WEAP_ZEALOT_ENERGYBLAST, true},
                                             {"GROUND_FIRE", WEAP_ZEALOT_GROUNDFIRE, true},
                                             {"SHOCKWAVE", WEAP_ZEALOT_SHOCKWAVE, true},
                                             {"FIREBALL", WEAP_DRUDGE_FIREBALL, true},
                                             {"LIGHTNING", WEAP_DRUDGE_LIGHTNING, true},
                                             {"HAND_SQUEEZE", WEAP_HAND_SQUEEZE, true},
                                             {"THIEF_SUCK", WEAP_THIEF_SUCK, true},
                                             {"BONELEECH_SUCK", WEAP_BONELEECH_SUCK, true},
                                             {"NIGHTMARE_BITE", 0, false},
                                             {"BEHEMOTH_SHOCKWAVE", WEAP_BEHEMOTH_SHOCKWAVE, true},
                                             {"DEATHSHROUD_ZAP", 0, false},
                                             {"NAGA_BLAST", WEAP_NAGA_EYEBEAM, true},
                                             {"NAGA_SPIKE", WEAP_NAGA_SPIKE, true},
                                             {"NAGA_DEBRIS", WEAP_NAGA_DEBRIS, true},
                                             {"GIDEON_SHIELD", WEAP_GIDEON_SHIELD, true},
                                             {"GIDEON_WIND", WEAP_GIDEON_WIND, true},
                                             {"GIDEON_GOO", 0, false},
                                             {"GIDEON_VOMIT", WEAP_GIDEON_VOMIT, true},
                                             {"ANCIENTONE_BEAM", WEAP_ANCIENTONE_BEAM, true},
                                             {"ANCIENTONE_TENTACLE", WEAP_ANCIENTONE_TENTACLE, true},
                                             {"GIDEON_SPEAR", WEAP_GIDEON_SPEAR, true},
                                             {"ANCIENTONE_BEAM", 0, false},
                                             {"ANCIENTONE_TENTACLE", 0, false},
                                             {"SKULL", WEAP_SKULL, true}};
    float type_mass(AI_Mgr::NPCType type)
    {
        using T = AI_Mgr::NPCType;
        return type == T::UndeadGideon                                          ? 3000
               : type == T::BoneLeech || type == T::Thief || type == T::TheHand ? 125
                                                                                : 250;
    }
    // Shipped nonhumanoid models without head/neck nodes use the explicit body-damage policy.
    bool body_hits(AI_Mgr::NPCType type)
    {
        using T = AI_Mgr::NPCType;
        return type == T::AncientOne || type == T::AncientOneTentacle || type == T::BoneLeech || type == T::Thief ||
               type == T::TheHand || type == T::DeathShroud || type == T::Naga;
    }
} // namespace
bool AI_Mgr::WeaponStrToInt(std::string_view name, DBYTE& id)
{
    for (const auto& entry : weapon_prefixes)
        if (name.starts_with(entry.prefix))
        {
            id = entry.id;
            return entry.supported; // Never substitute Beretta for a matched, missing family.
        }
    id = WEAP_BERETTA; // Source default for a name matching no compiled branch (e.g. COMBAT).
    return true;
}
void AI_Mgr::ExchangeRuntimeTables(AI_Mgr::RuntimeTables& other)
{
    tables.swap(other);
}
void AI_Mgr::ResetRuntimeTables()
{
    tables = {};
}
void AI_Mgr::ConfigureRuntime(NPCType type)
{
    npc_type = type;
    m_fHearingDist = m_fSeeingDist = type == NPCType::Cultist ? 1500 : 2000;
    m_fSensingDist = 200;
    m_fSmellingDist = 0;
    m_fWalkSpeed = 75;
    m_fRunSpeed = 180;
    m_fRollSpeed = type == NPCType::Cultist ? 105 : 3.5f;
    m_dwFlags = type == NPCType::Cultist ? 0x2f : 0x20;
    m_bCabal = true;
    npc_weapons[0] = type == NPCType::Cultist ? "BERETTA" : "NONE";
    npc_ammo[0] = type == NPCType::Cultist ? 150 : 0;
    npc_ammo[2] = type == NPCType::Cultist ? 60 : 0;
    npc_ammo[5] = type == NPCType::Cultist ? 20 : 0;
    npc_ammo[6] = type == NPCType::Civilian ? 100 : 0;
    if (type == NPCType::Fanatic || type == NPCType::Prophet)
    {
        // Original Fanatic/Prophet constructors (shared AI_Mgr defaults otherwise).
        const bool fanatic = type == NPCType::Fanatic;
        m_fHearingDist = m_fSeeingDist = 1500;
        m_fRollSpeed = 105;
        m_fJumpSpeed = 400;
        m_dwFlags = 0x0f; // FLAG_CRAWL | FLAG_JUMP | FLAG_LIMP | FLAG_DODGE.
        npc_hp = fanatic ? 50 : 200;
        npc_armor = fanatic ? 0 : 100;
        npc_weapons = fanatic
                          ? std::array<std::string, 5>{"BERETTA", "COMBAT", "ASSAULTRIFLE", "NONE", "NONE"}
                          : std::array<std::string, 5>{"BERETTA", "COMBAT", "ASSAULTRIFLE", "COMBAT", "ASSAULTRIFLE"};
        // Bullets, BMG, shells, grenades, rockets, flares, charges, cells, fuel, prox.
        npc_ammo = fanatic ? std::array<float, 10>{300, 50, 50, 50, 25, 40, 120, 200, 100, 0}
                           : std::array<float, 10>{750, 0, 150, 50, 100, 100, 400, 600, 200, 0};
    }
    if (type == NPCType::SoulDrudge)
    {
        // Original SoulDrudge constructor. Its weapon draw runs at the start of precreate
        // (RuntimePrecreate), before the skin, so a restore draws nothing.
        m_fHearingDist = m_fSeeingDist = 1000;
        m_fSensingDist = 100;
        m_fWalkSpeed = 60;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0x200; // FLAG_NOAMMOCOLLECT only.
        m_bCabal = false;
        npc_hp = 120;
        npc_armor = 0;
    }
    if (type == NPCType::Shikari)
    {
        // Original ShikariAI constructor.
        m_fHearingDist = 2000;
        m_fSensingDist = 500;
        m_fSmellingDist = 2000;
        m_fSeeingDist = 1000;
        m_fWalkSpeed = 180;
        m_fRunSpeed = m_fRollSpeed = 320;
        m_fJumpSpeed = 600;
        m_dwFlags = 0x0b; // FLAG_CRAWL | FLAG_JUMP | FLAG_DODGE.
        m_bCabal = false;
        npc_hp = 175;
        npc_armor = 50;
        npc_weapons[0] = "SHIKARI_CLAW";
        npc_weapons[1] = "SHIKARI_SPIT";
    }
    if (type == NPCType::BoneLeech || type == NPCType::DrudgeLord)
    {
        const bool leech = type == NPCType::BoneLeech;
        m_fHearingDist = leech ? 0 : 2000;
        m_fSensingDist = leech ? 300 : 200;
        m_fSmellingDist = leech ? 1000 : 0;
        m_fSeeingDist = leech ? 500 : 1200;
        m_fWalkSpeed = leech ? 75 : 70;
        m_fRunSpeed = leech ? 80 : 180;
        m_fRollSpeed = 3.5f;
        m_fJumpSpeed = leech ? 600 : 500;
        m_dwFlags = leech ? 0x40 : 0; // FLAG_ALWAYSGIB.
        m_bCabal = false;
        npc_hp = leech ? 10 : 500;
        npc_armor = leech ? 0 : 100;
        npc_weapons[0] = leech ? "BONELEECH_SUCK" : "SOUL_HOOK";
        npc_weapons[1] = leech ? "NONE" : "FIREBALL";
    }
    if (type == NPCType::Gabriella || type == NPCType::GabriellaREV || type == NPCType::Ophelia ||
        type == NPCType::OpheliaREV)
    {
        const bool ophelia = type == NPCType::Ophelia || type == NPCType::OpheliaREV;
        m_fHearingDist = m_fSeeingDist = 1500;
        m_fRollSpeed = 105;
        m_fJumpSpeed = 400;
        m_dwFlags = 0x0b;
        npc_hp = npc_armor = ophelia ? 400 : 600;
        m_szAIState = "PASSIVE";
        npc_weapons[0] = ophelia ? "TESLA" : "MINIGUN";
        npc_weapons[1] = ophelia ? "SNIPER" : "NAPALM";
        npc_ammo = {
            500, ophelia ? 100.f : 0.f, 150, 50, 100, 100, ophelia ? 500.f : 400.f, ophelia ? 500.f : 600.f, 500, 0};
    }
    if (type == NPCType::Ishmael || type == NPCType::IshmaelREV)
    {
        m_fHearingDist = 0;
        m_fSensingDist = 1000;
        m_fSeeingDist = 800;
        m_fRollSpeed = 105;
        m_fJumpSpeed = 400;
        m_dwFlags = 0x20; // Original ALWAYSRECOIL.
        npc_hp = npc_armor = 500;
        m_szAIState = "PASSIVE";
        npc_weapons[0] = "ORB";
        npc_weapons[1] = "HEAL";
        npc_weapons[2] = "SHOCKWAVE";
        npc_ammo = {500, 100, 150, 50, 100, 100, 500, 500, 500, 0};
    }
    if (type == NPCType::Caleb)
    {
        m_fHearingDist = 0;
        m_fSeeingDist = 1000;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0;
        m_bCabal = false;
        npc_hp = 30;
        npc_armor = 0;
    }
    if (type == NPCType::MadScientist)
    {
        // Original MadScientistAI constructor; roll/jump keep the AI_Mgr defaults.
        m_fHearingDist = 0;
        m_fSeeingDist = 1000;
        m_fRollSpeed = 2;
        m_fJumpSpeed = 500;
        m_dwFlags = 0;
        npc_hp = 30;
        npc_armor = 0;
    }
    if (type == NPCType::Thief)
    {
        m_fHearingDist = m_fSensingDist = 1000;
        m_fSmellingDist = 1500;
        m_fSeeingDist = 800;
        m_fWalkSpeed = 75;
        m_fRunSpeed = 80;
        m_fRollSpeed = 3.5f;
        m_fJumpSpeed = 600;
        m_dwFlags = 0x240;
        m_bCabal = false;
        npc_hp = 20;
        npc_armor = 0;
        npc_weapons[0] = "THIEF_SUCK";
    }
    if (type == NPCType::Naga)
    {
        m_fHearingDist = m_fSensingDist = m_fSmellingDist = m_fSeeingDist = 4000;
        m_fWalkSpeed = m_fRunSpeed = 0;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0x100;
        m_bCabal = false;
        npc_hp = 10000;
        npc_armor = 1000;
        npc_weapons = {"NAGA_SPIKE", "NAGA_BLAST", "NAGA_DEBRIS", "NONE", "NONE"};
        m_damage.apply_damage_physics = false;
    }
    if (type == NPCType::Zealot)
    {
        m_fHearingDist = 2000;
        m_fSensingDist = 3000;
        m_fSmellingDist = 0;
        m_fSeeingDist = 800;
        m_fWalkSpeed = 75;
        m_fRunSpeed = 200;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0x20;
        m_bCabal = true;
        npc_hp = 100;
        npc_armor = 50;
        npc_weapons = {"HEAL", "SHIELD", "ENERGY_BLAST", "GROUND_FIRE", "SHOCKWAVE"};
    }
    if (type == NPCType::Behemoth || type == NPCType::DrudgePriest || type == NPCType::DeathShroud)
    {
        const bool behemoth = type == NPCType::Behemoth, priest = type == NPCType::DrudgePriest;
        m_fHearingDist = behemoth ? 2000 : 3000;
        m_fSensingDist = behemoth ? 200 : 3000;
        m_fSmellingDist = behemoth ? 2000 : 0;
        m_fSeeingDist = behemoth ? 1000 : priest ? 1500 : 3000;
        m_fWalkSpeed = 100;
        m_fRunSpeed = behemoth ? 320 : 180;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0;
        m_bCabal = false;
        npc_hp = behemoth ? 5000 : priest ? 1000 : 800;
        npc_armor = behemoth ? 1000 : priest ? 100 : 200;
        npc_weapons = behemoth
                          ? std::array<std::string, 5>{"BEHEMOTH_CLAW", "BEHEMOTH_SHOCKWAVE", "NONE", "NONE", "NONE"}
                      : priest ? std::array<std::string, 5>{"FIREBALL", "LIGHTNING", "SOUL_HOOK", "NONE", "NONE"}
                               : std::array<std::string, 5>{"SKULL", "NONE", "NONE", "NONE", "NONE"};
        if (behemoth || priest)
            m_damage.apply_damage_physics = false;
    }
    if (type == NPCType::TheHand)
    {
        m_fHearingDist = m_fSensingDist = 1000;
        m_fSmellingDist = 0;
        m_fSeeingDist = 800;
        m_fRunSpeed = 80;
        m_fJumpSpeed = 600;
        m_fRollSpeed = 3.5f;
        m_dwFlags = 0x240;
        m_bCabal = false;
        npc_hp = 10;
        npc_armor = 0;
        npc_weapons[0] = "HAND_SQUEEZE";
    }
    if (type == NPCType::AncientOne || type == NPCType::AncientOneTentacle)
    {
        const bool boss = type == NPCType::AncientOne;
        m_fHearingDist = boss ? 10000 : 0;
        m_fSensingDist = boss ? 10000 : 1000;
        m_fSmellingDist = 0;
        m_fSeeingDist = boss ? 18000 : 2000;
        m_fWalkSpeed = m_fRunSpeed = m_fRollSpeed = 0;
        m_dwFlags = boss ? 0x100 : 0;
        m_bCabal = false;
        m_damage.apply_damage_physics = false;
        npc_hp = boss ? 22000 : 200;
        npc_armor = boss ? 2000 : 0;
        npc_weapons[0] = boss ? "ANCIENTONE_BEAM" : "ANCIENTONE_TENTACLE";
        if (boss)
            npc_weapons[1] = "ANCIENTONE_TENTACLE";
    }
    if (type == NPCType::UndeadGideon)
    {
        m_fHearingDist = m_fSmellingDist = 2000;
        m_fSensingDist = 500;
        m_fSeeingDist = 1000;
        m_fWalkSpeed = 180;
        m_fRunSpeed = 350;
        m_fRollSpeed = 320;
        m_fJumpSpeed = 360;
        m_damage.apply_damage_physics = true; // Original CDestructable default; this boss does not disable it.
        m_dwFlags = 0x2a;                     // JUMP | DODGE | ALWAYSRECOIL.
        m_bCabal = false;
        npc_hp = 12000;
        npc_armor = 0;
        npc_weapons = {"GIDEON_SPEAR", "SKULL", "GIDEON_VOMIT", "LIGHTNING", "BEHEMOTH_SHOCKWAVE"};
    }
    if (type == NPCType::Gideon || type == NPCType::EnhancedGideon)
    {
        m_fHearingDist = 0;
        m_fSeeingDist = m_fSensingDist = 10000;
        m_bCabal = false;
        const bool enhanced = type == NPCType::EnhancedGideon;
        npc_hp = enhanced ? 9000 : 4000;
        npc_armor = enhanced ? 0 : 200;
        m_fWalkSpeed = enhanced ? 175 : 75;
        m_fRunSpeed = enhanced ? 380 : 180;
        m_fRollSpeed = enhanced ? 200 : 3.5f;
        npc_weapons = {"GIDEON_SHIELD", "ENERGY_BLAST", "NAGA_BLAST", "GIDEON_WIND", "NONE"};
        m_damage.apply_damage_physics = false;
    }
}
bool AI_Mgr::RuntimeFailure(std::string_view why)
{
    runtime_failed = true;
    runtime_error = why;
    ++runtime_failures;
    auto* s = GetServerDE();
    if (s)
        s->RejectPrecreate(why); // Only kept while this object's precreate is refusing.
    if (s && s->IsObjectAlive(m_hObject))
    {
        s->SetNextUpdate(m_hObject, 0);
        DVector zero{};
        s->SetVelocity(m_hObject, &zero);
        s->SetObjectState(m_hObject, OBJSTATE_INACTIVE);
    }
    return false;
}
bool AI_Mgr::RuntimePrecreate(ObjectCreateStruct* state, int reason)
{
    if (!state)
        return RuntimeFailure("NPC precreate requires a creation structure");
    if (reason == PRECREATE_SAVEGAME)
    {
        // Original PRECREATE_SAVEGAME: no property, no PostPropRead and no random draw. The engine
        // record's model and skin (D13) and every saved field come back afterwards.
        state->m_ObjectType = OT_MODEL;
        return true;
    }
    if (!configure_error.empty())
        return RuntimeFailure(configure_error);
    auto* s = GetServerDE();
    if (npc_type == NPCType::SoulDrudge)
    {
        // Original SoulDrudge constructor draw, before any property (AIWeapon1 still overrides it).
        int weapon = 1;
        if (!s->RandomInt(1, 3, weapon))
            return RuntimeFailure("SoulDrudge weapon RNG failed");
        npc_weapons[0] = weapon == 1 ? "SOUL_CROWBAR" : weapon == 2 ? "SOUL_AXE" : "SOUL_PIPE";
    }
    GenericProp prop;
    if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
    {
        if (s->GetPropGeneric("RandomHitPoints", &prop) == DE_OK)
            npc_random_hp = prop.m_Float;
        auto text = [&](const char* key, std::string& out)
        {
            if (s->GetPropGeneric(key, &prop) == DE_OK && prop.m_String[0])
                out = prop.m_String;
        };
        text("AIState", m_szAIState);
        for (unsigned i = 0; i < npc_weapons.size(); ++i)
            text(("AIWeapon" + std::to_string(i + 1)).c_str(), npc_weapons[i]);
        constexpr const char* names[]{"Bullets", "BMG",     "Shells", "Grenades", "Rockets",
                                      "Flares",  "Charges", "Cells",  "Fuel",     "Proximity_Bombs"};
        for (unsigned i = 0; i < npc_ammo.size(); ++i)
            if (s->GetPropGeneric(names[i], &prop) == DE_OK)
                npc_ammo[i] = prop.m_Float;
        text("SpotTriggerTarget", spot_target);
        text("SpotTriggerMessage", spot_message);
        text("TriggerRelayTarget", trigger_relay_target);
    }
    // Source subclass booleans are read only for world-file creation.
    if (reason == PRECREATE_WORLDFILE && (npc_type == NPCType::Cultist || npc_type == NPCType::Civilian))
    {
        auto flag = [&](const char* key, bool& out)
        {
            if (s->GetPropGeneric(key, &prop) == DE_OK)
                out = prop.m_Bool;
        };
        flag("Male", npc_male);
        if (npc_type == NPCType::Cultist)
        {
            flag("ClownSkin", npc_clown);
            flag("RobeSkin", npc_robe);
        }
        else
        {
            flag("LabTech", npc_labtech);
            flag("SororitySkin", npc_sorority);
            if (auto* c = dynamic_cast<CivilianAI*>(this))
                flag("Scared", c->m_bScared);
        }
    }
    if (reason == PRECREATE_WORLDFILE && npc_type == NPCType::Zealot && s->GetPropGeneric("Divine", &prop) == DE_OK)
    {
        npc_divine = prop.m_Bool;
        if (npc_divine)
        {
            npc_hp *= 2;
            npc_random_hp *= 2;
        }
    }
    if (!std::isfinite(npc_random_hp) || npc_random_hp < 0 || npc_random_hp > 1e6f)
        return RuntimeFailure("Invalid NPC random health");
    for (float value : npc_ammo)
        if (!std::isfinite(value) || value < 0 || value > 1e6f)
            return RuntimeFailure("Invalid NPC ammunition");
    for (const auto& name : npc_weapons)
    {
        DBYTE id;
        if (!WeaponStrToInt(name, id))
            return RuntimeFailure("NPC authored weapon unavailable: " + name);
    }
    int initial;
    if (!StateStrToInt(m_szAIState, initial))
        return RuntimeFailure("Invalid NPC authored state");
    std::string model, skin;
    int variant = 1;
    if (npc_type == NPCType::Behemoth || npc_type == NPCType::DrudgePriest || npc_type == NPCType::DeathShroud ||
        npc_type == NPCType::TheHand)
    {
        const std::string name = npc_type == NPCType::Behemoth       ? "behemoth"
                                 : npc_type == NPCType::DrudgePriest ? "drudgepriest"
                                 : npc_type == NPCType::DeathShroud  ? "DeathShroud"
                                                                     : "TheHand";
        model = "Models/Enemies/" + name + ".abc";
        skin = "Skins/Enemies/" + name + ".dtx";
    }
    else if (npc_type == NPCType::AncientOne || npc_type == NPCType::AncientOneTentacle)
    {
        if (npc_type == NPCType::AncientOneTentacle && !s->RandomInt(1, 4, variant))
            return RuntimeFailure("Tentacle model RNG unavailable");
        model = npc_type == NPCType::AncientOne ? "Models/Enemies/AncientOne.abc"
                                                : "models/enemies/ancientone_t" + std::to_string(variant) + ".abc";
        skin = "Skins/Enemies/AncientOne.dtx";
    }
    else if (npc_type == NPCType::UndeadGideon)
    {
        model = "Models/Enemies/Undead_Gideon.abc";
        skin = "Skins/Enemies/Undead_Gideon.dtx";
    }
    else if (npc_type == NPCType::Gideon || npc_type == NPCType::EnhancedGideon)
    {
        model = "Models/Enemies/gideon.abc";
        skin = npc_type == NPCType::EnhancedGideon ? "Skins/Enemies/GideonE.dtx" : "Skins/Enemies/gideon.dtx";
    }
    else if (npc_type == NPCType::Fanatic || npc_type == NPCType::Prophet)
    {
        // Original PostPropRead: fixed model and skin for every non-savegame precreate.
        const std::string name = npc_type == NPCType::Fanatic ? "Fanatic" : "Prophet";
        model = "Models/Enemies/" + name + ".abc";
        skin = "Skins/Enemies/" + name + ".dtx";
    }
    else if (npc_type == NPCType::Gabriella || npc_type == NPCType::GabriellaREV || npc_type == NPCType::Ophelia ||
             npc_type == NPCType::OpheliaREV)
    {
        const bool ophelia = npc_type == NPCType::Ophelia || npc_type == NPCType::OpheliaREV;
        const bool revenant = npc_type == NPCType::GabriellaREV || npc_type == NPCType::OpheliaREV;
        const std::string name = ophelia ? "ophelia" : "gabriella";
        model = "Models/Characters/" + name + ".abc";
        skin = "Skins/Characters/" + (revenant ? std::string(ophelia ? "ophrev" : "gabrev") : name) + ".dtx";
    }
    else if (npc_type == NPCType::Ishmael || npc_type == NPCType::IshmaelREV)
    {
        model = "Models/Characters/ishmael.abc";
        skin = npc_type == NPCType::Ishmael ? "Skins/Characters/ishmael.dtx" : "Skins/Characters/ishrev.dtx";
    }
    else if (npc_type == NPCType::Caleb)
    {
        model = "Models/Characters/caleb.abc";
        skin = "Skins/Characters/caleb1.dtx";
    }
    else if (npc_type == NPCType::MadScientist)
    {
        model = "Models/Enemies/madscientist.abc";
        skin = "Skins/Enemies/madscientist.dtx";
    }
    else if (npc_type == NPCType::BoneLeech || npc_type == NPCType::DrudgeLord)
    {
        const std::string name = npc_type == NPCType::BoneLeech ? "boneleech" : "drudge_lord";
        model = "Models/Enemies/" + name + ".abc";
        skin = "Skins/Enemies/" + name + ".dtx";
    }
    else if (npc_type == NPCType::Thief || npc_type == NPCType::Naga || npc_type == NPCType::Zealot)
    {
        const std::string name = npc_type == NPCType::Thief ? "Thief" : npc_type == NPCType::Naga ? "Naga" : "Zealot";
        model = "Models/Enemies/" + name + ".abc";
        skin = "Skins/Enemies/" + name + (npc_divine ? "Divine" : "") + ".dtx";
    }
    else if (npc_type == NPCType::Shikari)
    {
        model = "Models/Enemies/Shikari.abc";
        skin = "Skins/Enemies/Shikari.dtx";
    }
    else if (npc_type == NPCType::SoulDrudge)
    {
        // Original PostPropRead: one of three skins on the shared model.
        if (!s->RandomInt(1, 3, variant))
            return RuntimeFailure("NPC skin RNG failed");
        model = "Models/Enemies/SoulDrudge.abc";
        skin = variant == 1 ? "Skins/Enemies/SoulDrudge.dtx"
                            : "Skins/Enemies/SoulDrudge" + std::to_string(variant) + ".dtx";
    }
    else if (npc_type == NPCType::Cultist)
    {
        if (npc_robe)
            return RuntimeFailure("Add-on robe Cultist initialization is not integrated");
        if (npc_clown)
            npc_male = true;
        if (!s->RandomInt(1, 3, variant))
            return RuntimeFailure("NPC skin RNG failed");
        const std::string gender = npc_male ? "m" : "f";
        model = "Models/Enemies/" + gender + "_Cultist1.abc";
        skin = (npc_clown ? "Skins_ao/Enemies_ao/m_Cultist" : "Skins/Enemies/" + gender + "_Cultist") +
               std::to_string(variant + (npc_clown ? 3 : 0)) + ".dtx";
    }
    else
    {
        std::string name;
        if (initial == StateSpecial8)
        {
            name = "m_civilian";
            variant = 4;
        }
        else if (npc_sorority)
        {
            name = "f_civilian";
            variant = 3;
        }
        else
        {
            if (!s->RandomInt(1, 3, variant))
                return RuntimeFailure("NPC skin RNG failed");
            name = npc_labtech ? "labtech" : npc_male ? "m_civilian" : "f_civilian";
            if (!npc_labtech && !npc_male)
                variant = 1; // Source consumes a draw but always selects female skin 1.
        }
        model = "Models/Enemies/" + name + (name == "labtech" ? "" : "1") + ".abc";
        skin = (npc_sorority && initial != StateSpecial8 ? "Skins_ao/Enemies_ao/" : "Skins/Enemies/") + name +
               std::to_string(variant) + ".dtx";
    }
    state->m_ObjectType = OT_MODEL;
    SAFE_STRCPY(state->m_Filename, model.c_str());
    SAFE_STRCPY(state->m_SkinName, skin.c_str());
    return true;
}
// The original subclass InitialUpdate (and Cultist/Civilian Load) table setup: the first
// instance of a class/gender builds the shared table, then InitStatics binds it.
bool AI_Mgr::InitializeSharedTable(std::string& error)
{
    error.clear();
    auto* s = GetServerDE();
    if (!s || !s->IsObjectAlive(m_hObject))
    {
        error = "NPC shared table requires a live owner";
        return false;
    }
    const bool male_group = npc_male || (npc_type == NPCType::Civilian && npc_labtech);
    const auto group = npc_type == NPCType::AncientOne           ? 28
                       : npc_type == NPCType::AncientOneTentacle ? 29
                       : npc_type == NPCType::UndeadGideon       ? 27
                       : npc_type == NPCType::EnhancedGideon     ? 26
                       : npc_type == NPCType::Ishmael            ? 24
                       : npc_type == NPCType::IshmaelREV         ? 25
                       : npc_type == NPCType::Gabriella          ? 20
                       : npc_type == NPCType::GabriellaREV       ? 21
                       : npc_type == NPCType::Ophelia            ? 22
                       : npc_type == NPCType::OpheliaREV         ? 23
                       : npc_type == NPCType::Caleb              ? 19
                       : npc_type == NPCType::Behemoth           ? 15
                       : npc_type == NPCType::DrudgePriest       ? 16
                       : npc_type == NPCType::DeathShroud        ? 17
                       : npc_type == NPCType::TheHand            ? 18
                       : npc_type == NPCType::Thief              ? 12
                       : npc_type == NPCType::Naga               ? 13
                       : npc_type == NPCType::Zealot             ? 14
                       : npc_type == NPCType::BoneLeech          ? 10
                       : npc_type == NPCType::DrudgeLord         ? 11
                       : npc_type == NPCType::Gideon             ? 4
                       : npc_type == NPCType::Fanatic            ? 5
                       : npc_type == NPCType::Prophet            ? 6
                       : npc_type == NPCType::SoulDrudge         ? 7
                       : npc_type == NPCType::Shikari            ? 8
                       : npc_type == NPCType::MadScientist
                           ? 9
                           : (npc_type == NPCType::Cultist ? 0 : 2) + (male_group ? 0 : 1);
    std::string table_error;
    auto initialize_table = [&](unsigned slot, bool refresh, const char* root)
    {
        if (!refresh)
            return true;
        auto candidate = std::make_shared<CAnim_Sound>();
        auto prepare = [&](CAnim_Sound& value)
        {
            if (!value.SetAnimationIndexes(m_hObject))
                return false;
            if (npc_type == NPCType::AncientOne || npc_type == NPCType::AncientOneTentacle ||
                npc_type == NPCType::BoneLeech || npc_type == NPCType::Thief || npc_type == NPCType::TheHand ||
                npc_type == NPCType::DeathShroud || npc_type == NPCType::Naga)
            {
                // These shipped nonhumanoid models have no head/neck. The donor reads uninitialized
                // head coordinates here. Keep localization unavailable; use the
                // explicit body-damage policy below instead of fabricated spheres.
                value.hit_sphere_error = "Nonhumanoid model has no head node; body damage policy";
            }
            else if (!value.GenerateHitSpheres(m_hObject))
            {
                table_error = value.hit_sphere_error;
                return false;
            }
            return value.SetSoundRoot(root);
        };
        if (!prepare(*candidate))
            return false; // Failed creation must not poison a shared table.
        if (!tables[slot])
        {
            tables[slot] = std::move(candidate);
            return true;
        }
        // Preflight above is read-only. Rebuild existing shared indexes without
        // copying/killing its independently owned voice (no callbacks in these queries).
        return prepare(*tables[slot]);
    };
    if (npc_type == NPCType::Civilian)
    {
        // Preserve BOTH independent source branches, including female LabTech.
        if ((npc_male || npc_labtech) && !initialize_table(2, npc_male || !tables[2], "sounds/enemies/civilian"))
        {
            error = "NPC male animation table initialization failed";
            return false;
        }
        if (!npc_male && !initialize_table(3, !tables[3], "sounds/enemies/f_civ"))
        {
            error = "NPC female animation table initialization failed";
            return false;
        }
    }
    else if (!initialize_table(
                 group, !tables[group],
                 (npc_type == NPCType::Ishmael || npc_type == NPCType::IshmaelREV)       ? "sounds/chosen/ishmael"
                 : (npc_type == NPCType::Gabriella || npc_type == NPCType::GabriellaREV) ? "sounds/chosen/gabriella"
                 : (npc_type == NPCType::Ophelia || npc_type == NPCType::OpheliaREV)     ? "sounds/chosen/ophelia"
                 : npc_type == NPCType::Caleb                                            ? "sounds/chosen/caleb"
                 : npc_type == NPCType::Behemoth                                         ? "sounds/enemies/Behemoth"
                 : npc_type == NPCType::DrudgePriest                                     ? "sounds/enemies/drudgepriest"
                 : npc_type == NPCType::DeathShroud                                      ? "sounds/enemies/DeathShroud"
                 : npc_type == NPCType::TheHand                                          ? "sounds/enemies/TheHand"
                 : npc_type == NPCType::Thief                                            ? "sounds/enemies/Thief"
                 : npc_type == NPCType::Naga                                             ? "sounds/enemies/Naga"
                 : npc_type == NPCType::Zealot                                           ? "sounds/enemies/Zealot"
                 : npc_type == NPCType::BoneLeech                                        ? "sounds/enemies/boneleech"
                 : npc_type == NPCType::DrudgeLord                                       ? "sounds/enemies/drudgelord"
                 : npc_type == NPCType::AncientOne                                       ? "sounds/enemies/AncientOne"
                 : npc_type == NPCType::AncientOneTentacle ? "sounds/enemies/AncientOneTentacle"
                 : npc_type == NPCType::UndeadGideon       ? "sounds/enemies/Undead_Gideon"
                 : npc_type == NPCType::EnhancedGideon     ? "sounds/enemies/EnhancedGideon"
                 : npc_type == NPCType::Gideon             ? "sounds/enemies/Gideon"
                 : npc_type == NPCType::Fanatic            ? "sounds/enemies/fanatic"
                 : npc_type == NPCType::Prophet            ? "sounds/enemies/Prophet"
                 : npc_type == NPCType::SoulDrudge         ? "sounds/enemies/souldrudge"
                 : npc_type == NPCType::Shikari            ? "sounds/enemies/shikari"
                 : npc_type == NPCType::MadScientist       ? "sounds/enemies/madscientist"
                 : male_group                              ? "sounds/enemies/m_cultist"
                                                           : "sounds/enemies/f_cultist"))
    {
        error = "NPC shared animation table initialization failed: " + table_error;
        return false;
    }
    if (!InitStatics(tables[group]))
    {
        error = "NPC shared animation table unavailable";
        return false;
    }
    return true;
}
bool AI_Mgr::RuntimeInitialize(int reason)
{
    if (reason == INITIALUPDATE_SAVEGAME)
        return RestoreInitialize();
    auto* s = GetServerDE();
    if (runtime_initialized || !s->IsObjectAlive(m_hObject))
        return RuntimeFailure("Invalid or repeated NPC initialization");
    std::string table_error;
    if (!InitializeSharedTable(table_error))
        return RuntimeFailure(table_error);
    if (!m_InventoryMgr.Init(m_hObject, nullptr) || !m_damage.Init(m_hObject, &m_InventoryMgr))
        return RuntimeFailure("NPC aggregates unavailable");
    m_InventoryMgr.SetStrength((npc_type == NPCType::Gideon || npc_type == NPCType::EnhancedGideon) ? 10 : 5);
    // Original weapon IDs are sparse (Gideon's loadout reaches 42). Track the
    // at-most-five requested IDs, not an array sized for player-only weapons.
    std::array<DBYTE, 5> obtained{};
    for (const auto& name : npc_weapons)
    {
        DBYTE id;
        if (!WeaponStrToInt(name, id))
            return RuntimeFailure("NPC weapon unavailable");
        if (id && std::find(obtained.begin(), obtained.end(), id) == obtained.end())
        {
            if (m_InventoryMgr.ObtainWeapon(id) != InventoryResult::Ok)
                return RuntimeFailure("NPC weapon acquisition failed");
            *std::find(obtained.begin(), obtained.end(), DBYTE{0}) = id;
        }
    }
    DBYTE first;
    WeaponStrToInt(npc_weapons[0], first);
    if (first)
    {
        const auto result = m_InventoryMgr.ChangeWeapon(first);
        if (result != InventoryResult::Ok && result != InventoryResult::Current)
            return RuntimeFailure("NPC weapon selection failed");
    }
    constexpr DBYTE ammo[]{1, 3, 2, 5, 6, 4, 8, 8, 7};
    for (unsigned i = 0; i < 9; ++i)
        if (m_InventoryMgr.AddAmmo(ammo[i], npc_ammo[i]) == InventoryResult::Invalid)
            return RuntimeFailure("NPC ammunition initialization failed");
    m_InventoryMgr.AddAmmo(9, 200);
    // Source AddInventoryWeapon(INV_PROXIMITY, (int)m_fProxBombs): a slot-10
    // inventory weapon the AI never selects (FindBestWeapon
    // scans slots 0..9 and MC_LayProximity has no caller). The source also makes the
    // call with a zero count; that empty weapon is omitted here.
    if (npc_ammo[9] != 0)
    {
        const auto result = m_InventoryMgr.AddInventoryWeapon(7, static_cast<DBYTE>(static_cast<int>(npc_ammo[9])));
        if (result != InventoryResult::Ok && !(IsDemoBuild() && result == InventoryResult::NotAvailable))
            return RuntimeFailure("NPC proximity inventory initialization failed");
    }
    const float difficulty = GetGameDifficulty() == 0 ? .25f : GetGameDifficulty() == 2 ? 1.5f : 1;
    // Cultist, Fanatic and Prophet InitialUpdate add a 0.5 multiplier before the base difficulty.
    const bool halved = npc_type == NPCType::Cultist || npc_type == NPCType::Fanatic || npc_type == NPCType::Prophet ||
                        npc_type == NPCType::DeathShroud;
    m_InventoryMgr.damage_multiplier = (halved ? .5f : npc_divine ? 1.5f : 1) * difficulty;
    float unit;
    if (!s->RandomFloat(0, 1, unit))
        return RuntimeFailure("NPC health RNG failed");
    // Source Random(1, RandomHitPoints) has reversed endpoints at the default zero.
    const float hp = (npc_hp + 1 + (npc_random_hp - 1) * unit) * difficulty;
    m_damage.SetHitPoints(hp);
    m_damage.m_fMaxHitPoints = m_damage.m_fMaxMegaHitPoints = hp;
    m_damage.SetArmorPoints(npc_armor * difficulty);
    m_damage.m_fMaxArmorPoints = m_damage.m_fMaxNecroArmorPoints = npc_armor * difficulty;
    m_fAIMass = type_mass(npc_type);
    m_damage.m_fMass = m_fAIMass;
    if (!s->ConfigureCharacterBody(m_hObject, m_damage.m_fMass, 5 + m_damage.m_fMass * 10 / 500))
        return RuntimeFailure("NPC mass/friction configuration failed");
    if (!s->SetBlockingPriority(m_hObject, 50) || !m_MoveObj.Init(m_hObject) || !StateStrToInt(m_szAIState, m_nState))
        return RuntimeFailure("NPC movement/state initialization failed");
    if (m_nState == StateGuard && s->GetObjectPos(m_hObject, &m_vGuardPos) != DE_OK)
        return RuntimeFailure("NPC guard position unavailable");
    s->SetObjectFlags(m_hObject, s->GetObjectFlags(m_hObject) | FLAG_VISIBLE | FLAG_SOLID | FLAG_TOUCH_NOTIFY |
                                     FLAG_GRAVITY | FLAG_STAIRSTEP | FLAG_SHADOW | FLAG_MODELKEYS |
                                     FLAG_REMOVEIFOUTSIDE);
    if (!InsertMe())
        return RuntimeFailure("NPC faction registration failed");
    float scale;
    if (!s->RandomFloat(-.05f, .1f, scale))
        return RuntimeFailure("NPC scale RNG failed");
    const float base_scale = npc_type == NPCType::AncientOne           ? 5
                             : npc_type == NPCType::AncientOneTentacle ? 2.5f
                             : npc_type == NPCType::BoneLeech          ? 2
                                                                       : 1;
    m_vScale = {base_scale - scale, base_scale + scale, base_scale - scale};
    // Supply the model's initial centered bounds before the later foot-anchored clip resize.
    DVector dims;
    if (s->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK || s->SetObjectDims(m_hObject, &dims) != DE_OK ||
        !s->ScaleObject(m_hObject, m_vScale) || !SetAnimation(0))
        return RuntimeFailure("NPC initial model dimensions/scale/animation failed: " + state_error);
    s->SetNextUpdate(m_hObject, .001f);
    m_bSetShutdown = false;
    if ((reason == INITIALUPDATE_WORLDFILE || npc_type == NPCType::AncientOne ||
         npc_type == NPCType::AncientOneTentacle) &&
        npc_type != NPCType::Gideon && npc_type != NPCType::EnhancedGideon && npc_type != NPCType::DrudgePriest)
    {
        const auto result = s->MoveObjectToGround(m_hObject);
        // Original AI_Mgr ignores a ground-ray miss: retain a clear authored body
        // and let gravity/FLAG_REMOVEIFOUTSIDE run. A collision failure is distinct.
        if (result != DE_OK && (result != DE_NOTFOUND || s->ResizeObject(m_hObject, m_vDims) != ResizeResult::Resized))
            return RuntimeFailure("NPC world ground placement failed: result " + std::to_string(result) + " body " +
                                  std::to_string(m_vDims.x) + "," + std::to_string(m_vDims.y) + "," +
                                  std::to_string(m_vDims.z));
    }
    // Replacement placement policy, not the source world-only ground drop:
    // spawned boxes can straddle authored walls too (Demo_01 spawn101). Separate
    // them once before simulation, without ground dropping or changing launch velocity.
    if ((reason != INITIALUPDATE_WORLDFILE || npc_type == NPCType::DrudgePriest) && npc_type != NPCType::Gideon &&
        npc_type != NPCType::EnhancedGideon && s->ResolveInitialWorldOverlap(m_hObject) != DE_OK)
        return RuntimeFailure("NPC spawned world-overlap placement failed");
    m_fLastUpdate = s->GetTime();
    m_damage.ai_body_hits = body_hits(npc_type);
    m_damage.ai_hit_localization = !m_damage.ai_body_hits;
    runtime_initialized = true;
    return true;
}
bool AI_Mgr::RestoreInitialize()
{
    auto* s = GetServerDE();
    DVector dims;
    if (runtime_initialized || !s->IsObjectAlive(m_hObject) || s->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK)
        return RuntimeFailure("NPC restore initialization requires a live model, once");
    // Original INITIALUPDATE_SAVEGAME: the subclass binds its statics, AI_Mgr initializes the
    // inventory and damage owners, then returns before weapons, health, flags, faction lists,
    // scale, animation, scheduling and ground placement. Load restores the rest. Cultist and
    // Civilian bind their gender table in Load, where the saved gender is known: the original
    // ran this step with the constructor's male default (save_load_contracts lot 11).
    if (npc_type != NPCType::Cultist && npc_type != NPCType::Civilian)
    {
        std::string error;
        if (!InitializeSharedTable(error))
            return RuntimeFailure(error);
    }
    if (!m_InventoryMgr.Init(m_hObject, nullptr) || !m_damage.Init(m_hObject, &m_InventoryMgr))
        return RuntimeFailure("NPC aggregates unavailable");
    m_fAIMass = type_mass(npc_type); // Constructor values; the saved body keeps the engine mass.
    m_damage.ai_body_hits = body_hits(npc_type);
    m_damage.ai_hit_localization = !m_damage.ai_body_hits;
    runtime_initialized = true;
    return true;
}
bool AI_Mgr::AI_Update()
{
    if (runtime_busy)
        return RuntimeFailure("Reentrant AI_Update rejected");
    if (!RuntimeReady())
        return false;
    runtime_busy = true;
    ++runtime_updates;
    const bool ok = RuntimeUpdateStep();
    runtime_busy = false;
    return ok && !runtime_failed;
}
bool AI_Mgr::RuntimeUpdateStep()
{
    auto* s = GetServerDE();
    if (!s->IsObjectAlive(m_hObject) || m_bRemoveMe)
        return false;
    const float now = s->GetTime();
    if (now - m_fLastUpdate >= 1 && m_bSetShutdown)
    {
        float delay;
        if (!s->RandomFloat(.01f, .5f, delay))
            return RuntimeFailure("NPC wake RNG failed");
        m_fLastUpdate = now;
        s->SetNextUpdate(m_hObject, delay);
        ++runtime_desyncs;
        return true;
    }
    m_fLastUpdate = now;
    s->SetNextUpdate(m_hObject, .001f);
    if (s->IsObjectAlive(g_hActiveCamera) && m_nState != StateScript && m_nState != StatePassive &&
        m_nState != StateIdle)
    {
        const auto* camera = dynamic_cast<CameraObj*>(s->HandleToObject(g_hActiveCamera));
        if (!camera)
            return RuntimeFailure("Active camera has wrong class");
        if (!camera->AllowPlayerMovement())
        {
            ++runtime_camera_pauses;
            return StopVelocity() || RuntimeFailure(state_error);
        }
    }
    if (!m_bSetShutdown)
    {
        if (!s->SetDeactivationTime(m_hObject, 3))
            return RuntimeFailure("NPC deactivation timer failed");
        s->SetObjectState(m_hObject, OBJSTATE_AUTODEACTIVATE_NOW);
        m_bSetShutdown = true;
    }
    if (m_hTarget && m_nState != StateEnemyAttach)
    {
        if (!s->IsObjectAlive(m_hTarget))
        {
            if (!SetNewTarget(nullptr))
                return RuntimeFailure(target_error);
            ++runtime_target_losses;
        }
        else
        {
            m_bHasTargeted = true;
            if (m_fSeeingDist > m_fSensingDist && m_fSensingDist > 0)
                m_fSensingDist = m_fSeeingDist;
            if (!RefreshTargetPosition())
                return RuntimeFailure(target_error);
            auto* character = dynamic_cast<CBaseCharacter*>(s->HandleToObject(m_hTarget));
            if (!character)
                return RuntimeFailure("AI_Update target is not a character");
            if (character->m_damage.IsDead())
            {
                if (!SetNewTarget(nullptr) || !SetNewState(StateIdle))
                    return RuntimeFailure("Dead NPC target transition failed");
                ++runtime_target_losses;
            }
            else if (!(m_dwFlags & 0x100u) && !CanSee(m_hTarget))
            {
                if (sight.outcome == AISightOutcome::Invalid || sight.outcome == AISightOutcome::Unavailable)
                    return RuntimeFailure("NPC target visibility unavailable");
                if (!SetNewTarget(nullptr))
                    return RuntimeFailure(target_error);
                ++runtime_target_losses;
            }
        }
    }
    // A sought pickup may be collected by this actor or another player. Retain
    // the owned route, but never query its removed object on a later update.
    if (m_hTrackObject && m_hTrackObject == searched_ammo_target && !s->IsObjectAlive(m_hTrackObject))
    {
        m_hTrackObject = nullptr;
        tracked_position_valid = false;
        ++ammo_target_losses;
    }
    if (m_hTrackObject && !RefreshTrackedPosition())
        return RuntimeFailure(target_error);
    if (m_damage.m_fHitPoints <= 0 && m_nState != StateDying && !SetNewState(StateDying))
        return RuntimeFailure(state_error);
    MovementState support;
    if (!s->GetMovementState(m_hObject, support))
        return RuntimeFailure("NPC support unavailable");
    if (support.ground_object && s->IsObjectAlive(support.ground_object))
    {
        auto flags = s->GetObjectFlags(m_hObject);
        if (s->IsKindOf(s->GetObjectClass(support.ground_object), s->GetClass("CBaseCharacter")))
            flags |= FLAG_DONTFOLLOWSTANDING;
        else
            flags &= ~FLAG_DONTFOLLOWSTANDING;
        s->SetObjectFlags(m_hObject, flags);
    }
    if (Metacmd <= 0)
        Metacmd = 1;
    const auto perception_failures_before = perception_failures;
    bool ok;
    if (m_nState == StateScript)
    {
        auto* paths = s->GetPathManager();
        ok = paths && AI_STATE_Script(*paths);
    }
    else
        ok = DispatchState();
    if (!s->IsObjectAlive(m_hObject))
        return false;
    if (!ok || runtime_failed)
    {
        auto detail = !navigation_error.empty()   ? navigation_error
                      : !locomotion_error.empty() ? locomotion_error
                      : !script_error.empty()     ? script_error
                      : !state_error.empty()      ? state_error
                                                  : "state action failed";
        if (perception_failures != perception_failures_before)
        {
            detail += "; " + perception_error;
            if (perception_error == "Target link acquisition failed")
                detail += "; " + target_error;
        }
        return RuntimeFailure("AI_Update: " + detail);
    }
    return true;
}
