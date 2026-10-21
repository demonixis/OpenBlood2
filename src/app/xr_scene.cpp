#include "app/xr_scene.h"
#include "build_info.h"
#include "engine/world_shaders.h"
#include "engine/xr/xr_runtime.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
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
            // Linear colours: the swapchain is sRGB.
            std::uint32_t first = begin();
            for (int i = -10; i <= 10; ++i)
            {
                const float t = i * 0.5f;
                const std::array<float, 4> color =
                    i % 2 ? std::array<float, 4>{0.05f, 0.05f, 0.07f, 1} : std::array<float, 4>{0.16f, 0.16f, 0.2f, 1};
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
                target.clear_color = {0.004f, 0.004f, 0.012f, 1};
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
} // namespace OpenBlood2
