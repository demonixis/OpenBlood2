#include "app/xr_scene.h"
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "build_info.h"
#include "engine/world_shaders.h"
#include "engine/xr/xr_runtime.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <vector>

namespace OpenBlood2
{
    namespace
    {
        struct LineVertex
        {
            std::array<float, 3> position;
            std::array<float, 4> color;
        };
        static_assert(sizeof(LineVertex) == 28);

        struct Range
        {
            std::uint32_t first = 0, count = 0;
        };

        // Static geometry in metres: floor grid, origin axes, two unit boxes and a unit ray.
        struct SceneGeometry
        {
            std::vector<LineVertex> vertices;
            Range grid, axes, left_box, right_box, ray;
        };

        void add_line(std::vector<LineVertex>& v, std::array<float, 3> a, std::array<float, 3> b,
                      std::array<float, 4> color)
        {
            v.push_back({a, color});
            v.push_back({b, color});
        }

        void add_box(std::vector<LineVertex>& v, std::array<float, 4> color)
        {
            const std::array<std::array<float, 3>, 8> c{
                {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}};
            constexpr std::array<std::array<int, 2>, 12> edges{
                {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};
            for (const auto& e : edges)
                add_line(v, c[e[0]], c[e[1]], color);
        }

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
            add_box(v, {0.1f, 0.8f, 1, 1});
            add_line(v, {0, 0, 1}, {0, 0, 1.6f}, {0.1f, 0.8f, 1, 1});
            close(g.left_box, first);
            first = begin();
            add_box(v, {1, 0.45f, 0.05f, 1});
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
            SDL_GPUGraphicsPipeline* pipeline = nullptr;
            SDL_GPUBuffer* vertices = nullptr;
            SDL_GPUTexture* depth = nullptr;
            SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

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
                if (vertices)
                    SDL_ReleaseGPUBuffer(device, vertices);
                if (pipeline)
                    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            }

