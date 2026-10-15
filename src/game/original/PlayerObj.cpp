// Selected original entry code; source ranges and adaptations are recorded in
// docs/M2_PLAYER_SOURCES.json. This is an executable lifecycle port, not the
// complete 7,657-line PlayerObj gameplay implementation.
#include "PlayerObj.h"
#include "GameStartPoint.h"
#include <algorithm>
#include <cmath>

DLink CDestructable::m_DestructableHead{};
DDWORD CDestructable::m_dwNumDestructables = 0;
CDestructable::CDestructable()
{
    if (m_dwNumDestructables == 0)
        dl_TieOff(&m_DestructableHead);
}
CDestructable::~CDestructable()
{
    if (m_Link.m_pData && m_dwNumDestructables > 0)
    {
        dl_Remove(&m_Link);
        --m_dwNumDestructables;
    }
}
DBOOL CDestructable::Init(HOBJECT object, CInventoryMgr* inventory)
{
    if (!object || !BaseClass::GetServerDE())
        return DFALSE;
    m_hObject = object;
    if (!m_Link.m_pData)
    {
        dl_Insert(&m_DestructableHead, &m_Link);
        m_Link.m_pData = this;
        ++m_dwNumDestructables;
    }
    m_pInventoryMgr = inventory;
    return DTRUE;
}
void CDestructable::CalculateResistance(DBYTE value)
{
    if (value == 1)
        m_fResistance = 1.20f;
    else if (value == 2)
        m_fResistance = 1.10f;
    else if (value == 3)
        m_fResistance = 1.0f;
    else if (value == 4)
        m_fResistance = 0.9f;
    else if (value == 5)
        m_fResistance = 0.8f;
    else if (value >= 6)
        m_fResistance = 0.7f;
}
CBaseCharacter::CBaseCharacter() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&m_damage);
    AddAggregate(&m_InventoryMgr);
    // Original m_MoveObj (steering/AI movement) is not imported in this subset.
}
DDWORD CBaseCharacter::EngineMessageFn(DDWORD message, void* data, DFLOAT fData)
{
    if (message == MID_INITIALUPDATE && fData != INITIALUPDATE_SAVEGAME)
    {
        auto flags = g_pServerDE->GetObjectFlags(m_hObject);
        g_pServerDE->SetObjectFlags(m_hObject, flags | FLAG_MODELGOURAUDSHADE | FLAG_ANIMTRANSITION);
    }
    if (message == MID_MODELSTRINGKEY && data && pending_model_commands.size() < 32)
    {
        auto* args = static_cast<ArgList*>(data);
        std::string command;
        for (int i = 0; args->argv && i < args->argc; ++i)
        {
            if (i)
                command += ' ';
            command += args->argv[i];
        }
        pending_model_commands.push_back(std::move(command));
    }
    return B2BaseClass::EngineMessageFn(message, data, fData);
}
CPlayerObj::~CPlayerObj()
{
    auto* server = GetServerDE();
    if (m_hClient)
        server->SetClientUserData(m_hClient, nullptr);
    if (m_hstrTrigger)
        server->FreeString(m_hstrTrigger);
    if (m_hstrPlayerName)
        server->FreeString(m_hstrPlayerName);
    // Creation rollback owns the initial smell until entry succeeds. Ordinary
    // lifetime leaves the world-owned scent to its original 20-second timeout.
    if (!attributes_received && initial_smell_)
        server->RemoveObject(initial_smell_);
}
void CPlayerObj::PostPropRead(ObjectCreateStruct* state)
{
    SAFE_STRCPY(state->m_Filename, "Models\\Characters\\Caleb.abc");
    SAFE_STRCPY(state->m_SkinName, "Skins\\Characters\\Caleb1.dtx");
    SAFE_STRCPY(state->m_Name, "player");
    state->m_ObjectType = OT_MODEL;
}
DBOOL CPlayerObj::InitialUpdate(int reason)
{
    auto* server = GetServerDE();
    if (reason == INITIALUPDATE_SAVEGAME)
    {
        entry_error = "Player save restoration is not ported";
        return DFALSE;
    }
    // InitialUpdate precedes SetClient, as in the original engine.
    if (!m_InventoryMgr.Init(m_hObject, m_hClient) || !m_damage.Init(m_hObject, &m_InventoryMgr))
        return DFALSE;
    m_bFirstUpdate = DTRUE;
    m_InventoryMgr.SetAmmoCount(1, 50);
    server->SetNextUpdate(m_hObject, .001f);
    m_damage.SetHitPoints(100);
    m_damage.SetArmorPoints(0);
    m_damage.m_fMaxHitPoints = m_damage.m_fMaxMegaHitPoints = 100;
    m_damage.m_fMaxArmorPoints = m_damage.m_fMaxNecroArmorPoints = 100;
    m_damage.m_fMass = 250;
    m_fSmellTime = server->GetTime();
    m_fSmellDelay = .5f;
    ObjectCreateStruct state;
    server->GetObjectPos(m_hObject, &state.m_Pos);
    if (auto* smell = server->CreateObject(server->GetClass("SmellHint"), &state))
        initial_smell_ = smell->m_hObject;
    else
        return DFALSE;
    DVector dims{12, 82.0f / 2, 12};
    server->SetObjectDims(m_hObject, &dims);
    m_hstrTrigger = server->CreateString("TRIGGER");
    initialized = m_hstrTrigger != nullptr;
    return initialized;
}
bool CPlayerObj::SetCharacter(DBYTE character)
{
    // Caleb, default skin only until the other original character branches,
    // sound roots and hit-sphere generation are ported. Reject before mutation.
    if (character != 0 || m_nSkin != 0)
    {
        entry_error = "Only Caleb's default entry profile is ported";
        return false;
    }
    if (GetServerDE()->SetModelFilenames(m_hObject, "Models\\Characters\\Caleb.abc", "Skins\\Characters\\Caleb1.dtx") !=
        DE_OK)
    {
        entry_error = "Player model could not be loaded";
        return false;
    }
    m_nCharacter = character;
    return true;
}
bool CPlayerObj::Respawn()
{
    auto* server = GetServerDE();
    // Preserve the original single-player dead-player gate. This operation is
    // an entry reset, not a replacement for loading a save after death.
    if (m_damage.IsDead())
    {
        entry_error = "A dead single-player character requires save restoration";
        return false;
    }
    if (!SetCharacter(m_nCharacter))
        return false;
    const DDWORD flags = FLAG_VISIBLE | FLAG_MODELKEYS | FLAG_YROTATION | FLAG_ANIMTRANSITION | FLAG_TOUCH_NOTIFY |
                         FLAG_SOLID | FLAG_STAIRSTEP | FLAG_SHADOW;
    server->SetObjectFlags(m_hObject, flags);
    door_push_pending = false;
    door_push_velocity = {};
    DVector velocity{};
    server->SetVelocity(m_hObject, &velocity);
    server->SetAcceleration(m_hObject, &velocity);
    m_damage.SetHitPoints(100);
    m_damage.SetArmorPoints(0);
    m_bDead = DFALSE;
    move_state = MoveState::None;
    locomotion_clip.clear();
    locomotion_requested_clip.clear();
    locomotion_error.clear();
    // Entry reset may occur beneath a ceiling. Keep the accepted stance until
    // UpdateAnim can actually grow the shared body; never advertise a tall eye
    // and standing speed while collision still has the crouched dimensions.
    crouch_requested = forced_crouch = false;
    eye_level = crouched ? 12.0f : 36.0f;
    m_fAirLevel = 100;
    containers = {};
    for (DBYTE i = 1; i <= 12; ++i)
        m_InventoryMgr.SetAmmoCount(i, 0);
    m_InventoryMgr.SetAmmoCount(1, 50);
    ++respawns;
    return UpdateClientPhysics() && TeleportClientToServerPos();
}
void CPlayerObj::ValidateAttributes()
{
    auto clamp = [](DBYTE value) { return std::clamp<DBYTE>(value, 1, 5); };
    m_nAttribStrength = clamp(m_nAttribStrength);
    m_nAttribSpeed = clamp(m_nAttribSpeed);
    m_nAttribResistance = clamp(m_nAttribResistance);
    m_nAttribMagic = clamp(m_nAttribMagic);
    if (m_nAttribStrength + m_nAttribSpeed + m_nAttribResistance + m_nAttribMagic > 12)
        m_nAttribStrength = m_nAttribSpeed = m_nAttribResistance = m_nAttribMagic = 3;
    m_damage.CalculateResistance(m_nAttribResistance);
    m_InventoryMgr.SetStrength(m_nAttribStrength);
    m_InventoryMgr.SetMagic(m_nAttribMagic);
    static constexpr float move[] = {220, 250, 280, 310, 330, 350};
    static constexpr float maximum[] = {100, 100, 125, 150, 200, 250};
    static constexpr float mega[] = {100, 150, 200, 250, 300, 400};
    m_fMoveVel = move[m_nAttribSpeed - 1];
    m_fJumpVel = 600;
    m_damage.m_fMaxHitPoints = maximum[m_nAttribStrength - 1];
    m_damage.m_fMaxMegaHitPoints = mega[m_nAttribStrength - 1];
    m_damage.m_fMaxArmorPoints = maximum[m_nAttribMagic - 1];
    m_damage.m_fMaxNecroArmorPoints = mega[m_nAttribMagic - 1];
}
bool CPlayerObj::OnSinglePlayerInit(const OpenBlood2::PlayerInit& init)
{
    if (attributes_received || !m_hClient || init.name.size() > 100 || init.name.find('\0') != std::string::npos ||
        init.character != 0 || init.skin != 0)
        return false;
    // Reject unsupported loadouts before allocating strings or weapons.
    for (auto type : init.weapons)
        if (type && !weapon_definition(type & ~DUAL_HAND_MASK))
            return false;
    m_hstrPlayerName = GetServerDE()->CreateString(init.name.c_str());
    if (!m_hstrPlayerName)
        return false;
    struct Rollback
    {
        CInventoryMgr& inventory;
        HSTRING& name;
        bool committed = false;
        ~Rollback()
        {
            if (!committed)
            {
                inventory.ClearWeapons();
                g_pServerDE->FreeString(name);
                name = nullptr;
            }
        }
    } rollback{m_InventoryMgr, m_hstrPlayerName};
    m_nCharacter = init.character;
    m_nSkin = init.skin;
    m_nAttribStrength = init.strength;
    m_nAttribSpeed = init.speed;
    m_nAttribResistance = init.resistance;
    m_nAttribMagic = init.magic;
    ValidateAttributes();
    m_InventoryMgr.requested_weapons = init.weapons;
    if (m_InventoryMgr.ObtainWeapon(WEAP_MELEE, 0) != InventoryResult::Ok)
        return false;
    for (unsigned i = 0; i < init.weapons.size(); ++i)
    {
        const auto type = static_cast<DBYTE>(init.weapons[i] & ~DUAL_HAND_MASK);
        if (!type)
            continue;
        auto obtained = m_InventoryMgr.ObtainWeapon(type, static_cast<int>(i + 1));
        if (obtained != InventoryResult::Ok && obtained != InventoryResult::AlreadyHave)
            return false;
        if (init.weapons[i] & DUAL_HAND_MASK)
            m_InventoryMgr.ObtainWeapon(type, static_cast<int>(i + 1));
    }
    m_InventoryMgr.ChangeWeaponSlot(1);
    if (!Respawn())
        return false;
    rollback.committed = true;
    attributes_received = true;
    return true;
}
bool CPlayerObj::GoToStartPoint(HSTRING start_name)
{
    auto* server = GetServerDE();
    auto* list = server->FindNamedObjects("Blood2StartPoint");
    if (!list)
        return false;
    // Fix the original empty-list early-return leak and use checked casts.
    struct Release
    {
        ServerDE* server;
        ObjectList* list;
        ~Release()
        {
            server->RelinquishList(list);
        }
    } release{server, list};
    bool remove_string = !start_name;
    if (remove_string)
        start_name = server->CreateString("start");
    if (!start_name)
        return false;
    GameStartPoint* start = nullptr;
    GameStartPoint* fallback = nullptr;
    for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
    {
        auto* candidate = dynamic_cast<GameStartPoint*>(server->HandleToObject(link->m_hObject));
        if (candidate && !candidate->IsMultiplayer())
        {
            fallback = candidate;
            if (server->CompareStringsUpper(start_name, candidate->GetName()))
            {
                start = candidate;
                break;
            }
        }
    }
    if (remove_string)
        server->FreeString(start_name);
    if (!start)
        start = fallback;
    if (!start)
    {
        entry_error = "No single-player GameStartPoint";
        return false;
    }
    m_fYaw = start->GetPitchYawRoll().y;
    m_fPitch = 0;
    DVector position;
    server->GetObjectPos(start->m_hObject, &position);
    start->SendTrigger();
    if (server->TeleportObject(m_hObject, &position) != DE_OK ||
        server->SendToClient(m_hClient, OpenBlood2::PlayerRotation{m_fPitch, m_fYaw}) != DE_OK)
        return false;
    if (server->MoveObjectToGround(m_hObject) != DE_OK)
    {
        entry_error = "Start point has no supported ground collision";
        return false;
    }
    return UpdateClientPhysics() && TeleportClientToServerPos();
}
bool CPlayerObj::UpdateClientPhysics()
{
    if (!m_hClient)
        return true; // Original InitialUpdate has no client yet.
    OpenBlood2::PlayerPhysics physics;
    physics.model = "Models\\Characters\\Caleb.abc";
    physics.skin = "Skins\\Characters\\Caleb1.dtx";
    auto* server = GetServerDE();
    if (server->GetObjectDims(m_hObject, &physics.dimensions) != DE_OK ||
        server->GetGlobalForce(&physics.gravity) != DE_OK)
        return false;
    physics.move_speed = m_fMoveVel;
    physics.jump_speed = m_fJumpVel;
    return server->SendToClient(m_hClient, physics) == DE_OK;
}
bool CPlayerObj::TeleportClientToServerPos()
{
    if (!m_hClient)
        return true;
    OpenBlood2::PlayerPosition position;
    GetServerDE()->GetObjectPos(m_hObject, &position.position);
    position.move_code = ++m_byClientMoveCode;
    return GetServerDE()->SendToClient(m_hClient, position) == DE_OK;
}
bool CPlayerObj::HandlePlayerPositionMessage(const OpenBlood2::PlayerMove& move)
{
    auto* server = GetServerDE();
    if (!m_hClient || !attributes_received || server->GetClientUserData(m_hClient) != m_hObject ||
        move.move_code != m_byClientMoveCode)
        return false;
    auto finite = [](DVector v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) <= 10000000 &&
               std::abs(v.y) <= 10000000 && std::abs(v.z) <= 10000000;
    };
    if (!finite(move.position) || !finite(move.velocity) || !finite(move.acceleration))
        return false;
    auto velocity = move.velocity, acceleration = move.acceleration, position = move.position;
    if (server->SetVelocity(m_hObject, &velocity) != DE_OK ||
        server->SetAcceleration(m_hObject, &acceleration) != DE_OK || server->MoveObject(m_hObject, &position) != DE_OK)
        return false;
    OpenBlood2::MovementState state;
    if (!server->GetMovementState(m_hObject, state))
        return false;
    // The original handler accepts the client's on-ground bit and enforces a
    // prediction leash. Here one local body resolves collisions once, so use
    // the resulting support and never teleport through a rejected collision.
    m_bOnGround = state.standing && state.ground_normal.y >= 0.5f;
    ++movement_messages;
    return true;
}
DBOOL CPlayerObj::Update(DVector*)
{
    auto* server = GetServerDE();
    server->SetNextUpdate(m_hObject, .001f);
    ++updates;
    if (!m_hClient)
        return DTRUE;
    if (m_bFirstUpdate)
    {
        ValidateAttributes();
        if (!UpdateClientPhysics() || !TeleportClientToServerPos())
            return DFALSE;
        m_bFirstUpdate = DFALSE;
    }
    return UpdateStats();
}
bool CPlayerObj::UpdateStats()
{
    const OpenBlood2::PlayerStats value{m_damage.m_fHitPoints, m_damage.m_fArmorPoints, m_InventoryMgr.GetAmmoCount(1)};
    if (value.health == last_sent_stats_.health && value.armor == last_sent_stats_.armor &&
        value.bullets == last_sent_stats_.bullets)
        return true;
    if (GetServerDE()->SendToClient(m_hClient, value) != DE_OK)
        return false;
    last_sent_stats_ = value;
    return true;
}
DDWORD CPlayerObj::EngineMessageFn(DDWORD message, void* data, DFLOAT fData)
{
    DBOOL result = DTRUE;
    switch (message)
    {
        case MID_PRECREATE:
        {
            auto ret = CBaseCharacter::EngineMessageFn(message, data, fData);
            if (data)
                PostPropRead(static_cast<ObjectCreateStruct*>(data));
            return ret;
        }
        case MID_INITIALUPDATE:
            result = InitialUpdate(static_cast<int>(fData));
            break;
        case MID_TOUCHNOTIFY:
            ObjectTouchDoor(static_cast<HOBJECT>(data));
            break;
        case MID_UPDATE:
            result = Update(static_cast<DVector*>(data));
            break;
        default:
            break;
    }
    if (result)
        return CBaseCharacter::EngineMessageFn(message, data, fData);
    GetServerDE()->RemoveObject(m_hObject);
    return result;
}

