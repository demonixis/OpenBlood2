#include "app/inspector.h"
#include "build_info.h"
#include "game/original/Key.h"

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
        ImGui::TextWrapped("WASD / ZQSD: walk | Shift: run | Space: jump | Arrows: look. F1 hides UI. Text editing and "
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
        movement_input_request = input;
        auto aim = client.rotation;
        if (ImGui::SliderAngle("Facing yaw", &aim.yaw, -180, 180))
            weapon_aim_request = aim;
        if (!ImGui::BeginTable("Movement diagnostics", 2, ImGuiTableFlags_Resizable))
            return;
        ImGui::TableNextColumn();
        ImGui::Text("Position  %.2f  %.2f  %.2f", body.position.x, body.position.y, body.position.z);
        ImGui::Text("Velocity  %.2f  %.2f  %.2f", body.velocity.x, body.velocity.y, body.velocity.z);
        ImGui::Text("Gravity  %.0f  %.0f  %.0f", move.physics.gravity.x, move.physics.gravity.y,
                    move.physics.gravity.z);
        ImGui::Text("Walk %.0f | Run %.0f | Jump %.0f", move.physics.move_speed, move.physics.move_speed * 1.5f,
                    move.physics.jump_speed);
        ImGui::Text("Horizontal speed %.2f | grounded %s", move.m_fVelMagnitude, move.m_bOnGround ? "yes" : "no");
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
        if (ImGui::CollapsingHeader("Motion details"))
        {
            ImGui::Text("Acceleration  %.1f  %.1f  %.1f", body.acceleration.x, body.acceleration.y,
                        body.acceleration.z);
            ImGui::Text("Half dimensions  %.1f  %.1f  %.1f", body.dimensions.x, body.dimensions.y, body.dimensions.z);
            ImGui::Text("Ground normal %.2f  %.2f  %.2f", body.ground_normal.x, body.ground_normal.y,
                        body.ground_normal.z);
            ImGui::Text("Controls 0x%04X | move code %u", move.m_dwControlFlags, move.m_ClientMoveCode);
            ImGui::Text("Step %.3f ms | discarded %.3f s", move.m_FrameTime * 1000, move.discarded_time);
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Local collision map / top view (+Z up)");
        ImGui::TextWrapped("Blue: world slice | orange: solid objects | white: player");
        const auto origin = ImGui::GetCursorScreenPos();
        const ImVec2 size{std::max(180.0f, ImGui::GetContentRegionAvail().x), 220};
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
                const auto p = r->state.m_Pos, d = r->dimensions;
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
            ImGui::TextWrapped("Standing dry-land subset. Stairs, crouch, liquids, ladders, moving platforms, body "
                               "animation selection and fall damage remain pending.");
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
        ImGui::TextWrapped("Continuous axis-aligned boxes, sliding and object touch callbacks. Moving/rotating BSP "
                           "models, stair stepping, crush response and original player movement are not implemented.");
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
