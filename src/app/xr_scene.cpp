#include "app/xr_scene.h"
#include "app/xr_lines.h"
#include "build_info.h"
#include "engine/xr/xr_runtime.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>

namespace OpenBlood2
{
    namespace
    {
        struct Range
        {
            std::uint32_t first = 0, count = 0;
        };

        // Static first-light geometry in metres: floor grid, origin axes, two unit boxes and a unit ray.
        struct SceneGeometry
        {
            std::vector<LineVertex> vertices;
            Range grid, axes, left_box, right_box, ray;
        };

        SceneGeometry scene_geometry()
        {
            SceneGeometry g;
            auto& v = g.vertices;
            const auto begin = [&v] { return static_cast<std::uint32_t>(v.size()); };
            const auto close = [&v](Range& r, std::uint32_t first) {
                r = {first, static_cast<std::uint32_t>(v.size()) - first};
            };
            // Gamma-encoded colours, like the renderer's (UNORM views of the sRGB swapchain).
            std::uint32_t first = begin();
            for (int i = -10; i <= 10; ++i)
            {
                const float t = i * 0.5f;
                const std::array<float, 4> color =
                    i % 2 ? std::array<float, 4>{0.25f, 0.25f, 0.29f, 1} : std::array<float, 4>{0.44f, 0.44f, 0.49f, 1};
                add_line(v, {t, 0, -5}, {t, 0, 5}, color);
                add_line(v, {-5, 0, t}, {5, 0, t}, color);
            }
            close(g.grid, first);
            first = begin();
            add_line(v, {0, 0.002f, 0}, {1, 0.002f, 0}, {0.9f, 0.05f, 0.05f, 1});
            add_line(v, {0, 0.002f, 0}, {0, 1, 0}, {0.05f, 0.9f, 0.05f, 1});
            add_line(v, {0, 0.002f, 0}, {0, 0.002f, 1}, {0.1f, 0.2f, 1, 1});
            close(g.axes, first);
            first = begin();
            add_box(v, {}, {1, 1, 1}, {0.1f, 0.8f, 1, 1});
            add_line(v, {0, 0, 1}, {0, 0, 1.6f}, {0.1f, 0.8f, 1, 1});
            close(g.left_box, first);
            first = begin();
            add_box(v, {}, {1, 1, 1}, {1, 0.45f, 0.05f, 1});
            add_line(v, {0, 0, 1}, {0, 0, 1.6f}, {1, 0.45f, 0.05f, 1});
            close(g.right_box, first);
            first = begin();
            add_line(v, {0, 0, 0}, {0, 0, 1}, {1, 0.9f, 0.1f, 1});
            close(g.ray, first);
            return g;
        }

        struct GpuScene
        {
            SDL_GPUDevice* device = nullptr;
            std::unique_ptr<LineRenderer> lines;
            SDL_GPUTexture* depth = nullptr;
            SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            bool uploaded = false;

            GpuScene() = default;
            GpuScene(const GpuScene&) = delete;
            GpuScene& operator=(const GpuScene&) = delete;
            ~GpuScene()
            {
                if (!device)
                    return;
                SDL_WaitForGPUIdle(device);
                if (depth)
                    SDL_ReleaseGPUTexture(device, depth);
                lines.reset();
            }

