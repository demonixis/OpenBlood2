#include "engine/config.h"
#include "engine/runtime.h"
#include "engine/capture.h"
#include "game/server_adapter.h"
#include "build_info.h"

#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <iostream>
#include <vector>

#if BLOOD2_INSPECTOR
#include "app/inspector.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#endif

namespace {

bool push_toggle(SDL_Window* window) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.key = SDLK_F1;
    event.key.scancode = SDL_SCANCODE_F1;
    event.key.down = true;
    if (!SDL_PushEvent(&event)) return false;
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    return SDL_PushEvent(&event);
}

int run(const blood2::Options& options, const blood2::Config& config, const blood2::DataStatus& data) {
    blood2::Runtime runtime;
    std::string error;
#ifdef NDEBUG
    constexpr bool gpu_debug = false;
#else
    constexpr bool gpu_debug = true;
#endif
    if (!runtime.initialize(gpu_debug, error)) {
        spdlog::error("GPU startup failed: {}", error);
        return 3;
    }
    spdlog::info("SDL {}.{}.{} [{}]; video={}; GPU={}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
                 blood2::build::sdl_revision, SDL_GetCurrentVideoDriver(), SDL_GetGPUDeviceDriver(runtime.gpu));
    spdlog::info("GPU device: {}", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
    blood2::GameSession game;
    if (!game.initialize() || !game.create_example("CameraSpot")) {
        spdlog::error("Original game boundary initialization failed"); return 4;
    }
    const auto key = game.create_example("Key");
    if (!key) { spdlog::error("Original Key creation failed"); return 4; }
    blood2::ObjectSelection selection{game.objects.get(key)->id};
    const auto original_selection = selection.id;
    bool selection_cleared = false;
    spdlog::info("M1: original CameraSpot and Key created through B2BaseClass; no world loaded");
    bool inspector_visible = BLOOD2_INSPECTOR && options.inspector;
#if BLOOD2_INSPECTOR
    blood2::Inspector inspector(runtime);
    if (!inspector.initialize(error)) {
        spdlog::error("{}", error);
        return 3;
    }
#else
    (void)config;
    (void)data;
#endif
    bool done = false;
    bool resized = false;
    bool quit_received = false;
    bool captured = false;
    std::uint32_t toggles = 0;
    std::uint64_t frames = 0, ui_frames = 0, hidden_frames = 0;
    const auto started = SDL_GetTicks();
    auto previous_tick = SDL_GetTicksNS();
    while (!done) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
#if BLOOD2_INSPECTOR
            ImGui_ImplSDL3_ProcessEvent(&event);
#endif
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(runtime.window))) {
                quit_received = true;
                done = true;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(runtime.window)) resized = true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
#if BLOOD2_INSPECTOR
                if (event.key.key == SDLK_F1) {
                    inspector_visible = !inspector_visible;
                    ++toggles;
                }
                if (event.key.key == SDLK_ESCAPE && !ImGui::GetIO().WantCaptureKeyboard) done = true;
#else
                if (event.key.key == SDLK_ESCAPE) done = true;
#endif
            }
        }
        if (done) break;
        if (options.gpu_smoke && SDL_GetTicks() - started > 15000) {
            spdlog::error("GPU smoke timed out before completing its frames/events");
            return 4;
        }
        const auto tick = SDL_GetTicksNS();
        const double delta = options.gpu_smoke ? 1.0 / 60.0 : static_cast<double>(tick - previous_tick) / 1e9;
        previous_tick = tick;
#if BLOOD2_INSPECTOR
        inspector.apply_commands(game, selection);
#endif
        if (!game.objects.advance(delta)) { spdlog::error("Object simulation step failed"); return 4; }
        selection.refresh(game.objects);
        if (options.gpu_smoke && frames >= 16 && !selection.id && !game.objects.get(original_selection)) selection_cleared = true;
#if BLOOD2_INSPECTOR
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        inspector.draw(config, data, inspector_visible, frames, game, selection);
        ImGui::Render();
