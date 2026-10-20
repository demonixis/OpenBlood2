// GPU skinning must reproduce the CPU-posed model path: the same ModelView inputs are
// rendered through WorldRenderer (CPU vertices) and ModelRenderer (node palettes) and
// compared pixel by pixel.
#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/model_renderer.h"
#include "engine/runtime.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << '\n';                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr Uint32 width = 320, height = 240;
    constexpr auto format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    struct Comparison
    {
        std::size_t filled_cpu = 0, filled_gpu = 0, differing = 0;
        int max_delta = 0;
    };
    bool read(Runtime& runtime, SDL_GPUCommandBuffer* commands, Capture& capture, std::vector<Uint8>& out)
    {
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
            return false;
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = width;
        target.rows_per_layer = height;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(runtime.gpu))
            return false;
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        if (!pixels)
            return false;
        out.assign(pixels, pixels + width * height * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        return true;
    }
    // Renders both paths for the same draws and camera and compares the results.
    bool compare(Runtime& runtime, WorldRenderer& cpu_renderer, ModelRenderer& gpu_renderer, GpuRegistry& registry,
                 const ModelView& cpu, const ModelView& gpu, std::size_t entry, DVector eye, DRotation view,
                 Comparison& result, std::string& error, const char* save = nullptr)
    {
        result = {};
        const auto& a = cpu.entries[entry];
        const auto& b = gpu.entries[entry];
        if (a.range == UINT32_MAX || b.range == UINT32_MAX)
            return a.range == b.range && a.triangles == b.triangles && a.hidden_triangles == b.hidden_triangles;
        auto find = [&](const ModelView& scene)
        {
            const auto& list = scene.entries[entry].close ? scene.close_draws : scene.world_draws;
            return std::find_if(list.begin(), list.end(),
                                [&](const WorldDraw& d) { return d.range == scene.entries[entry].range; });
        };
        const auto draw_a = find(cpu), draw_b = find(gpu);
        Capture first, second;
        if (!first.initialize(runtime.gpu, format, width, height) ||
            !second.initialize(runtime.gpu, format, width, height) ||
            !cpu_renderer.prepare(cpu.mesh, cpu.revision, format, width, height, error, true) ||
            !gpu_renderer.prepare(gpu, format, width, height, error) || !registry.flush(nullptr, error))
            return false;
        std::vector<Uint8> cpu_pixels, gpu_pixels;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands ||
            !cpu_renderer.draw(commands, first.texture, cpu.mesh, std::span(&*draw_a, 1), eye, view, false, error,
                               DrawLoad::Clear) ||
            !read(runtime, commands, first, cpu_pixels))
            return false;
        commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        SDL_GPUColorTargetInfo color{};
        color.texture = second.texture;
        color.clear_color = {.025f, .035f, .05f, 1};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = cpu_renderer.depth_target();
        depth.clear_depth = 1;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.cycle = true;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        if (!pass)
            return false;
        const bool recorded =
            gpu_renderer.record({commands, pass}, gpu, std::span(&*draw_b, 1), eye, view, error, WorldPass::Opaque);
        SDL_EndGPURenderPass(pass);
        if (!recorded || !read(runtime, commands, second, gpu_pixels))
            return false;
        for (std::size_t i = 0; i < width * height; ++i)
        {
            const auto* p = &cpu_pixels[i * 4];
            const auto* q = &gpu_pixels[i * 4];
            result.filled_cpu += p[0] != 6 || p[1] != 9 || p[2] != 13;
            result.filled_gpu += q[0] != 6 || q[1] != 9 || q[2] != 13;
            int delta = 0;
            for (unsigned c = 0; c < 3; ++c)
                delta = std::max(delta, std::abs(int(p[c]) - int(q[c])));
            result.differing += delta > 1;
            result.max_delta = std::max(result.max_delta, delta);
        }
        if (save && (!second.save(save) || !first.save((std::string(save) + ".cpu.png").c_str())))
            return false;
        return true;
    }
    // Floating-point association differs between the CPU sampler and the vertex shader;
    // allow isolated edge/texel flips but no systematic geometric difference.
    bool equivalent(const Comparison& c)
    {
        const auto filled = std::max<std::size_t>(1, std::max(c.filled_cpu, c.filled_gpu));
        return c.filled_cpu > 0 &&
               std::max(c.filled_cpu, c.filled_gpu) - std::min(c.filled_cpu, c.filled_gpu) <= filled / 200 + 2 &&
               c.differing <= filled / 100 + 4;
    }
    DVector frame_eye(DVector position, DRotation rotation, DVector min, DVector max)
    {
        const auto center = rotate_vector(rotation, {(min.x + max.x) / 2, (min.y + max.y) / 2, (min.z + max.z) / 2});
        const float span = std::max({max.x - min.x, max.y - min.y, max.z - min.z, 1.f});
        return {position.x + center.x, position.y + center.y, position.z + center.z - span * 1.5f};
    }
} // namespace
int main(int argc, char** argv)
{
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    WorldPipelines pipelines(runtime.gpu);
    GpuRegistry registry(runtime.gpu);
    WorldRenderer cpu_renderer(runtime.gpu);
    ModelRenderer gpu_renderer(DrawStyle::Opaque, pipelines, registry);
    Resources resources;
    // Synthetic hierarchy: rotated parent, translated child with packed deformation,
    // non-uniform scale, an instance transform and every hidden-node combination.
    {
        Model model;
        ModelAnimation clip;
        clip.duration = 100;
        clip.keys.resize(2);
        clip.keys[1].time = 100;
        for (unsigned n = 0; n < 2; ++n)
        {
            ModelNode node;
            node.name = n ? "child" : "root";
            node.index = static_cast<std::uint16_t>(n ? 7 : 3);
            node.parent = n ? 0 : -1;
            NodeAnimation track;
            track.frames = {{{0, n ? 1.f : 0, 0}, rotation_from_euler({0, n ? .4f : .2f, n ? .3f : 0})},
                            {{n ? .5f : 0, n ? 1.5f : 0, 0}, rotation_from_euler({.3f, n ? -.2f : .6f, 0})}};
            for (unsigned v = 0; v < 6; ++v)
            {
                const float x = (v % 3 == 1 ? 1.f : 0.f) - .5f, y = (v % 3 == 2 ? 1.f : 0.f) - .25f;
                model.vertices.push_back({{x + (v / 3) * 1.2f - .6f, y, 0}, {}, static_cast<std::uint8_t>(node.index)});
            }
            if (n)
            {
                // Two deformed vertices, one repeated: the later packed write wins on both paths.
                node.deformed_vertices = {7, 9, 7};
                track.scale = {.01f, .01f, .01f};
                track.translation = {-.5f, -.25f, -.2f};
                for (unsigned frame = 0; frame < 2; ++frame)
                    for (unsigned slot = 0; slot < 3; ++slot)
                        for (unsigned axis = 0; axis < 3; ++axis)
                            track.deformation.push_back(
                                static_cast<std::uint8_t>(40 + 60 * slot + 50 * frame + 7 * axis));
            }
            model.nodes.push_back(node);
            clip.nodes.push_back(track);
            for (unsigned triangle = 0; triangle < 2; ++triangle)
            {
                ModelFace face;
                for (unsigned c = 0; c < 3; ++c)
                {
                    face.vertices[c] = static_cast<std::uint16_t>(n * 6 + triangle * 3 + c);
                    face.uv[c] = {float(c) * .5f, float(triangle)};
                }
                model.faces.push_back(face);
            }
        }
        // A face spanning both nodes is hidden when either node is hidden.
        model.faces.push_back({{{{0, 0}, {1, 0}, {0, 1}}}, {1, 6, 8}, {}});
        model.animations.push_back(clip);
        const std::shared_ptr<const Model> asset = std::make_shared<Model>(model);
        ModelView cpu, gpu;
        gpu.gpu_skinning = true;
        for (unsigned mask = 0; mask < 3; ++mask)
            for (double time : {0.0, 37.5, 100.0})
            {
                const std::array<std::uint8_t, 2> hidden{std::uint8_t(mask == 1), std::uint8_t(mask == 2)};
                AnimationTrack track;
                track.time = time;
                const NodePose pose{{.3f, -.2f, 4}, rotation_from_euler({.1f, .5f, -.2f})};
                for (auto* scene : {&cpu, &gpu})
                {
                    scene->begin_frame();
                    CHECK(scene->add(asset, track, "missing.dtx", pose, {1.5f, .75f, 1}, true, false, {}, "synthetic",
                                     resources, error, hidden));
                }
                CHECK(gpu.entries[0].triangles == cpu.entries[0].triangles &&
                      gpu.entries[0].hidden_triangles == cpu.entries[0].hidden_triangles);
                CHECK(gpu.deformation.size() == 3 && gpu.palette.size() == 4);
                Comparison c;
                CHECK(compare(runtime, cpu_renderer, gpu_renderer, registry, cpu, gpu, 0, {}, {}, c, error));
                std::cout << "Synthetic mask " << mask << " time " << time << ": " << c.filled_cpu << "/"
                          << c.filled_gpu << " filled, " << c.differing << " differing, max delta " << c.max_delta
                          << '\n';
                CHECK(equivalent(c));
            }
        // Borrowed models are not GPU skinned; the entry stays inspectable.
        gpu.begin_frame();
        CHECK(gpu.add(&model, {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "borrowed", resources, error));
        CHECK(!gpu.entries[0].error.empty() && gpu.entries[0].range == UINT32_MAX && gpu.world_draws.empty());
    }
    if (argc == 3 || argc == 4)
    {
        // Every distinct retail asset of both worlds, at three clip times.
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = argv[2];
        CHECK(game.resources.mount_game(config, error));
        std::size_t compared = 0, filled = 0, differing = 0;
        int max_delta = 0;
        const bool undead = argc == 4 && std::string_view(argv[3]) == "undead";
        const bool enhanced = argc == 4 && std::string_view(argv[3]) == "enhanced";
        const bool ishmael = argc == 4 && std::string_view(argv[3]) == "ishmael";
        const bool chosen = argc == 4 && std::string_view(argv[3]) == "chosen";
        const auto worlds = undead              ? std::vector<const char*>{"24_ancientcity"}
                            : enhanced          ? std::vector<const char*>{"23_rooftop"}
                            : chosen || ishmael ? std::vector<const char*>{"end", "24b_arena"}
                                                : std::vector<const char*>{"01_SubwayA", "02_CityHub"};
        for (const auto* world : worlds)
        {
            CHECK(game.request_world(world, error));
            ModelView probe;
            CHECK(update_model_scene(game, probe, error));
            std::vector<std::string> seen;
            for (std::size_t i = 0; i < probe.entries.size(); ++i)
            {
                auto* r = game.objects.get(probe.entries[i].object);
                if (!r || !r->model || probe.entries[i].range == UINT32_MAX ||
                    std::find(seen.begin(), seen.end(), probe.entries[i].label) != seen.end())
                    continue;
                if (chosen && std::string_view(r->state.m_Filename) != "Models/Characters/gabriella.abc" &&
                    std::string_view(r->state.m_Filename) != "Models/Characters/ophelia.abc")
                    continue;
                if (ishmael && std::string_view(r->state.m_Filename) != "Models/Characters/ishmael.abc")
                    continue;
                if (undead && std::string_view(r->state.m_Filename) != "Models/Enemies/Undead_Gideon.abc")
                    continue;
                if (enhanced && std::string_view(r->state.m_SkinName) != "Skins/Enemies/GideonE.dtx")
                    continue;
                seen.push_back(probe.entries[i].label);
                const auto& clip = r->model->animations[r->animation.animation];
                const auto original = r->animation;
                for (double fraction : {0.0, 0.37, 1.0})
                {
                    r->animation.time = clip.duration * fraction;
                    ModelView cpu, gpu;
                    gpu.gpu_skinning = true;
                    CHECK(update_model_scene(game, cpu, error) && update_model_scene(game, gpu, error));
                    CHECK(cpu.entries.size() == gpu.entries.size());
                    const auto eye = frame_eye(r->state.m_Pos, r->state.m_Rotation,
                                               {clip.min.x * r->state.m_Scale.x, clip.min.y * r->state.m_Scale.y,
                                                clip.min.z * r->state.m_Scale.z},
                                               {clip.max.x * r->state.m_Scale.x, clip.max.y * r->state.m_Scale.y,
                                                clip.max.z * r->state.m_Scale.z});
                    Comparison c;
                    const auto path = std::string(argv[1]) + "-" + std::to_string(compared) + ".png";
                    CHECK(compare(runtime, cpu_renderer, gpu_renderer, registry, cpu, gpu, i, eye, {}, c, error,
                                  fraction == .37 ? path.c_str() : nullptr));
                    if (!equivalent(c))
                        std::cerr << "Mismatch " << world << " " << probe.entries[i].label << " t=" << r->animation.time
                                  << ": " << c.filled_cpu << "/" << c.filled_gpu << " filled, " << c.differing
                                  << " differing, max " << c.max_delta << '\n';
                    CHECK(equivalent(c));
                    filled += c.filled_cpu;
                    differing += c.differing;
                    max_delta = std::max(max_delta, c.max_delta);
                    cpu_renderer.clear_geometry();
                }
                r->animation = original;
                ++compared;
            }
            CHECK(game.unload_world());
            gpu_renderer.clear_geometry();
        }
        std::cout << "Retail CPU/GPU skinning equivalence: " << compared << " distinct assets at three clip times, "
                  << filled << " filled pixels, " << differing << " differing by more than 1, max delta " << max_delta
                  << '\n';
        CHECK(undead || enhanced ? compared == 1 : ishmael ? compared == 2 : chosen ? compared == 4 : compared >= 10);
    }
    gpu_renderer.clear_geometry();
    cpu_renderer.clear_geometry();
    std::cout << "GPU skinning matches CPU posing, deformation, scale, hidden faces and instance transforms\n";
    return 0;
}