            bool create(SDL_GPUDevice* gpu, SDL_GPUTextureFormat color, std::uint32_t width, std::uint32_t height,
                        std::string& error)
            {
                device = gpu;
                if (!SDL_GPUTextureSupportsFormat(device, depth_format, SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                                  SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
                    depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
                lines = std::make_unique<LineRenderer>(device);
                if (!lines->prepare(color, depth_format, error))
                    return false;
                SDL_GPUTextureCreateInfo depth_info{};
                depth_info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
                depth_info.format = depth_format;
                depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                depth_info.width = width;
                depth_info.height = height;
                depth_info.layer_count_or_depth = 2;
                depth_info.num_levels = 1;
                depth = SDL_CreateGPUTexture(device, &depth_info);
                if (!depth)
                {
                    error = std::string("XR depth array: ") + SDL_GetError();
                    return false;
                }
                return true;
            }
        };

        xr::LineUniform placed(xr::LineUniform u, const xr::Pose& pose, std::array<float, 3> scale)
        {
            u.translation = {pose.position[0], pose.position[1], pose.position[2], 0};
            u.quaternion = {pose.orientation[0], pose.orientation[1], pose.orientation[2], pose.orientation[3]};
            u.scale = {scale[0], scale[1], scale[2], 0};
            return u;
        }

        bool record(SDL_GPUCommandBuffer* command, GpuScene& scene, SDL_GPUTexture* color, std::uint32_t width,
                    std::uint32_t height, const SceneGeometry& geometry, const XrFrameInput& frame, float floor)
        {
            std::string error;
            if (!scene.uploaded)
            {
                if (!scene.lines->upload(command, geometry.vertices, error))
                {
                    SDL_SetError("%s", error.c_str());
                    return false;
                }
                scene.uploaded = true;
            }
            for (std::uint32_t eye = 0; eye < 2; ++eye)
            {
                SDL_GPUColorTargetInfo target{};
                target.texture = color;
                target.layer_or_depth_plane = eye;
                target.load_op = SDL_GPU_LOADOP_CLEAR;
                target.store_op = SDL_GPU_STOREOP_STORE;
                target.clear_color = {0.05f, 0.05f, 0.1f, 1};
                SDL_GPUDepthStencilTargetInfo depth{};
                depth.texture = scene.depth;
                depth.layer = static_cast<Uint8>(eye);
                depth.clear_depth = 1;
                depth.load_op = SDL_GPU_LOADOP_CLEAR;
                depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
                depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &target, 1, &depth);
                if (!pass)
                    return false;
                // The scene is in metres: one engine unit per metre for this test.
                const xr::LineUniform view =
                    xr::line_uniform(xr::engine_pose(frame.eye_pose[eye], 1), frame.eye_fov[eye], 0.05f, 100);
                const auto draw = [&](const xr::LineUniform& u, const Range& range)
                { scene.lines->draw(pass, command, u, width, height, 0, 1, range.first, range.count); };
                const xr::Pose floor_pose{{0, floor, 0}, {0, 0, 0, 1}};
                draw(placed(view, floor_pose, {1, 1, 1}), geometry.grid);
                draw(placed(view, floor_pose, {1, 1, 1}), geometry.axes);
                for (std::size_t hand = 0; hand < 2; ++hand)
                {
                    const auto& h = frame.hands[hand];
                    // Grip space: the box is roughly the size of a Touch controller.
                    if (h.grip_valid)
                        draw(placed(view, xr::engine_pose(h.grip, 1), {0.03f, 0.035f, 0.065f}),
                             hand ? geometry.right_box : geometry.left_box);
                    if (h.aim_valid)
                        draw(placed(view, xr::engine_pose(h.aim, 1), {1, 1, 2}), geometry.ray);
                }
                SDL_EndGPURenderPass(pass);
            }
            return true;
        }
    } // namespace