#endif
        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands) {
            spdlog::error("Command buffer: {}", SDL_GetError());
            return 3;
        }
        SDL_GPUTexture* swapchain = nullptr;
        Uint32 width = 0, height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &swapchain, &width, &height)) {
            spdlog::error("Swapchain: {}", SDL_GetError());
            SDL_CancelGPUCommandBuffer(commands);
            return 3;
        }
        blood2::Capture capture;
        const bool capture_frame = swapchain && frames >= 2 && !captured && !options.capture_file.empty();
        if (capture_frame && !capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width, height)) {
            spdlog::error("Capture target: {}", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commands);
            return 3;
        }
        if (swapchain) {
#if BLOOD2_INSPECTOR
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
#endif
            SDL_GPUColorTargetInfo color{};
            color.texture = capture_frame ? capture.texture : swapchain;
            color.clear_color = {0.025f, 0.035f, 0.05f, 1.0f};
            color.load_op = SDL_GPU_LOADOP_CLEAR;
            color.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
            if (!pass) {
                spdlog::error("Render pass: {}", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(commands);
                return 3;
            }
#if BLOOD2_INSPECTOR
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
            if (ImGui::GetDrawData()->TotalVtxCount > 0) ++ui_frames;
#endif
            SDL_EndGPURenderPass(pass);
            if (capture_frame && !capture.enqueue(commands, swapchain)) {
                spdlog::error("Capture copy: {}", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(commands);
                return 3;
            }
        }
        if (!SDL_SubmitGPUCommandBuffer(commands)) {
            spdlog::error("GPU submit: {}", SDL_GetError());
            return 3;
        }
        if (!swapchain) {
            SDL_Delay(10);
            continue;
        }
        if (capture_frame) {
            if (!capture.save(blood2::path_text(options.capture_file).c_str())) {
                spdlog::error("Capture save: {}", SDL_GetError());
                return 3;
            }
            captured = true;
            spdlog::info("Captured GPU frame to {}", blood2::path_text(options.capture_file));
        }
        ++frames;
        if (!inspector_visible) ++hidden_frames;
        if (options.gpu_smoke) {
            bool event_ok = true;
            if (frames == 4 || frames == 8) event_ok = push_toggle(runtime.window);
            if (frames == 12) {
                resized = false;
                event_ok = SDL_SetWindowSize(runtime.window, 1000, 680);
            }
            if (frames == 16) {
#if BLOOD2_INSPECTOR
                inspector.remove_request = selection.id; // Same queued command as the UI button.
#else
                game.objects.remove(key);
#endif
            }
            if (frames == 24) {
                SDL_Event quit{};
                quit.type = SDL_EVENT_QUIT;
                event_ok = SDL_PushEvent(&quit);
            }
            if (!event_ok) {
                spdlog::error("Smoke event injection failed: {}", SDL_GetError());
                return 4;
            }
        }
        if (options.frame_limit != 0 && frames >= options.frame_limit) done = true;
    }
    spdlog::info("Shutdown: frames={}, UI frames={}, hidden frames={}, toggles={}, resize={}, quit={}",
                 frames, ui_frames, hidden_frames, toggles, resized, quit_received);
    if (options.gpu_smoke) {
        int width = 0, height = 0;
        SDL_GetWindowSize(runtime.window, &width, &height);
        const bool ui_ok = BLOOD2_INSPECTOR ? toggles == 2 && ui_frames > 0 && hidden_frames > 0 : ui_frames == 0 && toggles == 0;
        if (frames < 24 || !resized || !quit_received || width != 1000 || height != 680 || !ui_ok ||
            !selection_cleared || game.server.string_count() != 0 || game.objects.snapshot().size() != 1) {
            spdlog::error("GPU smoke failed its render/toggle/resize/quit assertions");
            return 4;
        }
        spdlog::info("GPU smoke passed: rendering, toggle, resize, quit, original Key destruction, string release and selection invalidation");
    }
    if (!options.capture_file.empty() && !captured) {
        spdlog::error("Window closed before a frame could be captured");
        return 4;
    }
    return 0;
}

}

int main(int argc, char** argv) {
    spdlog::set_pattern("[%l] %v");
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    blood2::Options options;
    std::string error;
    if (!blood2::parse_options(args, options, error)) {
        spdlog::error("{}", error);
        return 2;
    }
    if (options.help) {
        std::cout << "Blood II reconstruction " << blood2::build::version << " (M1)\n"
                     "  --config FILE       Configuration file (default: blood2.local.ini in working directory)\n"
                     "  --data-dir DIR      External Blood II directory; overrides BLOOD2_DATA_DIR and config\n"
                     "  --check-data        Check file metadata without opening a window or parsing archives\n"
                     "  --no-inspector      Start with the inspector hidden\n"
                     "  --frames N          Exit after N presented frames\n"
                     "  --capture FILE.png  Save the third GPU frame after UI layout has settled\n"
                     "  --gpu-smoke         Exercise GPU presentation, inspector toggle, resize and quit\n"
                     "  --help              Show this help\n";
        return 0;
    }
    blood2::Config config;
    blood2::DataStatus data;
    const char* environment = std::getenv("BLOOD2_DATA_DIR");
    if (!blood2::load_config(options, environment ? environment : "", config, error) ||
        !blood2::inspect_data(config, data, error)) {
        spdlog::error("{}", error);
        return 2;
    }
    spdlog::info("External data: {} ({})", blood2::path_text(config.data_dir), config.source);
    for (const auto& archive : data.archives) {
        spdlog::info("{}: {}, {} bytes", archive.name, archive.present ? "detected" : "absent (optional)", archive.bytes);
    }
    if (!data.readme_version.empty()) spdlog::info("Readme declares: {}", data.readme_version);
    spdlog::info("Metadata only; no archives mounted, no game objects or resources loaded");
    return options.check_data ? 0 : run(options, config, data);
}
