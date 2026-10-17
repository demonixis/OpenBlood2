// Selected branches of BloodServerShell.cpp and BloodClientShell.cpp. The
// original call ordering is retained; source-level adaptations replace raw
// client data / packet reads with checked handles and owned typed values.
#include "BloodShells.h"
#include "game/game_settings.h"
#include <cmath>

void CBloodServerShell::PreStartWorld()
{
    paths.ClearPathList();
    world_started = false;
    updates = 0;
    inventory_capture = {};
    damage_capture = {};
    player_capture = {};
    // Team setup, active cinematics, voice and multiplayer state are outside
    // the accepted fresh single-player entry mode.
}
bool CBloodServerShell::PostStartWorld()
{
    world_started = false;
    if (!paths.BuildPathList())
        return false;
    world_started = true;
    return true;
}
CPlayerObj* CBloodServerShell::Player(HCLIENT client)
{
    auto object = server_.GetClientUserData(client);
    return object ? dynamic_cast<CPlayerObj*>(server_.HandleToObject(object)) : nullptr;
}
bool CBloodServerShell::CapturePlayerInventory(HCLIENT client)
{
    auto& capture = inventory_capture;
    ++capture.attempts;
    capture.valid = false;
    capture.state = {};
    capture.error.clear();
    const auto* player = Player(client);
    // Original KeepAliveSave enumerates active player objects. This captures
    // only their currently supported inventory aggregate, not a whole player.
    if (!player || !server_.IsObjectAlive(player->m_hObject) ||
        server_.GetObjectState(player->m_hObject) != OBJSTATE_ACTIVE)
    {
        capture.error = "Inventory capture requires an active associated player";
        return false;
    }
    if (!player->m_InventoryMgr.Save(capture.state, capture.error))
        return false;
    capture.valid = true;
    ++capture.captures;
    return true;
}
bool CBloodServerShell::CapturePlayerDamage(HCLIENT client)
{
    auto& capture = damage_capture;
    ++capture.attempts;
    capture.valid = false;
    capture.state = {};
    capture.error.clear();
    const auto* player = Player(client);
    if (!player || !server_.IsObjectAlive(player->m_hObject) ||
        server_.GetObjectState(player->m_hObject) != OBJSTATE_ACTIVE)
    {
        capture.error = "Damage capture requires an active associated player";
        return false;
    }
    if (!player->m_damage.Save(capture.state, capture.error))
        return false;
    capture.valid = true;
    ++capture.captures;
    return true;
}
bool CBloodServerShell::CapturePlayerFields(HCLIENT client)
{
    auto& capture = player_capture;
    ++capture.attempts;
    capture.valid = false;
    capture.state = {};
    capture.error.clear();
    const auto* player = Player(client);
    if (!player || !server_.IsObjectAlive(player->m_hObject) ||
        server_.GetObjectState(player->m_hObject) != OBJSTATE_ACTIVE)
    {
        capture.error = "Player fields capture requires an active associated player";
        return false;
    }
    if (!player->SaveFields(capture.state, capture.error))
        return false;
    capture.valid = true;
    ++capture.captures;
    return true;
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
CPlayerObj* CBloodServerShell::OnRestoredClientEnterWorld(HCLIENT client, HOBJECT object, const char* start_name)
{
    // Typed local equivalent of OnClientEnterWorld's saved HCLIENTREF branch.
    // The caller supplies a fully staged owner; never fall back to CreatePlayer.
    if (!world_started || !client || Player(client) || !server_.IsObjectAlive(object))
        return nullptr;
    auto* player = dynamic_cast<CPlayerObj*>(server_.HandleToObject(object));
    if (!player || !player->restore_initialization || !player->restore_fields_loaded || !player->restore_model_loaded ||
        !player->restore_engine_loaded || player->GetClient() || player->m_InventoryMgr.m_hClient)
        return nullptr;
    if (server_.SetClientUserData(client, object) != DE_OK)
        return nullptr;
    player->SetClient(client);
    // RespawnPlayer chooses named entry for keep-alive, saved facing otherwise.
    // Neither branch calls CPlayerObj::Respawn or grants a fresh profile.
    HSTRING name = start_name && *start_name ? server_.CreateString(start_name) : nullptr;
    bool ok =
        (!start_name || !*start_name || name) && server_.SendToClient(client, OpenBlood2::PlayerInitRequest{}) == DE_OK;
    if (ok)
        ok = start_name
                 ? player->GoToStartPoint(name)
                 : server_.SendToClient(client, OpenBlood2::PlayerRotation{player->m_fPitch, player->m_fYaw}) == DE_OK;
    if (name)
        server_.FreeString(name);
    if (!ok || !server_.IsObjectAlive(object) || !player->InitializeClientState())
    {
        player->SetClient(nullptr);
        server_.SetClientUserData(client, nullptr);
        return nullptr;
    }
    server_.SetGameConVar("GAME_GOTOSTARTPOINT", "1");
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
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerControls& input)
{
    auto* player = Player(client);
    return player && player->UpdateAnim(input);
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
bool CBloodClientShell::OnEnterWorld(HCLIENT client, bool restored)
{
    auto object = server_.GetClientUserData(client);
    const auto* owner = dynamic_cast<const CPlayerObj*>(server_.HandleToObject(object));
    if (!object || m_bInWorld || !owner || restored != owner->restore_engine_loaded)
        return false;
    if (!move_mgr.Init(server_, client))
        return false;
    client_ = client;
    player = object;
    m_bInWorld = true;
    m_bFirstUpdate = true;
    restored_entry = restored;
    m_bFirstWorld = !restored;
    position = {};
    camera_position = {};
    view = {};
    listener_position = {};
    player_visible = false;
    weapons_visible = true;
    rotation = {};
    physics = {};
    stats = {};
    world_exit = {};
    boss_health = {};
    boss_health_messages = 0;
    boss_health_visible = false;
    communication.Reset(server_);
    medkit_down = medkit_was_down = false;
    item_actions = item_sounds = item_voice = 0;
    item_audio_error.clear();
    objectives = {};
    objectives_error.clear();
    objective_messages = 0;
    objectives_resolved = false;
    messages = updates = 0;
    blocker = "Locomotion/weapon inspection: remaining container/FX and mover classes, full combat, audio and "
              "autosave remain unported";
    return true;
}
void CBloodClientShell::OnExitWorld()
{
    weapons = {};
    weapon_input = {};
    move_mgr.Term();
    movement_input = {};
    use_down = use_was_down = false;
    player = nullptr;
    client_ = nullptr;
    m_bInWorld = false;
    restored_entry = false;
    m_bFirstWorld = true;
    m_bFirstUpdate = true;
    position = {};
    camera_position = {};
    view = {};
    listener_position = {};
    player_visible = false;
    weapons_visible = true;
    rotation = {};
    physics = {};
    stats = {};
    world_exit = {};
    boss_health = {};
    boss_health_messages = 0;
    boss_health_visible = false;
    communication.Reset(server_);
    medkit_down = medkit_was_down = false;
    item_actions = item_sounds = item_voice = 0;
    item_audio_error.clear();
    objectives = {};
    objectives_error.clear();
    objective_messages = 0;
    objectives_resolved = false;
    blocker.clear();
}
bool CBloodClientShell::InitSinglePlayer(CBloodServerShell& server)
{
    if (!m_bFirstWorld)
        return restored_entry;   // Restored worlds skip the profile; retain fresh-entry duplicate rejection.
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
    if (const auto* value = std::get_if<OpenBlood2::InventoryAction>(&event))
    {
        if (value->type != 1)
            return false;
        ++item_actions;
        OpenBlood2::SoundRequest request;
        request.resource = "sounds/powerups/medikit.wav";
        request.space = OpenBlood2::SoundSpace::Local;
        request.priority = 4; // SOUNDPRIORITY_PLAYER_LOW.
        request.reverb_requested = false;
        item_audio_error.clear();
        item_voice = server_.PlayLocalSound(player, request, item_audio_error);
        item_sounds += item_voice != 0;
        return true; // Failed presentation never reverses original healing.
    }
    if (const auto* value = std::get_if<OpenBlood2::WorldExit>(&event))
    {
        ++world_exit.received;
        // Original HandleExitWorld returns before reading its payload when dead.
        const auto* original_player = server.Player(client_);
        if (!original_player)
            return false;
        if (original_player->m_damage.IsDead())
        {
            ++world_exit.ignored_dead;
            return true;
        }
        world_exit.request = *value;
        world_exit.error.clear();
        // StartNewWorld's default difficulty is EASY (0); NEW_LEVEL does not
        // replace the server's difficulty/game settings. Audio/menu teardown and
        // loaded-state mutation wait for an accepted transfer in this port.
        world_exit.pending = OpenBlood2::original_world_request(value->next_world.c_str(), 0,
                                                                OpenBlood2::LOADTYPE_NEW_LEVEL, 0, world_exit.load);
        if (!world_exit.pending)
            world_exit.error = "Invalid data in CBloodClientShell::HandleExitWorld";
        return true; // A refused exit must not disconnect an intact player.
    }
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
    else if (const auto* value = std::get_if<OpenBlood2::BossHealth>(&event))
    {
        if (!std::isfinite(value->fraction))
            return false;
        boss_health = *value;
        ++boss_health_messages;
        // Original SMSG_BOSSHEALTH hides the bar at full health or zero.
        boss_health_visible = value->fraction > 0 && value->fraction < 1;
    }
    else if (const auto* value = std::get_if<OpenBlood2::Conversation>(&event))
    {
        communication.Start(server_, *value); // Original ignores COMM_BUSY.
    }
    else if (const auto* value = std::get_if<OpenBlood2::MissionObjectives>(&event))
    {
        // SMSG_OBJECTIVES: single/custom only; resource IDs override authored strings.
        const auto mode = server_.GetVarValueFloat(server_.GetGameConVar("GAME_GAMETYPE"));
        if (mode == static_cast<float>(GAMETYPE_SINGLE) || mode == static_cast<float>(GAMETYPE_CUSTOM))
        {
            objectives = *value;
            objectives_error.clear();
            ++objective_messages;
            objectives_resolved = true;
            if (value->resource)
            {
                const bool title = server_.GetClientResourceString(5000 + value->resource - 1, objectives.title);
                const bool text = server_.GetClientResourceString(6000 + value->resource - 1, objectives.text);
                objectives_resolved = title && text;
                if (!objectives_resolved)
                    objectives_error = "Objective resource strings are missing or invalid";
            }
        }
    }
    return true;
}
bool CBloodClientShell::Update()
{
    if (!m_bInWorld || server_.GetClientUserData(client_) != player)
        return false;
    ++updates;
    communication.Update(server_, player);
    server_.GetObjectPos(player, &position);
    const auto* owner = dynamic_cast<CPlayerObj*>(server_.HandleToObject(player));
    if (!owner)
        return false;
    camera_position = position;
    camera_position.y += owner->eye_level;
    if (!UpdateCameraSelection())
        return false;
    m_bFirstUpdate = false;
    return true;
}
bool CBloodClientShell::UpdateMovement(CBloodServerShell& server, double seconds, std::string& error)
{
    auto* owner = server.Player(client_);
    if (!owner || !UpdateCameraSelection())
        return false;
    if (owner->camera_movement_blocked && !owner->m_damage.IsDead() &&
        !server.OnMessage(client_, OpenBlood2::PlayerControls{{}, false, rotation.yaw}))
        return false;
    if (!owner->ApplyDoorPush() || !move_mgr.Update(server, movement_input, rotation.yaw, seconds,
                                                    !owner->m_damage.IsDead() && !owner->camera_movement_blocked,
                                                    weapon_input.primary || weapon_input.alternate, rotation.pitch))
    {
        error = "Original MoveMgr step failed (invalid association, movement message or collision)";
        if (owner && !owner->locomotion_error.empty())
            error += ": " + owner->locomotion_error;
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
        if (changed[i] && weapon)
        {
            std::shared_ptr<const OpenBlood2::Model> held;
            if (!models.load(weapon->data.m_szHandModelFilename, held, error))
                return false;
            if (!resources.find(weapon->data.m_szHandModelSkin))
            {
                error = "Held weapon skin is not indexed";
                return false;
            }
        }
        if (changed[i] && type && !prepared[i].Create(type, i != 0, models, resources, error))
            return false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (changed[i])
            weapons[i] = std::move(prepared[i]);
    return true;
}
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerUse& command)
{
    auto* owner = Player(client);
    if (!owner || !std::isfinite(command.rotation.pitch) || !std::isfinite(command.rotation.yaw) ||
        std::abs(command.rotation.pitch) > 1000000 || std::abs(command.rotation.yaw) > 1000000)
        return false;
    owner->m_fPitch = command.rotation.pitch;
    owner->m_fYaw = command.rotation.yaw;
    return owner->OnCommandUse();
}
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerItemCommand& command)
{
    auto* owner = Player(client);
    return owner && owner->OnCommandItem(command.type);
}
bool CBloodClientShell::UpdateUse(CBloodServerShell& server)
{
    const bool item_pressed = medkit_down && !medkit_was_down;
    medkit_was_down = medkit_down;
    if (item_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{1});
    const bool pressed = use_down && !use_was_down;
    use_was_down = use_down;
    if (pressed)
        server.OnMessage(client_, OpenBlood2::PlayerUse{rotation});
    return true; // A blocked USE command does not terminate a valid session.
}
bool CBloodClientShell::UpdateWeapons(CBloodServerShell& server, double seconds, std::string& error)
{
    const float cp = std::cos(rotation.pitch);
    const DVector forward{std::sin(rotation.yaw) * cp, -std::sin(rotation.pitch), std::cos(rotation.yaw) * cp};
    auto input = weapon_input;
    if (const auto* owner = server.Player(client_); owner && owner->camera_movement_blocked)
        input.primary = input.alternate = false;
    for (auto& weapon : weapons)
        if (!weapon.Update(server_, server, client_, input, camera_position, forward, seconds, error))
            return false;
    return true;
}

bool CBloodClientShell::UpdateCameraSelection()
{
    auto* owner = dynamic_cast<CPlayerObj*>(server_.HandleToObject(player));
    if (!owner || server_.GetClientUserData(client_) != player)
        return false;
    view = server_.GetCameraView();
    owner->camera_movement_blocked = view.object && !view.settings.player_movement;
    player_visible = view.object && !view.settings.hide_player;
    weapons_visible = !view.object;
    if (!view.object)
    {
        view.position = camera_position;
        view.rotation.m_Vec = {}; // Build the same player pitch/yaw orientation.
        const float cp = std::cos(rotation.pitch);
        const DVector forward{std::sin(rotation.yaw) * cp, -std::sin(rotation.pitch), std::cos(rotation.yaw) * cp};
        if (server_.AlignRotation(view.rotation, forward) != DE_OK)
            return false;
    }
    listener_position = view.object && view.settings.listener ? view.position : position;
    return true;
}
