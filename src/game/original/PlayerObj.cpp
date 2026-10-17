// Selected original entry code; source ranges and adaptations are recorded in
// docs/m2_player_sources.json. This is an executable lifecycle port, not the
// complete 7,657-line PlayerObj gameplay implementation.
#include "PlayerObj.h"
#include "AI_Mgr.h"
#include "CameraObj.h"
#include "CharacterNodes.h"
#include "GameStartPoint.h"
#include <algorithm>
#include <cmath>

CBaseCharacter::CBaseCharacter() : B2BaseClass(OT_MODEL)
{
    AddAggregate(&m_damage);
    AddAggregate(&m_InventoryMgr);
    AddAggregate(&m_MoveObj); // Source order; only explicitly initialized owners sample transforms.
}
DDWORD CBaseCharacter::EngineMessageFn(DDWORD message, void* data, DFLOAT fData)
{
    if (message == MID_INITIALUPDATE && fData != INITIALUPDATE_SAVEGAME)
    {
        auto flags = g_pServerDE->GetObjectFlags(m_hObject);
        g_pServerDE->SetObjectFlags(m_hObject, flags | FLAG_MODELGOURAUDSHADE | FLAG_ANIMTRANSITION);
        g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (m_dwSurfType << 24));
    }
    if (message == MID_MODELSTRINGKEY && data && !OnStringKey(static_cast<ArgList*>(data)) &&
        pending_model_commands.size() < 32)
    {
        auto* args = static_cast<ArgList*>(data);
        std::string command;
        for (int i = 0; args->argv && i < args->argc; ++i)
        {
            if (i)
                command += ' ';
            if (args->argv[i])
                command += args->argv[i];
        }
        pending_model_commands.push_back(std::move(command));
    }
    return B2BaseClass::EngineMessageFn(message, data, fData);
}
CPlayerObj::~CPlayerObj()
{
    ClearRemoteBombs();
    AI_Mgr::RemovePlayer(*this);
    auto* server = GetServerDE();
    if (m_hCurSound)
        server->KillSound(m_hCurSound);
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
    // Original common restore initialization: bind aggregates/lists before
    // the savegame early return. Fresh stats, scent, strings and scheduling follow it.
    if (!m_InventoryMgr.Init(m_hObject, m_hClient) || !m_damage.Init(m_hObject, &m_InventoryMgr))
        return DFALSE;
    AI_Mgr::InsertPlayer(*this);
    m_bFirstUpdate = DTRUE;
    if (reason == INITIALUPDATE_SAVEGAME)
    {
        restore_initialization = initialized = true;
        m_fMoveVel = m_fJumpVel = 0;
        return DTRUE;
    }
    if (!server->SetBlockingPriority(m_hObject, 50))
        return DFALSE;
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
    {
        initial_smell_ = last_smell = smell->m_hObject;
        ++smell_drops;
    }
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
    // Caleb, default skin only until the other original character branches
    // are ported. Reject before mutation.
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
    if (!m_Anim_Sound.SetAnimationIndexes(m_hObject))
        return false;
    m_Anim_Sound.GenerateHitSpheres(m_hObject);
    m_Anim_Sound.SetSoundRoot("sounds\\chosen\\caleb");
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
    if (!OpenBlood2::reset_character_nodes(m_hObject, entry_error))
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
    ClearRemoteBombs();
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
    if (restore_initialization || attributes_received || !m_hClient || init.name.size() > 100 ||
        init.name.find('\0') != std::string::npos || init.character != 0 || init.skin != 0)
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
    entry_start_name.clear();
    entry_start_fallback = false;
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
        if (candidate && server->IsObjectAlive(link->m_hObject) && !candidate->IsMultiplayer())
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
    {
        start = fallback;
        entry_start_fallback = true;
    }
    if (!start)
    {
        entry_error = "No single-player GameStartPoint";
        return false;
    }
    const auto* selected_name = server->GetStringData(start->GetName());
    entry_start_name = selected_name ? selected_name : "";
    m_fYaw = start->GetPitchYawRoll().y;
    m_fPitch = 0;
    DVector position;
    if (server->GetObjectPos(start->m_hObject, &position) != DE_OK)
        return false;
    start->SendTrigger();
    // Trigger delivery may remove this owner or the marker. No marker reads after it.
    if (!server->IsObjectAlive(m_hObject) || server->TeleportObject(m_hObject, &position) != DE_OK ||
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
        server->SetAcceleration(m_hObject, &acceleration) != DE_OK)
        return false;
    OpenBlood2::MovementState before;
    if (!server->GetMovementState(m_hObject, before) ||
        !server->SetBlockingPriority(m_hObject, before.standing ? 50 : 40) ||
        server->MoveObject(m_hObject, &position) != DE_OK)
        return false;
    OpenBlood2::MovementState state;
    if (!server->GetMovementState(m_hObject, state))
        return false;
    // The original handler accepts the client's on-ground bit and enforces a
    // prediction leash. Here one local body resolves collisions once, so use
    // the resulting support and never teleport through a rejected collision.
    m_bOnGround = state.standing && state.ground_normal.y >= 0.5f;
    m_eLastSurface = static_cast<SurfaceType>(move.surface);
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
    if (m_bFirstUpdate && !InitializeClientState())
        return DFALSE;
    UpdateSound();
    if (server->GetTime() > m_fNextPingTime)
    {
        m_fNextPingTime = server->GetTime() + .5f;
        const auto observer = server->IsObjectAlive(g_hActiveCamera) ? g_hActiveCamera : m_hObject;
        if (!server->PingObjects(observer))
            ++activation_ping_failures;
    }
    return UpdateStats() && UpdateSmellTrail();
}
bool CPlayerObj::InitializeClientState()
{
    if (!m_hClient || m_nAttribSpeed < 1 || m_nAttribSpeed > 6)
        return false;
    // Original first update, PlayerObj.cpp:1591-1604. ValidateAttributes belongs
    // to fresh profile receipt, not restoration: it would alter saved caps/stats.
    static constexpr float move[] = {220, 250, 280, 310, 330, 350};
    m_fMoveVel = move[m_nAttribSpeed - 1];
    m_fJumpVel = 600;
    if (!UpdateClientPhysics() || !TeleportClientToServerPos() || !UpdateStats(true))
        return false;
    m_bFirstUpdate = DFALSE;
    return true;
}
bool CPlayerObj::UpdateStats(bool force)
{
    OpenBlood2::PlayerStats value{m_damage.m_fHitPoints, m_damage.m_fArmorPoints, m_InventoryMgr.GetAmmoCount(1)};
    // Original UpdateStats current primary/alternate ammo; never substitute the bullet pool.
    value.weapon_slot = m_InventoryMgr.m_nCurWeaponSlot;
    if (const auto* w = m_InventoryMgr.CurrentWeapon())
    {
        value.weapon = w->data.m_nType;
        value.ammo_type = w->GetAmmoType(false);
        value.alternate_ammo_type = w->GetAmmoType(true);
        value.ammo = m_InventoryMgr.GetAmmoCount(value.ammo_type);
        value.alternate_ammo = m_InventoryMgr.GetAmmoCount(value.alternate_ammo_type);
    }
    for (unsigned i = 0; i < value.weapon_slots.size(); ++i)
        if (const auto* w = m_InventoryMgr.GetWeapon(i))
            value.weapon_slots[i] = w->data.m_nType;
    if (!force && value == last_sent_stats_)
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
        case MID_LINKBROKEN:
            // This engine also notifies a pending owner before its destructor.
            // Keep that owner's list intact so original destruction removes charges.
            if (g_pServerDE->IsObjectAlive(m_hObject))
                RemoveRemoteBomb(static_cast<HOBJECT>(data));
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

FireResult CPlayerObj::HandleWeaponFireMessage(const OpenBlood2::WeaponFire& request)
{
    if (!m_hClient || !attributes_received || m_damage.IsDead() ||
        GetServerDE()->GetClientUserData(m_hClient) != m_hObject)
        return FireResult::Invalid;
    if (movement_blocked || camera_movement_blocked)
        return FireResult::Blocked;
    const auto result = m_InventoryMgr.FireCurrentWeapon(request);
    if (result == FireResult::Fired || result == FireResult::DeliveryFailed)
        UpdateStats();
    return result;
}
