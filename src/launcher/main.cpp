#include "build_info.h"
#include "engine/capture.h"
#include "launcher/controller.h"
#include "launcher/dialog.h"
#include "launcher/state.h"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <iostream>
#include <spdlog/spdlog.h>

namespace OpenBlood2::Launcher
{
    namespace fs = std::filesystem;
    struct Options
    {
        fs::path directory, game, data, capture;
        bool demo = false, smoke = false, launch_smoke = false, help = false;
        unsigned frames = 0;
        float scale = 0;
    };
    bool arguments(int argc, char** argv, Options& result, std::string& error)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg(argv[i]);
            if (arg == "--help")
                result.help = true;
            else if (arg == "--demo")
                result.demo = true;
            else if (arg == "--smoke")
                result.smoke = true;
            else if (arg == "--launch-smoke")
                result.launch_smoke = true;
            else if (arg == "--state-dir" || arg == "--game" || arg == "--data-dir" || arg == "--capture" ||
                     arg == "--frames" || arg == "--ui-scale")
            {
                if (++i >= argc)
                {
                    error = "Missing value for " + std::string(arg);
                    return false;
                }
                const std::string_view value(argv[i]);
                if (value.empty() || value.starts_with("--"))
                {
                    error = "Missing argument value";
                    return false;
                }
                if (arg == "--frames")
                {
                    const auto r = std::from_chars(value.data(), value.data() + value.size(), result.frames);
                    if (r.ec != std::errc{} || r.ptr != value.data() + value.size() || result.frames < 3 ||
                        result.frames > 10000)
                    {
                        error = "--frames requires 3..10000";
                        return false;
                    }
                }
                else if (arg == "--ui-scale")
                {
                    const auto r = std::from_chars(value.data(), value.data() + value.size(), result.scale);
                    if (r.ec != std::errc{} || r.ptr != value.data() + value.size() ||
                        !(result.scale >= .75f && result.scale <= 3.f))
                    {
                        error = "--ui-scale requires 0.75..3";
                        return false;
                    }
                }
                else
                {
                    if (!valid_text(value, 4096))
                    {
                        error = "Invalid path";
                        return false;
                    }
                    auto path = path_from_utf8(value);
                    if (!path.is_absolute())
                    {
                        error = "Launcher option paths must be absolute";
                        return false;
                    }
                    if (arg == "--state-dir")
                        result.directory = path;
                    else if (arg == "--game")
                        result.game = path;
                    else if (arg == "--data-dir")
                        result.data = path;
                    else
                        result.capture = path;
                }
            }
            else
            {
                error = "Unknown launcher argument: " + std::string(arg);
                return false;
            }
        }
        if (result.smoke && (result.directory.empty() || result.frames < 12))
        {
            error = "--smoke requires an isolated --state-dir and --frames >= 12";
            return false;
        }
        if (result.launch_smoke && (!result.smoke || result.data.empty() || result.game.empty()))
        {
            error = "--launch-smoke requires --smoke, --game and --data-dir";
            return false;
        }
        return true;
    }
    struct View
    {
        SDL_Window* window = nullptr;
        SDL_GPUDevice* gpu = nullptr;
        bool sdl = false, claimed = false, context = false, platform = false, renderer = false;
        std::unique_ptr<Capture> capture;
        float scale = 1;
        ~View()
        {
            if (gpu)
                SDL_WaitForGPUIdle(gpu);
            capture.reset();
            if (renderer)
                ImGui_ImplSDLGPU3_Shutdown();
            if (platform)
                ImGui_ImplSDL3_Shutdown();
            if (context)
                ImGui::DestroyContext();
            if (claimed)
                SDL_ReleaseWindowFromGPUDevice(gpu, window);
            if (gpu)
                SDL_DestroyGPUDevice(gpu);
            if (window)
                SDL_DestroyWindow(window);
            if (sdl)
                SDL_Quit();
        }
        void style(float value)
        {
            scale = value > 0 ? value : 1;
            ImGui::GetStyle() = ImGuiStyle{};
            ImGui::StyleColorsDark();
            auto& style = ImGui::GetStyle();
            style.FontSizeBase = 17;
            style.FontScaleDpi = scale;
            style.WindowPadding = ImVec2(24, 20);
            style.ItemSpacing = ImVec2(10, 12);
            style.FramePadding = ImVec2(10, 7);
            style.FrameRounding = 3;
            style.Colors[ImGuiCol_WindowBg] = ImVec4(.055f, .055f, .065f, 1);
            style.Colors[ImGuiCol_Button] = ImVec4(.40f, .08f, .10f, 1);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.58f, .12f, .14f, 1);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(.72f, .15f, .17f, 1);
            style.ScaleAllSizes(scale);
        }
        bool initialize(float requested)
        {
            SDL_SetAppMetadata("OpenBlood2 Launcher", build::version, "org.openblood2.launcher");
            if (!SDL_Init(SDL_INIT_VIDEO))
                return false;
            sdl = true;
            scale = requested > 0 ? requested : SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
            if (!(scale > 0))
                scale = 1;
            window = SDL_CreateWindow("Blood II - Launcher", int(960 * scale), int(640 * scale),
                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
            if (!window)
                return false;
            SDL_SetWindowMinimumSize(window, int(800 * scale), int(560 * scale));
            gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL |
                                          SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
                                      false, nullptr);
            if (!gpu)
                return false;
            claimed = SDL_ClaimWindowForGPUDevice(gpu, window);
            if (!claimed || !SDL_SetGPUSwapchainParameters(gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                                           SDL_GPU_PRESENTMODE_VSYNC))
                return false;
            SDL_PropertiesID props = SDL_GetGPUDeviceProperties(gpu);
            spdlog::info("Launcher GPU backend={} device={}", SDL_GetGPUDeviceDriver(gpu),
                         SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown"));
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            context = true;
            ImGui::GetIO().IniFilename = nullptr;
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            style(scale);
            platform = ImGui_ImplSDL3_InitForSDLGPU(window);
            if (!platform)
                return false;
            ImGui_ImplSDLGPU3_InitInfo info{};
            info.Device = gpu;
            info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu, window);
            info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
            info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
            info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
            renderer = ImGui_ImplSDLGPU3_Init(&info);
            return renderer;
        }
        bool present(const fs::path& screenshot)
        {
            ImGui::Render();
            auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
            if (!commands)
                return false;
            SDL_GPUTexture* swapchain = nullptr;
            Uint32 width = 0, height = 0;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &swapchain, &width, &height))
            {
                SDL_CancelGPUCommandBuffer(commands);
                return false;
            }
            if (!swapchain)
                return SDL_SubmitGPUCommandBuffer(commands);
            if (!screenshot.empty())
            {
                capture = std::make_unique<Capture>();
                if (!capture->initialize(gpu, SDL_GetGPUSwapchainTextureFormat(gpu, window), width, height))
                {
                    SDL_CancelGPUCommandBuffer(commands);
                    return false;
                }
            }
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
            SDL_GPUColorTargetInfo target{};
            target.texture = capture ? capture->texture : swapchain;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            target.clear_color = {.055f, .055f, .065f, 1};
            auto* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
            if (!pass)
            {
                SDL_CancelGPUCommandBuffer(commands);
                return false;
            }
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
            SDL_EndGPURenderPass(pass);
            if (capture && !capture->enqueue(commands, swapchain, width, height))
            {
                SDL_CancelGPUCommandBuffer(commands);
                return false;
            }
            if (!SDL_SubmitGPUCommandBuffer(commands))
                return false;
            if (capture)
            {
                const bool ok = capture->save(path_text(screenshot).c_str());
                capture.reset();
                return ok;
            }
            return true;
        }
        bool suspend()
        {
            // Retire presentation before unmapping the native window. Keep the
            // device/ImGui resources, but never reuse an old native swapchain.
            if (!SDL_WaitForGPUIdle(gpu))
                return false;
            SDL_ReleaseWindowFromGPUDevice(gpu, window);
            claimed = false;
            return SDL_HideWindow(window);
        }
        bool resume()
        {
            if (!SDL_ShowWindow(window))
                return false;
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
                SDL_RestoreWindow(window);
            SDL_RaiseWindow(window);
            claimed = SDL_ClaimWindowForGPUDevice(gpu, window);
            return claimed && SDL_SetGPUSwapchainParameters(gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                                            SDL_GPU_PRESENTMODE_VSYNC);
        }
    };
    void assign(std::array<char, 4097>& buffer, const fs::path& path)
    {
        const auto value = path_text(path);
        SDL_strlcpy(buffer.data(), value.c_str(), buffer.size());
    }
    std::string file_url(const fs::path& path)
    {
        const auto text = path_text(path);
        std::string url = text.starts_with('/') ? "file://" : "file:///";
        constexpr char hex[] = "0123456789ABCDEF";
        for (const unsigned char c : text)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == ':' ||
                c == '-' || c == '_' || c == '.' || c == '~')
                url += char(c);
            else
            {
                url += '%';
                url += hex[c >> 4];
                url += hex[c & 15];
            }
        }
        return url;
    }
    int run(const Options& options)
    {
        View view;
        if (!view.initialize(options.scale))
        {
            spdlog::error("Launcher UI: {}", SDL_GetError());
            return 3;
        }
        fs::path directory = options.directory;
        if (directory.empty())
        {
            char* value = SDL_GetPrefPath("OpenBlood2", "Launcher");
            if (!value)
            {
                spdlog::error("{}", SDL_GetError());
                return 2;
            }
            directory = path_from_utf8(value);
            SDL_free(value);
        }
        const auto state_file = directory / "launcher.cfg";
        State state;
        std::string error, notice;
        load_state(state_file, state, error);
        if (!options.game.empty())
            state.game = options.game;
        if (state.game.empty())
        {
            const char* base = SDL_GetBasePath();
            if (base)
                state.game = path_from_utf8(base) /
#ifdef _WIN32
                             "blood2.exe";
#else
                             "blood2";
#endif
        }
        std::array<char, 4097> game{}, data{};
        assign(game, state.game);
        bool demo = options.demo;
        if (state.selected >= 0)
        {
            assign(data, state.installations[state.selected].directory);
            demo = state.installations[state.selected].edition == Edition::Demo;
        }
        if (!options.data.empty())
        {
            assign(data, options.data);
            demo = options.demo;
        }
        if (options.demo)
            demo = true;
        Dialog dialog;
        Controller controller;
        auto remember = [&]() -> bool
        {
            State next = state;
            next.game = path_from_utf8(game.data());
            Installation item{demo ? Edition::Demo : Edition::Retail, path_from_utf8(data.data())};
            auto found = std::find(next.installations.begin(), next.installations.end(), item);
            next.selected = int(found - next.installations.begin());
            if (found == next.installations.end())
                next.installations.push_back(item);
            if (!save_state(state_file, next, error))
                return false;
            state = std::move(next);
            notice = "Installation remembered";
            return true;
        };
        auto prepare = [&]()
        {
            error.clear();
            notice.clear();
            controller.prepare({demo ? Edition::Demo : Edition::Retail, path_from_utf8(data.data())},
                               path_from_utf8(game.data()));
        };
        if (data[0])
            prepare();
        bool hidden_for_game = false;
        bool help = false;
        bool captured = false, smoke_started = false, smoke_finished = false;
        unsigned click_stage = 0;
        ImVec2 play_center{};
        bool quit = false;
        unsigned frame = 0;
        while (!quit && (!options.frames || frame < options.frames))
        {
            SDL_Event event;
            if (SDL_WaitEventTimeout(&event, 16))
            {
                do
                {
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                         event.window.windowID == SDL_GetWindowID(view.window)))
                        quit = true;
                    if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED && options.scale == 0)
                        view.style(SDL_GetWindowDisplayScale(view.window));
                } while (SDL_PollEvent(&event));
            }
            if (quit)
                break;
            DialogResult result;
            if (dialog.poll(result))
            {
                if (!result.error.empty())
                    error = result.error;
                else if (!result.path.empty())
                {
                    assign(result.executable ? game : data, path_from_utf8(result.path));
                    controller.cancel();
                }
            }
            controller.update();
            if (controller.status() == Controller::Status::Running)
            {
                smoke_started = true;
                if (!hidden_for_game)
                {
                    if (!view.suspend())
                    {
                        spdlog::error("Suspend launcher: {}", SDL_GetError());
                        return 3;
                    }
                    hidden_for_game = true;
                }
                continue;
            }
            if (hidden_for_game)
            {
                smoke_finished = controller.status() == Controller::Status::Finished;
                if (!view.resume())
                {
                    spdlog::error("Resume launcher: {}", SDL_GetError());
                    return 3;
                }
                hidden_for_game = false;
            }
            if (SDL_GetWindowFlags(view.window) & SDL_WINDOW_MINIMIZED)
                continue;
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            if (options.launch_smoke && controller.playable() && frame >= 6 && click_stage < 2)
            {
                // Diagnostic-only input uses the real Play widget and normal controller path.
                auto& io = ImGui::GetIO();
                io.AddFocusEvent(true);
                io.AddMousePosEvent(play_center.x, play_center.y);
                io.AddMouseButtonEvent(0, click_stage == 0);
                ++click_stage;
            }
            ImGui::NewFrame();
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("OpenBlood2", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(ImVec4(.85f, .19f, .21f, 1), "BLOOD II: THE CHOSEN");
            ImGui::TextDisabled("OpenBlood2 launcher");
            ImGui::Separator();
            ImGui::BeginDisabled(controller.busy());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##installation",
                                  state.selected < 0
                                      ? "Choose an installation"
                                      : path_text(state.installations[state.selected].directory).c_str()))
            {
                for (std::size_t i = 0; i < state.installations.size(); ++i)
                {
                    const auto& item = state.installations[i];
                    const auto label = std::string(item.edition == Edition::Demo ? "Demo - " : "Blood II - ") +
                                       path_text(item.directory);
                    if (ImGui::Selectable(label.c_str(), state.selected == int(i)))
                    {
                        state.selected = int(i);
                        demo = item.edition == Edition::Demo;
                        assign(data, item.directory);
                        controller.cancel();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SeparatorText("Game data");
            if (ImGui::RadioButton("Blood II", !demo))
            {
                demo = false;
                controller.cancel();
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Demo", demo))
            {
                demo = true;
                controller.cancel();
            }
            ImGui::SetNextItemWidth(-115 * view.scale);
            if (ImGui::InputText("##data", data.data(), data.size()))
                controller.cancel();
            ImGui::SameLine();
            ImGui::BeginDisabled(dialog.pending());
            if (ImGui::Button("Browse...##data"))
                dialog.open(view.window, false);
            ImGui::EndDisabled();
            ImGui::TextWrapped(
                "Select the directory containing BLOOD2.REZ or B2Demo.rez. Keep your original files in place.");
            if (ImGui::Button("Remember installation"))
                remember();
            ImGui::SameLine();
            ImGui::BeginDisabled(dialog.pending() || !data[0]);
            if (ImGui::Button("Check installation"))
                prepare();
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::BeginDisabled(!controller.playable() || dialog.pending());
            if (ImGui::Button("Play", ImVec2(190 * view.scale, 46 * view.scale)))
            {
                if (remember())
                    controller.play(directory);
            }
            play_center = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * .5f,
                                 (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * .5f);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled();
            ImGui::Button("Graphics");
            ImGui::SameLine();
            ImGui::Button("Mods");
            ImGui::SameLine();
            ImGui::Button("Install demo");
            ImGui::EndDisabled();
            switch (controller.status())
            {
                case Controller::Status::Querying:
                    ImGui::TextUnformatted("Checking game executable...");
                    break;
                case Controller::Status::Validating:
                    ImGui::TextUnformatted("Checking archives...");
                    break;
                case Controller::Status::Ready:
                case Controller::Status::Finished:
                    ImGui::TextColored(ImVec4(.4f, .8f, .5f, 1), "Ready to launch");
                    if (controller.status() == Controller::Status::Finished)
                        ImGui::TextUnformatted("The game exited normally.");
                    break;
                default:
                    ImGui::TextUnformatted("Check your installation before playing.");
                    break;
            }
            if (ImGui::CollapsingHeader("Game executable / diagnostics"))
            {
                ImGui::BeginDisabled(controller.busy());
                ImGui::SetNextItemWidth(-130 * view.scale);
                if (ImGui::InputText("##game", game.data(), game.size()))
                    controller.cancel();
                ImGui::SameLine();
                ImGui::BeginDisabled(dialog.pending());
                if (ImGui::Button("Browse...##game"))
                    dialog.open(view.window, true);
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::TextWrapped("Launcher state: %s", path_text(state_file).c_str());
                ImGui::TextWrapped("Choose the OpenBlood2 executable supplied with this launcher.");
                ImGui::Text("Game: %s / %s", controller.info().version.c_str(), controller.info().revision.c_str());
                ImGui::Text("Indexed resources: %zu in %zu archives", controller.content().resources,
                            controller.content().mounts);
                ImGui::TextWrapped("Index checks do not certify all game resources or campaign support.");
                for (const auto& archive : controller.content().metadata.archives)
                    ImGui::Text("%s: %s", archive.name.c_str(),
                                archive.name == "BLOOD2NM.REZ" ? "excluded"
                                : archive.present              ? "present"
                                                               : "absent (optional)");
                if (!controller.logs().empty())
                {
                    ImGui::TextWrapped("Game logs: %s", path_text(controller.logs()).c_str());
                    if (ImGui::Button("Open log folder") && !SDL_OpenURL(file_url(controller.logs()).c_str()))
                        error = SDL_GetError();
                }
            }
            if (!controller.error().empty())
                ImGui::TextWrapped("%s", controller.error().c_str());
            if (!error.empty())
                ImGui::TextWrapped("%s", error.c_str());
            if (!notice.empty())
                ImGui::TextWrapped("%s", notice.c_str());
            ImGui::Separator();
            ImGui::TextDisabled("%s | %s", build::version, build::revision);
            ImGui::SameLine();
            if (ImGui::Button("Help"))
                help = !help;
            ImGui::SameLine();
            if (ImGui::Button("Quit"))
                quit = true;
            if (help)
                ImGui::TextWrapped(
                    "Choose your original installation or demo folder, select the matching edition, then Check "
                    "installation and Play. For development builds, select the game executable under diagnostics. "
                    "Graphics, mods and demo installation are not available in this release.");
            ImGui::End();
            ++frame;
            const bool take_capture =
                !captured && frame >= 3 && !controller.busy() && (!options.launch_smoke || smoke_finished);
            if (!view.present(take_capture ? options.capture : fs::path{}))
            {
                spdlog::error("Presentation: {}", SDL_GetError());
                return 3;
            }
            if (take_capture)
                captured = true;
            if (options.smoke && frame == 5)
            {
                if (!SDL_SetWindowSize(view.window, int(1010 * view.scale), int(670 * view.scale)))
                    return 4;
                SDL_Event key{};
                key.type = SDL_EVENT_KEY_DOWN;
                key.key.windowID = SDL_GetWindowID(view.window);
                key.key.key = SDLK_TAB;
                key.key.scancode = SDL_SCANCODE_TAB;
                key.key.down = true;
                SDL_PushEvent(&key);
                key.type = SDL_EVENT_KEY_UP;
                key.key.down = false;
                SDL_PushEvent(&key);
            }
        }
        if (options.smoke)
        {
            int w = 0, h = 0;
            SDL_GetWindowSize(view.window, &w, &h);
            if (frame < 12 || w != int(1010 * view.scale) || h != int(670 * view.scale))
                return 4;
            if (options.launch_smoke && (!smoke_started || !smoke_finished))
            {
                spdlog::error("Play smoke did not complete: started={} finished={} status={} error={}", smoke_started,
                              smoke_finished, int(controller.status()), controller.error());
                return 4;
            }
            spdlog::info("Launcher UI smoke passed: frames={} scale={} resized={}x{}", frame, view.scale, w, h);
        }
        return 0;
    }
} // namespace OpenBlood2::Launcher
int main(int argc, char** argv)
{
    OpenBlood2::Launcher::Options options;
    std::string error;
    if (!OpenBlood2::Launcher::arguments(argc, argv, options, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    if (options.help)
    {
        std::cout << "OpenBlood2 Launcher\n  --state-dir ABS_PATH  Isolated launcher state\n"
                     "  --game ABS_PATH       Development game executable\n"
                     "  --data-dir ABS_PATH   Initial data directory\n  --demo                Initial demo edition\n"
                     "  --frames N            Quit after 3..10000 frames\n  --capture ABS_FILE    Capture settled UI\n"
                     "  --ui-scale SCALE      0.75..3 diagnostic scale\n"
                     "  --smoke               Isolated UI/resize smoke (requires --state-dir, --frames >= 12)\n"
                     "  --launch-smoke        Drive Play and return (requires --smoke, --game, --data-dir)\n";
        return 0;
    }
    return OpenBlood2::Launcher::run(options);
}