            bool create(SDL_GPUDevice* gpu, SDL_GPUTextureFormat color, std::uint32_t width, std::uint32_t height,
                        const SceneGeometry& geometry, std::string& error)
            {
                device = gpu;
                const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
                SDL_GPUShaderCreateInfo vertex_info{}, fragment_info{};
                if (!world_shader_info(WorldShader::LineVertex, formats, vertex_info) ||
                    !world_shader_info(WorldShader::LineFragment, formats, fragment_info))
                {
                    error = "Line shaders unavailable for this GPU";
                    return false;
                }
                SDL_GPUShader* vertex = SDL_CreateGPUShader(device, &vertex_info);
                SDL_GPUShader* fragment = SDL_CreateGPUShader(device, &fragment_info);
                if (!SDL_GPUTextureSupportsFormat(device, depth_format, SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                                  SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
                    depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
                if (vertex && fragment)
                {
                    const SDL_GPUVertexBufferDescription buffer{0, sizeof(LineVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX,
                                                                0};
                    const std::array<SDL_GPUVertexAttribute, 2> attributes{
                        {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0},
                         {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(LineVertex, color)}}};
                    SDL_GPUColorTargetDescription target{};
                    target.format = color;
                    SDL_GPUGraphicsPipelineCreateInfo info{};
                    info.vertex_shader = vertex;
                    info.fragment_shader = fragment;
                    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
                    info.vertex_input_state.num_vertex_buffers = 1;
                    info.vertex_input_state.vertex_attributes = attributes.data();
                    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
                    info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
                    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                    info.depth_stencil_state.enable_depth_test = true;
                    info.depth_stencil_state.enable_depth_write = true;
                    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
                    info.target_info.color_target_descriptions = &target;
                    info.target_info.num_color_targets = 1;
                    info.target_info.depth_stencil_format = depth_format;
                    info.target_info.has_depth_stencil_target = true;
                    pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
                }
                if (vertex)
                    SDL_ReleaseGPUShader(device, vertex);
                if (fragment)
                    SDL_ReleaseGPUShader(device, fragment);
                if (!pipeline)
                {
                    error = std::string("XR line pipeline: ") + SDL_GetError();
                    return false;
                }

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

                const auto bytes = static_cast<Uint32>(geometry.vertices.size() * sizeof(LineVertex));
                const SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX, bytes, 0};
                vertices = SDL_CreateGPUBuffer(device, &buffer_info);
                const SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0};
                SDL_GPUTransferBuffer* transfer =
                    vertices ? SDL_CreateGPUTransferBuffer(device, &transfer_info) : nullptr;
                void* mapped = transfer ? SDL_MapGPUTransferBuffer(device, transfer, false) : nullptr;
                bool uploaded = false;
                if (mapped)
                {
                    SDL_memcpy(mapped, geometry.vertices.data(), bytes);
                    SDL_UnmapGPUTransferBuffer(device, transfer);
                    SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
                    SDL_GPUCopyPass* copy = command ? SDL_BeginGPUCopyPass(command) : nullptr;
                    if (copy)
                    {
                        const SDL_GPUTransferBufferLocation source{transfer, 0};
                        const SDL_GPUBufferRegion destination{vertices, 0, bytes};
                        SDL_UploadToGPUBuffer(copy, &source, &destination, false);
                        SDL_EndGPUCopyPass(copy);
                        uploaded = SDL_SubmitGPUCommandBuffer(command);
                    }
                    else if (command)
                        SDL_CancelGPUCommandBuffer(command);
                }
                if (transfer)
                    SDL_ReleaseGPUTransferBuffer(device, transfer);
                if (!uploaded)
                {
                    error = std::string("XR line geometry upload: ") + SDL_GetError();
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

        bool record(SDL_GPUCommandBuffer* command, const GpuScene& scene, SDL_GPUTexture* color, std::uint32_t width,
                    std::uint32_t height, const SceneGeometry& geometry, const XrFrameInput& frame, float floor)
        {
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
                const SDL_GPUViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
                SDL_SetGPUViewport(pass, &viewport);
                SDL_BindGPUGraphicsPipeline(pass, scene.pipeline);
                const SDL_GPUBufferBinding binding{scene.vertices, 0};
                SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
                // The scene is in metres: one engine unit per metre for this test.
                const xr::LineUniform view =
                    xr::line_uniform(xr::engine_pose(frame.eye_pose[eye], 1), frame.eye_fov[eye], 0.05f, 100);
                const auto draw = [&](const xr::LineUniform& u, const Range& range)
                {
                    SDL_PushGPUVertexUniformData(command, 0, &u, sizeof(u));
                    SDL_DrawGPUPrimitives(pass, range.count, 1, range.first, 0);
                };
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
                !scene.create(xr.device(), xr.color_format(), xr.width(), xr.height(), geometry, error))
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
        // Nominal user eye height: the original player's standing eye height maps to it.
        constexpr float nominal_eye_height = 1.6f, fallback_units_per_meter = 45, max_lean = 0.5f;

        DRotation engine_rotation(const xr::Quat& q)
        {
            return {DVector{q[0], q[1], q[2]}, q[3]};
        }
        xr::Vec3 vec(DVector v)
        {
            return {v.x, v.y, v.z};
        }
        DVector dvec(const xr::Vec3& v)
        {
            return {v[0], v[1], v[2]};
        }
        EyeProjection eye_projection(const xr::Fov& fov)
        {
            const auto p = xr::projection(fov);
            return {p.scale_x,
                    p.scale_y,
                    p.offset_x,
                    p.offset_y,
                    std::tan(double(fov.left)),
                    std::tan(double(fov.right)),
                    std::tan(double(fov.down)),
                    std::tan(double(fov.up))};
        }
        bool clear_eyes(SDL_GPUCommandBuffer* command, SDL_GPUTexture* color)
        {
            for (Uint32 eye = 0; eye < 2; ++eye)
            {
                SDL_GPUColorTargetInfo target{};
                target.texture = color;
                target.layer_or_depth_plane = eye;
                target.load_op = SDL_GPU_LOADOP_CLEAR;
                target.store_op = SDL_GPU_STOREOP_STORE;
                target.clear_color = {0, 0, 0, 1};
                SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
                if (!pass)
                    return false;
                SDL_EndGPURenderPass(pass);
            }
            return true;
        }
        // World units per metre from the player's standing eye height above its feet.
        float units_per_meter(GameSession& game)
        {
            DVector dims{};
            const auto* player = game.server_shell.Player(game.local_client);
            if (!player || !game.client_shell.player ||
                game.server.GetObjectDims(game.client_shell.player, &dims) != DE_OK || dims.y <= 0)
                return fallback_units_per_meter;
            const float eye = dims.y + player->eye_level;
            return eye > 0 && std::isfinite(eye) ? eye / nominal_eye_height : fallback_units_per_meter;
        }
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

    int run_xr_world(const Options& options, const Config& config)
    {
#ifdef NDEBUG
        constexpr bool gpu_debug = false;
#else
        constexpr bool gpu_debug = true;
#endif
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
            auto game = std::make_unique<GameSession>();
            std::unique_ptr<ScenePresenter> presenter;
            if (!xr.create({gpu_debug}, error))
            {
                spdlog::error("XR world: {}", error);
                code = 7;
            }
            else if (!game->resources.mount_game(config, error) || !game->initialize())
            {
                spdlog::error("XR world: {}", error.empty() ? "cannot initialize original game services" : error);
                code = 5;
            }
            else
            {
                if (!game->server.audio.open_output(error))
                    spdlog::warn("Game audio output: {}", error);
                spdlog::info("Loading world: {} (XR direct entry)", options.world);
                if (!game->request_world(options.world, error) || !game->run_world(error))
                {
                    spdlog::error("World load: {}", error);
                    code = 6;
                }
                presenter = std::make_unique<ScenePresenter>(xr.device());
                presenter->model_shadows = options.model_shadows;
            }
            using Clock = std::chrono::steady_clock;
            auto window_start = Clock::now();
            std::uint64_t frames = 0, rendered = 0;
            double step_seconds = 0, update_seconds = 0, record_seconds = 0;
            bool calibrated = false, audio_paused = false;
            xr::Vec3 reference{};
            float scale = fallback_units_per_meter;
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
                if (xr.take_recentre())
                    calibrated = false;
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
                // Simulation and game audio run only with input focus (the system menu pauses them).
                if (audio_paused != !frame.focused && game->server.audio.pause_output(!frame.focused, error))
                    audio_paused = !frame.focused;
                auto start = Clock::now();
                if (frame.focused)
                {
                    const double delta = app_frame_delta(double(frame.display_period) / 1e9);
                    if (!step_app_session(*game, delta))
                    {
                        spdlog::error("Simulation failed: world={}, time={}", game->world_path, game->objects.time());
                        for (const auto& diagnostic : game->step_errors())
                            spdlog::error("{}", diagnostic);
                        code = 4;
                    }
                    else if (const auto change = GameSession::process_level_change(game, error);
                             change == LevelChangeResult::Committed)
                    {
                        spdlog::info("NEW_LEVEL committed: {} -> {}", game->level_change.source_world,
                                     game->world_path);
                        calibrated = false;
                    }
                    else if (change == LevelChangeResult::Rejected)
                        spdlog::warn("Level change retained world: {}", error);
                }
                step_seconds += std::chrono::duration<double>(Clock::now() - start).count();
                bool drawn = false;
                if (!code && frame.should_render && frame.views_valid)
                {
                    const std::array<xr::Pose, 2> eyes{frame.eye_pose[0], frame.eye_pose[1]};
                    if (!calibrated)
                    {
                        for (std::size_t i = 0; i < 3; ++i)
                            reference[i] = (eyes[0].position[i] + eyes[1].position[i]) / 2;
                        calibrated = true;
                        scale = units_per_meter(*game);
                        spdlog::info("XR calibration: {:.2f} units/m (player eye over a {:.2f} m user eye)", scale,
                                     nominal_eye_height);
                    }
                    // The game camera supplies position and yaw; the tracked head supplies the rest.
                    const auto& view = game->client_shell.view;
                    const auto forward = rotate_vector(DVector{0, 0, 1}, view.rotation);
                    const float yaw = std::atan2(forward.x, forward.z);
                    const auto stereo = xr::stereo_camera(vec(view.position), yaw, eyes, reference, scale, max_lean);
                    CameraView head = view;
                    head.position = dvec(stereo.head.position);
                    head.rotation = engine_rotation(stereo.head.orientation);
                    const auto look = rotate_vector(DVector{0, 0, 1}, head.rotation);
                    presenter->camera_override = head;
                    presenter->weapon_camera = ViewWeaponCamera{-std::asin(std::clamp(look.y, -1.f, 1.f)),
                                                                std::atan2(look.x, look.z), head.position};
                    start = Clock::now();
                    if (!presenter->update(*game, error))
                    {
                        spdlog::error("XR presentation: {}", error);
                        code = 5;
                    }
                    update_seconds += std::chrono::duration<double>(Clock::now() - start).count();
                    std::array<StereoEye, 2> views;
                    for (std::size_t i = 0; i < 2; ++i)
                        views[i] = {dvec(stereo.eyes[i].position), engine_rotation(stereo.eyes[i].orientation),
                                    eye_projection(frame.eye_fov[i])};
                    start = Clock::now();
                    SDL_GPUCommandBuffer* command = code ? nullptr : SDL_AcquireGPUCommandBuffer(xr.device());
                    SDL_GPUTexture* color = command ? xr.acquire_color(error) : nullptr;
                    if (!code && !color)
                    {
                        if (command)
                            SDL_CancelGPUCommandBuffer(command);
                        spdlog::error("XR image: {}", error.empty() ? SDL_GetError() : error);
                        code = 7;
                    }
                    else if (!code)
                    {
                        const bool ok = presenter->render_world
                                            ? presenter->record_eyes(*game, command, color, xr.color_format(),
                                                                     xr.width(), xr.height(), views, error)
                                            : clear_eyes(command, color);
                        if (!ok)
                        {
                            SDL_CancelGPUCommandBuffer(command);
                            spdlog::error("XR record: {}", error.empty() ? SDL_GetError() : error);
                            code = 5;
                        }
                        else if (!SDL_SubmitGPUCommandBuffer(command))
                        {
                            spdlog::error("XR submit: {}", SDL_GetError());
                            code = 7;
                        }
                        else
                            drawn = true;
                    }
                    record_seconds += std::chrono::duration<double>(Clock::now() - start).count();
                }
                if (!xr.end_frame(frame, drawn, error))
                {
                    spdlog::error("XR end frame: {}", error);
                    code = 7;
                    break;
                }
                ++frames;
                rendered += drawn;
                const double elapsed = std::chrono::duration<double>(Clock::now() - window_start).count();
                if (elapsed >= 5)
                {
                    const auto& p = presenter->stats;
                    spdlog::info("XR world: {:.1f} frames/s ({} rendered), step {:.2f} ms, update {:.2f} ms, record "
                                 "{:.2f} ms, {} draw calls, {} culled, {} passes, phase {}",
                                 frames / elapsed, rendered, frames ? step_seconds * 1e3 / frames : 0.0,
                                 rendered ? update_seconds * 1e3 / rendered : 0.0,
                                 rendered ? record_seconds * 1e3 / rendered : 0.0, p.draw_calls, p.culled, p.passes,
                                 xr_phase_name(xr.phase()));
                    window_start = Clock::now();
                    frames = rendered = 0;
                    step_seconds = update_seconds = record_seconds = 0;
                }
            }
            // Presentation resources go before the device, the world before the session.
            presenter.reset();
            if (!game->world_path.empty() && !game->unload_world())
                spdlog::warn("XR world: unload refused during callbacks");
            game.reset();
            if (!code)
                spdlog::info("XR world: session ended");
        }
        SDL_Quit();
        return code;
    }
} // namespace OpenBlood2