// Selected ordinary normal/melee damage branch from Destructable.cpp. AI hit
// limbs, damage impulses, powerups, triggers and death reactions are deferred.
DDWORD CDestructable::ObjectMessageFn(LPBASECLASS owner, HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == OpenBlood2::damage_message)
    {
        OpenBlood2::Damage damage;
        if (!BaseClass::GetServerDE()->ReadDamage(read, damage))
            return 0;
        if (!m_bGodMode && !m_bDead)
        {
            auto amount = damage.amount * m_fResistance;
            if (damage.type == OpenBlood2::DamageType::Death)
            {
                m_fArmorPoints = 0;
                amount = m_fHitPoints;
            }
            if (m_fArmorPoints > 0 && damage.type != OpenBlood2::DamageType::Suffocate)
            {
                const auto ratio = m_fArmorPoints <= 25    ? .3f
                                   : m_fArmorPoints <= 50  ? .5f
                                   : m_fArmorPoints <= 100 ? .7f
                                   : m_fArmorPoints <= 150 ? .8f
                                                           : .9f;
                auto absorb = amount * ratio;
                m_fArmorPoints -= absorb;
                if (m_fArmorPoints < 0)
                {
                    absorb += m_fArmorPoints;
                    m_fArmorPoints = 0;
                }
                amount -= absorb;
            }
            amount = std::max(amount, 0.0f);
            m_fHitPoints -= amount;
            m_fLastDamageAmount += amount;
            last_damage = damage;
            if (m_fHitPoints <= 0)
            {
                m_bDead = DTRUE;
                m_fHitPoints = 0;
            }
        }
    }
    return Aggregate::ObjectMessageFn(owner, sender, message, read);
}
FireResult CPlayerObj::HandleWeaponFireMessage(const OpenBlood2::WeaponFire& request)
{
    if (!m_hClient || !attributes_received || m_damage.IsDead() ||
        GetServerDE()->GetClientUserData(m_hClient) != m_hObject)
        return FireResult::Invalid;
    const auto result = m_InventoryMgr.FireCurrentWeapon(request);
    if (result == FireResult::Fired || result == FireResult::DeliveryFailed)
        UpdateStats();
    return result;
}
