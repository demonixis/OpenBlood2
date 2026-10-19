#include "build_info.h"
#include "engine/capture.h"
#include "launcher/artwork.h"
#include "launcher/controller.h"
#include "launcher/demo_install.h"
#include "launcher/dialog.h"
#include "launcher/mods.h"
#include "launcher/state.h"
#include "launcher_font.h"
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
        fs::path directory, game, data, capture, install_smoke;
        bool demo = false, smoke = false, launch_smoke = false, navigation_smoke = false, help = false;
        unsigned frames = 0;
        std::string panel = "main";
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
            else if (arg == "--navigation-smoke")
                result.navigation_smoke = true;
            else if (arg == "--launch-smoke")
                result.launch_smoke = true;
            else if (arg == "--state-dir" || arg == "--game" || arg == "--data-dir" || arg == "--capture" ||
                     arg == "--frames" || arg == "--ui-scale" || arg == "--panel" || arg == "--install-smoke")
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
                if (arg == "--panel")
                {
                    if (value != "main" && value != "settings" && value != "mods")
                    {
                        error = "Unknown panel";
                        return false;
                    }
                    result.panel = value;
                }
                else if (arg == "--frames")
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
                    else if (arg == "--install-smoke")
                        result.install_smoke = path;
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
        if ((result.navigation_smoke || !result.install_smoke.empty()) && !result.smoke)
        {
            error = "UI action diagnostics require --smoke and isolated state";
            return false;
        }
        if (!result.install_smoke.empty() && result.game.empty())
        {
            error = "--install-smoke requires a test --game";
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
        SDL_GPUTexture* artwork = nullptr;
        bool sdl = false, claimed = false, context = false, platform = false, renderer = false;
        std::unique_ptr<Capture> capture;
        float scale = 1;
        ~View()
        {
            if (gpu)
                SDL_WaitForGPUIdle(gpu);
            capture.reset();
            if (artwork)
                SDL_ReleaseGPUTexture(gpu, artwork);
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
            ImGui::StyleColorsLight();
            auto& style = ImGui::GetStyle();
            style.FontSizeBase = 16;
            style.FontScaleDpi = scale;
            style.WindowPadding = ImVec2(16, 16);
            style.ItemSpacing = ImVec2(8, 10);
            style.FramePadding = ImVec2(8, 5);
            style.FrameRounding = 0;
            style.FrameBorderSize = 1;
            style.WindowRounding = 0;
            style.Colors[ImGuiCol_WindowBg] = ImVec4(.82f, .81f, .78f, 1);
            style.Colors[ImGuiCol_ChildBg] = ImVec4(.82f, .81f, .78f, 1);
            style.Colors[ImGuiCol_Button] = ImVec4(.88f, .87f, .84f, 1);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.97f, .96f, .92f, 1);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(.70f, .69f, .66f, 1);
            style.Colors[ImGuiCol_Border] = ImVec4(.43f, .42f, .39f, 1);
            style.Colors[ImGuiCol_TextDisabled] = ImVec4(.36f, .35f, .33f, 1);
            style.Colors[ImGuiCol_CheckMark] = ImVec4(.45f, .06f, .06f, 1);
            style.Colors[ImGuiCol_FrameBg] = ImVec4(.97f, .96f, .93f, 1);
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
            window = SDL_CreateWindow("Blood II - Launcher", int(580 * scale), int(356 * scale),
                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
            if (!window)
                return false;
            SDL_SetWindowMinimumSize(window, int(580 * scale), int(356 * scale));
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
            ImFontConfig font;
            font.FontDataOwnedByAtlas = false;
            ImGui::GetIO().Fonts->AddFontFromMemoryTTF(launcher_font, sizeof(launcher_font), 16, &font);
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
        void load_picture(const fs::path& directory)
        {
            SDL_WaitForGPUIdle(gpu);
            if (artwork)
                SDL_ReleaseGPUTexture(gpu, artwork);
            artwork = nullptr;
            std::vector<std::uint8_t> rgba;
            if (!load_artwork(directory, rgba))
                return;
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = 320;
            info.height = 240;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            auto* texture = SDL_CreateGPUTexture(gpu, &info);
            SDL_GPUTransferBufferCreateInfo transfer_info{};
            transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transfer_info.size = Uint32(rgba.size());
            auto* transfer = SDL_CreateGPUTransferBuffer(gpu, &transfer_info);
            auto* mapped = transfer ? SDL_MapGPUTransferBuffer(gpu, transfer, false) : nullptr;
            bool ok = texture && mapped;
            if (mapped)
            {
                SDL_memcpy(mapped, rgba.data(), rgba.size());
                SDL_UnmapGPUTransferBuffer(gpu, transfer);
            }
            if (ok)
            {
                auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
                auto* pass = commands ? SDL_BeginGPUCopyPass(commands) : nullptr;
                ok = pass != nullptr;
                if (pass)
                {
                    SDL_GPUTextureTransferInfo source{};
                    source.transfer_buffer = transfer;
                    SDL_GPUTextureRegion target{};
                    target.texture = texture;
                    target.w = 320;
                    target.h = 240;
                    target.d = 1;
                    SDL_UploadToGPUTexture(pass, &source, &target, false);
                    SDL_EndGPUCopyPass(pass);
                    ok = SDL_SubmitGPUCommandBuffer(commands);
                }
                else if (commands)
                    SDL_CancelGPUCommandBuffer(commands);
            }
            if (transfer)
                SDL_ReleaseGPUTransferBuffer(gpu, transfer);
            if (ok)
                artwork = texture;
            else if (texture)
                SDL_ReleaseGPUTexture(gpu, texture);
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
        const auto state_file = directory / "launcher.cfg", mods_file = directory / "mods.cfg";
        State state;
        std::vector<Mod> mods;
        std::string error, notice;
        load_state(state_file, state, error);
        std::string mod_error;
        const bool mods_valid = load_mods(mods_file, mods, mod_error);
        if (!mods_valid)
            error = mod_error;
        if (!options.game.empty())
            state.game = options.game;
        const bool discover_game = state.game.empty();
        if (discover_game)
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
        if (discover_game && state.game.filename() ==
#ifdef _WIN32
                                 "blood2.exe"
#else
                                 "blood2"
#endif
        )
        {
            std::error_code ec;
            const auto base = path_from_utf8(SDL_GetBasePath() ? SDL_GetBasePath() : "").lexically_normal();
            const auto folder = base.filename().empty() ? base.parent_path() : base;
            const auto preset = path_text(folder.filename());
            if (!fs::exists(state.game, ec) && preset.starts_with("launcher-"))
            {
                const auto candidate = folder.parent_path() / preset.substr(9) / state.game.filename();
                if (fs::is_regular_file(candidate, ec))
                    state.game = candidate;
            }
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
        enum class Panel
        {
            Main,
            Settings,
            Mods
        };
        Panel panel = options.panel == "settings" ? Panel::Settings
                      : options.panel == "mods"   ? Panel::Mods
                                                  : Panel::Main;
        enum class Pick
        {
            Data,
            Game,
            Mod,
            Zip
        };
        Pick pick = Pick::Data;
        static std::string injected_zip;
        injected_zip = path_text(options.install_smoke);
        // A diagnostic-only native-dialog reply; normal UI always uses SDL's OS chooser.
        Dialog dialog(options.install_smoke.empty() ? nullptr : +[](SDL_DialogFileCallback callback, void* token, SDL_Window*, bool)
        {
            const char* paths[] = {injected_zip.c_str(), nullptr};
            callback(token, paths, 0);
        });
        if (!options.install_smoke.empty())
            panel = Panel::Settings;
        DemoInstaller installer;
        Controller controller;
        auto installation = [&]()
        { return Installation{demo ? Edition::Demo : Edition::Retail, path_from_utf8(data.data())}; };
        auto remember = [&]() -> bool
        {
            State next = state;
            next.game = path_from_utf8(game.data());
            if (data[0])
            {
                const auto item = installation();
                const auto found = std::find_if(next.installations.begin(), next.installations.end(),
                                                [&](const Installation& previous)
                                                {
                                                    if (previous.edition != item.edition)
                                                        return false;
                                                    std::error_code a, b;
                                                    const auto first = fs::weakly_canonical(previous.directory, a);
                                                    const auto second = fs::weakly_canonical(item.directory, b);
                                                    return !a && !b && first == second;
                                                });
                next.selected = int(found - next.installations.begin());
                if (found == next.installations.end())
                    next.installations.push_back(item);
            }
            else
            {
                next.installations.clear();
                next.selected = -1;
            }
            if (!save_state(state_file, next, error))
                return false;
            state = std::move(next);
            return true;
        };
        auto prepare = [&]()
        {
            controller.cancel();
            notice.clear();
            if (data[0])
                controller.prepare(installation(), path_from_utf8(game.data()), enabled_mods(mods, installation()));
            view.load_picture(path_from_utf8(data.data()));
        };
        auto choose_data = [&](const fs::path& path)
        {
            assign(data, path);
            // Select demo only when no retail base is present; mixed roots keep retail explicit.
            bool retail = false, demonstration = false;
            std::error_code ec;
            for (auto it = fs::directory_iterator(path, ec); !ec && it != fs::directory_iterator{}; it.increment(ec))
            {
                auto name = path_text(it->path().filename());
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return char(SDL_tolower(c)); });
                retail |= name == "blood2.rez";
                demonstration |= name == "b2demo.rez";
            }
            demo = demonstration && !retail;
            prepare();
        };
        auto open = [&](Pick kind)
        {
            pick = kind;
            static const SDL_DialogFileFilter rez{"REZ archives", "rez"};
            static const SDL_DialogFileFilter zip{"ZIP archives", "zip"};
            const auto* filter = kind == Pick::Mod ? &rez : kind == Pick::Zip ? &zip : nullptr;
            dialog.open(view.window, kind != Pick::Data, filter, filter ? 1 : 0);
        };
        if (data[0])
            prepare();
        if (panel != Panel::Main)
            SDL_SetWindowSize(view.window, int(580 * view.scale), int(420 * view.scale));
        auto previous_panel = panel;
        bool hidden_for_game = false, captured = false, smoke_started = false, smoke_finished = false;
        unsigned click_stage = 0, frame = 0, navigation_stage = 0, import_click = 0;
        unsigned visited_panels = 0;
        bool smoke_installed = false;
        std::array<ImVec2, 3> navigation_centers{};
        ImVec2 import_center{};
        ImVec2 play_center{};
        bool quit = false;
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
                    error.clear();
                    const auto path = path_from_utf8(result.path);
                    if (pick == Pick::Data)
                        choose_data(path);
                    else if (pick == Pick::Game)
                    {
                        assign(game, path);
                        prepare();
                    }
                    else if (pick == Pick::Zip)
                    {
                        if (!installer.start(directory, path))
                            error = "Cannot start demo installation";
                    }
                    else
                    {
                        auto next = mods;
                        next.push_back({installation(), path, false});
                        if (save_mods(mods_file, next, error))
                            mods = std::move(next);
                    }
                }
            }
            InstallResult installed;
            if (installer.poll(installed))
            {
                if (!installed.success)
                    error = installed.error;
                else
                {
                    const auto previous_data = data;
                    const auto previous_demo = demo;
                    controller.cancel();
                    assign(data, installed.directory);
                    demo = true;
                    if (remember())
                    {
                        prepare();
                        notice = "Demo installed. Ready to play.";
                        panel = Panel::Main;
                        smoke_installed = true;
                    }
                    else
                    {
                        error += " Installed files: " + path_text(installed.directory);
                        data = previous_data;
                        demo = previous_demo;
                        prepare();
                    }
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
                auto& io = ImGui::GetIO();
                io.AddFocusEvent(true);
                io.AddMousePosEvent(play_center.x, play_center.y);
                io.AddMouseButtonEvent(0, click_stage == 0);
                ++click_stage;
            }
            auto diagnostic_click = [&](ImVec2 center, bool down)
            {
                auto& io = ImGui::GetIO();
                io.AddFocusEvent(true);
                io.AddMousePosEvent(center.x, center.y);
                io.AddMouseButtonEvent(0, down);
            };
            if (options.navigation_smoke && frame >= 6 && frame % 2 == 0 && navigation_stage < 8)
            {
                const unsigned action = navigation_stage / 2;
                diagnostic_click(navigation_centers[action == 0 ? 0 : action == 2 ? 1 : 2], navigation_stage % 2 == 0);
                ++navigation_stage;
            }
            if (!options.install_smoke.empty() && frame >= 6 && import_click < 2)
            {
                diagnostic_click(import_center, import_click == 0);
                ++import_click;
            }
            ImGui::NewFrame();
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::Begin("Blood II", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
            const float scale = view.scale;
            const bool locked = installer.pending() || dialog.pending();
            const auto button_size = ImVec2(192 * scale, 38 * scale);
            visited_panels |= 1u << unsigned(panel);
            const auto item_center = []()
            {
                return ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) / 2,
                              (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) / 2);
            };
            if (panel == Panel::Main)
            {
                ImGui::BeginGroup();
                const auto at = ImGui::GetCursorScreenPos();
                const auto art_size = ImVec2(320 * scale, 240 * scale);
                if (view.artwork)
                    ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<uintptr_t>(view.artwork))), art_size);
                else
                {
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRectFilled(at, ImVec2(at.x + art_size.x, at.y + art_size.y), IM_COL32(22, 18, 17, 255));
                    auto* font = ImGui::GetFont();
                    const char* title = "BLOOD II";
                    const auto size = font->CalcTextSizeA(52 * scale, FLT_MAX, 0, title);
                    draw->AddText(font, 52 * scale, ImVec2(at.x + (art_size.x - size.x) / 2, at.y + 80 * scale),
                                  IM_COL32(178, 36, 28, 255), title);
                    const char* sub = "T H E   C H O S E N";
                    const auto subsize = ImGui::CalcTextSize(sub);
                    draw->AddText(ImVec2(at.x + (art_size.x - subsize.x) / 2, at.y + 144 * scale),
                                  IM_COL32(191, 177, 148, 255), sub);
                    draw->AddLine(ImVec2(at.x + 55 * scale, at.y + 174 * scale),
                                  ImVec2(at.x + 265 * scale, at.y + 174 * scale), IM_COL32(104, 51, 42, 255));
                    ImGui::Dummy(art_size);
                }
                ImGui::TextDisabled("OpenBlood2 %s", build::version);
                ImGui::EndGroup();
                ImGui::SameLine(0, 20 * scale);
                ImGui::BeginGroup();
                ImGui::BeginDisabled(!controller.playable() || locked || !mods_valid);
                if (ImGui::Button("Play", button_size))
                {
                    error.clear();
                    if (remember())
                        controller.play(directory);
                }
                play_center = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) / 2,
                                     (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) / 2);
                ImGui::EndDisabled();
                ImGui::BeginDisabled(controller.busy());
                if (ImGui::Button("Game Settings", button_size))
                    panel = Panel::Settings;
                navigation_centers[0] = item_center();
                if (ImGui::Button("Mods", button_size))
                    panel = Panel::Mods;
                navigation_centers[1] = item_center();
                ImGui::EndDisabled();
                ImGui::Dummy(ImVec2(0, 30 * scale));
                if (ImGui::Button("Quit", button_size))
                    quit = true;
                ImGui::EndGroup();
                ImGui::Separator();
                if (!data[0])
                    ImGui::TextWrapped("Choose your game folder in Game Settings to begin.");
                else if (controller.busy())
                    ImGui::TextUnformatted("Checking game files...");
                else if (controller.playable())
                    ImGui::TextUnformatted(demo ? "Blood II Demo" : "Blood II: The Chosen");
            }
            else if (panel == Panel::Settings)
            {
                ImGui::TextUnformatted("Game Settings");
                ImGui::Separator();
                ImGui::BeginDisabled(locked || controller.busy());
                ImGui::TextUnformatted("Game folder");
                ImGui::SetNextItemWidth(-100 * scale);
                if (ImGui::InputText("##data", data.data(), data.size()))
                    controller.cancel();
                ImGui::SameLine();
                if (ImGui::Button("Browse...##data", ImVec2(90 * scale, 0)))
                    open(Pick::Data);
                ImGui::SetNextItemWidth(160 * scale);
                int edition = demo ? 1 : 0;
                if (ImGui::Combo("Edition", &edition, "Blood II\0Demo\0"))
                {
                    demo = edition == 1;
                    prepare();
                }
                ImGui::EndDisabled();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Don't have the game?");
                if (!installer.pending())
                {
                    ImGui::BeginDisabled(locked || controller.busy());
                    if (ImGui::Button("Download Demo", ImVec2(170 * scale, 32 * scale)))
                    {
                        error.clear();
                        notice.clear();
                        if (!installer.start(directory))
                            error = "Cannot start demo download";
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Install from ZIP...", ImVec2(170 * scale, 32 * scale)))
                        open(Pick::Zip);
                    import_center = item_center();
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("36 MB from Archive.org. Installs in your user folder.");
                }
                else
                {
                    const auto stage = installer.progress.stage.load();
                    const char* text = stage == InstallStage::Downloading  ? "Downloading demo..."
                                       : stage == InstallStage::Extracting ? "Installing demo..."
                                                                           : "Checking demo files...";
                    const auto total = installer.progress.total.load();
                    ImGui::ProgressBar(total ? float(installer.progress.bytes.load()) / float(total) : 0,
                                       ImVec2(-100 * scale, 0), text);
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                        installer.cancel();
                }
                if (ImGui::CollapsingHeader("Advanced"))
                {
                    ImGui::BeginDisabled(locked || controller.busy());
                    ImGui::TextUnformatted("OpenBlood2 executable");
                    ImGui::SetNextItemWidth(-100 * scale);
                    if (ImGui::InputText("##game", game.data(), game.size()))
                        controller.cancel();
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...##game", ImVec2(90 * scale, 0)))
                        open(Pick::Game);
                    ImGui::EndDisabled();
                }
                ImGui::BeginDisabled(locked || controller.busy());
                if (ImGui::Button("OK", ImVec2(90 * scale, 30 * scale)))
                {
                    error.clear();
                    if (remember())
                    {
                        prepare();
                        panel = Panel::Main;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Back", ImVec2(90 * scale, 30 * scale)))
                {
                    prepare();
                    panel = Panel::Main;
                }
                navigation_centers[2] = item_center();
                ImGui::EndDisabled();
            }
            else
            {
                ImGui::TextUnformatted("Mods");
                ImGui::Separator();
                ImGui::TextWrapped("Enable local REZ mods for this installation. Later entries take priority.");
                ImGui::BeginDisabled(locked || controller.busy());
                ImGui::BeginChild("modlist", ImVec2(0, 150 * scale), ImGuiChildFlags_Borders);
                unsigned rows = 0;
                for (std::size_t i = 0; i < mods.size(); ++i)
                {
                    if (mods[i].installation != installation())
                        continue;
                    ++rows;
                    ImGui::PushID(int(i));
                    auto enabled = mods[i].enabled;
                    if (ImGui::Checkbox(path_text(mods[i].archive.filename()).c_str(), &enabled))
                    {
                        auto next = mods;
                        next[i].enabled = enabled;
                        if (save_mods(mods_file, next, error))
                        {
                            mods = std::move(next);
                            prepare();
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", path_text(mods[i].archive).c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove"))
                    {
                        auto next = mods;
                        next.erase(next.begin() + std::ptrdiff_t(i));
                        if (save_mods(mods_file, next, error))
                        {
                            mods = std::move(next);
                            prepare();
                        }
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (!rows)
                    ImGui::TextDisabled(data[0] ? "No mods added." : "Choose a game folder first.");
                ImGui::EndChild();
                ImGui::BeginDisabled(!data[0]);
                if (ImGui::Button("Add REZ...", ImVec2(130 * scale, 30 * scale)))
                    open(Pick::Mod);
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Back", ImVec2(90 * scale, 30 * scale)))
                    panel = Panel::Main;
                navigation_centers[2] = item_center();
                ImGui::EndDisabled();
                ImGui::TextDisabled("Data mods only. Nightmares is not supported yet.");
            }
            const auto message = !mods_valid ? mod_error : !error.empty() ? error : controller.error();
            if (!message.empty())
            {
                ImGui::TextWrapped("%s", message.c_str());
                if (!controller.logs().empty() && ImGui::SmallButton("Open game logs"))
                    SDL_OpenURL(file_url(controller.logs()).c_str());
            }
            else if (!notice.empty() && panel != Panel::Main)
                ImGui::TextWrapped("%s", notice.c_str());
            ImGui::End();
            if (panel != previous_panel)
            {
                int width = 0, height = 0;
                SDL_GetWindowSize(view.window, &width, &height);
                SDL_SetWindowSize(view.window, width, int((panel == Panel::Main ? 356 : 420) * view.scale));
                previous_panel = panel;
            }
            ++frame;
            const bool take_capture = !captured && frame >= 3 && !controller.busy() &&
                                      (!options.launch_smoke || smoke_finished) &&
                                      (!options.navigation_smoke || navigation_stage == 8) &&
                                      (options.install_smoke.empty() || smoke_installed);
            if (!view.present(take_capture ? options.capture : fs::path{}))
            {
                spdlog::error("Presentation: {}", SDL_GetError());
                return 3;
            }
            if (take_capture)
                captured = true;
            if (options.smoke && frame == 5)
            {
                if (!SDL_SetWindowSize(view.window, int(620 * scale), int(390 * scale)))
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
            if (frame < 12 || w != int(620 * view.scale) ||
                h != int((options.navigation_smoke || !options.install_smoke.empty() ? 356 : 390) * view.scale))
            {
                spdlog::error("UI smoke geometry frames={} size={}x{}", frame, w, h);
                return 4;
            }
            if (options.launch_smoke && (!smoke_started || !smoke_finished))
            {
                spdlog::error("Play smoke did not complete: {}", controller.error());
                return 4;
            }
            if (options.navigation_smoke && (visited_panels != 7 || panel != Panel::Main || controller.playable()))
            {
                spdlog::error("Navigation smoke: panels={} final={}", visited_panels, int(panel));
                return 4;
            }
            if (!options.install_smoke.empty() &&
                (!smoke_installed || !controller.playable() || !demo || state.selected < 0))
                return 4;
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
                     "  --panel NAME          Diagnostic initial panel: main, settings, mods\n"
                     "  --navigation-smoke    Drive menu/settings/mods navigation in isolated state\n"
                     "  --install-smoke ZIP   Drive local ZIP installation with an injected chooser reply\n"
                     "  --smoke               Isolated UI/resize smoke (requires --state-dir, --frames >= 12)\n"
                     "  --launch-smoke        Drive Play and return (requires --smoke, --game, --data-dir)\n";
        return 0;
    }
    return OpenBlood2::Launcher::run(options);
}
