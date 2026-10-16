#include "app/inspector.h"
#include "build_info.h"
#include "engine/container.h"
#include "engine/world_model.h"
#include "game/original/ClientLightFX.h"
#include "game/original/Door.h"
#include "game/original/Key.h"
#include "game/original/KeyPickup.h"
#include "game/original/RotatingBrush.h"
#include "game/original/RotatingDoor.h"
#include "game/original/Trigger.h"
#include "game/original/VolumeBrush.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <set>

namespace OpenBlood2
{

    bool Inspector::initialize(std::string& error)
    {
        IMGUI_CHECKVERSION();
        if (!ImGui::CreateContext())
        {
            error = "Cannot create Dear ImGui context";
            return false;
        }
        context = true;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.FontSizeBase = 17.0f;
        style.WindowRounding = 8.0f;
        style.FrameRounding = 4.0f;
        style.WindowPadding = ImVec2(20, 18);
        style.ItemSpacing = ImVec2(10, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.085f, 0.10f, 0.98f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.38f, 0.14f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TabSelected] = ImVec4(0.42f, 0.16f, 0.18f, 1.0f);
        const float scale = SDL_GetWindowDisplayScale(runtime.window);
        if (scale > 0.0f)
            style.FontScaleDpi = scale;
        platform = ImGui_ImplSDL3_InitForSDLGPU(runtime.window);
        if (!platform)
        {
            error = "Cannot initialize Dear ImGui SDL3 platform backend";
            return false;
        }
        ImGui_ImplSDLGPU3_InitInfo info{};
        info.Device = runtime.gpu;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
        renderer = ImGui_ImplSDLGPU3_Init(&info);
        if (!renderer)
            error = "Cannot initialize Dear ImGui SDL_GPU renderer backend";
        return renderer;
    }

