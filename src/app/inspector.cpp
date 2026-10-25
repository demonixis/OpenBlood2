#include "app/inspector.h"
#include "app/scene_presenter.h"
#include "build_info.h"
#include "engine/audio_stream.h"
#include "engine/container.h"
#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/CampaignWeapons.h"
#include "game/original/ClientLightFX.h"
#include "game/original/DestructableModel.h"
#include "game/original/Door.h"
#include "game/original/FlagObjects.h"
#include "game/original/FlareProjectile.h"
#include "game/original/Grenade.h"
#include "game/original/Key.h"
#include "game/original/KeyPickup.h"
#include "game/original/LeechProjectile.h"
#include "game/original/Minefield.h"
#include "game/original/NewStatusBar.h"
#include "game/original/OrbProjectile.h"
#include "game/original/Projectile.h"
#include "game/original/ProximityBomb.h"
#include "game/original/RemoteBomb.h"
#include "game/original/RotatingBrush.h"
#include "game/original/RotatingDoor.h"
#include "game/original/RotatingModel.h"
#include "game/original/SoundFX.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/Trigger.h"
#include "game/original/VolumeBrush.h"
#include "game/original/WorldModelDebris.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <set>

namespace OpenBlood2
{

    void Inspector::draw_levels(GameSession& game)
    {
        ImGui::TextWrapped("Every world in the mounted archives. Play starts a new game there with Caleb, like the "
                           "menu's New Game; Inspect loads it without a player (leave the player first).");
        constexpr const char* difficulties[]{"Easy", "Medium", "Hard"};
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("Difficulty", &start_level_difficulty, difficulties, 3);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("Filter", level_filter, sizeof(level_filter));
        ImGui::Text("Current: %s", game.world_path.empty() ? "none" : game.world_path.c_str());
        if (!world_error.empty())
            ImGui::TextWrapped("Last error: %s", world_error.c_str());
        std::string filter(level_filter);
        std::transform(filter.begin(), filter.end(), filter.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (!ImGui::BeginTable("Levels", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 0)))
            return;
        ImGui::TableSetupColumn("World");
        ImGui::TableSetupColumn("Play", ImGuiTableColumnFlags_WidthFixed, 45);
        ImGui::TableSetupColumn("Inspect", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();
        for (const auto& [path, resource] : game.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            const auto name = path.substr(7, path.size() - 11);
            if (!filter.empty() && name.find(filter) == std::string::npos)
                continue;
            ImGui::PushID(path.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool current = game.world_path == path;
            if (current)
                ImGui::TextColored(ImVec4(.4f, 1, .4f, 1), "%s", name.c_str());
            else
                ImGui::TextUnformatted(name.c_str());
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Play"))
                start_level_request = name;
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(game.local_client);
            if (ImGui::SmallButton("Inspect"))
            {
                SAFE_STRCPY(keep_world_name, name.c_str());
                keep_world_request = true;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    void Inspector::forget_world()
    {
        skeleton_selection = trigger_selection = trigger_request = scene_explosion_selection =
            scene_explosion_request = {};
        sprite_selection = random_sound_selection = random_sound_request = cinematic_selection = cinematic_request = {};
        light_selection = light_request = keyframe_selection = keyframe_request = camera_selection =
            camera_request = {};
        objective_selection = pickup_selection = brush_selection = brush_request = door_selection =
            door_trigger_request = {};
        container_selection = container_toggle_request = remove_request = {};
        world_sound_selection = 0;
        source_selection = model_selection = 0;
        player_position_request.reset();
        movement_input_request.reset();
        weapon_input_request.reset();
        weapon_aim_request.reset();
        trigger_command.clear();
        random_sound_command.clear();
        light_command.clear();
        keyframe_command.clear();
        camera_command.clear();
        brush_command.clear();
        spawn_request.clear();
        world_error.clear();
        weapon_slot_request = -1;
        detonate_request = false;
        // Commands were drained before the owner boundary; do not carry an identity
        // selection into the new object's independent slot/generation namespace.
    }
    void Inspector::apply_commands(GameSession& game, ObjectSelection& selection)
    {
        if (scene_explosion_request)
        {
            if (const auto* r = game.objects.get(scene_explosion_request);
                r && r->life == Life::Alive && r->type->name == "CClientExplosionSFX")
            {
                auto s = game.server.CreateString("TRIGGER");
                auto m = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                if (m)
                {
                    game.server.WriteToMessageHString(m, s);
                    game.server.EndMessage(m);
                }
                game.server.FreeString(s);
            }
            scene_explosion_request = {};
        }
        if (random_sound_request)
        {
            if (const auto* r = game.objects.get(random_sound_request);
                r && r->life == Life::Alive && r->type->name == "RandomSoundFX")
            {
                auto text = game.server.CreateString(random_sound_command.c_str());
                auto message = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            random_sound_request = {};
        }
        if (cinematic_request)
        {
            if (const auto* r = game.objects.get(cinematic_request);
                r && r->life == Life::Alive && r->type->name == "CinematicTrigger")
            {
                auto text = game.server.CreateString("TRIGGER");
                auto message = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            cinematic_request = {};
        }
        if (keyframe_request)
        {
            if (const auto* r = game.objects.get(keyframe_request);
                r && r->life == Life::Alive && r->type->name == "KeyFramer")
            {
                auto text = game.server.CreateString(keyframe_command.c_str());
                auto message = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            keyframe_request = {};
        }
        if (light_request)
        {
            if (const auto* r = game.objects.get(light_request);
                r && r->life == Life::Alive && dynamic_cast<const ClientLightFX*>(r->game.get()))
            {
                auto text = game.server.CreateString(light_command.c_str());
                auto message = game.server.StartMessageToObject(r->game.get(), r->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            light_request = {};
        }
        if (camera_request)
        {
            if (const auto* r = game.objects.get(camera_request);
                r && r->life == Life::Alive && r->type->name == "CameraObj")
            {
                auto text = game.server.CreateString(camera_command.c_str());
                auto message = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            camera_request = {};
        }
        if (trigger_request)
        {
            auto* r = game.objects.get(trigger_request);
            auto* t = r && r->life == Life::Alive ? dynamic_cast<Trigger*>(r->game.get()) : nullptr;
            auto* player = game.server_shell.Player(game.local_client);
            if (t && grant_key_request && player && !t->key_name.empty())
                player->m_InventoryMgr.AddKey({t->key_name, t->key_name, "key.pcx", "key_h.pcx", 1});
            else if (t)
            {
                auto text = game.server.CreateString(trigger_command.c_str());
                auto msg = game.server.StartMessageToObject(player, r->handle, 1000);
                if (msg)
                {
                    game.server.WriteToMessageHString(msg, text);
                    game.server.EndMessage(msg);
                }
                game.server.FreeString(text);
            }
            trigger_request = {};
            grant_key_request = false;
        }
        if (brush_request)
        {
            if (const auto* record = game.objects.get(brush_request);
                record && record->life == Life::Alive && dynamic_cast<CDestructableBrush*>(record->game.get()))
            {
                auto text = game.server.CreateString(brush_command.c_str());
                auto message = game.server.StartMessageToObject(record->game.get(), record->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                if (text)
                    game.server.FreeString(text);
            }
            brush_request = {};
        }
        if (door_trigger_request)
        {
            if (const auto* record = game.objects.get(door_trigger_request);
                record && record->world_model && record->life == Life::Alive)
            {
                auto text = game.server.CreateString(door_close_request ? "TRIGGERCLOSE" : "TRIGGER");
                auto message = game.server.StartMessageToObject(nullptr, record->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            door_trigger_request = {};
        }
        if (container_toggle_request)
        {
            if (auto* record = game.objects.get(container_toggle_request);
                record && record->container() && record->life == Life::Alive)
            {
                auto text = game.server.CreateString("TOGGLE");
                auto message = game.server.StartMessageToObject(nullptr, record->handle, 1000);
                if (message)
                {
                    game.server.WriteToMessageHString(message, text);
                    game.server.EndMessage(message);
                }
                game.server.FreeString(text);
            }
            container_toggle_request = {};
        }
        if (player_position_request)
        {
            world_error.clear();
            auto* player = game.server_shell.Player(game.local_client);
            if (player)
            {
                DVector destination = *player_position_request, dimensions;
                game.server.GetObjectDims(player->m_hObject, &dimensions);
                CollisionHit hit;
                const auto query = game.collision.sweep(destination, destination, dimensions, hit);
                bool clear = query == CollisionQuery::Miss;
                for (auto id : game.objects.snapshot())
                {
                    const auto* other = game.objects.get(id);
                    if (!other || other->life != Life::Alive || other->handle == player->m_hObject ||
                        !(other->state.m_Flags & FLAG_SOLID))
                        continue;
                    if (sweep_object(*other, destination, destination, dimensions, hit) != CollisionQuery::Miss)
                        clear = false;
                }
                if (clear && game.server.TeleportObject(player->m_hObject, &destination) == DE_OK)
                {
                    player->TeleportClientToServerPos();
                    game.pump_player_messages(world_error);
                    player->UpdateContainerState();
                }
                else
                    world_error = "Inspection teleport rejected: invalid position or occupied player body";
            }
            player_position_request.reset();
        }
        if (unload_request)
        {
            if (!game.world_path.empty())
                reload_world_name = game.world_path;
            game.unload_world();
            selection.id = {};
            unload_request = false;
        }
        if (reload_request)
        {
            game.request_world(game.world_path.empty() ? reload_world_name : game.world_path, world_error);
            selection.refresh(game.objects);
            source_selection = model_selection = 0;
            reload_request = false;
        }
        if (run_request)
        {
            game.run_world(world_error);
            run_request = false;
        }
        if (leave_player_request)
        {
            game.leave_player();
            selection.refresh(game.objects);
            leave_player_request = false;
        }
        if (enter_player_request)
        {
            if (game.enter_player(world_error))
                selection.id = game.objects.get(game.client_shell.player)->id;
            enter_player_request = false;
        }
        if (capture_player_request)
        {
            PlayerRestoreState saved;
            if (game.capture_player(saved, game.restoration.error))
            {
                game.restoration.saved = std::move(saved);
                ++game.restoration.captures;
            }
            capture_player_request = false;
        }
        if (inspect_destination_request)
        {
            WorldMessage message;
            if (original_world_request(keep_world_name, 0, LOADTYPE_NEW_LEVEL, 0, message))
                game.inspect_destination(message, keep_start_name, world_error);
            else
                world_error = "Invalid destination request";
            inspect_destination_request = false;
        }
        if (trial_destination_request)
        {
            WorldMessage message;
            if (original_world_request(keep_world_name, 0, LOADTYPE_NEW_LEVEL, 0, message))
                game.rehearse_destination(message, keep_start_name, world_error);
            else
                world_error = "Invalid destination request";
            trial_destination_request = false;
        }
        if (level_change_request)
        {
            WorldMessage message;
            if (original_world_request(keep_world_name, 0, LOADTYPE_NEW_LEVEL, 0, message))
                game.queue_level_change(message, keep_start_name, world_error);
            else
                world_error = "Invalid destination request";
            level_change_request = false;
        }
        if (keep_world_request)
        {
            if (!game.local_client)
                game.request_world(keep_world_name, world_error);
            keep_world_request = false;
        }
        if (restore_player_request || keep_player_request)
        {
            if (game.restoration.saved && game.restore_player(*game.restoration.saved, world_error,
                                                              keep_player_request ? PlayerRestoreMode::KeepAlive
                                                                                  : PlayerRestoreMode::SavedPosition,
                                                              keep_player_request ? keep_start_name : ""))
                selection.id = game.restoration.last_player;
            restore_player_request = keep_player_request = false;
        }
        if (respawn_request)
        {
            if (auto* player = game.server_shell.Player(game.local_client))
            {
                if (!player->Respawn())
                    world_error = player->entry_error;
            }
            respawn_request = false;
        }
        if (detonate_request)
        {
            game.server_shell.OnMessage(game.local_client, PlayerDetonate{});
            detonate_request = false;
        }
        if (weapon_slot_request >= 0)
        {
            game.select_weapon(static_cast<DBYTE>(weapon_slot_request), world_error);
            weapon_slot_request = -1;
        }
        if (auto* player = game.server_shell.Player(game.local_client))
        {
            if (add_beretta_request)
                player->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA);
            if (add_flare_request)
                player->m_InventoryMgr.ObtainWeapon(WEAP_FLAREGUN);
            if (add_flares_request)
                player->m_InventoryMgr.AddAmmo(AMMO_FLARE, 20);
            if (add_assault_request)
                player->m_InventoryMgr.ObtainWeapon(WEAP_ASSAULTRIFLE);
            if (add_charges_request)
                player->m_InventoryMgr.AddAmmo(AMMO_DIEBUGDIE, 20);
            if (add_mac10_request)
                player->m_InventoryMgr.ObtainWeapon(WEAP_SUBMACHINEGUN);
            if (add_ammo_request)
                player->m_InventoryMgr.AddAmmo(AMMO_BULLET, 50);
            if (weapon_input_request)
                game.client_shell.weapon_input = *weapon_input_request;
            if (weapon_aim_request)
                game.client_shell.rotation = *weapon_aim_request;
            if (movement_input_request)
                game.client_shell.movement_input = *movement_input_request;
        }
        add_beretta_request = add_mac10_request = add_flare_request = add_flares_request = add_ammo_request =
            add_assault_request = add_charges_request = false;
        weapon_input_request.reset();
        weapon_aim_request.reset();
        movement_input_request.reset();
        if (remove_request)
        {
            if (const auto* object = game.objects.get(remove_request))
                game.objects.remove(object->handle);
            remove_request = {};
        }
        if (!spawn_request.empty())
        {
            if (const auto* object = game.objects.get(game.create_example(spawn_request)))
                selection.id = object->id;
            spawn_request.clear();
        }
    }

    void Inspector::draw(const Config& config, const DataStatus& data, bool& visible, std::uint64_t frames,
                         GameSession& game, ObjectSelection& selection)
    {
        weapon_input_request = WeaponInput{}; // Releasing/hiding/changing tabs stops held input.
        movement_input_request = MovementInput{};
        if (!visible)
            return;
        const auto display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(display.x - 32, display.y - 32), ImGuiCond_FirstUseEver);
        const ImVec2 maximum(std::max(1.0f, display.x - 32), std::max(1.0f, display.y - 32));
        ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(480.0f, maximum.x), std::min(300.0f, maximum.y)), maximum);
        const bool expanded = ImGui::Begin("Blood II | In-game inspector", &visible);
        collapsed = ImGui::IsWindowCollapsed();
        if (expanded)
        {
            ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.43f, 1),
                               game.world_path.empty() ? "ORIGINAL OBJECT DIAGNOSTICS" : "M2  /  WORLD INSPECTION");
            ImGui::TextWrapped("Bounded original-code session, models and world collision. M2 acceptance remains "
                               "incomplete; inspect observed service coverage in Services.");
            ImGui::PushTextWrapPos(0);
            ImGui::TextDisabled("Title arrow: fold/unfold | Drag borders: resize | F1: inspector | F3: mouse capture | "
                                "Esc: release/quit");
            ImGui::PopTextWrapPos();
            ImGui::Checkbox("Game controls (F2)", &keyboard_movement);
            ImGui::TextWrapped("WASD / ZQSD: walk | Shift: run | Space: jump | Ctrl: crouch | Arrows: look | E: use | "
                               "H: medkit; L: flashlight (release Shift)");
            ImGui::TextWrapped(
                "F3: capture mouse and hide inspector | Mouse: look | Left/F: fire | Right/G: alternate fire");
            ImGui::Text("Mouse: %s", mouse_captured ? "captured" : "released for UI");
            if (!mouse_error.empty())
                ImGui::TextWrapped("Mouse capture failed: %s", mouse_error.c_str());
            if (waiting_for_player)
            {
                ImGui::TextWrapped(
                    "Simulation waiting for player entry. NPCs and world clocks are held at their current state.");
                if (ImGui::Button("Enter player and play"))
                    enter_player_request = true;
                if (!world_error.empty())
                    ImGui::TextWrapped("Player entry: %s", world_error.c_str());
            }
            else if (!game.client_shell.m_bInWorld)
                ImGui::TextDisabled("Enter the player in Session to use game controls.");
            else
            {
                if (ImGui::Button("Play / resume (mouse + keyboard)"))
                    play_request = true;
                if (game.client_shell.view.object && !game.client_shell.view.settings.player_movement)
                    ImGui::TextWrapped("The original cinematic currently blocks movement and firing. Controls return "
                                       "when it ends.");
            }
            ImGui::Separator();
            if (ImGui::BeginTabBar("Inspection"))
            {
                if (ImGui::BeginTabItem("Levels"))
                {
                    draw_levels(game);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("3D view"))
                {
                    ImGui::TextWrapped(
                        "Diagnostic textured geometry with depth. Fold the title bar to see the world. F2 toggles "
                        "WASD/ZQSD, arrows to look, Shift run, Space jump, Ctrl crouch, E USE. Enter "
                        "the player in Session first, or launch --view-world.");
                    ImGui::TextWrapped(
                        "World textures/models, glass, PolyGrids and alpha sprites are unlit. Lightmaps "
                        "remain unavailable. Bound sky world models render in index order. World alpha uses sorted "
                        "faces; FX sort separately.");
                    ImGui::Text("Interactive clock: %llu limited frames / %.3f discarded wall seconds",
                                (unsigned long long)limited_frames, discarded_wall_time);
                    if (world_mesh && world_render)
                    {
                        if (ImGui::TreeNode("Original sky objects and projection"))
                        {
                            const auto& sky = game.server.sky;
                            const auto& d = sky.definition;
                            ImGui::Text("Definitions: %llu / additions: %llu / submitted: %zu batches, %llu passes",
                                        (unsigned long long)sky.definitions, (unsigned long long)sky.additions,
                                        world_render->sky_batches, (unsigned long long)world_render->sky_frames);
                            ImGui::Text("Bounds: (%.3f %.3f %.3f) to (%.3f %.3f %.3f)", d.min.x, d.min.y, d.min.z,
                                        d.max.x, d.max.y, d.max.z);
                            ImGui::Text("View box: (%.3f %.3f %.3f) to (%.3f %.3f %.3f)", d.view_min.x, d.view_min.y,
                                        d.view_min.z, d.view_max.x, d.view_max.y, d.view_max.z);
                            DVector eye;
                            if (OpenBlood2::sky_view_position(d, game.world.min, game.world.max,
                                                              game.client_shell.view.position, eye))
                                ImGui::Text("Sky eye: %.4f %.4f %.4f", eye.x, eye.y, eye.z);
                            for (unsigned i = 0; i < sky.objects.size(); ++i)
                                if (sky.objects[i])
                                {
                                    const auto* r = game.objects.get(sky.objects[i]);
                                    ImGui::Text("%u: %s / %s", i, r ? r->state.m_Name : "removed",
                                                r && r->life == Life::Alive
                                                    ? ((r->state.m_Flags & FLAG_VISIBLE) ? "visible" : "hidden")
                                                    : "stale or pending");
                                }
                            if (!sky.error.empty())
                                ImGui::TextWrapped("Last error: %s", sky.error.c_str());
                            ImGui::TextWrapped("World-model skies only; no fog/lightmaps or retail pixel parity. "
                                               "Empty/failed skies retain portal markers.");
                            ImGui::TreePop();
                        }
                        ImGui::Text("GPU: %llu frames / %llu world uploads", (unsigned long long)world_render->frames,
                                    (unsigned long long)world_render->uploads);
                        if (presentation)
                        {
                            ImGui::Text("Last frame: %zu scene pass / %zu draw calls / %zu culled draw records",
                                        presentation->passes, presentation->draw_calls, presentation->culled);
                            ImGui::Text("CPU: update %.3f world + %.3f models + %.3f effects ms | record %.3f ms",
                                        presentation->world_ns / 1e6, presentation->models_ns / 1e6,
                                        presentation->effects_ns / 1e6, presentation->record_ns / 1e6);
                        }
                        if (post)
                        {
                            static constexpr const char* tonemaps[]{"off", "neutral", "filmic"};
                            static constexpr const char* occlusion[]{"off", "low", "high"};
                            ImGui::Text("Post-processing: %s scene, MSAA %ux (%ux requested), FXAA %s, bloom %s (%zu "
                                        "levels), tonemap %s, ambient occlusion %s, reflections %s | %zu passes, %zu "
                                        "targets",
                                        post->hdr ? "HDR" : "direct", post->samples, post->requested_samples,
                                        post->fxaa ? "on" : "off", post->bloom ? "on" : "off", post->bloom_levels,
                                        tonemaps[std::size_t(post->tonemap)],
                                        occlusion[std::size_t(post->ambient_occlusion)],
                                        occlusion[std::size_t(post->reflections)], post->passes, post->targets);
                        }
                        if (gpu_resources)
                            ImGui::Text("GPU registry: %zu textures (%zu bytes, %llu shared hits) / %zu buffers (%zu "
                                        "bytes) / %zu shared assets | %zu bytes staged last frame",
                                        gpu_resources->textures, gpu_resources->texture_bytes,
                                        (unsigned long long)gpu_resources->texture_hits, gpu_resources->buffers,
                                        gpu_resources->buffer_bytes, gpu_resources->shared,
                                        gpu_resources->staged_bytes);
                        ImGui::Text("Prepared: %zu triangles / %zu instances / %zu hidden instances",
                                    world_render->triangles, world_render->draws, world_render->hidden);
                        ImGui::Text("Unbound models: %zu / excluded containers: %zu", world_render->unbound,
                                    world_render->containers);
                        ImGui::Text(
                            "Faces: %zu invisible / %zu degenerate / %zu authored transparent / %zu authored sky",
                            world_mesh->invisible, world_mesh->degenerate, world_mesh->transparent, world_mesh->sky);
                        ImGui::Text("Baked light: %zu lightmapped / %zu per-vertex lit / %zu fully bright textured "
                                    "faces",
                                    world_mesh->lightmapped, world_mesh->lit, world_mesh->fullbright);
                        ImGui::Text("Lightmaps: %zu pages (%u px), %zu texels, %zu unfitted origin axes; x%.0f "
                                    "(Saturate)",
                                    world_mesh->lightmap_pages, WorldMesh::lightmap_page_size,
                                    world_mesh->lightmap_texels, world_mesh->lightmap_unfitted,
                                    double(world_saturate_scale));
                        if (model_lighting)
                        {
                            const auto& l = *model_lighting;
                            ImGui::Text("Model light: %zu models (%zu sampled, %zu reused, %zu flat, %zu "
                                        "FLAG_NOLIGHT), %zu directional sources, %zu occluded lights",
                                        l.models, l.sampled, l.reused, l.flat, l.unlit, l.sources, l.occluded);
                            ImGui::Text("Static lights: %zu (%zu without LightObjects, %zu invalid, %zu OuterColor) | "
                                        "%llu occlusion traces, %llu blocked",
                                        l.lights.lights, l.lights.without_objects, l.lights.invalid, l.lights.outer,
                                        (unsigned long long)l.shade.traces, (unsigned long long)l.shade.blocked);
                            ImGui::Text("AmbientLight %.0f %.0f %.0f | ModelAdd %.0f %.0f %.0f | ModelDirAdd %.0f %.0f "
                                        "%.0f (read, not applied)",
                                        l.world_ambient.x, l.world_ambient.y, l.world_ambient.z, l.model_add.x,
                                        l.model_add.y, l.model_add.z, l.model_dir_add.x, l.model_dir_add.y,
                                        l.model_dir_add.z);
                            ImGui::TextWrapped("Original light term per static light; Lambert N.L, source limit and "
                                               "occlusion are replacement policy, not retail parity.");
                        }
                        if (model_shadows)
                        {
                            const auto& m = *model_shadows;
                            ImGui::Text("Model shadows: %zu FLAG_SHADOW candidates, %zu drawn (%zu beyond limit, %zu "
                                        "distant, %zu outside view, %zu unbounded)%s",
                                        m.candidates, m.casters, m.beyond_limit, m.distant, m.outside, m.unbounded,
                                        m.available ? "" : " | atlas unavailable or not yet created");
                        }
                        ImGui::Text("Alpha: %zu prepared faces / %zu submitted batches / %llu passes",
                                    world_mesh->blended, world_render->alpha_batches,
                                    (unsigned long long)world_render->alpha_frames);
                        if (ImGui::TreeNode("Transparent world faces (including hidden/unbound models)"))
                        {
                            for (const auto& range : world_mesh->ranges)
                                for (const auto& batch : range.batches)
                                    if (batch.translucent)
                                        ImGui::Text("%s: %s, %u triangles", range.name.c_str(),
                                                    world_mesh->materials[batch.material].path.c_str(),
                                                    batch.count / 3);
                            ImGui::TreePop();
                        }
                        ImGui::Text("Surface effects: %zu authored / %zu opaque + %zu alpha Pan batches submitted",
                                    world_mesh->effects.size(), world_render->pan_batches,
                                    world_render->alpha_pan_batches);
                        ImGui::Text("Surface clock: %.4f seconds (session time)", world_render->surface_time);
                        if (ImGui::TreeNode("Surface effects (including invisible/hidden/unbound surfaces)"))
                        {
                            for (const auto& effect : world_mesh->effects)
                            {
                                ImGui::TextWrapped("%s / surface %u: [%s] [%s]",
                                                   world_mesh->ranges[effect.model].name.c_str(), effect.surface,
                                                   effect.name.c_str(), effect.parameters.c_str());
                                if (!effect.error.empty())
                                    ImGui::TextWrapped("Unavailable: %s; authored mapping remains static",
                                                       effect.error.c_str());
                                else
                                    ImGui::Text(
                                        "P/Q speeds %.3f / %.3f; origin velocity %.3f %.3f %.3f; UV texels/s %.3f %.3f",
                                        effect.speed[0], effect.speed[1], effect.origin_velocity.x,
                                        effect.origin_velocity.y, effect.origin_velocity.z, effect.texel_rate[0],
                                        effect.texel_rate[1]);
                            }
                            ImGui::TextUnformatted(
                                "Prepared animated batches (preparation does not imply visibility):");
                            for (const auto& range : world_mesh->ranges)
                                for (const auto& batch : range.batches)
                                    if (batch.uv_rate[0] || batch.uv_rate[1])
                                    {
                                        std::array<float, 4> offset{};
                                        if (world_texture_offset(batch.uv_rate, game.server.world_time(), offset))
                                            ImGui::Text("%s: %s; cycles/s %.4f %.4f; UV offset %.4f %.4f",
                                                        range.name.c_str(),
                                                        world_mesh->materials[batch.material].path.c_str(),
                                                        batch.uv_rate[0], batch.uv_rate[1], offset[0], offset[1]);
                                    }
                            ImGui::TreePop();
                        }
                        draw_materials();
                        std::size_t decoded = 0, failed = 0;
                        for (std::size_t i = 1; i < world_mesh->materials.size(); ++i)
                            world_mesh->materials[i].error.empty() ? ++decoded : ++failed;
                        ImGui::Text("Materials: %zu decoded / %zu rejected / %zu RGBA bytes / %zu batches", decoded,
                                    failed, world_mesh->texture_bytes, world_render->batches);
                        if (ImGui::TreeNode("Texture resources (mip zero, nearest repeat)"))
                        {
                            for (std::size_t i = 1; i < world_mesh->materials.size(); ++i)
                            {
                                const auto& material = world_mesh->materials[i];
                                if (!material.frames.empty() && material.error.empty())
                                {
                                    std::uint32_t image = 0;
                                    const bool valid = world_material_frame(*world_mesh, static_cast<std::uint32_t>(i),
                                                                            game.server.world_time(), image);
                                    ImGui::Text("%s: %zu frames / %u fps / %ux%u", material.path.c_str(),
                                                material.frames.size(), material.fps, material.image.width,
                                                material.image.height);
                                    ImGui::Text("Current frame image: %s",
                                                valid ? world_mesh->materials[image].path.c_str() : "unavailable");
                                }
                                else if (material.error.empty())
                                    ImGui::Text("%s: %ux%u, %u stored mips, flags 0x%x", material.path.c_str(),
                                                material.image.width, material.image.height, material.image.mipmaps,
                                                material.image.flags);
                                else
                                    ImGui::Text("%s: %s", material.path.c_str(), material.error.c_str());
                            }
                            ImGui::TreePop();
                        }
                        ImGui::Text("Camera: %s at %.2f %.2f %.2f",
                                    !game.client_shell.m_bInWorld   ? "no entered player"
                                    : game.client_shell.view.object ? "original scripted camera"
                                                                    : "player eye",
                                    game.client_shell.view.position.x, game.client_shell.view.position.y,
                                    game.client_shell.view.position.z);
                    }
                    if (model_view)
                    {
                        ImGui::Text("Model snapshot: %zu world / %zu close draws / %zu skin bytes",
                                    model_view->world_draws.size(), model_view->close_draws.size(),
                                    model_view->mesh.texture_bytes);
                        if (model_view->gpu_skinning)
                            ImGui::Text("GPU skinning: %zu instances / %zu posed / %zu palette and %zu deformation "
                                        "vec4 per frame",
                                        model_view->instances.size(), model_view->preparation.sampled,
                                        model_view->palette.size(), model_view->deformation.size());
                        else
                        {
                            ImGui::Text("CPU preparation: %zu sampled / %zu reused / %zu uncached; %zu vertices",
                                        model_view->preparation.sampled, model_view->preparation.reused,
                                        model_view->preparation.uncached, model_view->mesh.vertices.size());
                            ImGui::Text("Prepared cache: %zu assets / %zu vertex bytes (weak asset ownership)",
                                        model_view->preparation.cached_assets, model_view->preparation.cached_bytes);
                        }
                        ImGui::TextWrapped(
                            "Current ABC poses; first-person body/held children follow local camera visibility. "
                            "View weapons use original offsets with zero bob; no lighting, chrome or muzzle FX.");
                        if (ImGui::TreeNode("Model visibility and skin diagnostics"))
                        {
                            for (const auto& entry : model_view->entries)
                                ImGui::TextWrapped(
                                    "%s: %s, %zu triangles / %zu hidden triangles / %zu hidden nodes, skin %s%s%s%s",
                                    entry.label.c_str(), entry.visible ? "visible" : "hidden", entry.triangles,
                                    entry.hidden_triangles, entry.hidden_nodes, entry.skin.c_str(),
                                    entry.error.empty() ? "" : " / ", entry.error.c_str(),
                                    entry.reused_geometry ? " / reused geometry" : "");
                            ImGui::TreePop();
                        }
                    }
                    ImGui::EndTabItem();
                }
                if ((!game.resources.mounts().empty()) && ImGui::BeginTabItem("World"))
                {
                    draw_world(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Objects"))
                {
                    const auto ids = game.objects.snapshot();
                    ImGui::Text("%zu indexed records  |  Time %.3f s  |  Strings %zu  |  Messages %zu",
                                game.objects.record_count(), game.objects.time(), game.server.string_count(),
                                game.server.message_count());
                    ImGui::TextUnformatted("Snapshot owns slot-ordered identities; records include pending removal.");
                    const auto& messages = game.server.message_stats;
                    ImGui::Text(
                        "Messages, adapter lifetime: %llu created / %llu released / %llu refused; peak %zu / %zu live",
                        static_cast<unsigned long long>(messages.created),
                        static_cast<unsigned long long>(messages.released),
                        static_cast<unsigned long long>(messages.refused), messages.peak_live,
                        ServerAdapter::max_live_messages);
                    if (ImGui::Button("Spawn CameraSpot"))
                        spawn_request = "CameraSpot";
                    ImGui::SameLine();
                    if (ImGui::Button("Spawn Key"))
                        spawn_request = "Key";
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!selection.id);
                    if (ImGui::Button("Remove selected"))
                        remove_request = selection.id;
                    ImGui::EndDisabled();
                    if (ImGui::BeginTable("Object list", 5,
                                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_ScrollY,
                                          ImVec2(0, 120)))
                    {
                        for (const char* label :
                             {"ID : generation", "Name", "Original class", "Lifecycle", "Next update"})
                            ImGui::TableSetupColumn(label);
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableHeadersRow();
                        for (auto id : ids)
                        {
                            const auto* object = game.objects.get(id);
                            ImGui::PushID(static_cast<int>(id.slot));
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            const auto label = std::to_string(id.slot) + " : " + std::to_string(id.generation);
                            if (ImGui::Selectable(label.c_str(), selection.id == id,
                                                  ImGuiSelectableFlags_SpanAllColumns))
                                selection.id = id;
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(object->state.m_Name);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(object->type->name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(life_name(object->life));
                            ImGui::TableNextColumn();
                            if (object->scheduled)
                                ImGui::Text("at %.4f s", object->due);
                            else
                                ImGui::TextUnformatted("Disabled");
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (const auto* object = game.objects.get(selection.id))
                    {
                        const auto& state = object->state;
                        ImGui::SeparatorText("Selected object");
                        ImGui::Text("%s / %s   |   %s   |   Updates: %llu", object->type->name.c_str(), state.m_Name,
                                    creation_name(object->reason), static_cast<unsigned long long>(object->updates));
                        ImGui::Text("Position %.2f, %.2f, %.2f   |   Scale %.2f, %.2f, %.2f", state.m_Pos.x,
                                    state.m_Pos.y, state.m_Pos.z, state.m_Scale.x, state.m_Scale.y, state.m_Scale.z);
                        ImGui::Text("Rotation (quaternion) %.2f, %.2f, %.2f, %.2f", state.m_Rotation.m_Vec.x,
                                    state.m_Rotation.m_Vec.y, state.m_Rotation.m_Vec.z, state.m_Rotation.m_Spin);
                        ImGui::Text("Type %u  |  Flags 0x%08X  |  Activity %u  |  Links %zu",
                                    static_cast<unsigned>(state.m_ObjectType), state.m_Flags, object->activity,
                                    game.objects.link_count(object->handle));
                        if (state.m_ObjectType == OT_MODEL)
                            ImGui::Text("Model additive RGB (lighting pending) %.3f %.3f %.3f | alpha %.3f",
                                        object->color.x, object->color.y, object->color.z, object->alpha);
                        if (const auto* shape =
                                object->world_model ? object->world_model.get() : object->container().get())
                        {
                            ImGui::Text("Geometry: %zu closed components | %zu shared edges | %zu triangles",
                                        shape->closed_components, shape->shared_edges, shape->triangles.size());
                            if (shape->off_plane_faces || shape->boundary_only)
                                ImGui::Text("Authored triangle boundary: %zu off-plane faces / %zu open edges%s",
                                            shape->off_plane_faces, shape->open_edges,
                                            shape->boundary_only ? " (no solid interior)" : "");
                            ImGui::Text("BSP model #%u of %u same-named%s", object->world_model_index,
                                        object->world_model_candidates,
                                        object->world_model_candidates > 1 ? " (first in file order bound)" : "");
                        }
                        if (const auto* sound = dynamic_cast<const SoundFX*>(object->game.get()))
                        {
                            ImGui::SeparatorText("Original SoundFX");
                            ImGui::Text("State %s | startup %s | %s | streaming requested %s", sound->state_name(),
                                        sound->start_on ? "on" : "off", sound->ambient ? "ambient" : "spatial",
                                        sound->file_stream ? "yes (PCM windows)" : "no");
                            ImGui::TextWrapped("Ramp up: %s | Main: %s | Ramp down: %s", sound->ramp_up.c_str(),
                                               sound->sound_path.c_str(), sound->ramp_down.c_str());
                            ImGui::Text("Radii %.1f / %.1f | volume byte %u | priority %u", sound->inner_radius,
                                        sound->outer_radius, unsigned(sound->volume), unsigned(sound->priority));
                            ImGui::Text("Commands %llu | starts %llu | failures %llu",
                                        (unsigned long long)sound->commands, (unsigned long long)sound->attempts,
                                        (unsigned long long)sound->failures);
                            if (const auto* voice = game.server.audio.get(sound->sound))
                            {
                                ImGui::Text("Voice %llu | %s | loop %s | %.3f / %.3f s | fixed %.1f %.1f %.1f",
                                            (unsigned long long)voice->id,
                                            !voice->error.empty() ? "failed"
                                            : voice->done         ? "finished"
                                                                  : "playing",
                                            voice->request.loop ? "yes" : "no", voice->cursor / voice->clip->rate,
                                            voice->clip->duration(), voice->position.x, voice->position.y,
                                            voice->position.z);
                                if (const auto& stream = voice->clip->stream)
                                    ImGui::Text("Disk buffers %zu KiB | read %llu bytes / %llu refills",
                                                PcmStream::buffer_bytes / 1024, (unsigned long long)stream->bytes_read,
                                                (unsigned long long)stream->refills);
                                if (!voice->error.empty())
                                    ImGui::TextWrapped("Stream failure: %s", voice->error.c_str());
                            }
                            else
                                ImGui::TextUnformatted("No live voice.");
                            if (!sound->error.empty())
                                ImGui::TextWrapped("Playback failure: %s", sound->error.c_str());
                            ImGui::TextWrapped(
                                "Only ambient ON loops. Reverb/compressed streaming/save are unavailable. "
                                "VoiceType=0 is retained unused historical metadata. Failed source schemas remain in "
                                "World.");
                        }
                        if (const auto* model = dynamic_cast<const CDestructableModel*>(object->game.get()))
                        {
                            ImGui::SeparatorText("Original destructible model");
                            if (const auto* rotating = dynamic_cast<const RotatingModel*>(model))
                            {
                                const auto& v = rotating->rotating;
                                ImGui::Text("Rotating aggregate: state %d | result %d | commands %u", int(v.state),
                                            int(v.last_move), v.commands);
                                ImGui::Text("Euler %.3f %.3f %.3f | velocity %.3f %.3f %.3f", v.pose_angles.x,
                                            v.pose_angles.y, v.pose_angles.z, v.velocity.x, v.velocity.y, v.velocity.z);
                            }
                            ImGui::Text("HP %.2f / %.2f | damage threshold %.2f | dead %s | destructible %s",
                                        model->m_damage.m_fHitPoints, model->hit_points, model->damage_hit_points,
                                        model->dead_state ? "yes" : "no", model->destructable ? "yes" : "no");
                            ImGui::Text("Body %s | pushable %s | mass %.2f | friction %.2f | priority %u",
                                        object->model_body ? "configured" : "unavailable",
                                        model->pushable ? "yes" : "no", object->body_properties.mass,
                                        object->body_properties.friction, object->blocking_priority);
                            ImGui::Text("Dimensions %.2f %.2f %.2f | shared client/body publications %u",
                                        object->dimensions.x, object->dimensions.y, object->dimensions.z,
                                        model->dimensions_publications);
                            ImGui::Text("Velocity %.2f %.2f %.2f | body steps %llu | pushes %llu | blocks %llu",
                                        object->velocity.x, object->velocity.y, object->velocity.z,
                                        static_cast<unsigned long long>(object->body_steps),
                                        static_cast<unsigned long long>(object->body_pushes),
                                        static_cast<unsigned long long>(object->mover_blocks));
                            ImGui::Text("Alpha %.3f -> %.3f | cascade delay %.3f | destruction callbacks %u",
                                        model->alpha, model->target_alpha, model->m_damage.m_fDeathDelay,
                                        model->destructions);
                            ImGui::Text("Ground placement result %d | sliding %s / standing %s | loop %llu",
                                        model->ground_result, model->sliding ? "yes" : "no",
                                        model->standing ? "yes" : "no",
                                        static_cast<unsigned long long>(model->sliding_sound));
                            ImGui::TextWrapped("Replacement models: damage %s / %s; destroyed %s / %s",
                                               model->damage_model.c_str(), model->damage_skin.c_str(),
                                               model->destroy_model.c_str(), model->destroy_skin.c_str());
                            ImGui::TextWrapped(
                                "Sliding WAV: %s. Contact forces are unavailable; ordinary zero-force touches do not "
                                "start a sliding loop. Gib requests are not rendered debris.",
                                model->sliding_path.c_str());
                            for (const auto* detail : {&model->error, &model->trigger_error, &model->sound_error})
                                if (!detail->empty())
                                    ImGui::TextWrapped("%s", detail->c_str());
                        }
                        if (object->type->name == "Model")
                        {
                            ImGui::TextWrapped("Original built-in Model: BaseClass properties and engine animation. "
                                               "No destructible-object rules or light emission.");
                            ImGui::Text("Collision half dimensions %.2f %.2f %.2f (no automatic model body)",
                                        object->dimensions.x, object->dimensions.y, object->dimensions.z);
                            if (!object->model && object->model_error.empty())
                                ImGui::TextUnformatted("No ABC resource requested.");
                        }
                        if (state.m_Filename[0] || state.m_SkinName[0])
                            ImGui::TextWrapped("Model: %s / Skin (decode status in 3D view): %s", state.m_Filename,
                                               state.m_SkinName);
                        if (object->model)
                            ImGui::Text("Decoded ABC: %zu nodes / %zu animations; clip %u at %.1f ms",
                                        object->model->nodes.size(), object->model->animations.size(),
                                        object->animation.animation, object->animation.time);
                        if (object->model && object->model->invalid_key_bounds)
                            ImGui::Text("ABC warning: %u unused key bounds have inverted ordering",
                                        object->model->invalid_key_bounds);
                        if (!object->model_error.empty())
                            ImGui::TextWrapped("Model unavailable: %s", object->model_error.c_str());
                        ImGui::Text("Velocity %.2f %.2f %.2f / Grounded: %s", object->velocity.x, object->velocity.y,
                                    object->velocity.z, object->grounded ? "yes" : "no");
                        ImGui::Text("Stopping force %.1f | incoming %.1f %.1f %.1f | notifications %llu",
                                    object->last_impact_force, object->last_impact_velocity.x,
                                    object->last_impact_velocity.y, object->last_impact_velocity.z,
                                    static_cast<unsigned long long>(object->impact_notifications));
                        ImGui::Text("Contact normal %.2f %.2f %.2f / Polygon %u", object->contact_normal.x,
                                    object->contact_normal.y, object->contact_normal.z, object->contact_polygon);
                        if (const auto* projectile = dynamic_cast<const CProjectile*>(object->game.get()))
                        {
                            ImGui::Text("Original projectile | configured %s | contact %s | steps %llu",
                                        projectile->configured ? "yes" : "no", projectile->m_bExplode ? "yes" : "no",
                                        (unsigned long long)projectile->flight_steps);
                            ImGui::Text("Damage %.2f / radius %d / HP %.2f / start %.3f / lifetime %.2f",
                                        projectile->m_fDamage, projectile->m_nRadius, projectile->m_damage.m_fHitPoints,
                                        projectile->m_fStartTime, projectile->m_fLifeTime);
                            ImGui::Text("Shooter alive %s / hit alive %s / contact polygon %u",
                                        game.server.IsObjectAlive(projectile->m_hFiredFrom) ? "yes" : "no",
                                        game.server.IsObjectAlive(projectile->m_hHitObject) ? "yes" : "no",
                                        projectile->contact.polygon);
                            if (const auto* remote = dynamic_cast<const CRemoteBomb*>(projectile))
                            {
                                ImGui::Text("Remote: armed %d / falling %d / contacts %llu", remote->armed,
                                            remote->falling, (unsigned long long)remote->contacts);
                                ImGui::Text("Pitch %.3f / yaw %.3f / spin %.3f %.3f / no fuse", remote->pitch,
                                            remote->yaw, remote->pitch_velocity, remote->yaw_velocity);
                            }
                            if (const auto* bomb = dynamic_cast<const CProximityBomb*>(projectile))
                            {
                                ImGui::Text("Proximity: armed %d / direct contact %d / scans %llu / contacts %llu",
                                            bomb->armed, bomb->hit_character, (unsigned long long)bomb->scans,
                                            (unsigned long long)bomb->contacts);
                                ImGui::Text("Detect %.1f / getaway %.2f / delay %.2f / sounds %u", bomb->detect_radius,
                                            bomb->get_away, bomb->explode_delay, bomb->sounds);
                                ImGui::Text("Detected identity alive: %d / inherited fuse unused",
                                            game.server.IsObjectAlive(bomb->detected));
                                ImGui::Text("Pitch %.3f / yaw %.3f / spin %.3f %.3f", bomb->pitch, bomb->yaw,
                                            bomb->pitch_velocity, bomb->yaw_velocity);
                                if (!bomb->sound_error.empty())
                                    ImGui::TextWrapped("Audio: %s", bomb->sound_error.c_str());
                            }
                            if (const auto* grenade = dynamic_cast<const CGrenade*>(projectile))
                            {
                                ImGui::Text("Grenade: bounces left %d / contacts %llu / pitch %.3f / yaw %.3f",
                                            grenade->bounces_left, (unsigned long long)grenade->contacts,
                                            grenade->pitch, grenade->yaw);
                                ImGui::Text("Loop %llu / bounce sounds %u / spin %.3f %.3f",
                                            (unsigned long long)grenade->sound, grenade->bounce_sounds,
                                            grenade->pitch_velocity, grenade->yaw_velocity);
                                if (!grenade->sound_error.empty())
                                    ImGui::TextWrapped("Audio: %s", grenade->sound_error.c_str());
                            }
                            if (const auto* napalm = dynamic_cast<const CNapalmProjectile*>(projectile))
                                ImGui::TextWrapped("Napalm smoke %llu: %s / %s", (unsigned long long)napalm->smoke,
                                                   napalm->smoke_error.c_str(), napalm->light_error.c_str());
                            if (const auto* fragment = dynamic_cast<const CHowitzerAltFrag*>(projectile))
                                ImGui::TextWrapped("Howitzer trail %llu: %s", (unsigned long long)fragment->trail,
                                                   fragment->light_error.c_str());
                            if (const auto* ball = dynamic_cast<const CTeslaBallProjectile*>(projectile))
                            {
                                ImGui::Text("Tesla ball: %llu damage arcs / %llu visual arcs / last effect %llu",
                                            (unsigned long long)ball->bolts, (unsigned long long)ball->lightning_count,
                                            (unsigned long long)ball->lightning);
                                if (!ball->lightning_error.empty())
                                    ImGui::TextWrapped("%s", ball->lightning_error.c_str());
                            }
                            if (const auto* flare = dynamic_cast<const CFlareProjectile*>(projectile))
                            {
                                ImGui::Text("Flare tracking %s / start %.3f / last tick %.3f / damage deliveries %u",
                                            flare->tracking ? "yes" : "no", flare->track_start, flare->last_damage_time,
                                            flare->ticks);
                                ImGui::Text("World offset %.2f %.2f %.2f / loop voice %llu (%s)", flare->offset.x,
                                            flare->offset.y, flare->offset.z, (unsigned long long)flare->sound,
                                            game.server.audio.get(flare->sound) ? "resolved" : "absent");
                                if (!flare->sound_error.empty())
                                    ImGui::TextWrapped("Loop: %s", flare->sound_error.c_str());
                            }
                            if (const auto* flare = dynamic_cast<const CFlareProjectile*>(projectile))
                                ImGui::TextWrapped("Trail %llu / %s", (unsigned long long)flare->trail,
                                                   flare->light_error.c_str());
                            if (const auto* flare = dynamic_cast<const CFlareBurstProjectile*>(projectile))
                                ImGui::TextWrapped("Fragment trail %llu / %s", (unsigned long long)flare->trail,
                                                   flare->light_error.c_str());
                            if (const auto* flare = dynamic_cast<const CFlareAltProjectile*>(projectile))
                                ImGui::Text(
                                    "Alternate flare: cached velocity %s / emitted children %u / effect type %u",
                                    flare->get_velocity ? "pending" : "yes", flare->children, flare->explosion_type);
                            if (const auto* orb = dynamic_cast<const COrbProjectile*>(projectile))
                            {
                                ImGui::Text("Orb: alternate %d / attached %d / ticks %llu / trail %llu / camera %d",
                                            orb->alternate, orb->attached, (unsigned long long)orb->damage_ticks,
                                            (unsigned long long)orb->trail, orb->camera_started);
                                if (!orb->sound_error.empty())
                                    ImGui::TextWrapped("Audio: %s", orb->sound_error.c_str());
                                if (!orb->camera_error.empty())
                                    ImGui::TextWrapped("Camera: %s", orb->camera_error.c_str());
                            }
                            if (const auto* leech = dynamic_cast<const CLeechPrimeProjectile*>(projectile))
                            {
                                ImGui::Text(
                                    "Leech: curve %.3f / amplitude %.2f / no base expiry / trail %llu scale %.2f",
                                    leech->curve, leech->amplitude, (unsigned long long)leech->trail,
                                    leech->trail_scale);
                                ImGui::Text("Loop %llu / light at %.2f %.2f %.2f",
                                            (unsigned long long)leech->flight_sound, leech->light_position.x,
                                            leech->light_position.y, leech->light_position.z);
                                ImGui::TextWrapped("%s %s", leech->light_error.c_str(), leech->sound_error.c_str());
                            }
                            if (const auto* leech = dynamic_cast<const CLeechAltProjectile*>(projectile))
                                ImGui::Text("Leech alternate: pushes %llu", (unsigned long long)leech->pushes);
                            ImGui::TextWrapped("Pending: %s", projectile->trail_error.c_str());
                            if (!projectile->error.empty())
                                ImGui::TextWrapped("Failure: %s", projectile->error.c_str());
                        }
                        if (const auto* key = dynamic_cast<const Key*>(object->game.get()))
                        {
                            ImGui::Text("Original Key: timestamp %.3f  |  sound radius %.1f  |  loop %s",
                                        key->m_fTimeStamp, key->m_fSoundRadius, key->m_bLoopSound ? "yes" : "no");
                            const auto* target = game.server.GetStringData(key->m_hstrMessageTarget);
                            const auto* message = game.server.GetStringData(key->m_hstrMessageName);
                            ImGui::Text("Original strings: target=%s  message=%s", target ? target : "(null)",
                                        message ? message : "(null)");
                        }
                        if (ImGui::BeginTable("Creation properties", 2,
                                              ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_ScrollY,
                                              ImVec2(0, 180)))
                        {
                            ImGui::TableSetupColumn("Creation property (includes inherited defaults)");
                            ImGui::TableSetupColumn("Value");
                            ImGui::TableSetupScrollFreeze(0, 1);
                            ImGui::TableHeadersRow();
                            for (const auto& [name, value] : object->properties)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(property_text(value).c_str());
                            }
                            ImGui::EndTable();
                        }
                    }
                    else
                        ImGui::TextDisabled("Select an object. Selection is cleared when its generation is destroyed.");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Runtime"))
                {
                    ImGui::Text("Renderer: %s    Window system: %s", SDL_GetGPUDeviceDriver(runtime.gpu),
                                SDL_GetCurrentVideoDriver());
                    ImGui::TextWrapped("Device: %s",
                                       SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu),
                                                             SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
                    const int version = SDL_GetVersion();
                    ImGui::Text("SDL %d.%d.%d (main)    Dear ImGui %s", SDL_VERSIONNUM_MAJOR(version),
                                SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version), IMGUI_VERSION);
                    ImGui::Text("Presented frames: %llu    UI rate: %.1f fps", static_cast<unsigned long long>(frames),
                                ImGui::GetIO().Framerate);
                    ImGui::TextDisabled("OpenXR: not started. No headset is required.");
                    ImGui::Spacing();
                    ImGui::SeparatorText("External content");
                    ImGui::TextWrapped("%s", path_text(config.data_dir).c_str());
                    ImGui::TextDisabled("Configuration source: %s", config.source.c_str());
                    if (!data.readme_version.empty())
                        ImGui::TextWrapped("Readme: %s", data.readme_version.c_str());
                    if (ImGui::BeginTable("Archives", 3,
                                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Archive");
                        ImGui::TableSetupColumn("Size");
                        ImGui::TableSetupColumn("Status");
                        ImGui::TableHeadersRow();
                        for (const auto& archive : data.archives)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(archive.name.c_str());
                            ImGui::TableNextColumn();
                            if (archive.present)
                                ImGui::Text("%.1f MiB", static_cast<double>(archive.bytes) / (1024 * 1024));
                            else
                                ImGui::TextUnformatted("-");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(!archive.present                  ? "Absent (optional)"
                                                   : archive.name == "BLOOD2NM.REZ"  ? "Expansion excluded"
                                                   : game.resources.mounts().empty() ? "Detected; not mounted"
                                                                                     : "Mounted (see Resources)");
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TextWrapped("Resource states and archive precedence are shown in Resources; presence alone "
                                       "does not mean decoded or rendered.");
                    if (ImGui::CollapsingHeader("Dependency revisions"))
                    {
                        ImGui::TextWrapped("SDL: %s", build::sdl_revision);
                        ImGui::TextWrapped("ImGui: %s", build::imgui_revision);
                        ImGui::TextWrapped("spdlog: %s", build::spdlog_revision);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Diagnostics"))
                {
                    ImGui::TextWrapped(
                        "Most recent contract failures (up to 32). Unknown classes and properties are rejected.");
                    for (const auto& item : game.trace)
                        ImGui::TextWrapped("%s", item.c_str());
                    if (ImGui::TreeNode("Projectile terminal history"))
                    {
                        for (const auto& report : game.server.projectile_reports)
                        {
                            ImGui::Text("%.3f %s: %s at %.2f %.2f %.2f / delivered %u", report.time,
                                        report.source.c_str(), report.outcome.c_str(), report.position.x,
                                        report.position.y, report.position.z, report.delivered);
                            if (report.remote)
                                ImGui::Text("Remote: armed %d / falling %d / contacts %llu", report.armed,
                                            report.falling, (unsigned long long)report.contacts);
                            if (report.time_bomb)
                                ImGui::Text("Time bomb: armed %d / character impact %d", report.armed,
                                            report.hit_character);
                            if (report.proximity)
                                ImGui::Text(
                                    "Proximity: armed %d / character impact %d / scans %llu / contacts %llu / audio %s",
                                    report.armed, report.hit_character, (unsigned long long)report.scans,
                                    (unsigned long long)report.contacts, report.audio_error.c_str());
                            if (report.grenade)
                                ImGui::Text("Grenade: contacts %llu / bounces left %d / explosion passes %u",
                                            (unsigned long long)report.contacts, report.bounces_left,
                                            report.explosions);
                            if (report.flare)
                                ImGui::Text("Flare: tracking %s / burn deliveries %u / burst children %u",
                                            report.tracked ? "yes" : "no", report.burn_ticks, report.burst_children);
                            if (report.shockwave)
                            {
                                ImGui::Text("Shockwave ground query %d: %.1f %.1f %.1f -> %.1f %.1f %.1f",
                                            int(report.query), report.query_from.x, report.query_from.y,
                                            report.query_from.z, report.query_to.x, report.query_to.y,
                                            report.query_to.z);
                                ImGui::Text("Blast %.1f %.1f %.1f / normal %.1f %.1f %.1f / candidates %u / in radius "
                                            "%u / interrupted %s",
                                            report.blast_position.x, report.blast_position.y, report.blast_position.z,
                                            report.blast_normal.x, report.blast_normal.y, report.blast_normal.z,
                                            report.candidates, report.in_radius, report.cancelled ? "yes" : "no");
                            }
                            if (!report.error.empty())
                                ImGui::TextWrapped("Failure: %s", report.error.c_str());
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TextWrapped("Service coverage, including unexercised entries, is in the Services tab.");
                    for (const auto& diagnostic : game.objects.diagnostics())
                        ImGui::TextWrapped("%s", diagnostic.c_str());
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Resources"))
                {
                    draw_resources(game);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Models", nullptr, select_models ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_models = false;
                    draw_models(game);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Services", nullptr, select_services ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_services = false;
                    draw_services(game);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Session", nullptr, select_session ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_session = false;
                    draw_session(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Weapons", nullptr, select_weapons ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_weapons = false;
                    draw_weapons(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Skeleton", nullptr, select_skeleton ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_skeleton = false;
                    draw_skeleton(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Movement", nullptr, select_movement ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_movement = false;
                    draw_movement(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Triggers", nullptr, select_triggers ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_triggers = false;
                    draw_triggers(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Lights", nullptr, select_lights ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_lights = false;
                    draw_lights(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Explosions", nullptr,
                                        select_scene_explosions ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_scene_explosions = false;
                    draw_scene_explosions(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Sprites", nullptr, select_sprites ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_sprites = false;
                    draw_sprites(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("World sounds", nullptr,
                                        select_world_sounds ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_world_sounds = false;
                    draw_world_sounds(game);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Random sounds", nullptr,
                                        select_random_sounds ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_random_sounds = false;
                    draw_random_sounds(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Cinematics", nullptr, select_cinematics ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_cinematics = false;
                    draw_cinematics(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Keyframes", nullptr, select_keyframes ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_keyframes = false;
                    draw_keyframes(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Cameras", nullptr, select_cameras ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_cameras = false;
                    draw_cameras(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Objectives", nullptr, select_objectives ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_objectives = false;
                    draw_objectives(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Pickups", nullptr, select_pickups ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_pickups = false;
                    draw_pickups(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Brushes", nullptr, select_brushes ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_brushes = false;
                    draw_brushes(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Doors", nullptr, select_doors ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_doors = false;
                    draw_doors(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Containers", nullptr, select_containers ? ImGuiTabItemFlags_SetSelected : 0))
                {
                    select_containers = false;
                    draw_containers(game, selection);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void Inspector::draw_world(GameSession& game, ObjectSelection& selection)
    {
        const auto& world = game.world;
        const auto* resource = game.resources.find(game.world_path);
        ImGui::Text("%s  |  v%u  |  %zu source objects  |  %zu BSP models", game.world_path.c_str(), world.version,
                    world.objects.size(), world.models.size());
        if (resource)
            ImGui::TextDisabled("Source: %s  |  %u overridden version(s)",
                                path_text(resource->archive.filename()).c_str(), resource->overrides);
        if (ImGui::Button("Reload world"))
            reload_request = true;
        ImGui::SameLine();
        if (ImGui::Button("Unload"))
            unload_request = true;
        ImGui::SameLine();
        if (ImGui::Button("Check session startup"))
            run_request = true;
        ImGui::SameLine();
        ImGui::TextUnformatted(game.state == SessionState::Ready     ? "Startup checkpoint passed"
                               : game.state == SessionState::Blocked ? "Startup check rejected"
                                                                     : "Inspection");
        if (game.startup.attempts)
        {
            const auto& report = game.startup;
            ImGui::Text(
                "Last startup check: %s at %.3f s | %zu decoded images | %zu unknown / %zu failed / %zu omitted",
                report.accepted ? "passed" : "rejected", report.time, report.checked_textures, report.unknown,
                report.failed, report.omitted);
            if (!report.error.empty())
                ImGui::TextWrapped("%s", report.error.c_str());
            ImGui::TextDisabled("Snapshot only; future requests and full M2 acceptance are separate.");
        }
        if (!world_error.empty())
            ImGui::TextWrapped("%s", world_error.c_str());
        ImGui::InputTextWithHint("##class-filter", "Filter class or object name", world_filter, sizeof(world_filter));
        if (ImGui::BeginTable("World source list", 4,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 160)))
        {
            for (auto label : {"DAT index", "Original class", "Source name", "Status"})
                ImGui::TableSetupColumn(label);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < world.objects.size(); ++i)
            {
                const auto& o = world.objects[i];
                const auto& e = game.entries[i];
                const auto* p = o.find("Name");
                const auto name = p ? property_text(*p) : "";
                if (o.class_name.find(world_filter) == std::string::npos &&
                    name.find(world_filter) == std::string::npos)
                    continue;
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::to_string(i).c_str(), source_selection == static_cast<int>(i),
                                      ImGuiSelectableFlags_SpanAllColumns))
                {
                    source_selection = static_cast<int>(i);
                    selection.id = e.object;
                    selection.refresh(game.objects);
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(o.class_name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(e.object && !game.objects.get(e.object) ? "Destroyed"
                                                                               : entry_state_name(e.state));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (ImGui::BeginTable("World details", 2, ImGuiTableFlags_Resizable))
        {
            ImGui::TableNextColumn();
            if (!world.models.empty())
            {
                model_selection = std::clamp(model_selection, 0, static_cast<int>(world.models.size() - 1));
                if (ImGui::BeginCombo("BSP model", world.models[model_selection].name.c_str()))
                {
                    for (std::size_t i = 0; i < world.models.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(world.models[i].name.c_str(), static_cast<int>(i) == model_selection))
                            model_selection = static_cast<int>(i);
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                const auto& m = world.models[model_selection];
                ImGui::Text("%zu points / %zu polygons / %zu surfaces", m.points.size(), m.polygons.size(),
                            m.surfaces.size());
                ImGui::TextDisabled("Top view Z/X, raw coordinates; diagnostic wireframe");
                const ImVec2 origin = ImGui::GetCursorScreenPos(),
                             size(std::max(100.f, ImGui::GetContentRegionAvail().x), 170);
                ImGui::InvisibleButton("BSP preview", size);
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(12, 18, 25, 255));
                if (!m.points.empty())
                {
                    float minx = m.points[0].x, maxx = minx, minz = m.points[0].z, maxz = minz;
                    for (auto p : m.points)
                    {
                        minx = std::min(minx, p.x);
                        maxx = std::max(maxx, p.x);
                        minz = std::min(minz, p.z);
                        maxz = std::max(maxz, p.z);
                    }
                    const float scale = std::min((size.x - 20) / std::max(1.f, maxz - minz),
                                                 (size.y - 20) / std::max(1.f, maxx - minx));
                    auto project = [&](DVector p)
                    {
                        return ImVec2(origin.x + size.x / 2 + (p.z - (minz + maxz) / 2) * scale,
                                      origin.y + size.y / 2 - (p.x - (minx + maxx) / 2) * scale);
                    };
                    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
                    // Budget only affects visualization, never the parsed resource.
                    std::size_t edges = 0;
                    for (const auto& poly : m.polygons)
                        for (std::size_t j = 0; j < poly.points.size() && edges < 40000; ++j, ++edges)
                            draw->AddLine(project(m.points[poly.points[j]]),
                                          project(m.points[poly.points[(j + 1) % poly.points.size()]]),
                                          IM_COL32(90, 174, 170, 160));
                    if (model_selection == 0 && source_selection >= 0 &&
                        source_selection < static_cast<int>(world.objects.size()))
                        if (const auto* p = world.objects[source_selection].find("Pos"))
                            if (const auto* v = std::get_if<DVector>(p))
                                draw->AddCircleFilled(project(*v), 5, IM_COL32(255, 106, 83, 255));
                    draw->PopClipRect();
                }
                ImGui::Text("Stored translation: %.1f %.1f %.1f", m.translation.x, m.translation.y, m.translation.z);
                ImGui::TextWrapped("%u nodes / %u leaves / %u portals; %zu opaque tail bytes. Collision tree and "
                                   "lightmaps are not decoded.",
                                   m.nodes, m.leaves, m.portals, m.opaque_bytes);
            }
            ImGui::TableNextColumn();
            if (source_selection >= 0 && source_selection < static_cast<int>(world.objects.size()))
            {
                const auto& o = world.objects[source_selection];
                const auto& e = game.entries[source_selection];
                ImGui::Text("Source #%d: %s", source_selection, o.class_name.c_str());
                if (!e.detail.empty())
                    ImGui::TextWrapped("%s", e.detail.c_str());
                if (ImGui::BeginTable("DAT properties", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                      ImVec2(0, 250)))
                {
                    for (auto label : {"Property", "Type / flags", "Value"})
                        ImGui::TableSetupColumn(label);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    for (const auto& p : o.properties)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(p.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%u / %08X", p.type, p.flags);
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", property_text(p.value).c_str());
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTable();
        }
    }
    void Inspector::draw_resources(const GameSession& game)
    {
        std::set<std::string> decoded_models;
        if (game.preview_model)
            decoded_models.emplace("models/characters/caleb.abc");
        for (auto id : game.objects.snapshot())
        {
            const auto* object = game.objects.get(id);
            if (object->model)
                if (const auto* resource = game.resources.find(object->state.m_Filename))
                    decoded_models.insert(resource->path);
        }
        auto decoded = [&](std::string_view path)
        {
            const auto* resource = game.resources.find(path);
            return resource && decoded_models.contains(resource->path);
        };
        ImGui::Text("%zu indexed resources / %zu mounts", game.resources.index().size(),
                    game.resources.mounts().size());
        for (const auto& mount : game.resources.mounts())
            ImGui::TextDisabled("%s: %zu entries", path_text(mount.path.filename()).c_str(), mount.entries);
        ImGui::TextWrapped("Later archives override exact normalized paths. This index identifies loaded DAT and ABC "
                           "data. Texture and sound decoding/playback are reported in their subsystem views; "
                           "an indexed resource alone does not imply successful use.");
        if (ImGui::CollapsingHeader("World resource references / missing content"))
        {
            for (const auto& use : game.resource_uses)
                ImGui::TextWrapped("%s | %s | %s",
                                   !use.indexed        ? "MISSING"
                                   : decoded(use.path) ? "ABC decoded"
                                                       : "Indexed",
                                   use.purpose.c_str(), use.path.c_str());
        }
        ImGui::InputTextWithHint("##resource-filter", "Filter normalized path", resource_filter,
                                 sizeof(resource_filter));
        std::vector<const Resource*> rows;
        for (const auto& [name, r] : game.resources.index())
            if (name.find(resource_filter) != std::string::npos)
                rows.push_back(&r);
        if (ImGui::BeginTable("Resource index", 5,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                              ImVec2(0, 330)))
        {
            for (auto label : {"Path", "Archive", "Offset / bytes", "Overrides", "State"})
                ImGui::TableSetupColumn(label);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            ImGuiListClipper clip;
            clip.Begin(static_cast<int>(rows.size()));
            while (clip.Step())
                for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
                {
                    const auto& r = *rows[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(r.path.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(path_text(r.archive.filename()).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u / %u", r.offset, r.size);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", r.overrides);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(r.path == game.world_path         ? "DAT decoded"
                                           : decoded_models.contains(r.path) ? "ABC decoded"
                                                                             : "Indexed");
                }
            ImGui::EndTable();
        }
    }

} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_session(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextUnformatted("Original shell and CPlayerObj entry subset");
        ImGui::TextWrapped(
            "Original shell/player state and copied AI paths. Complete AI behavior, remaining combat/FX, audio "
            "callers and save support are still unavailable.");
        unsigned ready = 0, failed = 0, active = 0;
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get());
            if (!ai || ai->npc_type == AI_Mgr::NPCType::Diagnostic)
                continue;
            ready += ai->RuntimeReady();
            failed += ai->runtime_failed;
            active += r->activity == OBJSTATE_ACTIVE;
        }
        ImGui::Text("NPCs: %u ready / %u failed / %u active", ready, failed, active);
        ImGui::TextWrapped("%s", waiting_for_player ? "Simulation waits for player entry; no world time is consumed."
                                                    : "Simulation runs independently of inspector visibility.");
        if (failed)
            ImGui::TextWrapped("Failed NPCs remain suspended. Select them in Skeleton to inspect the cause; entry does "
                               "not reset failures.");
        auto* player = game.server_shell.Player(game.local_client);
        if (!player)
        {
            if (ImGui::Button("Enter player and play"))
                enter_player_request = true;
        }
        else
        {
            if (ImGui::Button("Select CPlayerObj"))
                selection.id = game.objects.get(player->m_hObject)->id;
            ImGui::SameLine();
            if (ImGui::Button("Reset live player"))
                respawn_request = true;
            ImGui::SameLine();
            if (ImGui::Button("Leave world"))
                leave_player_request = true;
        }
        if (!world_error.empty())
            ImGui::TextWrapped("%s", world_error.c_str());
        ImGui::SeparatorText("Player restoration / keep-alive entry");
        ImGui::BeginDisabled(!player);
        if (ImGui::Button("Capture player state"))
            capture_player_request = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(player || !game.restoration.saved);
        if (ImGui::Button("Restore captured player"))
            restore_player_request = true;
        ImGui::EndDisabled();
        ImGui::InputText("Destination world", keep_world_name, sizeof(keep_world_name));
        ImGui::InputText("Destination start (empty = start)", keep_start_name, sizeof(keep_start_name));
        ImGui::BeginDisabled(!game.local_client);
        if (ImGui::Button("Change level safely with current player"))
            level_change_request = true;
        ImGui::EndDisabled();
        if (ImGui::Button("Inspect destination without leaving"))
            inspect_destination_request = true;
        ImGui::BeginDisabled(!player);
        if (ImGui::Button("Rehearse destination entry; retain current session"))
            trial_destination_request = true;
        ImGui::EndDisabled();
        draw_destination_trial(game);
        draw_level_change(game);
        draw_saved_games(game);
        draw_destination(game);
        ImGui::BeginDisabled(player);
        if (ImGui::Button("Load destination for inspection"))
            keep_world_request = true;
        ImGui::EndDisabled();
        ImGui::TextWrapped(
            "Leave the player before loading. Loading replaces the current world; the captured player is retained.");
        ImGui::BeginDisabled(player || !game.restoration.saved || game.world_path.empty());
        if (ImGui::Button("Enter loaded world with captured player"))
            keep_player_request = true;
        ImGui::EndDisabled();
        const auto& restore = game.restoration;
        if (restore.restored)
        {
            ImGui::TextWrapped("Last success: %s | destination %s | requested start %s | selected %s | fallback %s",
                               restore.last_mode == PlayerRestoreMode::KeepAlive ? "keep-alive" : "saved position",
                               restore.destination_world.c_str(), restore.requested_start.c_str(),
                               restore.selected_start.c_str(), restore.start_fallback ? "yes" : "no");
        }
        else
            ImGui::TextDisabled("No completed restoration");
        ImGui::Text("Captures %llu | Restored %llu / %llu | Restored client: %s", (unsigned long long)restore.captures,
                    (unsigned long long)restore.restored, (unsigned long long)restore.attempts,
                    game.client_shell.restored_entry ? "yes" : "no");
        if (!restore.error.empty())
            ImGui::TextWrapped("%s", restore.error.c_str());
        ImGui::TextWrapped(
            "Capture, leave the player, then restore. Original Load resets velocity/standard node "
            "visibility; client force-position clears acceleration. Keep-alive enters the already loaded "
            "destination at its named start and discards keys. Automatic level transfer, world/client saves "
            "and rollback of start-trigger effects remain unavailable.");
        if (restore.saved && ImGui::TreeNode("Saved engine/player values"))
        {
            const auto& s = *restore.saved;
            ImGui::TextWrapped("World: %s | Model: %s | Skin: %s", s.world.c_str(), s.model.c_str(), s.skin.c_str());
            ImGui::Text("Position %.2f %.2f %.2f | Half dimensions %.2f %.2f %.2f", s.position.x, s.position.y,
                        s.position.z, s.dimensions.x, s.dimensions.y, s.dimensions.z);
            ImGui::Text("Animation %u at %.2fms | loop %d done %d first %d | next update %.6fs", s.animation.animation,
                        s.animation.time, s.animation.looping, s.animation.done, s.animation.first, s.next_update);
            ImGui::Text("Crouched %d | eye %.1f | flags %x / user %x | priority %u", s.crouched, s.eye_level, s.flags,
                        s.user_flags, s.blocking_priority);
            ImGui::Text("HP %.2f | armor %.2f | bullets %.2f | air %.2f", s.damage.health, s.damage.armor,
                        s.inventory.ammo[1], s.player.air);
            ImGui::TreePop();
        }
        ImGui::SeparatorText("Shell state");
        ImGui::Text("Server world started: %s | Path snapshots: %zu | Updates: %llu",
                    game.server_shell.world_started ? "yes" : "no", game.server_shell.paths.size(),
                    static_cast<unsigned long long>(game.server_shell.updates));
        ImGui::Text("Clients: %zu | Client in world: %s | Messages: %llu | Updates: %llu", game.server.client_count(),
                    game.client_shell.m_bInWorld ? "yes" : "no",
                    static_cast<unsigned long long>(game.client_shell.messages),
                    static_cast<unsigned long long>(game.client_shell.updates));
        const auto& hud = game.client_shell.stats;
        ImGui::Text("HUD snapshot: HP %.1f | armor %.1f | weapon %u / slot %u", hud.health, hud.armor, hud.weapon,
                    hud.weapon_slot);
        ImGui::Text("Ammo %.1f (type %u) | alternate %.1f (type %u) | external camera %s", hud.ammo, hud.ammo_type,
                    hud.alternate_ammo, hud.alternate_ammo_type,
                    game.client_shell.view.object ? "yes (HUD hidden)" : "no");
        ImGui::TextUnformatted(
            "1..9 / 0: original inventory slots. Original PCX stat tabs remain visible with inspector hidden.");
        const auto& shell = game.client_shell;
        ImGui::Text("Original crosshair %02u: %s", unsigned(shell.m_nCrosshair),
                    shell.m_bShowCrosshair && shell.m_bInWorld && !shell.view.object && !shell.death_camera.active
                        ? "visible"
                        : "hidden");
        ImGui::Text("Zoom %s | FOV %.2f deg (presented %.2f) | messages %llu", shell.m_bZoomView ? "on" : "off",
                    shell.m_fovX * 57.29578f, shell.ViewFovX() * 57.29578f,
                    static_cast<unsigned long long>(shell.zoom_messages));
        ImGui::Text("HUD maxima: health %.1f | armor %.1f", hud.max_health, hud.max_armor);
        if (hud_status && ImGui::TreeNode("Original status bar resources"))
        {
            ImGui::Text("Layout %s | composition revision %llu", hud_status->high_graphics() ? "high" : "low",
                        static_cast<unsigned long long>(hud_status->revision));
            for (const auto& [path, surface] : hud_status->canvas.images())
                ImGui::Text("%s: %u x %u", path.c_str(), surface.width, surface.height);
            ImGui::TreePop();
        }
        else if (!hud_status)
            ImGui::TextUnformatted("Original HUD resources await the first visible player frame.");
        if (player && player->ctf_team)
        {
            ImGui::Text("CTF local inspection: team %d, player score %d, teams %d / %d", player->ctf_team,
                        player->ctf_score, g_ctf.team_scores[0], g_ctf.team_scores[1]);
            if (ImGui::Button("Join blue team"))
                player->SetCTFTeam(1);
            ImGui::SameLine();
            if (ImGui::Button("Join red team"))
                player->SetCTFTeam(2);
            ImGui::SameLine();
            if (ImGui::Button("Drop carried flag"))
                player->DropFlag();
            ImGui::TextWrapped("%s", game.client_shell.ctf.notice.c_str());
            ImGui::TextWrapped("%s %s", player->ctf_error.c_str(), game.client_shell.ctf.error.c_str());
            for (const auto* f : g_ctf.flags)
                if (f)
                    ImGui::Text("Flag team %d: stand %d, ground %d, carried %d; grabs %llu returns %llu drops %llu",
                                f->team, f->in_stand, f->on_ground, f->with_player, (unsigned long long)f->grabs,
                                (unsigned long long)f->returns, (unsigned long long)f->drops);
        }
        const auto& boss_client = game.client_shell;
        ImGui::Text("Boss health messages %llu | raw ratio %.3f | bar %s",
                    (unsigned long long)boss_client.boss_health_messages, boss_client.boss_health.fraction,
                    boss_client.boss_health_visible ? "visible" : "hidden");
        if (boss_client.boss_health_visible)
            ImGui::ProgressBar(boss_client.boss_health.fraction, ImVec2(-1, 0), "Original boss-health state");
        ImGui::TextUnformatted("Inspection boss bar; the original boss/power HUD is not yet connected.");
        draw_paths(game, selection);
        if (player)
        {
            const auto* record = game.objects.get(player->m_hObject);
            const auto& client = game.client_shell;
            ImGui::SeparatorText("CPlayerObj / Caleb");
            ImGui::Text("Object %u:%u | Initial update: %s | Attributes received: %s", record->id.slot,
                        record->id.generation, player->initialized ? "yes" : "no",
                        player->attributes_received ? "yes" : "no");
            ImGui::Text("Player updates: %llu | Entry resets: %llu | Destructable list: %u",
                        static_cast<unsigned long long>(player->updates),
                        static_cast<unsigned long long>(player->respawns), CDestructable::m_dwNumDestructables);
            ImGui::Text("Entry marker: %s | multiplayer inspection %d", player->entry_start_name.c_str(),
                        player->entry_start_multiplayer);
            ImGui::Text("Server position %.3f %.3f %.3f | Half dimensions %.1f %.1f %.1f", record->state.m_Pos.x,
                        record->state.m_Pos.y, record->state.m_Pos.z, record->dimensions.x, record->dimensions.y,
                        record->dimensions.z);
            ImGui::Text("Client position %.3f %.3f %.3f | Pitch / yaw %.3f %.3f", client.position.x, client.position.y,
                        client.position.z, client.rotation.pitch, client.rotation.yaw);
            ImGui::Text("Camera %.3f %.3f %.3f | Animation %.1f ms", client.camera_position.x, client.camera_position.y,
                        client.camera_position.z, record->animation.time);
            ImGui::Text("USE (E with F2 controls): %llu commands / %llu messages | door refused %s",
                        static_cast<unsigned long long>(player->use_commands),
                        static_cast<unsigned long long>(player->use_messages), player->use_door_refused ? "yes" : "no");
            ImGui::Text("Grab: %llu commands / %llu moves | Taunts: %llu | Drops: %llu/%llu",
                        static_cast<unsigned long long>(player->grab_commands),
                        static_cast<unsigned long long>(player->grab_moves),
                        static_cast<unsigned long long>(player->taunt_commands),
                        static_cast<unsigned long long>(player->m_InventoryMgr.drops),
                        static_cast<unsigned long long>(player->m_InventoryMgr.drop_attempts));
            if (!player->control_error.empty())
                ImGui::TextWrapped("Control: %s", player->control_error.c_str());
            if (ImGui::TreeNode("Original player death"))
            {
                const auto& death = client.death_camera;
                ImGui::Text("Committed %d / deaths %llu / client messages %llu / received %llu", player->m_bDead,
                            static_cast<unsigned long long>(player->deaths),
                            static_cast<unsigned long long>(player->death_messages),
                            static_cast<unsigned long long>(client.death_notifications));
                ImGui::Text("Voice group %d / corpse slot %d / corpses %llu / gib requests %llu / limbs %llu",
                            player->death_voice_event, player->death_slot,
                            static_cast<unsigned long long>(player->death_corpses),
                            static_cast<unsigned long long>(player->death_gibs),
                            static_cast<unsigned long long>(player->death_limbs));
                ImGui::Text("Corpse currently alive %d / placement result %d",
                            game.server.IsObjectAlive(player->last_death_corpse),
                            static_cast<int>(player->death_placement.result));
                ImGui::Text("Death camera active %d / orbit %.3f rad / transition %.3f / obstruction %d", death.active,
                            death.angle, death.fraction, static_cast<int>(death.result));
                ImGui::Text("Death camera position %.2f %.2f %.2f", death.position.x, death.position.y,
                            death.position.z);
                for (const auto* issue :
                     std::array<const std::string*, 3>{&player->death_error, &player->death_effect_error, &death.error})
                    if (!issue->empty())
                        ImGui::TextWrapped("%s", issue->c_str());
                ImGui::TextWrapped("Single-player death keeps the player/client. Movement, fire and item commands "
                                   "are blocked; USE retains its original ray. Loading a live checkpoint is separate "
                                   "from full world-save support.");
                ImGui::TreePop();
            }
            constexpr const char* ray_results[] = {"invalid", "miss", "hit", "unavailable"};
            const auto& fade = game.client_shell.fade;
            ImGui::Text("Screen fade: %s | value %.2f / 255 | scale %.3f | messages %llu",
                        fade.fade_in    ? "in"
                        : fade.fade_out ? "out (holds black)"
                                        : "inactive",
                        fade.value, fade.Scale(), (unsigned long long)fade.messages);
            ImGui::Text("Player triggers %llu | delivered fades %llu | unsupported %llu",
                        (unsigned long long)player->trigger_messages, (unsigned long long)player->fade_messages,
                        (unsigned long long)player->unsupported_triggers);
            ImGui::Text("Unknown commands ignored by source: %llu", (unsigned long long)player->ignored_triggers);
            ImGui::TextWrapped("Last unsupported: %s", player->last_unsupported_player_trigger.c_str());
            ImGui::TextWrapped("Last command: %s | %s", player->last_player_trigger.c_str(),
                               player->player_trigger_error.c_str());
            if (ImGui::TreeNode("Original player voices"))
            {
                const auto& vm = client.voice_manager;
                const auto* sound = game.server.audio.get(vm.voice);
                ImGui::Text("Requests %llu | starts %llu | refused %llu | failures %llu | stops %llu",
                            (unsigned long long)vm.requests, (unsigned long long)vm.starts,
                            (unsigned long long)vm.refused, (unsigned long long)vm.failures,
                            (unsigned long long)vm.stops);
                ImGui::Text("Enabled %s | retained voice %llu (%s) | next allowed after %.3f", vm.on ? "yes" : "no",
                            (unsigned long long)vm.voice,
                            !sound        ? "none / expired"
                            : sound->done ? "done"
                                          : "playing",
                            vm.next_play_time);
                ImGui::TextWrapped("Resource: %s | refusal: %s | error: %s", vm.last_resource.c_str(),
                                   vm.reason.c_str(), vm.error.c_str());
                ImGui::Text("Weapon pickup branches %llu | queued replies %llu | failed delivery %llu",
                            (unsigned long long)player->weapon_pickup_voice_requests,
                            (unsigned long long)player->voice_messages,
                            (unsigned long long)player->voice_delivery_failures);
                ImGui::Text("Damage voice: integer HP %u | last loss %u | draw %d | event %d", player->m_nLastHitPoints,
                            player->m_nLastDamage, player->damage_voice_roll, player->damage_voice_event);
                ImGui::Text("Damage checks %llu / eligible requests %llu | %s",
                            (unsigned long long)player->damage_voice_checks,
                            (unsigned long long)player->damage_voice_requests, player->damage_voice_error.c_str());
                ImGui::Text("SOUND attempts %llu / started %llu / stops queued %llu | last voice %llu",
                            (unsigned long long)player->trigger_sound_requests,
                            (unsigned long long)player->trigger_sound_starts,
                            (unsigned long long)player->trigger_voice_stops, (unsigned long long)player->trigger_sound);
                ImGui::TextWrapped("SOUND: %s | %s", player->trigger_sound_resource.c_str(),
                                   player->trigger_sound_error.c_str());
                for (std::size_t c = 0; c < vm.groups.size(); ++c)
                {
                    for (std::size_t e = 0; e < vm.groups[c].size(); ++e)
                    {
                        const auto& group = vm.groups[c][e];
                        ImGui::Text("Character %zu / event %zu: %zu WAVs | %s | last %d | rejected %u", c, e,
                                    group.sounds.size(), group.custom ? "custom" : "standard", group.last,
                                    group.rejected);
                    }
                }
                for (std::size_t e = 0; e < vm.played[0].size(); ++e)
                    ImGui::Text("Caleb unique %zu: %s", e, vm.played[0][e] ? "played" : "not played");
                ImGui::TreePop();
            }
            const auto* use_target = game.objects.get(player->use_hit.object);
            ImGui::Text(
                "Ray %s | target %s | distance %.2f / 82", ray_results[static_cast<unsigned>(player->use_result)],
                use_target && use_target->life == Life::Alive ? use_target->state.m_Name : "world / none / removed",
                player->use_hit.fraction * 82);
            ImGui::Text("Eye %.2f %.2f %.2f -> %.2f %.2f %.2f", player->use_from.x, player->use_from.y,
                        player->use_from.z, player->use_to.x, player->use_to.y, player->use_to.z);
            ImGui::Text("Door push requests %llu | pending %s", static_cast<unsigned long long>(player->door_pushes),
                        player->door_push_pending ? "yes" : "no");
            ImGui::Text("Health %.0f / %.0f | Armor %.0f | Bullets %.0f", player->m_damage.m_fHitPoints,
                        player->m_damage.m_fMaxHitPoints, player->m_damage.m_fArmorPoints,
                        player->m_InventoryMgr.GetAmmoCount(1));
            ImGui::Text("Strength %u | Speed %u | Resistance %u | Magic %u", player->m_nAttribStrength,
                        player->m_nAttribSpeed, player->m_nAttribResistance, player->m_nAttribMagic);
            ImGui::Text("Death HP %.2f / delay %.3fs / raw dead %d / effective dead %d / nigh-invulnerable %d",
                        player->m_damage.m_fDeathHitPoints, player->m_damage.m_fDeathDelay, player->m_damage.m_bDead,
                        player->m_damage.IsDead(), player->m_damage.m_bNighInvulnerable);
            ImGui::TextWrapped("Model: %s | Skin: %s (decode status in 3D view)", record->state.m_Filename,
                               record->state.m_SkinName);
            ImGui::TextWrapped(
                "Requested slot 1: %u. Current slot: %u. Original inventory owns supported original weapons; "
                "inspect their use in Weapons. Muzzle light exists but is not rendered.",
                player->m_InventoryMgr.requested_weapons[0], player->m_InventoryMgr.m_nCurWeaponSlot);
            if (!player->pending_model_commands.empty())
            {
                ImGui::TextUnformatted("Unhandled character model commands:");
                for (const auto& command : player->pending_model_commands)
                    ImGui::TextWrapped("%s", command.c_str());
            }
        }
        if (ImGui::CollapsingHeader("Entry trace"))
            for (const auto& event : game.trace)
                ImGui::TextWrapped("%s", event.c_str());
    }
    void Inspector::draw_movement(GameSession& game, ObjectSelection& selection)
    {
        auto* player = game.server_shell.Player(game.local_client);
        if (!player)
        {
            ImGui::TextUnformatted("Enter the original player in Session to inspect movement.");
            return;
        }
        auto& client = game.client_shell;
        const auto& move = client.move_mgr;
        const auto& body = move.body;
        if (ImGui::Button("Select player"))
            selection.id = game.objects.get(player->m_hObject)->id;
        ImGui::TextWrapped(
            "WASD / ZQSD: walk | Shift: run | Space: jump | Ctrl: crouch | Arrows: look. F1 hides UI. Text editing and "
            "focus loss suspend keys.");
        MovementInput input;
        ImGui::Button("Forward");
        input.forward = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Back");
        input.backward = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Left");
        input.left = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Right");
        input.right = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Jump");
        input.jump = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Crouch");
        input.crouch = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Run forward");
        if (ImGui::IsItemActive())
            input.run = input.forward = true;
        movement_input_request = input;
        auto aim = client.rotation;
        if (ImGui::SliderAngle("Facing yaw", &aim.yaw, -180, 180))
            weapon_aim_request = aim;
        if (!ImGui::BeginTable("Movement diagnostics", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextColumn();
        ImGui::Text("Position  %.2f  %.2f  %.2f", body.position.x, body.position.y, body.position.z);
        ImGui::Text("Velocity  %.2f  %.2f  %.2f", body.velocity.x, body.velocity.y, body.velocity.z);
        ImGui::Text("Horizontal speed %.2f | grounded %s", move.m_fVelMagnitude, move.m_bOnGround ? "yes" : "no");
        ImGui::Text("Crouch %s | requested %s | forced %s", player->crouched ? "yes" : "no",
                    player->crouch_requested ? "yes" : "no", player->forced_crouch ? "yes" : "no");
        ImGui::Text("Half-size %.0f %.0f %.0f | eye %.0f", body.dimensions.x, body.dimensions.y, body.dimensions.z,
                    player->eye_level);
        ImGui::Text("Stairs %llu / %llu | rise %.1f | limit %.1f", static_cast<unsigned long long>(body.stair_steps),
                    static_cast<unsigned long long>(body.stair_attempts), body.last_step_height,
                    body.dimensions.y * 0.5f);
        ImGui::Text("Allowed-overlap contacts %llu | last depth %.6f",
                    static_cast<unsigned long long>(body.overlap_contacts), body.last_overlap_depth);
        ImGui::Text("Moves left/slid along %llu initially overlapped faces",
                    static_cast<unsigned long long>(body.start_departures));
        const auto* record = game.objects.get(player->m_hObject);
        ImGui::TextWrapped("Requested clip: %s", player->locomotion_requested_clip.c_str());
        ImGui::TextWrapped("Clip: %s | fallback %s", player->locomotion_clip.c_str(),
                           player->locomotion_clip_retained                               ? "current clip kept"
                           : player->locomotion_requested_clip != player->locomotion_clip ? "yes"
                                                                                          : "no");
        ImGui::Text("Invalid source index requests %llu",
                    static_cast<unsigned long long>(player->retained_clip_requests));
        if (record && record->model && record->animation.animation < record->model->animations.size())
            ImGui::Text("%.1f / %u ms | loop %s | done %s", record->animation.time,
                        record->model->animations[record->animation.animation].duration,
                        record->animation.looping ? "yes" : "no", record->animation.done ? "yes" : "no");
        if (!player->locomotion_error.empty())
            ImGui::TextWrapped("%s", player->locomotion_error.c_str());
        if (ImGui::CollapsingHeader("Motion details"))
        {
            ImGui::Text("Gravity  %.0f  %.0f  %.0f", move.physics.gravity.x, move.physics.gravity.y,
                        move.physics.gravity.z);
            ImGui::Text("Walk %.0f | Run %.0f | Jump %.0f", move.physics.move_speed, move.physics.move_speed * 1.5f,
                        move.physics.jump_speed);
            if (body.ground_polygon != UINT32_MAX)
                ImGui::Text("World support polygon %u", body.ground_polygon);
            else if (const auto* support = game.objects.get(body.ground_object))
                ImGui::Text("Object support %s / %u", support->type->name.c_str(), support->id.slot);
            else
                ImGui::TextUnformatted("No support");
            ImGui::Text("Jump voice requests %llu / starts %llu",
                        static_cast<unsigned long long>(move.jump_voice_requests),
                        static_cast<unsigned long long>(move.jump_voice_starts));
            if (const auto* player = game.server_shell.Player(game.local_client))
            {
                ImGui::Text("Landing audio: contacts %llu / requests %llu / starts %llu | fall span %.2f",
                            static_cast<unsigned long long>(player->landing_contacts),
                            static_cast<unsigned long long>(player->landing_requests),
                            static_cast<unsigned long long>(player->landing_starts), player->landing_distance);
                ImGui::Text("Fall tracking: client %d / server %d / start %.3f / min %.2f / max %.2f",
                            player->m_bFalling, player->m_bServerFalling, player->m_startFall, player->m_fMinFallPos,
                            player->m_fMaxFallPos);
                ImGui::Text("Landing surface %d / foot container %d | %s", player->landing_surface,
                            player->landing_container, player->landing_resource.c_str());
                ImGui::Text("Fall damage: requests %llu / deliveries %llu | force %.1f / incoming Y %.1f / raw %.2f",
                            static_cast<unsigned long long>(player->fall_damage_requests),
                            static_cast<unsigned long long>(player->fall_damage_deliveries), player->fall_damage_force,
                            player->fall_damage_speed, player->fall_damage_amount);
                if (!player->fall_damage_error.empty())
                    ImGui::TextWrapped("%s", player->fall_damage_error.c_str());
                const auto& damage = player->m_damage;
                ImGui::Text("Damage impulses: enabled %d | accepted %llu | last %s (%.2f, %.2f, %.2f)",
                            damage.apply_damage_physics, static_cast<unsigned long long>(damage.damage_impulses),
                            damage.last_impulse_ok ? (damage.damage_impulses ? "accepted" : "none") : "rejected",
                            damage.last_impulse.x, damage.last_impulse.y, damage.last_impulse.z);
                if (!player->landing_error.empty())
                    ImGui::TextWrapped("%s", player->landing_error.c_str());
            }
            ImGui::Text("Jumps %llu | Landings %llu", static_cast<unsigned long long>(move.jumps),
                        static_cast<unsigned long long>(move.landings));
            ImGui::Text("Steps %llu | Messages %llu", static_cast<unsigned long long>(move.substeps),
                        static_cast<unsigned long long>(player->movement_messages));

            ImGui::Text("Acceleration  %.1f  %.1f  %.1f", body.acceleration.x, body.acceleration.y,
                        body.acceleration.z);
            ImGui::Text("Half dimensions  %.1f  %.1f  %.1f", body.dimensions.x, body.dimensions.y, body.dimensions.z);
            ImGui::Text("Ground normal %.2f  %.2f  %.2f", body.ground_normal.x, body.ground_normal.y,
                        body.ground_normal.z);
            ImGui::Text("Controls 0x%04X | move code %u", move.m_dwControlFlags, move.m_ClientMoveCode);
            ImGui::Text("Step %.3f ms | discarded %.3f s", move.m_FrameTime * 1000, move.discarded_time);
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Live player ABC / local front view");
        if (record && record->model)
        {
            const auto& model = *record->model;
            std::vector<DVector> vertices;
            const auto origin = ImGui::GetCursorScreenPos();
            const ImVec2 size{std::max(180.0f, ImGui::GetContentRegionAvail().x), 170};
            ImGui::InvisibleButton("Live locomotion pose", size);
            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 end{origin.x + size.x, origin.y + size.y};
            draw->AddRectFilled(origin, end, IM_COL32(12, 17, 23, 255));
            draw->PushClipRect(origin, end, true);
            const auto scale = std::min(size.x / 150.0f, size.y / 110.0f);
            auto project = [&](DVector p)
            { return ImVec2{origin.x + size.x * .5f + p.x * scale, origin.y + size.y * .5f - p.y * scale}; };
            if (sample_vertices(model, record->animation.animation, record->animation.time, vertices))
                for (const auto& face : model.faces)
                    for (unsigned i = 0; i < 3; ++i)
                        draw->AddLine(project(vertices[face.vertices[i]]),
                                      project(vertices[face.vertices[(i + 1) % 3]]), IM_COL32(126, 193, 210, 180));
            draw->AddRect(project({-body.dimensions.x, body.dimensions.y, 0}),
                          project({body.dimensions.x, -body.dimensions.y, 0}), IM_COL32(255, 170, 70, 255));
            draw->AddCircleFilled(project({0, player->eye_level, 0}), 3, IM_COL32(150, 255, 170, 255));
            draw->PopClipRect();
        }
        ImGui::TextUnformatted("Local collision map / top view (+Z up)");
        ImGui::TextWrapped("Blue: world slice | orange: solid objects | white: player");
        const auto origin = ImGui::GetCursorScreenPos();
        const ImVec2 size{std::max(180.0f, ImGui::GetContentRegionAvail().x), 100};
        ImGui::InvisibleButton("Collision map", size);
        const ImVec2 end{origin.x + size.x, origin.y + size.y}, center{origin.x + size.x / 2, origin.y + size.y / 2};
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, end, IM_COL32(19, 24, 31, 255));
        draw->PushClipRect(origin, end, true);
        constexpr float scale = .6f;
        auto project = [&](DVector p)
        { return ImVec2{center.x + (p.x - body.position.x) * scale, center.y - (p.z - body.position.z) * scale}; };
        unsigned edges = 0;
        if (!game.world.models.empty())
            for (const auto& poly : game.world.models.front().polygons)
            {
                const auto& model = game.world.models.front();
                if (!(model.surfaces[poly.surface].flags & 1))
                    continue;
                for (std::size_t i = 0; i < poly.points.size() && edges < 20000; ++i, ++edges)
                {
                    auto a = model.points[poly.points[i]], b = model.points[poly.points[(i + 1) % poly.points.size()]];
                    if (std::min(a.y, b.y) > body.position.y + body.dimensions.y ||
                        std::max(a.y, b.y) < body.position.y - body.dimensions.y - 2)
                        continue;
                    draw->AddLine(project(a), project(b), IM_COL32(70, 130, 180, 170));
                }
            }
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id); r && r->life == Life::Alive && (r->state.m_Flags & FLAG_SOLID))
            {
                const auto p = object_bounds_center(*r), d = r->dimensions;
                if (std::abs(p.y - body.position.y) > d.y + body.dimensions.y)
                    continue;
                draw->AddRect(project({p.x - d.x, p.y, p.z + d.z}), project({p.x + d.x, p.y, p.z - d.z}),
                              r->handle == player->m_hObject ? IM_COL32_WHITE : IM_COL32(255, 170, 70, 255));
            }
        const auto front = project({body.position.x + std::sin(client.rotation.yaw) * 45, body.position.y,
                                    body.position.z + std::cos(client.rotation.yaw) * 45});
        draw->AddLine(center, front, IM_COL32(130, 255, 180, 255), 2);
        draw->AddCircleFilled(center, 3, IM_COL32_WHITE);
        draw->PopClipRect();
        ImGui::EndTable();
        if (ImGui::CollapsingHeader("Movement limitations"))
            ImGui::TextWrapped("Locomotion subset. Container details are in Containers. Stair solver is a replacement, "
                               "not retail parity. "
                               "Translating platforms are supported; rotating transport, injury/death transitions, "
                               "audio and fall damage remain pending.");
    }
    void Inspector::draw_models(GameSession& game)
    {
        if (!game.preview_error.empty())
            ImGui::TextWrapped("%s", game.preview_error.c_str());
        if (!game.preview_model)
        {
            ImGui::TextWrapped("Load a retail world to inspect Caleb's ABC model. No model is loaded.");
            return;
        }
        const auto& m = *game.preview_model;
        auto& track = game.preview_animation;
        ImGui::TextUnformatted("Caleb model preview / no CPlayerObj instance");
        ImGui::Text("%zu vertices / %zu faces / %zu nodes / %zu animations", m.vertices.size(), m.faces.size(),
                    m.nodes.size(), m.animations.size());
        if (ImGui::BeginCombo("Animation", m.animations[track.animation].name.c_str()))
        {
            for (std::uint32_t i = 0; i < m.animations.size(); ++i)
                if (ImGui::Selectable(m.animations[i].name.c_str(), track.animation == i))
                {
                    track.animation = i;
                    track.reset();
                    game.preview_events.clear();
                }
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Play preview", &game.preview_playing);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &track.looping);
        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            track.reset();
            game.preview_events.clear();
        }
        const auto& clip = m.animations[track.animation];
        ImGui::Text("%.1f / %u ms / %zu keys / dimensions %.2f %.2f %.2f", track.time, clip.duration, clip.keys.size(),
                    clip.dimensions.x, clip.dimensions.y, clip.dimensions.z);
        if (ImGui::BeginTable("Model details", 2, ImGuiTableFlags_Resizable))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto origin = ImGui::GetCursorScreenPos();
            const float width = std::max(100.0f, ImGui::GetContentRegionAvail().x), height = 300;
            ImGui::InvisibleButton("Model pose", ImVec2(width, height));
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(12, 17, 23, 255));
            std::vector<NodePose> pose;
            std::vector<DVector> vertices;
            if (sample_pose(m, track.animation, track.time, pose) &&
                sample_vertices(m, track.animation, track.time, vertices))
            {
                const auto scale =
                    std::min(width / std::max(1.0f, m.max.x - m.min.x), height / std::max(1.0f, m.max.y - m.min.y)) *
                    .85f;
                auto project = [&](DVector p)
                {
                    return ImVec2(origin.x + width / 2 + (p.x - (m.min.x + m.max.x) / 2) * scale,
                                  origin.y + height / 2 - (p.y - (m.min.y + m.max.y) / 2) * scale);
                };
                draw->PushClipRect(origin, ImVec2(origin.x + width, origin.y + height), true);
                for (const auto& face : m.faces)
                {
                    std::array<ImVec2, 3> points;
                    for (unsigned j = 0; j < 3; ++j)
                        points[j] = project(vertices[face.vertices[j]]);
                    for (unsigned j = 0; j < 3; ++j)
                        draw->AddLine(points[j], points[(j + 1) % 3], IM_COL32(108, 151, 171, 100));
                }
                for (std::size_t i = 0; i < m.nodes.size(); ++i)
                {
                    auto p = project(pose[i].position);
                    if (m.nodes[i].parent >= 0)
                        draw->AddLine(p, project(pose[m.nodes[i].parent].position), IM_COL32(250, 126, 97, 255), 2);
                    draw->AddCircleFilled(p, 2.5f, IM_COL32(255, 201, 148, 255));
                }
                draw->PopClipRect();
            }
            ImGui::TextWrapped("Engine +Z forward; skeletal and vertex animation. Wireframe preview; skin decode "
                               "status is in 3D view.");
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("Animation keys", ImVec2(0, 300)))
            {
                for (const auto& key : clip.keys)
                    if (!key.text.empty())
                        ImGui::TextWrapped("%u ms: %s", key.time, key.text.c_str());
                ImGui::SeparatorText("Crossed on the last preview step");
                for (auto index : game.preview_events)
                    ImGui::TextWrapped("%s", clip.keys[index].text.c_str());
                ImGui::SeparatorText("Skeleton");
                for (std::size_t i = 0; i < m.nodes.size(); ++i)
                    ImGui::Text("%u: %s / parent %d / deformed vertices %zu", m.nodes[i].index, m.nodes[i].name.c_str(),
                                m.nodes[i].parent, m.nodes[i].deformed_vertices.size());
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }
    }
    void Inspector::draw_services(const GameSession& game)
    {
        if (ImGui::CollapsingHeader("Service coverage", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Current world trace: loading, player entry/restore, updates and teardown. "
                               "Counts are attempts, not successful results. Unexercised does not mean unsupported. "
                               "This is the bounded port interface, not the full original SDK or client API.");
            if (ImGui::BeginTable("Service coverage table", 4,
                                  ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, 260)))
            {
                ImGui::TableSetupColumn("Service");
                ImGui::TableSetupColumn("Boundary");
                ImGui::TableSetupColumn("Implementation");
                ImGui::TableSetupColumn("Observed calls");
                ImGui::TableHeadersRow();
                for (const auto& row : service_coverage(game.server.service_calls))
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(service_boundary_name(row.boundary));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(service_support_name(row.support));
                    ImGui::TableNextColumn();
                    if (row.calls)
                        ImGui::Text("%llu", static_cast<unsigned long long>(row.calls));
                    else
                        ImGui::TextUnformatted("Unexercised");
                }
                ImGui::EndTable();
            }
            for (const auto& [reason, count] : game.server.unavailable_history)
                ImGui::TextWrapped("Unavailable request (%llu): %s", static_cast<unsigned long long>(count),
                                   reason.c_str());
        }

        ImGui::Text("Primary-world collision: %s / %zu triangles / %zu BVH nodes",
                    game.server.collision_ready ? "indexed" : "not loaded", game.collision.triangles(),
                    game.collision.nodes());
        const auto& work = game.collision.work();
        ImGui::Text("Since collision build: %llu node tests / %llu rejected",
                    static_cast<unsigned long long>(work.node_tests),
                    static_cast<unsigned long long>(work.node_rejections));
        ImGui::Text("Triangle bounds: %llu tests / %llu rejected / %llu full SAT tests",
                    static_cast<unsigned long long>(work.triangle_bounds_tests),
                    static_cast<unsigned long long>(work.triangle_rejections),
                    static_cast<unsigned long long>(work.triangle_tests));
        ImGui::TextWrapped(
            "Continuous axis-aligned boxes, sliding and object touch callbacks. Selected translating/rotating "
            "world models and player locomotion are active. These counters cover primary-world queries, "
            "including placement; they exclude object boxes and separate world-model indices.");
        ImGui::Text("Gravity: %.2f %.2f %.2f", game.server.gravity.x, game.server.gravity.y, game.server.gravity.z);
        if (ImGui::BeginTable("World console variables", 2,
                              ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 300)))
        {
            ImGui::TableSetupColumn("Variable");
            ImGui::TableSetupColumn("Original value");
            ImGui::TableHeadersRow();
            for (const auto& [name, value] : game.server.variables())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", value.text.c_str());
            }
            ImGui::EndTable();
        }
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_weapon_sound(const CWeapon& weapon, const GameSession& game)
    {
        if (weapon.data.m_szProjectileClass)
        {
            ImGui::Text("Original projectile alive: %s",
                        BaseClass::GetServerDE()->IsObjectAlive(weapon.projectile) ? "yes" : "no");
            if (!weapon.projectile_error.empty())
                ImGui::TextWrapped("Projectile: %s", weapon.projectile_error.c_str());
        }
        if (const auto* tesla = dynamic_cast<const CTeslaCannon*>(&weapon))
        {
            ImGui::Text("Tesla alternate count %u / lightning %llu", tesla->numAltFires,
                        (unsigned long long)tesla->lightning);
            if (!tesla->lightning_error.empty())
                ImGui::TextWrapped("%s", tesla->lightning_error.c_str());
        }
        if (const auto* drudge = dynamic_cast<const CDrudgeLightning*>(&weapon))
        {
            ImGui::Text("Drudge lightning %llu", (unsigned long long)drudge->lightning);
            if (!drudge->effect_error.empty())
                ImGui::TextWrapped("%s", drudge->effect_error.c_str());
        }
        if (const auto* shield = dynamic_cast<const CGideonShield*>(&weapon))
        {
            ImGui::Text("Shield particle effect %llu (historical); no weapon armor mutation",
                        (unsigned long long)shield->effect);
            if (!shield->effect_error.empty())
                ImGui::TextWrapped("Shield: %s", shield->effect_error.c_str());
        }
        if (const auto* naga = dynamic_cast<const CNagaEyes*>(&weapon))
        {
            ImGui::Text("Original Naga beam: %llu | visual ray %d | endpoint %.2f %.2f %.2f",
                        (unsigned long long)naga->beam, int(naga->beam_collision), naga->beam_end.x, naga->beam_end.y,
                        naga->beam_end.z);
            ImGui::TextWrapped("Visual ray ignores owner class; ordinary damage ray does not.");
            if (!naga->beam_error.empty())
                ImGui::TextWrapped("Beam failure: %s", naga->beam_error.c_str());
            if (!naga->explosion_error.empty())
                ImGui::TextWrapped("Pending: %s", naga->explosion_error.c_str());
        }
        if (ImGui::TreeNodeEx("Original vector impacts", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Last shot %zu contacts | total %llu | sounds %llu | failed %llu", weapon.impacts.size(),
                               (unsigned long long)weapon.impact_contacts, (unsigned long long)weapon.impact_sounds,
                               (unsigned long long)weapon.impact_sound_failures);
            if (!weapon.impact_error.empty())
                ImGui::TextWrapped("Impact failure: %s", weapon.impact_error.c_str());
            for (unsigned i = 0; i < weapon.impacts.size(); ++i)
            {
                const auto& impact = weapon.impacts[i];
                const char* branch = "object";
                switch (impact.branch)
                {
                    case WeaponImpactBranch::World:
                        branch = "world";
                        break;
                    case WeaponImpactBranch::Brush:
                        branch = "brush";
                        break;
                    case WeaponImpactBranch::Door:
                        branch = "door";
                        break;
                    case WeaponImpactBranch::Volume:
                        branch = "volume";
                        break;
                    case WeaponImpactBranch::Other:
                        break;
                }
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Contact", "%u: %s / material %u / %s", i + 1, branch, impact.surface,
                                    impact.continued ? "continue" : "stop"))
                {
                    const auto* object = game.objects.get(impact.hit.object);
                    const auto* voice = game.server.audio.get(impact.effect.sound);
                    ImGui::TextWrapped("Target: %s | stopping material %u | damage delivered %s",
                                       object ? object->state.m_Name
                                              : (impact.hit.object ? "removed" : "primary world"),
                                       impact.stopping_surface, impact.damage_delivered ? "yes" : "no");
                    ImGui::Text("Position %.2f %.2f %.2f", impact.hit.point.x, impact.hit.point.y, impact.hit.point.z);
                    ImGui::TextWrapped("Impact audio: %s / %s", impact.effect.resource.c_str(),
                                       voice ? (voice->done ? "complete" : "playing")
                                             : (impact.effect.sound ? "released" : "no voice"));
                    if (!impact.effect.error.empty())
                        ImGui::TextWrapped("Audio failure: %s", impact.effect.error.c_str());
                    ImGui::Text("Flash/smoke report %llu (details in Sprites)",
                                (unsigned long long)impact.effect.sprite_event);
                    if (!impact.effect.sprite_error.empty())
                        ImGui::TextWrapped("Visual failure: %s", impact.effect.sprite_error.c_str());
                    ImGui::TextWrapped(
                        "Blood roll %d / requested %d | events %llu, %llu | %s", impact.effect.blood_roll,
                        impact.effect.blood_requested, (unsigned long long)impact.effect.blood_events[0],
                        (unsigned long long)impact.effect.blood_events[1], impact.effect.blood_error.c_str());
                    ImGui::TextWrapped("Sparks requested %d / large metal %d | event %llu | %s",
                                       impact.effect.sparks_requested, impact.effect.sparks_big,
                                       (unsigned long long)impact.effect.spark_event,
                                       impact.effect.spark_error.c_str());
                    ImGui::Text("Impact hearing: %u deliveries / %s", impact.alert.delivered,
                                impact.alert.complete ? "complete" : "not completed");
                    if (!impact.alert.error.empty())
                        ImGui::TextWrapped("Hearing failure: %s", impact.alert.error.c_str());
                    if (impact.target_removed)
                        ImGui::TextUnformatted("Target removed by hearing before damage.");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TextWrapped("Ordered historical contacts; resolve handles before inspection. Wood/stone flash and "
                               "smoke are connected; remaining impact FX, decals and reverb are unavailable.");
            ImGui::TreePop();
        }
        if (!ImGui::TreeNodeEx("Original weapon fire audio", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::TextWrapped("Attempts %llu / played %llu / failed %llu / evicted %llu / released %llu",
                           (unsigned long long)weapon.sound_attempts, (unsigned long long)weapon.sounds_played,
                           (unsigned long long)weapon.sound_failures, (unsigned long long)weapon.sound_evictions,
                           (unsigned long long)weapon.sounds_released);
        ImGui::TextWrapped("Last resource: %s", weapon.fire_sound_resource.c_str());
        if (!weapon.fire_sound_error.empty())
            ImGui::TextWrapped("Audio failure: %s", weapon.fire_sound_error.c_str());
        const auto& alert = weapon.fire_sound_alert;
        ImGui::Text("Gunfire alert: %u deliveries / %s", alert.delivered,
                    alert.complete ? "complete" : "not completed");
        if (!alert.error.empty())
            ImGui::TextWrapped("Alert failure: %s", alert.error.c_str());
        for (unsigned i = 0; i < weapon.fire_sounds.size(); ++i)
        {
            const auto id = weapon.fire_sounds[i];
            const auto* voice = game.server.audio.get(id);
            ImGui::TextWrapped("Slot %u: %llu / %s / expected end %.3f", i, (unsigned long long)id,
                               voice ? (voice->done ? "complete" : "playing") : (id ? "unresolved" : "empty"),
                               weapon.fire_sound_ends[i]);
            if (voice)
                ImGui::TextWrapped("  Cursor %.0f / %zu frames; priority %u; attached spatial / dry reverb",
                                   voice->cursor, voice->clip->frames(), unsigned(voice->request.priority));
        }
        ImGui::TextWrapped(
            "Playback and hearing are separate results. View-weapon SOUND_KEY references do not start another voice.");
        ImGui::TreePop();
    }
    void Inspector::draw_weapons(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextUnformatted(
            "Original inventory / Beretta / Mac-10 / Flare / M16 / Knife / Proximity + Remote + Time Bombs");
        auto* player = game.server_shell.Player(game.local_client);
        if (!player)
        {
            if (ImGui::Button("Enter player and play"))
                enter_player_request = true;
            return;
        }
        auto& inventory = player->m_InventoryMgr;
        ImGui::TextWrapped("T selects collected time bombs (five-second fuse after first contact).");
        ImGui::TextWrapped("P selects collected proximity bombs. Hold primary to charge, then release; holding over "
                           "1.25 seconds explodes in hand. Secondary drops a bomb.");
        ImGui::TextWrapped(
            "R selects collected remote bombs; B detonates all registered charges, regardless of selected weapon or "
            "ammo. Primary charges/throws, secondary drops. Remotes have no overhold explosion.");
        ImGui::Text("Remote list %u / commands %llu / signalled %llu",
                    player->remote_bombs ? player->remote_bombs->m_nInList : 0,
                    (unsigned long long)player->remote_commands, (unsigned long long)player->remote_signals);
        if (!player->remote_error.empty())
            ImGui::TextWrapped("Remote error: %s", player->remote_error.c_str());
        if (ImGui::Button("Detonate remote bombs"))
            detonate_request = true;
        if (inventory_bomb(game.client_shell.weapons[0].type))
            ImGui::Text("Bomb charge %.3f s / speed %.1f / ammo %.0f", game.client_shell.weapons[0].bomb_held,
                        game.client_shell.weapons[0].bomb_speed,
                        inventory.GetAmmoCount(game.client_shell.weapons[0].type == WEAP_PROXIMITYBOMB ? 10
                                               : game.client_shell.weapons[0].type == WEAP_REMOTEBOMB  ? 11
                                                                                                       : 12));
        ImGui::TextWrapped("Hold a fire button to drive original animation, shots, decoded fire sounds and AI hearing. "
                           "The model preview is diagnostic; remaining impact FX and audio reverb are unavailable.");
        if (!world_error.empty())
            ImGui::TextWrapped("%s", world_error.c_str());
        if (!ImGui::BeginTable("Weapon inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::BeginTable("Inventory slots", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Right hand");
            ImGui::TableSetupColumn("Left hand");
            ImGui::TableSetupColumn("Select");
            ImGui::TableHeadersRow();
            for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
            {
                const auto* right = inventory.GetWeapon(slot);
                const auto* left = inventory.GetWeapon(slot, true);
                if (!right && !left)
                    continue;
                ImGui::PushID(static_cast<int>(slot));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u%s", slot, inventory.m_nCurWeaponSlot == slot ? " *" : "");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(right ? right->data.m_szWeaponName : "empty");
                if (right)
                    ImGui::TextWrapped(
                        "Impacts %llu / sounds %llu / failures %llu", (unsigned long long)right->impact_contacts,
                        (unsigned long long)right->impact_sounds, (unsigned long long)right->impact_sound_failures);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(left ? left->data.m_szWeaponName : "empty");
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Equip"))
                    weapon_slot_request = static_cast<int>(slot);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::TextWrapped("Bullets %.0f / %.0f | Health %.1f | Armor %.1f | Last applied damage %.2f",
                           inventory.GetAmmoCount(1), inventory.MaxAmmo(1), player->m_damage.m_fHitPoints,
                           player->m_damage.m_fArmorPoints, player->m_damage.m_fLastDamageAmount);
        if (ImGui::CollapsingHeader("All ammo pools / inspection mutations"))
        {
            if (ImGui::BeginTable("Ammo pools", 3))
            {
                for (DBYTE type = 1; type <= 12; ++type)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%u: %.1f / %.1f", type, inventory.GetAmmoCount(type), inventory.MaxAmmo(type));
                }
                ImGui::EndTable();
            }
            if (ImGui::Button("Add 50 bullets (inspection)"))
                add_ammo_request = true;
            ImGui::SameLine();
            if (ImGui::Button("Obtain second Beretta (inspection)"))
                add_beretta_request = true;
            if (ImGui::Button("Obtain Mac-10 (inspection)"))
                add_mac10_request = true;
            if (ImGui::Button("Obtain M16 (inspection)"))
                add_assault_request = true;
            ImGui::SameLine();
            if (ImGui::Button("Add 20 M16 alternate charges (inspection)"))
                add_charges_request = true;
            if (ImGui::Button("Obtain Flare Gun (inspection)"))
                add_flare_request = true;
            ImGui::SameLine();
            if (ImGui::Button("Add 20 flares (inspection)"))
                add_flares_request = true;
            ImGui::TextWrapped(
                "These diagnostic grants call inventory directly. Beretta/Mac-10/Flare Gun/M16 world pickups also use "
                "original collection rules; chrome and muzzle flashes remain unavailable.");
        }
        auto aim = game.client_shell.rotation;
        if (ImGui::SliderAngle("Inspection yaw", &aim.yaw, -180, 180) |
            ImGui::SliderAngle("Inspection pitch", &aim.pitch, -85, 85))
            weapon_aim_request = aim;
        ImGui::Checkbox("Use left hand", &weapon_left);
        WeaponInput input;
        input.left = weapon_left;
        ImGui::Button("Hold primary fire");
        input.primary = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::Button("Hold alternate fire");
        input.alternate = ImGui::IsItemActive();
        weapon_input_request = input;
        auto* weapon = inventory.CurrentWeapon(weapon_left);
        const auto& view = game.client_shell.weapons[weapon_left ? 1 : 0];
        if (!weapon)
        {
            ImGui::TextUnformatted("No weapon in this hand.");
            ImGui::EndTable();
            return;
        }
        ImGui::TableNextColumn();
        const auto& definition = weapon->data;
        ImGui::TextWrapped("Damage %.0f-%.0f / alt %.0f-%.0f | Range %.0f | Ammo %u | Shots %llu",
                           definition.m_fMinDamage, definition.m_fMaxDamage, definition.m_fMinAltDamage,
                           definition.m_fMaxAltDamage, definition.m_fRange, definition.m_nAmmoType,
                           static_cast<unsigned long long>(weapon->shots));
        ImGui::TextWrapped("State %u | FIRE_KEY events %llu | Client shots %llu", static_cast<unsigned>(view.state),
                           static_cast<unsigned long long>(view.fire_keys),
                           static_cast<unsigned long long>(view.shots));
        ImGui::Text("Reload %.2f / %.2f s | Loop primary %s / alternate %s", definition.m_fReloadTime,
                    definition.m_fAltReloadTime, definition.m_bLoopAnim ? "yes" : "no",
                    definition.m_bAltLoopAnim ? "yes" : "no");
        if (view.model && animation_index(*view.model, "alt_fire") == no_animation)
            ImGui::TextUnformatted("Original alternate animation fallback: primary start/fire/end clips.");
        ImGui::Text("Ammo pools: primary %u (%.0f) / alternate %u (%.0f)", weapon->GetAmmoType(), weapon->CheckAmmo(),
                    weapon->GetAmmoType(true), weapon->CheckAmmo(true));
        if (view.model && view.model->invalid_key_bounds)
            ImGui::Text("ABC warning: %u unused key bounds have inverted ordering", view.model->invalid_key_bounds);
        draw_weapon_sound(*weapon, game);
        if (const auto* voodoo = dynamic_cast<const CVoodooDoll*>(weapon))
        {
            ImGui::Text("Voodoo: next variant %u | last effect %u | damage type %u | deliveries %u",
                        unsigned(view.voodoo_hit), voodoo->hit_effect, unsigned(voodoo->damage_type),
                        voodoo->deliveries);
            ImGui::TextUnformatted("Last-shot targets (historical handles; self damage is counted separately):");
            for (auto handle : voodoo->targets)
            {
                const auto* target = game.objects.get(handle);
                ImGui::Text("%s", target ? target->state.m_Name : "removed object");
            }
            if (!voodoo->voodoo_error.empty())
                ImGui::TextWrapped("Voodoo failed: %s", voodoo->voodoo_error.c_str());
        }
        if (view.model && view.animation.animation < view.model->animations.size())
        {
            const auto& model = *view.model;
            const auto& clip = model.animations[view.animation.animation];
            ImGui::TextWrapped("ABC: %zu vertices / %zu nodes / %zu clips | %s: %.1f / %u ms", model.vertices.size(),
                               model.nodes.size(), model.animations.size(), clip.name.c_str(), view.animation.time,
                               clip.duration);
            if (ImGui::CollapsingHeader("Resource paths"))
                ImGui::TextWrapped("%s | skin indexed: %s",
                                   weapon_left ? definition.m_szLeftViewModelFilename
                                               : definition.m_szViewModelFilename,
                                   definition.m_szViewModelSkin);
            std::vector<DVector> vertices;
            if (sample_vertices(model, view.animation.animation, view.animation.time, vertices) && !vertices.empty())
            {
                const auto origin = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x, height = 145;
                ImGui::InvisibleButton("Weapon pose", {width, height});
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(origin, {origin.x + width, origin.y + height}, IM_COL32(12, 17, 23, 255));
                auto lo = vertices.front(), hi = lo;
                for (auto v : vertices)
                {
                    lo.x = std::min(lo.x, v.x);
                    lo.y = std::min(lo.y, v.y);
                    lo.z = std::min(lo.z, v.z);
                    hi.x = std::max(hi.x, v.x);
                    hi.y = std::max(hi.y, v.y);
                    hi.z = std::max(hi.z, v.z);
                }
                const auto scale =
                    std::min(width / std::max(1.0f, hi.z - lo.z), height / std::max(1.0f, hi.y - lo.y)) * .85f;
                auto project = [&](DVector v)
                {
                    return ImVec2(origin.x + width / 2 + (v.z - (lo.z + hi.z) / 2) * scale,
                                  origin.y + height / 2 - (v.y - (lo.y + hi.y) / 2) * scale);
                };
                draw->PushClipRect(origin, {origin.x + width, origin.y + height}, true);
                for (const auto& face : model.faces)
                    for (unsigned i = 0; i < 3; ++i)
                        draw->AddLine(project(vertices[face.vertices[i]]),
                                      project(vertices[face.vertices[(i + 1) % 3]]), IM_COL32(126, 193, 210, 160));
                draw->PopClipRect();
            }
        }
        if (!view.pending_sound.empty())
        {
            ImGui::Text("View/model-key sound reference indexed: %s (see server fire voices above)",
                        view.sound_indexed ? "yes" : "no");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", view.pending_sound.c_str());
        }
        if (weapon->shots || weapon->last_shot.result == FireResult::Dry)
        {
            const auto& shot = weapon->last_shot;
            const auto* hit = game.objects.get(shot.hit.object);
            ImGui::TextWrapped("Last shot: %s | damage %.2f | seed %u | %s",
                               shot.result == FireResult::Dry     ? "dry"
                               : shot.result == FireResult::Fired ? "fired"
                                                                  : "failed",
                               shot.damage, shot.request.seed, shot.request.alternate ? "alternate" : "primary");
            if (shot.interrupted)
                ImGui::TextUnformatted("Shot interrupted before impact: owner removed by hearing callback.");
            ImGui::TextWrapped("Hit: %s | damage delivered %s",
                               hit                                    ? hit->type->name.c_str()
                               : shot.hit.object                      ? "removed object"
                               : shot.collision == SegmentResult::Hit ? "world"
                                                                      : "none",
                               shot.damage_delivered ? "yes" : "no");
            if (shot.collision == SegmentResult::Hit)
            {
                ImGui::Text("Fraction %.4f", shot.hit.fraction);
                if (!shot.hit.object)
                {
                    ImGui::SameLine();
                    ImGui::Text("| world polygon %u", shot.hit.polygon);
                }
            }
            if (hit && ImGui::SmallButton("Inspect hit object"))
                selection.id = hit->id;
            ImGui::TextWrapped("From %.2f %.2f %.2f | direction %.3f %.3f %.3f", shot.request.position.x,
                               shot.request.position.y, shot.request.position.z, shot.direction.x, shot.direction.y,
                               shot.direction.z);
        }
        if (ImGui::CollapsingHeader("Animation clips and keys") && view.model)
            for (const auto& clip : view.model->animations)
            {
                ImGui::Text("%s (%u ms)", clip.name.c_str(), clip.duration);
                for (const auto& key : clip.keys)
                    if (!key.text.empty())
                        ImGui::Text("  %u: %s", key.time, key.text.c_str());
            }
        ImGui::EndTable();
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_containers(GameSession& game, ObjectSelection& selection)
    {
        auto* player = game.server_shell.Player(game.local_client);
        ImGui::TextUnformatted("Original volumes / liquids, ladders, conveyors and damage");
        if (ImGui::CollapsingHeader("All PolyGrid surfaces / failures"))
        {
            ImGui::TextUnformatted("High detail / always update; independent server surface lifetime.");
            for (const auto& i : game.server.polygrids.active)
            {
                const auto& f = i.effect;
                ImGui::Text("#%llu | %u x %u | %s | frame %u | updates %llu", (unsigned long long)f.id, f.width,
                            f.width, f.visible ? "visible" : "hidden", f.track.frame, (unsigned long long)f.updates);
                ImGui::TextWrapped("%s", f.settings.sprite.c_str());
                if (!f.texture_error.empty())
                    ImGui::TextWrapped("Zero-area texture lookup: %s", f.texture_error.c_str());
            }
            for (const auto& report : game.server.polygrids.reports)
                if (!report.error.empty())
                    ImGui::TextWrapped("%s: %s", report.source.c_str(), report.error.c_str());
            if (polygrid_view)
                for (const auto& e : polygrid_view->entries)
                    if (!e.error.empty())
                        ImGui::TextWrapped("Texture %s: %s", e.texture.c_str(), e.error.c_str());
        }
        if (!world_error.empty())
            ImGui::TextWrapped("%s", world_error.c_str());
        ImGui::TextWrapped(
            "Space: up/jump | Ctrl: down/crouch | Shift: faster | Arrows: look. Hidden volumes stay listed.");
        if (player)
        {
            const auto& state = player->containers;
            const auto& move = game.client_shell.move_mgr;
            ImGui::Text("Air %.1f / 100 | head code %u", player->m_fAirLevel, unsigned(state.head.code));
            ImGui::Text("Body %zu / %zu | liquid %s | ladder %s | conveyor %s | gravity %s", state.count, state.total,
                        move.m_bBodyInLiquid ? "yes" : "no", move.m_bBodyOnLadder ? "yes" : "no",
                        move.m_bBodyOnConveyor ? "yes" : "no", move.m_bUseGravity ? "yes" : "no");
            ImGui::Text("Surface swim %s | swim jump %s | speed %.0f / ladder %.0f",
                        move.m_bSwimmingOnSurface ? "yes" : "no", move.m_bSwimmingJump ? "yes" : "no", move.m_fSwimVel,
                        move.m_fLadderVel);
            ImGui::Text("Health %.1f | armor %.1f | level %.2fs | protected volume hits %llu",
                        player->m_damage.m_fHitPoints, player->m_damage.m_fArmorPoints, game.server.GetWorldTime(),
                        static_cast<unsigned long long>(player->protected_volume_hits));
            if (ImGui::CollapsingHeader("Membership / player position"))
            {
                for (std::size_t i = 0; i < state.count; ++i)
                    if (const auto* r = game.objects.get(state.body[i].object))
                        ImGui::TextDisabled("Body[%zu] %s / code %u / %s", i, r->state.m_Name,
                                            unsigned(state.body[i].code), state.body[i].hidden ? "hidden" : "active");
                auto position = game.client_shell.position;
                float coordinates[]{position.x, position.y, position.z};
                if (ImGui::InputFloat3("Inspect player position", coordinates, "%.1f",
                                       ImGuiInputTextFlags_EnterReturnsTrue))
                    player_position_request = DVector{coordinates[0], coordinates[1], coordinates[2]};
            }
        }
        else if (ImGui::Button("Enter original player"))
            enter_player_request = true;
        ImGui::Text("Bound container index: %zu (slot ordered; includes pending destruction)",
                    game.objects.bound_container_count());
        if (!ImGui::BeginTable("Volumes", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextColumn();
        unsigned count = 0;
        for (auto id : game.objects.container_snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || !r->container() || r->life != Life::Alive)
                continue;
            const auto* brush = dynamic_cast<const VolumeBrush*>(r->game.get());
            if (!brush)
                continue;
            if (!game.objects.get(container_selection))
                container_selection = id;
            ++count;
            ImGui::PushID(static_cast<int>(id.slot));
            const std::string label =
                std::string(r->state.m_Name) + " / " + r->type->name + (brush->info.hidden ? " [hidden]" : "");
            if (ImGui::Selectable(label.c_str(), container_selection == id))
            {
                container_selection = id;
                selection.id = id;
            }
            ImGui::PopID();
        }
        if (!count)
            ImGui::TextWrapped("No live supported containers in this world. CityHub contains water and ladders. Failed "
                               "and missing classes remain listed in World.");
        const auto* r = game.objects.get(container_selection);
        const auto* brush = r ? dynamic_cast<const VolumeBrush*>(r->game.get()) : nullptr;
        if (r && brush && r->container())
        {
            const auto& shape = *r->container();
            ImGui::TextUnformatted("Volume + player body / front (+Y up)");
            const auto origin = ImGui::GetCursorScreenPos();
            const ImVec2 size{std::max(160.0f, ImGui::GetContentRegionAvail().x), 100};
            ImGui::InvisibleButton("Container geometry", size);
            const ImVec2 end{origin.x + size.x, origin.y + size.y};
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, end, IM_COL32(12, 17, 23, 255));
            draw->PushClipRect(origin, end, true);
            const auto mid = vector_scale(vector_add(shape.min, shape.max), .5f);
            const auto scale = std::min((size.x - 20) / std::max(1.0f, shape.max.x - shape.min.x),
                                        (size.y - 20) / std::max(1.0f, shape.max.y - shape.min.y));
            auto project = [&](DVector p)
            {
                return ImVec2{origin.x + size.x * .5f + (p.x - mid.x) * scale,
                              origin.y + size.y * .5f - (p.y - mid.y) * scale};
            };
            const auto color = brush->info.hidden              ? IM_COL32(130, 130, 130, 150)
                               : brush->info.code == CC_LADDER ? IM_COL32(255, 190, 80, 190)
                                                               : IM_COL32(80, 180, 255, 190);
            for (const auto& triangle : shape.triangles)
                for (unsigned i = 0; i < 3; ++i)
                    draw->AddLine(project(triangle[i]), project(triangle[(i + 1) % 3]), color);
            if (player)
            {
                const auto* body = game.objects.get(player->m_hObject);
                const auto p = body->state.m_Pos, d = body->dimensions;
                draw->AddRect(project({p.x - d.x, p.y + d.y, p.z}), project({p.x + d.x, p.y - d.y, p.z}),
                              IM_COL32_WHITE);
                draw->AddCircleFilled(project({p.x, p.y + player->eye_level, p.z}), 3, IM_COL32(140, 255, 160, 255));
            }
            draw->PopClipRect();
        }
        ImGui::TableNextColumn();
        if (r && brush && r->container())
        {
            const auto& shape = *r->container();
            ImGui::Text("%s | code %u | %s", r->state.m_Name, unsigned(brush->info.code),
                        brush->info.hidden ? "hidden" : "active");
            if (ImGui::Button("Trigger ON / OFF"))
                container_toggle_request = r->id;
            ImGui::SameLine();
            if (ImGui::Button("Inspect properties"))
                selection.id = r->id;
            ImGui::Text("Current %.1f %.1f %.1f", brush->info.current.x, brush->info.current.y, brush->info.current.z);
            ImGui::Text("Liquid gravity %.0f | viscosity %.2f", brush->info.gravity, brush->info.viscosity);
            ImGui::Text("Damage %.1f / type %u", brush->damage, unsigned(brush->damage_type));
            ImGui::Text("Server physics %llu | damage messages %llu",
                        static_cast<unsigned long long>(brush->physics_calls),
                        static_cast<unsigned long long>(brush->damage_events));
            if (dynamic_cast<const Conveyor*>(brush))
                ImGui::TextWrapped("Player: ladder braking/cap, gravity retained. Server: supported characters only; "
                                   "original Hidden exception retained. No volume damage.");
            if (const auto* mine = dynamic_cast<const Minefield*>(brush))
            {
                ImGui::TextWrapped(
                    "Minefield: player or supported character; self-attributed Death damage. Hidden does not disarm.");
                ImGui::Text("Explosions %llu", static_cast<unsigned long long>(mine->explosions));
                if (!mine->error.empty())
                    ImGui::TextWrapped("Failure: %s", mine->error.c_str());
            }
            if (dynamic_cast<const FreeFall*>(brush))
                ImGui::TextWrapped("FreeFall: zero liquid gravity, no air depletion; source damage remains active.");
            ImGui::Text("%zu triangles | %s closed shell", shape.triangles.size(),
                        shape.convex ? "convex" : "nonconvex");
            ImGui::Text("%zu closed components | %zu shared edges", shape.closed_components, shape.shared_edges);
            ImGui::Text("Fog %s | near %.0f / far %.0f", brush->fog ? "requested" : "off", brush->fog_near,
                        brush->fog_far);
            ImGui::Text("Surface %s | height %.1f | alpha %.2f", brush->show_surface ? "requested" : "off",
                        brush->surface_height, brush->surface_alpha);
            ImGui::TextWrapped("SPR: %s", brush->sprite.c_str());
            if (!brush->surface_error.empty())
                ImGui::TextWrapped("Surface failure: %s", brush->surface_error.c_str());
            const auto* child = game.objects.get(brush->surface_object);
            ImGui::Text("Independent PolyGrid: %s", child && child->life == Life::Alive ? "alive" : "absent/removed");
            for (const auto& i : game.server.polygrids.active)
                if (i.owner == brush->surface_object)
                {
                    const auto& fx = i.effect;
                    ImGui::Text("Grid %u x %u | %s | updates %llu", fx.width, fx.width,
                                fx.visible ? "visible" : "hidden", static_cast<unsigned long long>(fx.updates));
                    ImGui::Text("Position %.1f %.1f %.1f | scale %.3f %.3f", fx.pose.position.x, fx.pose.position.y,
                                fx.pose.position.z, fx.scale[0], fx.scale[1]);
                    ImGui::Text("Frame %u | plasma %u | alpha %.2f", fx.track.frame, fx.settings.plasma,
                                fx.settings.alpha);
                    if (polygrid_view)
                        for (const auto& e : polygrid_view->entries)
                            if (e.id == fx.id)
                            {
                                ImGui::Text("Prepared triangles: %u",
                                            e.range == UINT32_MAX ? 0 : polygrid_view->mesh.ranges[e.range].count / 3);
                                if (!e.error.empty())
                                    ImGui::TextWrapped("Texture failure: %s", e.error.c_str());
                            }
                }
            ImGui::Text("World PolyGrids: %zu active | %llu failed", game.server.polygrids.active.size(),
                        static_cast<unsigned long long>(game.server.polygrids.failed));
            if (polygrid_render)
                ImGui::Text("Previous GPU surface record: %zu surface batches", polygrid_render->alpha_batches);
            if (brush->locked)
                ImGui::TextWrapped("Key lock pending: %s (remove key %s)", brush->unlock_key.c_str(),
                                   brush->remove_key ? "yes" : "no");
        }
        ImGui::EndTable();
        if (ImGui::CollapsingHeader("Container limits / pending effects"))
            ImGui::TextWrapped("Static closed DAT shells; four body containers in stable object order. PolyGrid "
                               "surfaces use high-detail, always-update simulation. Underwater "
                               "fog/audio/color effects, key handshakes, crane "
                               "controls, minefields and moving volume transforms are not implemented. Queries and "
                               "movement use the real shell, not its bounding box.");
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_doors(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original Door / RotatingDoor rules. Commands use original object messages; movers push "
                           "bodies and carry standing players. Failed/unported entries remain in World.");
        if (!ImGui::BeginTable("Doors", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextColumn();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive || !dynamic_cast<const Door*>(r->game.get()))
                continue;
            if (!game.objects.get(door_selection))
                door_selection = id;
            ImGui::PushID(static_cast<int>(id.slot));
            const auto label = std::string(r->state.m_Name) + ((r->state.m_Flags & FLAG_VISIBLE) ? "" : " [hidden]");
            if (ImGui::Selectable(label.c_str(), door_selection == id))
                door_selection = selection.id = id;
            ImGui::PopID();
        }
        const auto* r = game.objects.get(door_selection);
        const auto* d = r && r->life == Life::Alive ? dynamic_cast<const Door*>(r->game.get()) : nullptr;
        if (d && r->world_model)
        {
            const auto& shape = *r->world_model;
            const auto offset = vector_subtract(r->state.m_Pos, shape.origin);
            const auto* rotating = dynamic_cast<const RotatingDoor*>(d);
            const auto mid = rotating ? r->state.m_Pos : vector_scale(vector_add(d->closed_pos, d->open_pos), .5f);
            const ImVec2 size{std::max(180.0f, ImGui::GetContentRegionAvail().x), 140};
            ImGui::TextUnformatted("Transformed shell + travel / top view (X/Z)");
            const auto canvas = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("Door geometry", size);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(canvas, {canvas.x + size.x, canvas.y + size.y}, IM_COL32(12, 17, 23, 255));
            draw->PushClipRect(canvas, {canvas.x + size.x, canvas.y + size.y}, true);
            const float scale = std::min(
                (size.x - 20) / std::max(1.0f, shape.max.x - shape.min.x + std::abs(d->open_pos.x - d->closed_pos.x)),
                (size.y - 20) / std::max(1.0f, shape.max.z - shape.min.z + std::abs(d->open_pos.z - d->closed_pos.z)));
            const auto bounds_mid = vector_scale(vector_add(shape.min, shape.max), .5f);
            const auto view_mid = vector_add(bounds_mid, vector_subtract(mid, shape.origin));
            auto project = [&](DVector p)
            {
                return ImVec2{canvas.x + size.x * .5f + (p.x - view_mid.x) * scale,
                              canvas.y + size.y * .5f - (p.z - view_mid.z) * scale};
            };
            for (const auto& triangle : shape.triangles)
                for (unsigned i = 0; i < 3; ++i)
                    draw->AddLine(project(vector_add(triangle[i], offset)),
                                  project(vector_add(triangle[(i + 1) % 3], offset)), IM_COL32(80, 180, 255, 190));
            draw->AddLine(project(d->closed_pos), project(d->open_pos), IM_COL32(255, 190, 80, 255), 2);
            draw->AddCircleFilled(project(r->state.m_Pos), 3, IM_COL32_WHITE);
            if (rotating)
            {
                draw->AddCircle(project(rotating->rotation_point), 5, IM_COL32(255, 100, 160, 255));
                draw->AddLine(project(rotating->rotation_point), project(r->state.m_Pos), IM_COL32(255, 100, 160, 255));
            }
            if (const auto* player = game.objects.get(game.client_shell.player))
                draw->AddCircleFilled(project(player->state.m_Pos), 4, IM_COL32(140, 255, 160, 255));
            draw->PopClipRect();
        }
        ImGui::TableNextColumn();
        if (d && r->world_model)
        {
            constexpr const char* states[] = {"invalid", "closed", "closing", "open", "opening"};
            ImGui::Text("%s / %s", r->state.m_Name, states[d->state]);
            if (ImGui::Button("TRIGGER"))
            {
                door_trigger_request = r->id;
                door_close_request = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("TRIGGERCLOSE"))
            {
                door_trigger_request = r->id;
                door_close_request = true;
            }
            ImGui::Text("Speed %.1f / close %.1f %s | wave %u", d->speed, d->closing_speed,
                        dynamic_cast<const RotatingDoor*>(d) ? "rad/s" : "units/s", d->waveform);
            ImGui::Text("Distance %.1f | delay %.2f | wait %.2f", d->move_dist, d->open_delay, d->wait_time);
            ImGui::Text("Pivot %.2f %.2f %.2f", r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z);
            ImGui::Text(
                "Move %llu | blocked %llu | pushes %llu | rider moves %llu",
                static_cast<unsigned long long>(r->mover_moves), static_cast<unsigned long long>(r->mover_blocks),
                static_cast<unsigned long long>(r->mover_pushes), static_cast<unsigned long long>(r->mover_riders));
            constexpr const char* results[] = {"moved", "blocked", "invalid", "unavailable"};
            ImGui::Text("Last command: %s", results[static_cast<unsigned>(d->last_move)]);
            if (r->mover_blocks)
            {
                const auto* blocker = game.objects.get(r->mover_blocker);
                ImGui::Text("Last blocker: %s", blocker ? blocker->state.m_Name : "world / removed");
            }
            ImGui::Text("Collision: %s | %zu shell triangles", d->box_physics ? "authored box" : "polygon",
                        r->world_model->triangles.size());
            ImGui::Text("%zu closed components | %zu shared edges", r->world_model->closed_components,
                        r->world_model->shared_edges);
            if (const auto* rotating = dynamic_cast<const RotatingDoor*>(d))
            {
                ImGui::Text("Euler radians %.3f %.3f %.3f | substeps %u", rotating->angles.x, rotating->angles.y,
                            rotating->angles.z, r->rotation_steps);
                ImGui::Text("RotationAngles degrees %.1f %.1f %.1f", rotating->rotation_angles.x,
                            rotating->rotation_angles.y, rotating->rotation_angles.z);
                ImGui::Text("RotationPoint %.2f %.2f %.2f", rotating->rotation_point.x, rotating->rotation_point.y,
                            rotating->rotation_point.z);
                ImGui::Text("PushPlayerBack %s", rotating->push_player_back ? "yes" : "no");
                ImGui::TextWrapped("Source CalcAngle applies two increments; rotation collision uses a bounded "
                                   "replacement solver. Speed above is radians/s.");
            }
            ImGui::Text("TriggerClosed %s | RemainsOpen %s | StartOpen %s", d->trigger_closed ? "yes" : "no",
                        d->remains_open ? "yes" : "no", d->start_open ? "yes" : "no");
            ImGui::Text("FireThrough %s | triggers %llu", d->fire_through ? "yes" : "no",
                        static_cast<unsigned long long>(d->triggers));
            const auto* child = game.objects.get(d->trigger);
            const auto* trigger =
                child && child->life == Life::Alive ? dynamic_cast<const Trigger*>(child->game.get()) : nullptr;
            ImGui::Text("SelfTrigger: %s", trigger           ? (trigger->active ? "active" : "inactive")
                                           : d->self_trigger ? "missing"
                                                             : "off");
            if (trigger)
            {
                ImGui::Text("%s | key: %s", trigger->locked ? "locked" : "unlocked", trigger->key_name.c_str());
                if (ImGui::Button("Inspect SelfTrigger"))
                {
                    trigger_selection = child->id;
                    select_triggers = true;
                }
            }
            ImGui::TextWrapped("Audio request: %s (%s)", d->last_sound.c_str(), d->sound_loop ? "loop" : "once");
            ImGui::Text("Sound attempts %llu | started %llu | failed %llu", (unsigned long long)d->sound_requests,
                        (unsigned long long)d->sound_starts, (unsigned long long)d->sound_failures);
            ImGui::Text("Retained loop %llu | last voice %llu", (unsigned long long)d->loop_voice,
                        (unsigned long long)d->last_voice);
            if (const auto* voice = game.server.audio.get(d->last_voice))
                ImGui::Text("Decoded frames %zu | cursor %.1f | wraps %llu | position %.1f %.1f %.1f",
                            static_cast<std::size_t>(voice->clip->frames()), voice->cursor,
                            (unsigned long long)voice->loops, voice->position.x, voice->position.y, voice->position.z);
            else
                ImGui::TextUnformatted("Last voice is absent, completed or removed.");
            ImGui::TextWrapped("%s", d->sound_error.c_str());
            ImGui::TextWrapped("Attached sound uses original 500/1500 radii; SoundRadius is not used by StartSound. "
                               "Reverb and priority are requested metadata; playback is dry.");
            ImGui::TextWrapped("Portal: %s / requested %s (visibility pending)", d->portal.c_str(),
                               d->portal_open ? "open" : "closed");
        }
        else
            ImGui::TextUnformatted("No supported live Door selected.");
        ImGui::EndTable();
        ImGui::TextWrapped("Bounded translating/rotating collision. Crushing damage, chained pushers, "
                           "portals and save/restore remain pending. M2 is not complete.");
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_triggers(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped("Original Trigger / BigTrigger / ToggleTrigger. Hidden sensors and door-owned SelfTriggers; "
                           "source failures remain in World.");
        if (!ImGui::BeginTable("Trigger inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 230);
        ImGui::TableSetupColumn("Properties");
        ImGui::TableNextColumn();
        ImGui::BeginChild("Trigger list", ImVec2(0, 265));
        for (auto id : game.objects.snapshot())
        {
            auto* r = game.objects.get(id);
            auto* t = r && r->life == Life::Alive ? dynamic_cast<Trigger*>(r->game.get()) : nullptr;
            if (!t)
                continue;
            if (!game.objects.get(trigger_selection))
                trigger_selection = id;
            auto* owner = game.objects.get(t->owner_door);
            std::string label = owner ? std::string(owner->state.m_Name) + " [SelfTrigger]" : r->state.m_Name;
            label += t->active ? "" : " [inactive]";
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), id == trigger_selection))
                trigger_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (const auto* p = game.server_shell.Player(game.local_client))
        {
            ImGui::Text("Player keys: %zu", p->m_InventoryMgr.keys.size());
            for (const auto& k : p->m_InventoryMgr.keys)
                ImGui::BulletText("%s: %u uses", k.name.c_str(), unsigned(k.uses));
        }
        else
            ImGui::TextUnformatted("Enter player in Session to test access.");
        ImGui::TableNextColumn();
        auto* r = game.objects.get(trigger_selection);
        auto* t = r && r->life == Life::Alive ? dynamic_cast<Trigger*>(r->game.get()) : nullptr;
        if (t)
        {
            ImGui::Text("%s #%u | %s | %s", r->type->name.c_str(), r->id.slot, t->active ? "active" : "inactive",
                        t->locked ? "locked" : "unlocked");
            ImGui::Text("Position %.1f %.1f %.1f | half dims %.1f %.1f %.1f", r->state.m_Pos.x, r->state.m_Pos.y,
                        r->state.m_Pos.z, r->dimensions.x, r->dimensions.y, r->dimensions.z);
            ImGui::Text("Touch %d | Player %d | AI %d | Object %d | Relay %d", t->touch_activate, t->player_activate,
                        t->ai_activate, t->object_activate, t->relay_activate);
            ImGui::Text("Named filter %d: %s", t->named_activate, t->activation_name.c_str());
            ImGui::Text("Count %llu / %u | fired %llu | rejected %llu", (unsigned long long)t->current_activation,
                        t->activation_count, (unsigned long long)t->activations, (unsigned long long)t->rejected);
            ImGui::Text("Reset %.3f s | timer %s", t->reset_time, r->scheduled ? "scheduled" : "off");
            ImGui::Text("Key %s | queries %llu", t->key_name.c_str(), (unsigned long long)t->key_queries);
            if (auto* toggle = dynamic_cast<ToggleTrigger*>(t))
                ImGui::Text("Toggle state: %s", toggle->on ? "on" : "off");
            const auto* sender = game.objects.get(t->last_sender);
            ImGui::Text("Last sender: %s | command: %s",
                        sender && sender->life == Life::Alive ? sender->state.m_Name : "none / removed",
                        t->last_command.c_str());
            for (const auto* command : {"TRIGGER", "RESET", "LOCK", "UNLOCK"})
            {
                if (ImGui::Button(command))
                {
                    trigger_request = r->id;
                    trigger_command = command;
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            if (ImGui::Button("Debug: grant one matching key"))
            {
                trigger_request = r->id;
                grant_key_request = true;
            }
            ImGui::TextWrapped("Commands retain original filters. Inactive triggers ignore RESET/LOCK/UNLOCK too. Key "
                               "queries consume a use. Debug grant bypasses pickup.");
            if (ImGui::BeginTable("Targets", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                for (const auto* h : {"Slot", "Target / matched", "Message / off", "Delay / pending", "Sends"})
                    ImGui::TableSetupColumn(h);
                ImGui::TableHeadersRow();
                for (unsigned i = 0; i < t->slots.size(); ++i)
                {
                    const auto& s = t->slots[i];
                    if (s.target.empty() && s.message.empty() && s.off_message.empty() && !s.delay)
                        continue;
                    unsigned matches = 0;
                    for (auto id : game.objects.snapshot())
                        if (auto* o = game.objects.get(id); o && o->life == Life::Alive && s.target == o->state.m_Name)
                            ++matches;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", i + 1);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s / %u", s.target.c_str(), matches);
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s / %s", s.message.c_str(), s.off_message.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f / %s", s.delay, s.pending ? "yes" : "no");
                    ImGui::TableNextColumn();
                    ImGui::Text("%llu", (unsigned long long)s.sent);
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Activation / locked / unlocked: %s / %s / %s", t->activation_sound.c_str(),
                               t->locked_sound.c_str(), t->unlocked_sound.c_str());
            ImGui::TextWrapped("Last sound: %s", t->last_sound.c_str());
            ImGui::Text("Sound attempts %llu | started %llu | failed %llu", (unsigned long long)t->sound_requests,
                        (unsigned long long)t->sound_starts, (unsigned long long)t->sound_failures);
            const auto* sound_owner = game.objects.get(t->last_sound_owner);
            ImGui::Text("Attachment: %s | radius %.1f | priority %u | voice %llu",
                        sound_owner && sound_owner->life == Life::Alive ? sound_owner->state.m_Name : "absent",
                        t->last_sound_radius, unsigned(t->last_sound_priority), (unsigned long long)t->last_voice);
            if (const auto* voice = game.server.audio.get(t->last_voice))
                ImGui::Text("Decoded frames %zu | cursor %.1f | sampled position %.1f %.1f %.1f",
                            static_cast<std::size_t>(voice->clip->frames()), voice->cursor, voice->position.x,
                            voice->position.y, voice->position.z);
            else
                ImGui::TextUnformatted("Last voice is absent, completed or removed.");
            ImGui::TextWrapped("%s", t->sound_error.c_str());
            ImGui::TextWrapped("Attached one-shots may overlap. Playback is dry; reverb/priority arbitration and "
                               "Action-mode console feedback remain unavailable.");
        }
        else
            ImGui::TextUnformatted("No live Trigger selected.");
        ImGui::EndTable();
        if (ImGui::CollapsingHeader("World key pickups"))
            for (auto id : game.objects.snapshot())
            {
                auto* r = game.objects.get(id);
                auto* k = r && r->life == Life::Alive ? dynamic_cast<KeyPickup*>(r->game.get()) : nullptr;
                if (!k)
                    continue;
                ImGui::TextWrapped("%s: %s (%u uses), model %s, resource #%d; %s", r->state.m_Name,
                                   k->item.name.c_str(), unsigned(k->item.uses), r->model ? "loaded" : "missing",
                                   k->resource_id, k->error.c_str());
                ImGui::TextWrapped("Pickup target: %s / %s | icons %s / %s", k->pickup_target.c_str(),
                                   k->pickup_command.c_str(), k->item.icon.c_str(), k->item.highlighted_icon.c_str());
            }
        ImGui::TextWrapped("Collection sounds and removed key records are in Pickups. Trigger activation audio, "
                           "localized key names and complete gameplay remain pending.");
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_brushes(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original CDestructableBrush / RotatingBrush / WorldModelDebris. Commands are queued to the simulation. "
            "Failed source entries and authored properties remain in World. CDebris requests are inspectable below.");
        ImGui::Text("Explosion requests: %zu | debris requests: %zu (FX/audio pending)",
                    game.server.explosion_effects.size(), game.server.debris_effects.size());
        if (!game.server.explosion_effects.empty())
        {
            const auto& event = game.server.explosion_effects.back();
            ImGui::Text("Last: %s | damage %.0f / radius %.0f | delivered %u/%u%s", event.source.c_str(), event.damage,
                        event.radius, event.targets.delivered, event.targets.candidates,
                        event.targets.cancelled ? " [cancelled]" : "");
        }
        if (!ImGui::BeginTable("Brush inspection", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableSetupColumn("Objects", ImGuiTableColumnFlags_WidthFixed, 240);
        ImGui::TableSetupColumn("State");
        ImGui::TableNextColumn();
        ImGui::BeginChild("Brush list", {0, 120});
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive || !dynamic_cast<const CDestructableBrush*>(r->game.get()))
                continue;
            if (!game.objects.get(brush_selection))
                brush_selection = id;
            auto label = std::string(r->state.m_Name) + ((r->state.m_Flags & FLAG_VISIBLE) ? "" : " [hidden]") +
                         (r->activity == OBJSTATE_ACTIVE ? "" : " [inactive]");
            ImGui::PushID(static_cast<int>(id.slot));
            if (ImGui::Selectable(label.c_str(), brush_selection == id))
                brush_selection = selection.id = id;
            ImGui::PopID();
        }
        ImGui::EndChild();
        const auto* r = game.objects.get(brush_selection);
        const auto* b = r && r->life == Life::Alive ? dynamic_cast<const CDestructableBrush*>(r->game.get()) : nullptr;
        if (b && r->world_model)
        {
            const auto& shape = *r->world_model;
            const ImVec2 size{std::max(180.0f, ImGui::GetContentRegionAvail().x), 180};
            ImGui::TextUnformatted("Posed shell / XZ");
            const auto canvas = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("Brush geometry", size);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(canvas, {canvas.x + size.x, canvas.y + size.y}, IM_COL32(12, 17, 23, 255));
            draw->PushClipRect(canvas, {canvas.x + size.x, canvas.y + size.y}, true);
            const auto center = vector_scale(vector_add(shape.min, shape.max), .5f);
            const auto scale = std::min((size.x - 20) / std::max(1.0f, shape.max.x - shape.min.x),
                                        (size.y - 20) / std::max(1.0f, shape.max.z - shape.min.z));
            auto project = [&](DVector p)
            {
                return ImVec2{canvas.x + size.x * .5f + (p.x - center.x) * scale,
                              canvas.y + size.y * .5f - (p.z - center.z) * scale};
            };
            for (const auto& t : shape.triangles)
                for (unsigned i = 0; i < 3; ++i)
                    draw->AddLine(project(t[i]), project(t[(i + 1) % 3]), IM_COL32(80, 180, 255, 220));
            draw->AddCircleFilled(project(shape.origin), 4, IM_COL32(255, 190, 80, 255));
            draw->PopClipRect();
        }
        ImGui::TableNextColumn();
        if (b)
        {
            ImGui::Text("%s / %s", r->state.m_Name, r->type->name.c_str());
            const auto* debris = dynamic_cast<const WorldModelDebris*>(b);
            const auto* rotating = dynamic_cast<const RotatingBrush*>(b);
            if (debris)
            {
                ImGui::Text("World debris: %s | starts %u | landings %u",
                            debris->failed ? "failed"
                            : debris->on   ? "flying"
                                           : "off",
                            debris->starts, debris->landings);
                ImGui::Text("Initial velocity %.2f %.2f %.2f | gravity multiplier %.3f", debris->initial_velocity.x,
                            debris->initial_velocity.y, debris->initial_velocity.z, debris->gravity_multiplier);
                ImGui::TextWrapped("%s", debris->error.c_str());
            }
            ImGui::Text("Named debris starts %u | failures %u", b->world_debris_started, b->world_debris_failed);
            if (rotating || debris)
            {
                const auto& v = rotating ? rotating->rotating : debris->rotating;
                constexpr const char* states[] = {"off", "normal", "spin up", "spin down"};
                constexpr const char* results[] = {"moved", "blocked", "invalid", "unavailable"};
                ImGui::Text("%s | last motion %s", states[v.state], results[unsigned(v.last_move)]);
                for (auto cmd : {"ON", "OFF", "TOGGLE"})
                {
                    if (ImGui::Button(cmd))
                    {
                        brush_request = r->id;
                        brush_command = cmd;
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Text("Euler rad %.3f %.3f %.3f", v.pose_angles.x, v.pose_angles.y, v.pose_angles.z);
                ImGui::Text("Velocity rad/s %.3f %.3f %.3f", v.velocity.x, v.velocity.y, v.velocity.z);
                ImGui::Text("Spin time left %.3f %.3f %.3f", v.time_left.x, v.time_left.y, v.time_left.z);
                ImGui::Text("Substeps %u | moves %llu | blocks %llu", r->rotation_steps,
                            static_cast<unsigned long long>(r->mover_moves),
                            static_cast<unsigned long long>(r->mover_blocks));
                ImGui::Text("Pushes %llu | riders %llu", static_cast<unsigned long long>(r->mover_pushes),
                            static_cast<unsigned long long>(r->mover_riders));
                const auto* blocker = game.objects.get(r->mover_blocker);
                if (r->mover_blocks)
                    ImGui::Text("Blocker: %s", blocker ? blocker->state.m_Name : "world / removed");
                ImGui::TextWrapped("Sound: %s (%s; playback pending)", v.last_sound.c_str(),
                                   v.sound_loop ? "loop" : "once");
            }
            if (r->world_body)
            {
                constexpr const char* results[] = {"moved", "blocked", "invalid", "unavailable"};
                ImGui::Text("%s | pushable %s | priority %u",
                            r->body_properties.rotating_debris  ? "Dynamic debris bounds"
                            : r->body_properties.polygon_bounds ? "Dynamic polygon (conservative bounds)"
                                                                : "Dynamic box",
                            b->pushable ? "yes" : "no", unsigned(r->blocking_priority));
                ImGui::Text("Mass %.1f | friction %.3f | gravity %s", r->body_properties.mass,
                            r->body_properties.friction, r->state.m_Flags & FLAG_GRAVITY ? "yes" : "no");
                ImGui::Text("Position %.2f %.2f %.2f", r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z);
                ImGui::Text("Velocity %.2f %.2f %.2f", r->velocity.x, r->velocity.y, r->velocity.z);
                ImGui::Text("Acceleration %.2f %.2f %.2f", r->acceleration.x, r->acceleration.y, r->acceleration.z);
                MovementState support;
                const bool supported = game.server.GetMovementState(r->handle, support) && support.standing;
                ImGui::Text("Standing %s | last move %s | steps %llu", supported ? "yes" : "no",
                            results[unsigned(r->body_result)], static_cast<unsigned long long>(r->body_steps));
                ImGui::Text(
                    "Player pushes %llu | riders %llu | blocks %llu", static_cast<unsigned long long>(r->body_pushes),
                    static_cast<unsigned long long>(r->mover_riders), static_cast<unsigned long long>(r->mover_blocks));
                const auto* blocker = game.objects.get(r->mover_blocker);
                ImGui::Text("Blocker: %s", blocker ? blocker->state.m_Name : "world / none / removed");
                ImGui::Text("Damage impulses %llu | last %s (%.1f %.1f %.1f)",
                            static_cast<unsigned long long>(b->m_damage.damage_impulses),
                            b->m_damage.last_impulse_ok ? "accepted" : "rejected", b->m_damage.last_impulse.x,
                            b->m_damage.last_impulse.y, b->m_damage.last_impulse.z);
            }
            ImGui::Text("HP %.1f | dead %s | CanDamage %s", b->m_damage.m_fHitPoints,
                        b->m_damage.IsDead() ? "yes" : "no", b->m_damage.can_damage ? "yes" : "no");
            ImGui::Text("Death HP %.2f / delay %.3fs / raw dead %d", b->m_damage.m_fDeathHitPoints,
                        b->m_damage.m_fDeathDelay, b->m_damage.m_bDead);
            ImGui::Text("Destructable %s | TriggerDestroyOnly %s", b->destructable ? "yes" : "no",
                        b->m_damage.trigger_only ? "yes" : "no");
            ImGui::Text("Collision %s | FireThrough %s", r->state.m_Flags & FLAG_BOXPHYSICS ? "box" : "polygon",
                        b->fire_through ? "yes" : "no");
            ImGui::Text("Surface %u (authored %u) | debris amount %u / scale %.2f", b->GetSurfaceType(), b->surface,
                        unsigned(b->debris.amount), b->debris.scale);
            ImGui::Text("Exploding %s | effective damage 100 / radius 200", b->debris.exploding ? "yes" : "no");
            if (ImGui::TreeNode("Debris properties / resource status"))
            {
                for (unsigned i = 0; i < CDebris::names.size(); ++i)
                    ImGui::Text("%s: %s", CDebris::names[i], b->debris.materials[i] ? "yes" : "no");
                auto resource = [&](const char* label, const std::string& path)
                {
                    ImGui::TextWrapped("%s: %s (%s)", label, path.c_str(),
                                       path.empty()                ? "not authored"
                                       : game.resources.find(path) ? "indexed"
                                                                   : "MISSING");
                };
                resource("Model", b->debris.model);
                resource("Texture", b->debris.texture);
                resource("Sound", b->debris.sound);
                ImGui::TextWrapped(
                    "Custom flag: %s. Resources enable custom FX independently. Original ReadProp "
                    "ignores authored ExplodeDamage / DamageRadius; effective values above are preserved.",
                    b->debris.custom ? "yes" : "no");
                ImGui::TreePop();
            }
            if (ImGui::Button("KILL (diagnostic)"))
            {
                brush_request = r->id;
                brush_command = "KILL";
            }
        }
        else
            ImGui::TextUnformatted("No live brush selected.");
        ImGui::EndTable();
        ImGui::TextWrapped("Explosion damage is active. Debris/FX rendering, sound, world fragments, polygon dynamics, "
                           "Other SpawnObject classes and save/restore are pending. "
                           "Rotation uses replacement engine collision; M2 remains open.");
        if (ImGui::CollapsingHeader("Explosions: damage executed / FX and audio pending",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto& event : game.server.explosion_effects)
            {
                ImGui::Text("%s: damage %.0f / radius %.0f | messages %u/%u%s", event.source.c_str(), event.damage,
                            event.radius, event.targets.delivered, event.targets.candidates,
                            event.targets.cancelled ? " [cancelled]" : "");
                ImGui::PushID(&event);
                if (ImGui::TreeNode("Request details"))
                {
                    ImGui::Text("Origin %.1f %.1f %.1f | type %u | in radius %u", event.position.x, event.position.y,
                                event.position.z, event.type, event.targets.in_radius);
                    ImGui::TextWrapped("Sound %s (%s; playback pending)", event.sound.c_str(),
                                       game.resources.find(event.sound) ? "indexed" : "MISSING");
                    ImGui::TextUnformatted("Delivered messages include protected/dead receivers; no visibility test.");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        if (ImGui::TreeNode("Destruction debris (wood, metal, glass; latest 64 requests)"))
        {
            for (const auto& event : game.server.debris_effects)
            {
                ImGui::Text("%s: flags 0x%x / amount %u / scale %.2f", event.source.c_str(), event.flags,
                            unsigned(event.amount), event.scale);
                ImGui::Text("Velocity %.1f %.1f %.1f | dims %.1f %.1f %.1f", event.velocity.x, event.velocity.y,
                            event.velocity.z, event.dimensions.x, event.dimensions.y, event.dimensions.z);
                ImGui::Text("Client event %llu", static_cast<unsigned long long>(event.event));
                if (!event.error.empty())
                    ImGui::TextWrapped("%s", event.error.c_str());
                for (const auto* path : {&event.model, &event.texture, &event.sound})
                    if (!path->empty())
                        ImGui::TextWrapped("%s (%s)", path->c_str(),
                                           game.resources.find(*path) ? "indexed" : "MISSING");
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Live destruction fragments"))
        {
            const auto& debris = game.server.destruction_debris;
            ImGui::Text("%zu models (80 live + 50 fading maximum)", debris.active.size());
            for (const auto& fragment : debris.active)
            {
                ImGui::PushID(static_cast<int>(fragment.id));
                if (ImGui::TreeNode("fragment", "%llu: %s", static_cast<unsigned long long>(fragment.id),
                                    fragment.model_path.c_str()))
                {
                    ImGui::Text("Position %.1f %.1f %.1f | velocity %.1f %.1f %.1f", fragment.position.x,
                                fragment.position.y, fragment.position.z, fragment.velocity.x, fragment.velocity.y,
                                fragment.velocity.z);
                    ImGui::Text("Alpha %.3f | rest %s | fade %u | impacts %llu", fragment.alpha,
                                fragment.resting ? "yes" : "no", fragment.fade_updates,
                                static_cast<unsigned long long>(fragment.impacts));
                    ImGui::TextWrapped("Skin %s | sound %s", fragment.skin_path.c_str(), fragment.sound_path.c_str());
                    if (!fragment.error.empty())
                        ImGui::TextWrapped("Motion: %s", fragment.error.c_str());
                    if (!fragment.sound_error.empty())
                        ImGui::TextWrapped("Audio: %s", fragment.sound_error.c_str());
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Brush destruction history"))
        {
            for (const auto& event : game.server.brush_destructions)
                ImGui::Text("%s: damage %.1f, authored amount %u | world debris %u started / %u failed",
                            event.name.c_str(), event.damage, event.debris_amount, event.world_debris_started,
                            event.world_debris_failed);
            ImGui::TreePop();
        }
    }
} // namespace OpenBlood2
