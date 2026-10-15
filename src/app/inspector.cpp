#include "app/inspector.h"
#include "build_info.h"
#include "game/original/Key.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace blood2 {

bool Inspector::initialize(std::string& error) {
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
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
    if (scale > 0.0f) style.FontScaleDpi = scale;
    platform = ImGui_ImplSDL3_InitForSDLGPU(runtime.window);
    if (!platform) {
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
    if (!renderer) error = "Cannot initialize Dear ImGui SDL_GPU renderer backend";
    return renderer;
}

Inspector::~Inspector() {
    if (runtime.gpu) SDL_WaitForGPUIdle(runtime.gpu);
    if (renderer) ImGui_ImplSDLGPU3_Shutdown();
    if (platform) ImGui_ImplSDL3_Shutdown();
    if (context) ImGui::DestroyContext();
}

void Inspector::apply_commands(GameSession& game, ObjectSelection& selection) {
    if (remove_request) {
        if (const auto* object = game.objects.get(remove_request)) game.objects.remove(object->handle);
        remove_request = {};
    }
    if (!spawn_request.empty()) {
        if (const auto* object = game.objects.get(game.create_example(spawn_request))) selection.id = object->id;
        spawn_request.clear();
    }
}

void Inspector::draw(const Config& config, const DataStatus& data, bool& visible, std::uint64_t frames,
                     GameSession& game, ObjectSelection& selection) {
    if (!visible) return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display.x - 32, display.y - 32), ImGuiCond_Always);
    if (ImGui::Begin("Blood II | In-game inspector", &visible,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.43f, 1), "M1  /  ORIGINAL OBJECTS");
        ImGui::TextWrapped("Original CameraSpot and Key code is running. These diagnostic objects are not a loaded level.");
        ImGui::TextDisabled("F1: toggle inspector    |    Escape: quit when UI does not capture input");
        ImGui::Separator();
        if (ImGui::BeginTabBar("Inspection")) {
            if (ImGui::BeginTabItem("Objects")) {
                const auto ids = game.objects.snapshot();
                ImGui::Text("%zu live records  |  Time %.3f s  |  Strings %zu  |  Messages %zu",
                    ids.size(), game.objects.time(), game.server.string_count(), game.server.message_count());
                if (ImGui::Button("Spawn CameraSpot")) spawn_request = "CameraSpot";
                ImGui::SameLine();
                if (ImGui::Button("Spawn Key")) spawn_request = "Key";
                ImGui::SameLine();
                ImGui::BeginDisabled(!selection.id);
                if (ImGui::Button("Remove selected")) remove_request = selection.id;
                ImGui::EndDisabled();
                if (ImGui::BeginTable("Object list", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 120))) {
                    for (const char* label : {"ID : generation", "Name", "Original class", "Lifecycle", "Next update"}) ImGui::TableSetupColumn(label);
                    ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
                    for (auto id : ids) {
                        const auto* object = game.objects.get(id);
                        ImGui::PushID(static_cast<int>(id.slot));
                        ImGui::TableNextRow(); ImGui::TableNextColumn();
                        const auto label = std::to_string(id.slot) + " : " + std::to_string(id.generation);
                        if (ImGui::Selectable(label.c_str(), selection.id == id, ImGuiSelectableFlags_SpanAllColumns)) selection.id = id;
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(object->state.m_Name);
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(object->type->name.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(life_name(object->life));
                        ImGui::TableNextColumn();
                        if (object->scheduled) ImGui::Text("at %.4f s", object->due);
                        else ImGui::TextUnformatted("Disabled");
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                if (const auto* object = game.objects.get(selection.id)) {
                    const auto& state = object->state;
                    ImGui::SeparatorText("Selected object");
                    ImGui::Text("%s / %s   |   %s   |   Updates: %llu", object->type->name.c_str(), state.m_Name,
                        creation_name(object->reason), static_cast<unsigned long long>(object->updates));
                    ImGui::Text("Position %.2f, %.2f, %.2f   |   Scale %.2f, %.2f, %.2f", state.m_Pos.x, state.m_Pos.y, state.m_Pos.z,
                        state.m_Scale.x, state.m_Scale.y, state.m_Scale.z);
                    ImGui::Text("Rotation (quaternion) %.2f, %.2f, %.2f, %.2f", state.m_Rotation.m_Vec.x, state.m_Rotation.m_Vec.y,
                        state.m_Rotation.m_Vec.z, state.m_Rotation.m_Spin);
                    ImGui::Text("Type %u  |  Flags 0x%08X  |  Activity %u  |  Links %zu", static_cast<unsigned>(state.m_ObjectType),
                        state.m_Flags, object->activity, game.objects.link_count(object->handle));
                    if (state.m_Filename[0] || state.m_SkinName[0]) ImGui::TextWrapped("Resource requests (not loaded): %s / %s", state.m_Filename, state.m_SkinName);
                    if (const auto* key = dynamic_cast<const Key*>(object->game.get())) {
                        ImGui::Text("Original Key: timestamp %.3f  |  sound radius %.1f  |  loop %s", key->m_fTimeStamp, key->m_fSoundRadius, key->m_bLoopSound ? "yes" : "no");
                        const auto* target = game.server.GetStringData(key->m_hstrMessageTarget);
                        const auto* message = game.server.GetStringData(key->m_hstrMessageName);
                        ImGui::Text("Original strings: target=%s  message=%s", target ? target : "(null)", message ? message : "(null)");
                    }
                    if (ImGui::BeginTable("Creation properties", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 180))) {
                        ImGui::TableSetupColumn("Creation property (includes inherited defaults)"); ImGui::TableSetupColumn("Value");
                        ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
                        for (const auto& [name, value] : object->properties) {
                            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(name.c_str());
                            ImGui::TableNextColumn(); ImGui::TextUnformatted(property_text(value).c_str());
                        }
                        ImGui::EndTable();
                    }
                } else ImGui::TextDisabled("Select an object. Selection is cleared when its generation is destroyed.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Runtime")) {
                ImGui::Text("Renderer: %s    Window system: %s", SDL_GetGPUDeviceDriver(runtime.gpu), SDL_GetCurrentVideoDriver());
                ImGui::TextWrapped("Device: %s", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
                const int version = SDL_GetVersion();
                ImGui::Text("SDL %d.%d.%d (main)    Dear ImGui %s", SDL_VERSIONNUM_MAJOR(version),
                            SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version), IMGUI_VERSION);
                ImGui::Text("Presented frames: %llu    UI rate: %.1f fps", static_cast<unsigned long long>(frames), ImGui::GetIO().Framerate);
                ImGui::TextDisabled("OpenXR: not started. No headset is required.");
                ImGui::Spacing();
                ImGui::SeparatorText("External content");
                ImGui::TextWrapped("%s", path_text(config.data_dir).c_str());
                ImGui::TextDisabled("Configuration source: %s", config.source.c_str());
                if (!data.readme_version.empty()) ImGui::TextWrapped("Readme: %s", data.readme_version.c_str());
                if (ImGui::BeginTable("Archives", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Archive");
                    ImGui::TableSetupColumn("Size");
                    ImGui::TableSetupColumn("Status");
                    ImGui::TableHeadersRow();
                    for (const auto& archive : data.archives) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(archive.name.c_str());
                        ImGui::TableNextColumn();
                        if (archive.present) ImGui::Text("%.1f MiB", static_cast<double>(archive.bytes) / (1024 * 1024));
                        else ImGui::TextUnformatted("-");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(!archive.present ? "Absent (optional)" :
                            archive.name == "BLOOD2NM.REZ" ? "Detected; expansion excluded" : "Detected; not mounted");
                    }
                    ImGui::EndTable();
                }
                ImGui::TextWrapped("File checks only. Archive contents have not been parsed or loaded by this application.");
                if (ImGui::CollapsingHeader("Dependency revisions")) {
                    ImGui::TextWrapped("SDL: %s", build::sdl_revision);
                    ImGui::TextWrapped("ImGui: %s", build::imgui_revision);
                    ImGui::TextWrapped("spdlog: %s", build::spdlog_revision);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::TextWrapped("Most recent contract failures (up to 32). Unknown classes and properties are rejected.");
                for (const auto& diagnostic : game.objects.diagnostics()) ImGui::TextWrapped("%s", diagnostic.c_str());
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Resources (0)")) {
                ImGui::TextUnformatted("No archives are mounted and no game resources are loaded.");
                ImGui::TextWrapped("Detected files are listed in Runtime. M2 adds bounded readers, resource state and missing-content diagnostics.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

}