    Inspector::~Inspector()
    {
        if (runtime.gpu)
            SDL_WaitForGPUIdle(runtime.gpu);
        if (renderer)
            ImGui_ImplSDLGPU3_Shutdown();
        if (platform)
            ImGui_ImplSDL3_Shutdown();
        if (context)
            ImGui::DestroyContext();
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
                record && record->container && record->life == Life::Alive)
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
        if (respawn_request)
        {
            if (auto* player = game.server_shell.Player(game.local_client))
            {
                if (!player->Respawn())
                    world_error = player->entry_error;
            }
            respawn_request = false;
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
            if (add_ammo_request)
                player->m_InventoryMgr.AddAmmo(AMMO_BULLET, 50);
            if (weapon_input_request)
                game.client_shell.weapon_input = *weapon_input_request;
            if (weapon_aim_request)
                game.client_shell.rotation = *weapon_aim_request;
            if (movement_input_request)
                game.client_shell.movement_input = *movement_input_request;
        }
        add_beretta_request = add_ammo_request = false;
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
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display.x - 32, display.y - 32), ImGuiCond_Always);
        if (ImGui::Begin("Blood II | In-game inspector", &visible,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.43f, 1),
                               game.world_path.empty() ? "ORIGINAL OBJECT DIAGNOSTICS" : "M2  /  WORLD INSPECTION");
            ImGui::TextWrapped("Original objects, ABC animation and primary-world collision. Full original player "
                               "session remains blocked.");
            ImGui::TextDisabled("F1: toggle inspector    |    Escape: quit when UI does not capture input");
            ImGui::Separator();
            if (ImGui::BeginTabBar("Inspection"))
            {
                if ((!game.resources.mounts().empty()) && ImGui::BeginTabItem("World"))
                {
                    draw_world(game, selection);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Objects"))
                {
                    const auto ids = game.objects.snapshot();
                    ImGui::Text("%zu live records  |  Time %.3f s  |  Strings %zu  |  Messages %zu", ids.size(),
                                game.objects.time(), game.server.string_count(), game.server.message_count());
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
                        if (state.m_Filename[0] || state.m_SkinName[0])
                            ImGui::TextWrapped("Model: %s / Skin (not decoded): %s", state.m_Filename,
                                               state.m_SkinName);
                        if (object->model)
                            ImGui::Text("Decoded ABC: %zu nodes / %zu animations; clip %u at %.1f ms",
                                        object->model->nodes.size(), object->model->animations.size(),
                                        object->animation.animation, object->animation.time);
                        if (!object->model_error.empty())
                            ImGui::TextWrapped("Model unavailable: %s", object->model_error.c_str());
                        ImGui::Text("Velocity %.2f %.2f %.2f / Grounded: %s", object->velocity.x, object->velocity.y,
                                    object->velocity.z, object->grounded ? "yes" : "no");
                        ImGui::Text("Contact normal %.2f %.2f %.2f / Polygon %u", object->contact_normal.x,
                                    object->contact_normal.y, object->contact_normal.z, object->contact_polygon);
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
                    ImGui::SeparatorText("Reached engine services (original object callbacks / teardown)");
                    for (const auto& [name, count] : game.server.service_calls)
                        ImGui::Text("%s: %llu", name.c_str(), static_cast<unsigned long long>(count));
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
                if (ImGui::BeginTabItem("Services"))
                {
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
        ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "Inspection / full gameplay blocked");
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
        ImGui::TextWrapped("Later archives override exact normalized paths. Loaded DAT and ABC data are identified "
                           "below. Texture/skin and sound payloads remain undecoded.");
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
            "Inspect the original entry/lifetime code. Beretta/knife inspection is in Weapons. Movement, full combat "
            "reactions, audio and autosave remain unported; this is not a playable session.");
        auto* player = game.server_shell.Player(game.local_client);
        if (!player)
        {
            if (ImGui::Button("Enter player inspection"))
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
        ImGui::SeparatorText("Shell state");
        ImGui::Text("Server world started: %s | Path snapshots: %zu | Updates: %llu",
                    game.server_shell.world_started ? "yes" : "no", game.server_shell.paths.size(),
                    static_cast<unsigned long long>(game.server_shell.updates));
        ImGui::Text("Clients: %zu | Client in world: %s | Messages: %llu | Updates: %llu", game.server.client_count(),
                    game.client_shell.m_bInWorld ? "yes" : "no",
                    static_cast<unsigned long long>(game.client_shell.messages),
                    static_cast<unsigned long long>(game.client_shell.updates));
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
            constexpr const char* ray_results[] = {"invalid", "miss", "hit", "unavailable"};
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
            ImGui::TextWrapped("Model: %s | Skin: %s (indexed, not decoded)", record->state.m_Filename,
                               record->state.m_SkinName);
            ImGui::TextWrapped("Requested slot 1: %u. Current slot: %u. Original inventory owns the Beretta/knife; "
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
        ImGui::SameLine();
        ImGui::Checkbox("Keyboard movement (F2)", &keyboard_movement);
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
        const auto* record = game.objects.get(player->m_hObject);
        ImGui::TextWrapped("Requested clip: %s", player->locomotion_requested_clip.c_str());
        ImGui::TextWrapped("Clip: %s | fallback %s", player->locomotion_clip.c_str(),
                           player->locomotion_requested_clip != player->locomotion_clip ? "yes" : "no");
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
            ImGui::TextWrapped(
                "Native coordinates; skeletal and vertex animation. Wireframe preview; skin is not decoded.");
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
        ImGui::Text("Primary-world collision: %s / %zu triangles / %zu BVH nodes",
                    game.server.collision_ready ? "indexed" : "not loaded", game.collision.triangles(),
                    game.collision.nodes());
        ImGui::TextWrapped(
            "Continuous axis-aligned boxes, sliding and object touch callbacks. Moving/rotating BSP "
            "models and crush response are not implemented. Grounded FLAG_STAIRSTEP and player locomotion are active.");
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
    void Inspector::draw_weapons(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextUnformatted("Original inventory / Beretta 92F / Chosen Knife");
        auto* player = game.server_shell.Player(game.local_client);
        if (!player)
        {
            if (ImGui::Button("Enter player inspection"))
                enter_player_request = true;
            return;
        }
        auto& inventory = player->m_InventoryMgr;
        ImGui::TextWrapped("Hold a fire button to drive the original client animation and server shot path. Preview is "
                           "wireframe; sound requests and impacts are inspected, not rendered or played.");
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
            for (unsigned slot = 0; slot < 10; ++slot)
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
            ImGui::TextWrapped("These call the inventory directly; world pickup classes are not yet connected.");
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
            ImGui::Text("Sound indexed: %s | playback unavailable", view.sound_indexed ? "yes" : "no");
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
        if (!world_error.empty())
            ImGui::TextWrapped("%s", world_error.c_str());
        ImGui::TextWrapped(
            "Space: up/jump | Ctrl: down/crouch | Shift: faster | Arrows: look. Hidden volumes stay listed.");
        if (player)
        {
            const auto& state = player->containers;
            const auto& move = game.client_shell.move_mgr;
            ImGui::Checkbox("Keyboard movement (F2)", &keyboard_movement);
            ImGui::SameLine();
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
        if (!ImGui::BeginTable("Volumes", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextColumn();
        unsigned count = 0;
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || !r->container || r->life != Life::Alive)
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
        if (r && brush && r->container)
        {
            const auto& shape = *r->container;
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
        if (r && brush && r->container)
        {
            const auto& shape = *r->container;
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
            if (dynamic_cast<const FreeFall*>(brush))
                ImGui::TextWrapped("FreeFall: zero liquid gravity, no air depletion; source damage remains active.");
            ImGui::Text("%zu triangles | %s closed shell", shape.triangles.size(),
                        shape.convex ? "convex" : "nonconvex");
            ImGui::Text("Fog %s | near %.0f / far %.0f", brush->fog ? "requested" : "off", brush->fog_near,
                        brush->fog_far);
            ImGui::Text("Surface %s | height %.1f | alpha %.2f", brush->show_surface ? "requested" : "off",
                        brush->surface_height, brush->surface_alpha);
            if (brush->locked)
                ImGui::TextWrapped("Key lock pending: %s (remove key %s)", brush->unlock_key.c_str(),
                                   brush->remove_key ? "yes" : "no");
        }
        ImGui::EndTable();
        if (ImGui::CollapsingHeader("Container limits / pending effects"))
            ImGui::TextWrapped("Static closed DAT shells; four body containers in stable object order. PolyGrid liquid "
                               "surfaces, underwater fog/audio/color effects, key handshakes, crane "
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
            ImGui::TextWrapped("Audio request: %s (%s, not played)", d->last_sound.c_str(),
                               d->sound_loop ? "loop" : "once");
            ImGui::TextWrapped("Portal: %s / requested %s (visibility pending)", d->portal.c_str(),
                               d->portal_open ? "open" : "closed");
        }
        else
            ImGui::TextUnformatted("No supported live Door selected.");
        ImGui::EndTable();
        ImGui::TextWrapped("Bounded translating/rotating collision. Crushing damage, chained pushers, "
                           "audio, portals and save/restore remain pending. M2 is not complete.");
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
            ImGui::TextWrapped("Last sound request: %s (not played)", t->last_sound.c_str());
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
        ImGui::TextWrapped("Audio, localized key names, generic pickups, save/restore and the complete playable "
                           "session remain pending.");
    }
} // namespace OpenBlood2

namespace OpenBlood2
{
    void Inspector::draw_brushes(GameSession& game, ObjectSelection& selection)
    {
        ImGui::TextWrapped(
            "Original CDestructableBrush / RotatingBrush. Commands are queued to the simulation. "
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
            if (const auto* rotating = dynamic_cast<const RotatingBrush*>(b))
            {
                const auto& v = rotating->rotating;
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
                ImGui::Text("Dynamic box | pushable %s | priority %u", b->pushable ? "yes" : "no",
                            unsigned(r->blocking_priority));
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
            ImGui::Text("Destructable %s | TriggerDestroyOnly %s", b->destructable ? "yes" : "no",
                        b->m_damage.trigger_only ? "yes" : "no");
            ImGui::Text("Collision %s | FireThrough %s", b->box_physics ? "box" : "polygon",
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
                                       : game.resources.find(path) ? "indexed, not loaded"
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
        if (ImGui::TreeNode("Debris FX requests (latest 64; not rendered)"))
        {
            for (const auto& event : game.server.debris_effects)
            {
                ImGui::Text("%s: flags 0x%x / amount %u / scale %.2f", event.source.c_str(), event.flags,
                            unsigned(event.amount), event.scale);
                ImGui::Text("Velocity %.1f %.1f %.1f | dims %.1f %.1f %.1f", event.velocity.x, event.velocity.y,
                            event.velocity.z, event.dimensions.x, event.dimensions.y, event.dimensions.z);
                for (const auto* path : {&event.model, &event.texture, &event.sound})
                    if (!path->empty())
                        ImGui::TextWrapped("%s (%s)", path->c_str(),
                                           game.resources.find(*path) ? "indexed, not loaded" : "MISSING");
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Destruction requests (not rendered)"))
        {
            for (const auto& event : game.server.brush_destructions)
                ImGui::Text("%s: damage %.1f, authored amount %u", event.name.c_str(), event.damage,
                            event.debris_amount);
            ImGui::TreePop();
        }
    }
} // namespace OpenBlood2