    int run_xr_test(const Options&)
    {
#ifdef NDEBUG
        constexpr bool gpu_debug = false;
#else
        constexpr bool gpu_debug = true;
#endif
        // OpenXR must keep receiving events while Android pauses the activity.
        SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
        SDL_SetAppMetadata("OpenBlood 2", build::version, build::game_identifier);
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            spdlog::error("SDL_Init: {}", SDL_GetError());
            return 7;
        }
        int code = 0;
        {
            std::string error;
            XrRuntime xr;
            GpuScene scene;
            const SceneGeometry geometry = scene_geometry();
            if (!xr.create({gpu_debug}, error) ||
                !scene.create(xr.device(), xr.color_format(), xr.width(), xr.height(), error))
            {
                spdlog::error("XR test: {}", error);
                code = 7;
            }
            using Clock = std::chrono::steady_clock;
            auto window_start = Clock::now();
            std::uint64_t frames = 0, rendered = 0;
            double record_seconds = 0;
            std::int64_t period = 0;
            while (!code && !xr.exit_requested())
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING)
                        xr.request_exit();
                if (!xr.poll(error))
                {
                    spdlog::error("XR events: {}", error);
                    code = 7;
                    break;
                }
                if (!xr.session_running())
                {
                    SDL_Delay(10);
                    continue;
                }
                XrFrameInput frame;
                if (!xr.begin_frame(frame, error))
                {
                    spdlog::error("XR frame: {}", error);
                    code = 7;
                    break;
                }
                const auto start = Clock::now();
                bool drawn = false;
                if (frame.should_render && frame.views_valid)
                {
                    SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(xr.device());
                    SDL_GPUTexture* color = command ? xr.acquire_color(error) : nullptr;
                    if (!color)
                    {
                        if (command)
                            SDL_CancelGPUCommandBuffer(command);
                        spdlog::error("XR image: {}", error.empty() ? SDL_GetError() : error);
                        code = 7;
                    }
                    else if (!record(command, scene, color, xr.width(), xr.height(), geometry, frame,
                                     xr.floor_height()) ||
                             !SDL_SubmitGPUCommandBuffer(command))
                    {
                        spdlog::error("XR record: {}", SDL_GetError());
                        code = 7;
                    }
                    else
                        drawn = true;
                }
                record_seconds += std::chrono::duration<double>(Clock::now() - start).count();
                if (!xr.end_frame(frame, drawn, error))
                {
                    spdlog::error("XR end frame: {}", error);
                    code = 7;
                    break;
                }
                ++frames;
                rendered += drawn;
                period = frame.display_period;
                const double elapsed = std::chrono::duration<double>(Clock::now() - window_start).count();
                if (elapsed >= 5)
                {
                    spdlog::info("XR test: {:.1f} frames/s ({} rendered), display period {:.2f} ms, record {:.2f} "
                                 "ms/frame, phase {}, controllers {}/{}",
                                 frames / elapsed, rendered, period / 1e6, frames ? record_seconds * 1e3 / frames : 0.0,
                                 xr_phase_name(xr.phase()), frame.hands[0].grip_valid, frame.hands[1].grip_valid);
                    window_start = Clock::now();
                    frames = rendered = 0;
                    record_seconds = 0;
                }
            }
            if (!code)
                spdlog::info("XR test: session ended");
        }
        SDL_Quit();
        return code;
    }
    namespace
    {
        bool has_file(const std::filesystem::path& directory, std::string_view name)
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
            {
                const auto file = entry.path().filename().string();
                if (file.size() == name.size() &&
                    std::equal(file.begin(), file.end(), name.begin(),
                               [](char a, char b)
                               { return std::toupper((unsigned char)a) == std::toupper((unsigned char)b); }))
                    return entry.is_regular_file(ec);
            }
            return false;
        }
    } // namespace

    void select_android_xr_data(Options& options)
    {
        const std::filesystem::path root = "/sdcard/openblood2";
        if (has_file(root / "game", "BLOOD2.REZ"))
        {
            options.data_dir = root / "game";
            options.demo = false;
        }
        else if (has_file(root / "demo", "B2DEMO.REZ"))
        {
            options.data_dir = root / "demo";
            options.demo = true;
            options.world = first_level(true);
        }
        else
        {
            std::error_code ec;
            spdlog::warn("No game data in {0}/game or {0}/demo{1}; opening the XR first light", root.string(),
                         std::filesystem::exists(root, ec) ? " (folder present: check the all-files access permission)"
                                                           : "");
            options.xr_test = true;
            return;
        }
        options.xr_world = true;
        spdlog::info("XR data: {} ({})", options.data_dir.string(), options.demo ? "demo" : "retail");
    }

} // namespace OpenBlood2
