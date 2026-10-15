// Selected branches of BloodServerShell.cpp and BloodClientShell.cpp. The
// original call ordering is retained; source-level adaptations replace raw
// client data / packet reads with checked handles and owned typed values.
#include "BloodShells.h"
#include "PathPoint.h"
#include <cmath>

void CBloodServerShell::PreStartWorld()
{
    paths.clear();
    world_started = false;
    updates = 0;
    // Team setup, active cinematics, voice and multiplayer state are outside
    // the accepted fresh single-player entry mode.
}
bool CBloodServerShell::PostStartWorld()
{
    paths.clear();
    // Original PathMgr::BuildPathList / PathListData::Copy, with value-owned
    // strings. No AI GetPath/sorting/save support is implied by this snapshot.
    for (HOBJECT object = server_.GetNextObject(nullptr); object; object = server_.GetNextObject(object))
    {
        if (!server_.IsKindOf(server_.GetObjectClass(object), server_.GetClass("PathPoint")))
            continue;
        auto* point = dynamic_cast<PathPoint*>(server_.HandleToObject(object));
        if (!point)
            return false;
        PathEntry entry;
        server_.GetObjectPos(object, &entry.position);
        if (const char* value = server_.GetObjectName(object))
            entry.name = value;
        if (const char* value = server_.GetStringData(point->GetActionTarget()))
            entry.action_target = value;
        if (const char* value = server_.GetStringData(point->GetActionMessage()))
            entry.action_message = value;
        paths.push_back(std::move(entry));
    }
    world_started = true;
    return true;
}
CPlayerObj* CBloodServerShell::Player(HCLIENT client)
{
    auto object = server_.GetClientUserData(client);
    return object ? dynamic_cast<CPlayerObj*>(server_.HandleToObject(object)) : nullptr;
}
CPlayerObj* CBloodServerShell::CreatePlayer(HCLIENT client)
{
    ObjectCreateStruct state; // Original zero position, identity rotation, no flags.
    auto type = server_.GetClass("CPlayerObj");
    auto* player = type ? dynamic_cast<CPlayerObj*>(server_.CreateObject(type, &state)) : nullptr;
    if (!player)
        return nullptr;
    player->SetClient(client);
    if (server_.SetClientUserData(client, player->m_hObject) != DE_OK || !player->Respawn())
    {
        player->SetClient(nullptr);
        server_.SetClientUserData(client, nullptr);
        server_.RemoveObject(player->m_hObject);
        return nullptr;
    }
    return player;
}
bool CBloodServerShell::RespawnPlayer(CPlayerObj* player, HCLIENT client)
{
    if (!player || !client)
        return false;
    player->SetClient(client);
    if (server_.SetClientUserData(client, player->m_hObject) != DE_OK ||
        server_.SendToClient(client, OpenBlood2::PlayerInitRequest{}) != DE_OK)
        return false;
    // Fresh single-player only: no saved client refs, team selection or restore.
    if (!player->GoToStartPoint(nullptr))
        return false;
    server_.SetGameConVar("GAME_GOTOSTARTPOINT", "1");
    return true;
}
CPlayerObj* CBloodServerShell::OnClientEnterWorld(HCLIENT client)
{
    if (!world_started || !client || Player(client))
        return nullptr;
    auto* player = CreatePlayer(client);
    if (!player || !RespawnPlayer(player, client))
    {
        OnClientExitWorld(client);
        return nullptr;
    }
    return player;
}
void CBloodServerShell::OnClientExitWorld(HCLIENT client)
{
    auto* player = Player(client);
    if (player)
    {
        player->SetClient(nullptr);
        server_.RemoveObject(player->m_hObject);
    }
    server_.SetClientUserData(client, nullptr);
}
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerInit& init)
{
    auto* player = Player(client);
    return player && player->OnSinglePlayerInit(init);
}
FireResult CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::WeaponFire& fire)
{
    auto* player = Player(client);
    return player ? player->HandleWeaponFireMessage(fire) : FireResult::Invalid;
}
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerMove& move)
{
    auto* player = Player(client);
    return player && player->HandlePlayerPositionMessage(move);
}
void CBloodServerShell::Update(float)
{
    ++updates;
    // Original Update returns here for GAMETYPE_SINGLE. Remaining code is MP.
}
bool CBloodClientShell::StartNewWorld(const char* world, OpenBlood2::WorldMessage& message)
{
    if (!OpenBlood2::original_world_request(world, 0, OpenBlood2::LOADTYPE_NEW_GAME, 1, message))
        return false;
    return true; // State changes wait until the engine accepts the world.
}
bool CBloodClientShell::OnEnterWorld(HCLIENT client)
{
    auto object = server_.GetClientUserData(client);
    if (!object || m_bInWorld)
        return false;
    if (!move_mgr.Init(server_, client))
        return false;
    client_ = client;
    player = object;
    m_bInWorld = true;
    m_bFirstUpdate = true;
    m_bFirstWorld = true;
    position = {};
    camera_position = {};
    rotation = {};
    physics = {};
    stats = {};
    messages = updates = 0;
    blocker = "Standing movement/weapon inspection: stairs, containers, moving platforms, full combat, audio and "
              "autosave remain unported";
    return true;
}
void CBloodClientShell::OnExitWorld()
{
    weapons = {};
    weapon_input = {};
    move_mgr.Term();
    movement_input = {};
    player = nullptr;
    client_ = nullptr;
    m_bInWorld = false;
    m_bFirstWorld = true;
    m_bFirstUpdate = true;
    position = {};
    camera_position = {};
    rotation = {};
    physics = {};
    stats = {};
    blocker.clear();
}
bool CBloodClientShell::InitSinglePlayer(CBloodServerShell& server)
{
    if (!m_bFirstWorld)
        return false;
    OpenBlood2::PlayerInit init; // Default entry profile; original fields/order retained.
    if (!server.OnMessage(client_, init))
        return false;
    m_bFirstWorld = false;
    // Original next requests CMSG_AUTOSAVE and sets GS_PLAYING. Both remain
    // gated: successful attribute exchange cannot imply complete gameplay.
    return true;
}
bool CBloodClientShell::OnMessage(const OpenBlood2::PlayerEvent& event, CBloodServerShell& server)
{
    if (!m_bInWorld || server_.GetClientUserData(client_) != player)
        return false;
    ++messages;
    if (std::holds_alternative<OpenBlood2::PlayerInitRequest>(event))
        return InitSinglePlayer(server);
    if (const auto* value = std::get_if<OpenBlood2::PlayerRotation>(&event))
        rotation = *value;
    else if (const auto* value = std::get_if<OpenBlood2::PlayerPosition>(&event))
    {
        if (!move_mgr.OnServerForcePos(*value))
            return false;
        position = value->position;
    }
    else if (const auto* value = std::get_if<OpenBlood2::PlayerPhysics>(&event))
    {
        if (!move_mgr.OnPhysicsUpdate(*value))
            return false;
        physics = *value;
    }
    else if (const auto* value = std::get_if<OpenBlood2::PlayerStats>(&event))
        stats = *value;
    return true;
}
bool CBloodClientShell::Update()
{
    if (!m_bInWorld || server_.GetClientUserData(client_) != player)
        return false;
    ++updates;
    server_.GetObjectPos(player, &position);
    // Caleb's source standing eye level. Original camera effects remain absent.
    camera_position = position;
    camera_position.y += 36;
    m_bFirstUpdate = false;
    return true;
}
bool CBloodClientShell::UpdateMovement(CBloodServerShell& server, double seconds, std::string& error)
{
    auto* owner = server.Player(client_);
    if (!owner || !move_mgr.Update(server, movement_input, rotation.yaw, seconds, !owner->m_damage.IsDead()))
    {
        error = "Original MoveMgr step failed (invalid association, movement message or collision)";
        return false;
    }
    return true;
}
bool CBloodClientShell::SyncWeapons(OpenBlood2::ModelStore& models, OpenBlood2::Resources& resources,
                                    CBloodServerShell& server, std::string& error)
{
    auto* owner = server.Player(client_);
    if (!owner)
        return false;
    std::array<CViewWeapon, 2> prepared;
    std::array<bool, 2> changed{};
    for (unsigned i = 0; i < 2; ++i)
    {
        const auto* weapon = owner->m_InventoryMgr.CurrentWeapon(i != 0);
        const auto type = weapon ? weapon->data.m_nType : 0;
        changed[i] = type != weapons[i].type;
        if (changed[i] && type && !prepared[i].Create(type, i != 0, models, resources, error))
            return false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (changed[i])
            weapons[i] = std::move(prepared[i]);
    return true;
}
bool CBloodClientShell::UpdateWeapons(CBloodServerShell& server, double seconds, std::string& error)
{
    const float cp = std::cos(rotation.pitch);
    const DVector forward{std::sin(rotation.yaw) * cp, -std::sin(rotation.pitch), std::cos(rotation.yaw) * cp};
    for (auto& weapon : weapons)
        if (!weapon.Update(server_, server, client_, weapon_input, camera_position, forward, seconds, error))
            return false;
    return true;
}
