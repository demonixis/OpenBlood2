// Selected branches of BloodServerShell.cpp and BloodClientShell.cpp. The
// original call ordering is retained; source-level adaptations replace raw
// client data / packet reads with checked handles and owned typed values.
#include "BloodShells.h"
#include "ClientCrosshair.h"
#include "MenuIds.h"
#include "engine/string_resources.h"
#include "game/game_settings.h"
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>

void CBloodServerShell::PreStartWorld()
{
    paths.ClearPathList();
    inspection_start = false;
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
        player->DropFlag(false);
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
    if (!player->GoToStartPoint(nullptr, inspection_start))
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
        player->DropFlag(false);
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
        player->DropFlag(false);
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
    music.EnterWorld(music_catalog_, audio_);
    voice_manager.Init(resources_);
    if (!restored)
        voice_manager.Reset();
    else
        voice_manager.StopAll();
    client_ = client;
    player = object;
    m_bInWorld = true;
    m_bRollCredits = false;
    if (const auto var = server_.GetGameConVar("GAME_WORLDNAME"))
        if (const auto* value = server_.GetVarValueString(var))
        {
            // Original accepts "END"/"CREDITS" with or without the Worlds prefix (case-insensitive).
            std::string world = value;
            std::transform(world.begin(), world.end(), world.begin(),
                           [](unsigned char c) { return c == '\\' ? '/' : char(std::tolower(c)); });
            if (world.starts_with("worlds/"))
                world.erase(0, 7);
            if (world.ends_with(".dat"))
                world.resize(world.size() - 4);
            m_bRollCredits = world == "end" || world == "credits";
        }
    m_Credits.Term();
    m_bFirstUpdate = true;
    restored_entry = restored;
    m_bFirstWorld = !restored;
    position = {};
    camera_position = {};
    view = {};
    listener_position = {};
    player_visible = false;
    weapons_visible = true;
    death_camera = {};
    death_notifications = 0;
    rotation = {};
    physics = {};
    stats = {};
    world_exit = {};
    ctf = {};
    boss_health = {};
    boss_health_messages = 0;
    boss_health_visible = false;
    communication.Reset(server_);
    medkit_down = medkit_was_down = false;
    flashlight_down = flashlight_was_down = false;
    flashlight = {};
    ClearInventoryEffects();
    binoculars_down = binoculars_was_down = alt_fire_was_down = false;
    // StartNewWorld/OnEnterWorld: zoom off, original crosshair and default FOV.
    m_bZoomView = m_bBinocularsActive = false;
    m_bShowCrosshair = m_bCrosshairOriginallyOn;
    m_nCrosshair = m_nCrosshairOriginalNum;
    m_fovX = OpenBlood2::original_fov_normal;
    zoom_messages = 0;
    time_bomb_down = time_bomb_was_down = false;
    proximity_down = proximity_was_down = false;
    remote_down = remote_was_down = detonate_down = detonate_was_down = false;
    item_actions = item_sounds = item_voice = 0;
    item_audio_error.clear();
    fade = {};
    objectives = {};
    objectives_error.clear();
    objective_messages = 0;
    objectives_resolved = false;
    messages = updates = 0;
    blocker = "M2 is closed for the selected SubwayA-to-CityHub session with reservation M2-GPU-001. "
              "Whole-world saves and unexercised gameplay paths are not validated.";
    return true;
}
void CBloodClientShell::OnExitWorld()
{
    music.ExitWorld(audio_);
    voice_manager.StopAll();
    weapons = {};
    weapon_input = {};
    move_mgr.Term();
    movement_input = {};
    use_down = use_was_down = false;
    player = nullptr;
    client_ = nullptr;
    m_bInWorld = false;
    m_bRollCredits = false;
    m_Credits.Term();
    restored_entry = false;
    m_bFirstWorld = true;
    m_bFirstUpdate = true;
    position = {};
    camera_position = {};
    view = {};
    listener_position = {};
    player_visible = false;
    weapons_visible = true;
    death_camera = {};
    death_notifications = 0;
    rotation = {};
    physics = {};
    stats = {};
    world_exit = {};
    ctf = {};
    boss_health = {};
    boss_health_messages = 0;
    boss_health_visible = false;
    communication.Reset(server_);
    medkit_down = medkit_was_down = false;
    flashlight_down = flashlight_was_down = false;
    flashlight = {};
    ClearInventoryEffects();
    binoculars_down = binoculars_was_down = alt_fire_was_down = false;
    // StartNewWorld/OnEnterWorld: zoom off, original crosshair and default FOV.
    m_bZoomView = m_bBinocularsActive = false;
    m_bShowCrosshair = m_bCrosshairOriginallyOn;
    m_nCrosshair = m_nCrosshairOriginalNum;
    m_fovX = OpenBlood2::original_fov_normal;
    zoom_messages = 0;
    time_bomb_down = time_bomb_was_down = false;
    proximity_down = proximity_was_down = false;
    remote_down = remote_was_down = detonate_down = detonate_was_down = false;
    item_actions = item_sounds = item_voice = 0;
    item_audio_error.clear();
    fade = {};
    objectives = {};
    objectives_error.clear();
    objective_messages = 0;
    objectives_resolved = false;
    blocker.clear();
}
bool CBloodClientShell::InitSinglePlayer(CBloodServerShell& server)
{
    if (!m_bFirstWorld)
        return restored_entry; // Restored worlds skip the profile; retain fresh-entry duplicate rejection.
    if (!server.OnMessage(client_, entry_profile))
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
    if (const auto* value = std::get_if<OpenBlood2::MusicCommand>(&event))
    {
        music.HandleMusicMessage(*value, music_catalog_, audio_);
        return true; // Delivery survives playback failure; inspect the actual music result.
    }
    if (std::holds_alternative<OpenBlood2::VoiceStop>(event))
    {
        voice_manager.StopAll();
        return true;
    }
    if (std::holds_alternative<OpenBlood2::PlayerDead>(event))
    {
        death_camera.Begin(server_.GetTime());
        ++death_notifications;
        weapons_visible = false;
        return true;
    }
    if (const auto* value = std::get_if<OpenBlood2::VoiceEvent>(&event))
    {
        if (value->group)
            voice_manager.PlayEventSound(player, value->character, value->event, view.object != nullptr);
        else
            voice_manager.PlayUniqueSound(player, value->character, value->event, view.object != nullptr);
        return true; // Delivery is distinct from gated or failed cosmetic playback.
    }
    if (const auto* value = std::get_if<OpenBlood2::ScreenFade>(&event))
    {
        fade.Begin(value->in);
        return true;
    }
    if (const auto* value = std::get_if<OpenBlood2::ZoomView>(&event))
    {
        ++zoom_messages;
        m_bZoomView = value->on;
        if (m_bZoomView)
        {
            m_bCrosshairOriginallyOn = m_bShowCrosshair;
            m_nCrosshairOriginalNum = m_nCrosshair;
            if (value->type)
            {
                m_bShowCrosshair = true;
                m_nCrosshair = value->type;
            }
        }
        else
        {
            m_bShowCrosshair = m_bCrosshairOriginallyOn;
            m_nCrosshair = m_nCrosshairOriginalNum;
        }
        return true;
    }
    if (std::holds_alternative<OpenBlood2::OrbView>(event) ||
        std::holds_alternative<OpenBlood2::SeeingEyeView>(event) ||
        std::holds_alternative<OpenBlood2::CreatureAttach>(event))
        return HandleInventoryView(event, server);
    if (const auto* value = std::get_if<OpenBlood2::InventoryAction>(&event))
    {
        if (value->type == 3)
        {
            night_goggles = value->active;
            ++item_actions;
            return UpdateInventoryEffects();
        }
        if (value->type != 1 && value->type != 2 && value->type != 5)
            return false;
        ++item_actions;
        if (value->type == 5)
            m_bBinocularsActive = value->active;
        OpenBlood2::SoundRequest request;
        request.resource = value->type == 1   ? "sounds/powerups/medikit.wav"
                           : value->type == 2 ? "sounds/powerups/flashlight.wav"
                                              : "sounds/powerups/binoculars.wav";
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
        if (resources_.demo() && OpenBlood2::same_name(value->next_world, "EndGame"))
        {
            world_exit.pending = false;
            world_exit.demo_end_pending = true;
            return true; // The application ends the session at its frame boundary.
        }
        if (world_exit.demo_end_pending)
            return true;
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
    else if (const auto* value = std::get_if<OpenBlood2::CTFEvent>(&event))
        return HandleCTF(*value);
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
        voice_manager.StopAll();
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
bool CBloodClientShell::InitCredits(bool clear_screen, std::string& error)
{
    // CreditsWin_GetTextBuffer: FindResource(cres_hinstance, "CREDITS", "TEXT"), read as data.
    std::vector<std::uint8_t> dll, text;
    error.clear();
    const auto* resource = resources_.find("cres.dll");
    if (!resource)
        error = "Credits: cres.dll is missing";
    else if (resource->size > 32 * 1024 * 1024)
        error = "Credits: cres.dll exceeds the 32 MiB resource reader limit";
    else if (resources_.read("cres.dll", dll, error))
        OpenBlood2::read_named_resource(dll, "TEXT", "CREDITS", text, error);
    std::string parse_error;
    // An unavailable resource still leaves an inited, empty roll, like the original.
    const bool parsed = m_Credits.Init(text, CM_CREDITS, clear_screen, 640, &parse_error);
    // SetStaticInfo: FormatString(IDS_CREDITSFONT), falling back to "Arial".
    OpenBlood2::StringResources strings;
    std::string strings_error;
    if (!dll.empty() && OpenBlood2::read_string_resources(dll, strings, strings_error))
        if (const auto found = strings.strings.find(IDS_CREDITSFONT);
            found != strings.strings.end() && !found->second.empty())
            m_Credits.font_name = found->second;
    if (error.empty())
        error = parse_error;
    else
        m_Credits.error = error;
    if (error.empty() && !parsed)
        error = "Credits: TEXT/CREDITS could not be parsed";
    if (!error.empty())
    {
        credits_error = error;
        ++credits_failures;
        return false;
    }
    return true;
}
void CBloodClientShell::UpdateRollCredits(double seconds)
{
    if (!m_bInWorld || !m_bRollCredits)
        return;
    const auto frame = static_cast<float>(seconds);
    if (m_Credits.IsInited())
        m_Credits.Update(frame);
    else
    {
        std::string error;
        InitCredits(false, error);
    }
    if (m_Credits.IsInited())
        m_Credits.Update(frame);
}
bool CBloodClientShell::Update()
{
    if (!m_bInWorld || server_.GetClientUserData(client_) != player)
        return false;
    ++updates;
    communication.Update(server_, player, voice_manager);
    server_.GetObjectPos(player, &position);
    const auto* owner = dynamic_cast<CPlayerObj*>(server_.HandleToObject(player));
    if (!owner)
        return false;
    camera_position = position;
    camera_position.y += owner->eye_level;
    if (!UpdateCameraSelection() || !UpdateInventoryEffects())
        return false;
    m_bFirstUpdate = false;
    return true;
}
bool CBloodClientShell::UpdateMovement(CBloodServerShell& server, double seconds, std::string& error)
{
    auto* owner = server.Player(client_);
    if (!owner || !server.OnMessage(client_, rotation) || !UpdateCameraSelection())
        return false;
    if ((owner->camera_movement_blocked || owner->MovementBlocked()) && !owner->m_damage.IsDead() &&
        !server.OnMessage(client_, OpenBlood2::PlayerControls{{}, false, rotation.yaw}))
        return false;
    if (!owner->ApplyDoorPush() ||
        !move_mgr.Update(server, voice_manager, view.object != nullptr, movement_input, rotation.yaw, seconds,
                         !owner->m_damage.IsDead() && !owner->camera_movement_blocked && !owner->MovementBlocked(),
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
        changed[i] = type != weapons[i].type || (type && weapons[i].character != owner->m_nCharacter);
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
        if (changed[i] && type && !prepared[i].Create(type, i != 0, models, resources, error, owner->m_nCharacter))
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
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerDetonate&)
{
    auto* owner = Player(client);
    return owner && owner->OnCommandDetonate();
}
bool CBloodServerShell::OnMessage(HCLIENT client, const OpenBlood2::PlayerAltFire&)
{
    auto* owner = Player(client);
    return owner && owner->OnCommandAltFire();
}
bool CBloodClientShell::UpdateUse(CBloodServerShell& server)
{
    const bool goggles_pressed = goggles_down && !goggles_was_down;
    goggles_was_down = goggles_down;
    if (goggles_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{3});
    const bool eye_pressed = eye_down && !eye_was_down;
    eye_was_down = eye_down;
    if (eye_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{4});
    const bool flashlight_pressed = flashlight_down && !flashlight_was_down;
    flashlight_was_down = flashlight_down;
    if (flashlight_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{2});
    const bool remote_pressed = remote_down && !remote_was_down;
    remote_was_down = remote_down;
    if (remote_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{8});
    const bool detonate_pressed = detonate_down && !detonate_was_down;
    detonate_was_down = detonate_down;
    if (detonate_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerDetonate{});
    const bool time_bomb_pressed = time_bomb_down && !time_bomb_was_down;
    time_bomb_was_down = time_bomb_down;
    if (time_bomb_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{9});
    const bool proximity_pressed = proximity_down && !proximity_was_down;
    proximity_was_down = proximity_down;
    if (proximity_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{7});
    const bool binoculars_pressed = binoculars_down && !binoculars_was_down;
    binoculars_was_down = binoculars_down;
    if (binoculars_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{5});
    const bool item_pressed = medkit_down && !medkit_was_down;
    medkit_was_down = medkit_down;
    if (item_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerItemCommand{1});
    const bool pressed = use_down && !use_was_down;
    use_was_down = use_down;
    if (pressed && creature)
    {
        ++total_use_hits;
        use_hits = server_.GetTime() - last_use_hit < .5f ? use_hits + 1 : 0;
        last_use_hit = server_.GetTime();
        if (use_hits >= 5 || total_use_hits >= 25)
        {
            server.OnMessage(client_, OpenBlood2::CreatureResponse{creature, false});
            creature = nullptr;
            creature_light = 1;
        }
    }
    if (pressed)
        server.OnMessage(client_, OpenBlood2::PlayerUse{rotation});
    return true; // A blocked USE command does not terminate a valid session.
}
bool CBloodClientShell::UpdateWeapons(CBloodServerShell& server, double seconds, std::string& error)
{
    const float cp = std::cos(rotation.pitch);
    const DVector forward{std::sin(rotation.yaw) * cp, -std::sin(rotation.pitch), std::cos(rotation.yaw) * cp};
    auto input = weapon_input;
    const bool alt_pressed = input.alternate && !alt_fire_was_down;
    alt_fire_was_down = input.alternate;
    if (alt_pressed)
        server.OnMessage(client_, OpenBlood2::PlayerAltFire{});
    if (const auto* owner = server.Player(client_);
        owner && (owner->camera_movement_blocked || (owner->m_InventoryMgr.eye && owner->m_InventoryMgr.eye->active)))
        input.primary = input.alternate = false;
    for (auto& weapon : weapons)
    {
        weapon.binoculars_active = m_bBinocularsActive;
        if (!weapon.Update(server_, server, client_, input, camera_position, forward, seconds, error))
            return false;
    }
    return true;
}

bool CBloodClientShell::UpdateZoom(double seconds)
{
    // Original UpdateCamera first-person branch: 8 rad/s toward 12.5 or default 90 degrees.
    // External cameras keep the stored zoom value.
    if (view.object || !std::isfinite(seconds) || seconds < 0)
        return true;
    if (!m_fovX)
        m_fovX = OpenBlood2::original_fov_normal;
    const float speed = 8 * static_cast<float>(std::min(seconds, 60.0));
    if (m_bZoomView && m_fovX > OpenBlood2::original_fov_zoomed)
        m_fovX = std::max(m_fovX - speed, OpenBlood2::original_fov_zoomed);
    else if (!m_bZoomView && m_fovX < OpenBlood2::original_fov_normal)
        m_fovX = std::min(m_fovX + speed, OpenBlood2::original_fov_normal);
    else if (!m_bZoomView && m_fovX > OpenBlood2::original_fov_normal)
        m_fovX = std::max(m_fovX - speed, OpenBlood2::original_fov_normal);
    return true;
}
float CBloodClientShell::ViewFovX() const
{
    // Original Orb camera overrides the stored zoom with default +45 degrees.
    if (view.object && view.object == orb_view)
        return OpenBlood2::original_fov_normal * 1.5f;
    // Letterbox cameras use the default FOV; other views use the current value.
    if ((view.object && view.settings.type == 1) || !m_fovX)
        return OpenBlood2::original_fov_normal;
    return m_fovX;
}
bool CBloodClientShell::UpdateCameraSelection()
{
    auto* owner = dynamic_cast<CPlayerObj*>(server_.HandleToObject(player));
    if (!owner || server_.GetClientUserData(client_) != player)
        return false;
    view = server_.GetCameraView();
    if (seeing_eye && server_.IsObjectAlive(seeing_eye) && !owner->m_damage.IsDead())
    {
        view.object = seeing_eye;
        view.settings = {0, false, false, true};
        server_.GetObjectPos(seeing_eye, &view.position);
        view.rotation = OpenBlood2::rotation_from_euler({rotation.pitch, rotation.yaw, 0});
        server_.SetObjectRotation(seeing_eye, &view.rotation);
    }
    else if (orb_view && server_.IsObjectAlive(orb_view) && !owner->m_damage.IsDead())
    {
        view.object = orb_view;
        view.settings = {0, false, false, true};
        if (server_.GetObjectPos(orb_view, &view.position) != DE_OK)
            return false;
        view.rotation = OpenBlood2::rotation_from_euler({rotation.pitch, rotation.yaw, 0});
    }
    owner->camera_movement_blocked = view.object && !view.settings.player_movement;
    player_visible = view.object && !view.settings.hide_player && !death_camera.active;
    weapons_visible = !view.object && !death_camera.active;
    if (!view.object)
    {
        view.position = camera_position;
        view.rotation.m_Vec = {}; // Build the same player pitch/yaw orientation.
        const float cp = std::cos(rotation.pitch);
        const DVector forward{std::sin(rotation.yaw) * cp, -std::sin(rotation.pitch), std::cos(rotation.yaw) * cp};
        if (server_.AlignRotation(view.rotation, forward) != DE_OK)
            return false;
        if (death_camera.active)
        {
            if (!death_camera.Update(server_, player, camera_position))
                return false;
            view.position = death_camera.position;
            view.rotation = death_camera.rotation;
        }
    }
    listener_position = view.object && view.settings.listener ? view.position : position;
    return true;
}

// Original SMSG_*THEFLAG / NEEDYOURFLAG local client branches.
bool CBloodClientShell::HandleCTF(const OpenBlood2::CTFEvent& event)
{
    using namespace OpenBlood2;
    if (event.team > 2 || static_cast<DBYTE>(event.kind) > static_cast<DBYTE>(CTFEventKind::Captured) ||
        event.points < 0 || event.points > 1000000)
        return false;
    ++ctf.messages;
    std::uint32_t notice = 0;
    if (event.kind == CTFEventKind::Team)
        ctf.team = event.team;
    else if (event.kind == CTFEventKind::Grabbed || event.kind == CTFEventKind::Returned)
    {
        if (event.kind == CTFEventKind::Grabbed)
            ctf.flag_icon = ctf.team == 1 ? 1 : ctf.team == 2 ? 2 : 0;
        SoundRequest sound;
        sound.resource = "sounds_multipatch\\CtfFlagGrab.wav";
        sound.priority = 1; // Original CBloodClientShell::PlaySound: MISC_MEDIUM.
        sound.space = SoundSpace::Local;
        sound.reverb_requested = false;
        ctf.voice = server_.PlayLocalSound(player, sound, ctf.error);
        ctf.sounds += ctf.voice != 0;
        notice = event.kind == CTFEventKind::Grabbed ? 7000 : 7001;
    }
    else if (event.kind == CTFEventKind::Dropped)
        ctf.flag_icon = 0;
    else if (event.kind == CTFEventKind::NeedFlag)
    {
        if (ctf.last_need_flag + 20 < server_.GetTime())
        {
            notice = 7009;
            ctf.last_need_flag = server_.GetTime();
        }
    }
    else if (event.kind == CTFEventKind::Captured)
    {
        ctf.flag_icon = 0;
        if (event.capturer == player)
        {
            if (ctf.score > INT_MAX - event.points)
                return false;
            ctf.score += event.points;
            notice = 7005;
        }
        // Remote player names/rosters are not represented by this local client.
    }
    if (notice)
    {
        ctf.notice_resource = notice;
        ++ctf.notices;
        if (!server_.GetClientResourceString(notice, ctf.notice))
            ctf.error = "Original CTF string resource unavailable";
    }
    return true;
}
