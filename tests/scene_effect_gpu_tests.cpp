#include "engine/capture.h"
#include "engine/mark_clip.h"
#include "engine/model_renderer.h"
#include "engine/runtime.h"
#include "engine/transparency.h"
#include "engine/world_renderer.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
    bool download(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* cmd, Capture& c, std::vector<Uint8>& pixels)
    {
        auto* pass = SDL_BeginGPUCopyPass(cmd);
        if (!pass)
            return false;
        SDL_GPUTextureRegion source{};
        source.texture = c.texture;
        source.w = c.width;
        source.h = c.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = c.transfer;
        target.pixels_per_row = c.width;
        target.rows_per_layer = c.height;
        SDL_DownloadFromGPUTexture(pass, &source, &target);
        SDL_EndGPUCopyPass(pass);
        if (!SDL_SubmitGPUCommandBuffer(cmd) || !SDL_WaitForGPUIdle(gpu))
            return false;
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, c.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + c.width * c.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, c.transfer);
        return true;
    }
    WorldMesh quad()
    {
        WorldMesh m;
        for (DVector p : {DVector{-5, 5, 5}, {5, 5, 5}, {5, -5, 5}, {-5, 5, 5}, {5, -5, 5}, {-5, -5, 5}})
            m.vertices.push_back({p, {.2f, .2f, .2f}, {0, 0}});
        m.ranges.push_back({0, 6, {}, "dark wall", {{0, 6, 0, false, {0, 0, 5}}}});
        return m;
    }
} // namespace
int main()
{
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu), sprites(runtime.gpu, DrawStyle::SpriteAlpha);
    auto mesh = quad();
    WorldDraw draw;
    Capture capture;
    CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
    CHECK(renderer.prepare(mesh, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    CHECK(sprites.prepare(mesh, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    std::vector<Uint8> pixels, baseline;
    for (unsigned mode = 0; mode < 8; ++mode)
    {
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        SDL_GPUColorTargetInfo color{};
        color.texture = capture.texture;
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = renderer.depth_target();
        depth.clear_depth = 1;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
        CHECK(pass);
        WorldRecordTarget target{commands, pass};
        SceneEffects fx;
        if (mode != 0 && mode != 6)
        {
            fx.lights = {{{{0, 0, 5}, {.2f, 0, 0}, 2}, 0}, {{{0, 0, 5}, {0, .3f, 0}, 2}, 0}};
        }
        if (mode == 2 || mode == 3 || mode == 4)
        {
            fx.uniform.fog_color = {0, 0, 1, 1};
            fx.uniform.fog_eye_near = {0, 0, 0, 0};
            fx.uniform.fog_forward_far = {0, 0, 1, 10};
        }
        if (mode == 7)
            fx.lights[0].flags = fx.lights[1].flags = 64;
        if (mode != 0)
            target.effects = &fx;
        auto& r = mode == 4 || mode == 5 ? sprites : renderer;
        CHECK(r.record(target, mesh, std::span(&draw, 1), {}, {}, error, WorldPass::Opaque, 0, {}, mode == 3));
        SDL_EndGPURenderPass(pass);
        CHECK(download(runtime.gpu, commands, capture, pixels));
        const auto at = (120 * 320 + 160) * 4;
        std::cout << mode << ": " << unsigned(pixels[at]) << ',' << unsigned(pixels[at + 1]) << ','
                  << unsigned(pixels[at + 2]) << '\n';
        if (mode == 0)
            baseline = pixels;
        if (mode == 6 || mode == 3 || mode == 5 || mode == 7)
            CHECK(pixels == baseline);
        if (mode == 1)
            CHECK(std::abs(int(pixels[at]) - 102) < 3 && std::abs(int(pixels[at + 1]) - 127) < 3 &&
                  pixels[at + 2] == 51);
        if (mode == 2)
            CHECK(std::abs(int(pixels[at]) - 51) < 3 && std::abs(int(pixels[at + 1]) - 64) < 3 &&
                  std::abs(int(pixels[at + 2]) - 153) < 3);
        if (mode == 4)
            CHECK(std::abs(int(pixels[at]) - 26) < 3 && std::abs(int(pixels[at + 1]) - 26) < 3 &&
                  std::abs(int(pixels[at + 2]) - 153) < 3);
    }
    // Red/blue sprites in one uploaded material batch straddle a green world face.
    // Expected alpha-over composition is red -> green -> blue, with unchanged opaque depth.
    WorldMesh alpha;
    for (auto [z, tint] : {std::pair{6.f, DVector{1, 0, 0}}, {4.f, DVector{0, 0, 1}}, {5.f, DVector{0, 1, 0}}})
        for (auto v : quad().vertices)
        {
            v.position.z = z;
            v.color = tint;
            alpha.vertices.push_back(v);
        }
    alpha.ranges.push_back({0, 12, {}, "sprite batch", {{0, 12, 0, false, {0, 0, 5}}}});
    alpha.ranges.push_back({12, 6, {}, "world face", {{12, 6, 0, true, {0, 0, 5}}}});
    CHECK(renderer.prepare(alpha, 2, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    CHECK(sprites.prepare(alpha, 2, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    std::vector<TransparentDraw> queue{{TransparentKind::Sprite, {0, {}, {}, .5f, 0, 0, 6}, 6},
                                       {TransparentKind::Sprite, {0, {}, {}, .5f, 0, 6, 6}, 4},
                                       {TransparentKind::World, {1, {}, {}, .5f, 0}, 5}};
    sort_transparency(queue);
    auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
    CHECK(commands);
    SDL_GPUColorTargetInfo color{};
    color.texture = capture.texture;
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = renderer.depth_target();
    depth.clear_depth = 1;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
    CHECK(pass);
    WorldRecordTarget target{commands, pass};
    for (const auto& item : queue)
    {
        auto& r = item.kind == TransparentKind::World ? renderer : sprites;
        CHECK(r.record(target, alpha, std::span(&item.draw, 1), {}, {}, error,
                       item.kind == TransparentKind::World ? WorldPass::Translucent : WorldPass::Opaque));
    }
    auto invalid = queue[0].draw;
    invalid.vertex_count = 18;
    CHECK(!sprites.record(target, alpha, std::span(&invalid, 1), {}, {}, error));
    SDL_EndGPURenderPass(pass);
    CHECK(download(runtime.gpu, commands, capture, pixels));
    const auto at = (120 * 320 + 160) * 4;
    std::cout << "Interleaved: " << unsigned(pixels[at]) << ',' << unsigned(pixels[at + 1]) << ','
              << unsigned(pixels[at + 2]) << '\n';
    CHECK(std::abs(int(pixels[at]) - 32) < 3 && std::abs(int(pixels[at + 1]) - 64) < 3 &&
          std::abs(int(pixels[at + 2]) - 128) < 3);

    auto mark = quad();
    std::vector<WorldVertex> clipped;
    const std::vector<DVector> polygon{{-5, -5, 6.25f}, {0, -5, 6.25f}, {0, 5, 6.25f}, {-5, 5, 6.25f}};
    CHECK(clip_mark(mark.vertices, {}, polygon, clipped, error));
    mark.vertices = clipped;
    mark.ranges[0].count = mark.ranges[0].batches[0].count = static_cast<std::uint32_t>(clipped.size());
    CHECK(sprites.prepare(mark, 3, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
    CHECK(commands);
    pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
    CHECK(pass);
    target.commands = commands;
    target.pass = pass;
    CHECK(sprites.record(target, mark, std::span(&draw, 1), {}, {}, error));
    SDL_EndGPURenderPass(pass);
    CHECK(download(runtime.gpu, commands, capture, pixels));
    CHECK(pixels[(120 * 320 + 100) * 4] == 51 && pixels[(120 * 320 + 220) * 4] == 0);
    // One static world range has two spatially separate material batches. Neither
    // receiver may lose its light to the other receiver's >32 live candidates.
    WorldMesh receivers;
    for (float x : {-3.f, 3.f})
        for (auto vertex : quad().vertices)
        {
            vertex.position.x = vertex.position.x / 5 + x;
            vertex.position.y /= 5;
            receivers.vertices.push_back(vertex);
        }
    receivers.ranges.push_back(
        {0, 12, {}, "two world batches", {{0, 6, 0, false, {-3, 0, 5}}, {6, 6, 0, false, {3, 0, 5}}}});
    CHECK(renderer.prepare(receivers, 4, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error));
    GpuRegistry registry(runtime.gpu);
    WorldPipelines pipelines(runtime.gpu);
    ModelRenderer models(DrawStyle::Opaque, pipelines, registry);
    auto asset = std::make_shared<Model>();
    asset->nodes.push_back({});
    for (auto vertex : quad().vertices)
        asset->vertices.push_back({{vertex.position.x / 5, vertex.position.y / 5, 0}, {0, 0, -1}, 0});
    asset->faces.push_back({{}, {0, 1, 2}, {0, 0, -1}});
    asset->faces.push_back({{}, {3, 4, 5}, {0, 0, -1}});
    ModelView instances;
    instances.gpu_skinning = true;
    instances.palette = {{0, 0, 0, 1}, {0, 0, 0, 0}};
    for (float x : {-3.f, 3.f})
    {
        SkinnedInstance instance;
        instance.model = asset.get();
        instance.owner = asset;
        instance.min = {-1, -1, 0};
        instance.max = {1, 1, 0};
        instance.bounded = true;
        instance.center = {x, 0, 5};
        instance.light.ambient = {.2f, .2f, .2f};
        instances.world_draws.push_back({static_cast<std::uint32_t>(instances.instances.size()), {x, 0, 5}, {}});
        instances.instances.push_back(instance);
    }
    CHECK(models.prepare(instances, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error) &&
          registry.flush(nullptr, error));
    for (bool model : {false, true})
        for (auto budget : {mobile_effect_lights, desktop_effect_lights})
        {
            SceneEffects effects;
            effects.light_budget = budget;
            // World-only lights are stronger but cannot consume the model's budget.
            if (model)
                for (unsigned i = 0; i < 40; ++i)
                    effects.lights.push_back({{{-3, 0, 5}, {0, 0, 1}, 1}, 32});
            for (unsigned i = 0; i < 40; ++i)
                effects.lights.push_back({{{-3, 0, 5}, {.01f, 0, 0}, 1}, 0});
            effects.lights.push_back({{{3, 0, 5}, {0, .4f, 0}, 1}, 0});
            commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(commands);
            pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
            CHECK(pass);
            target = {commands, pass};
            target.effects = &effects;
            if (model)
                CHECK(models.record(target, instances, instances.world_draws, {}, {}, error));
            else
                CHECK(renderer.record(target, receivers, std::span(&draw, 1), {}, {}, error));
            SDL_EndGPURenderPass(pass);
            CHECK(download(runtime.gpu, commands, capture, pixels));
            const auto left = (120 * 320 + 64) * 4, right = (120 * 320 + 256) * 4;
            const double radial = 1 - std::sqrt(2.) / 64;
            CHECK(std::abs(int(pixels[left]) - int(std::lround(255 * (.2 + budget * .01 * radial)))) <= 2);
            CHECK(pixels[left + 1] == 51 && pixels[left + 2] == 51);
            CHECK(pixels[right] == 51 &&
                  std::abs(int(pixels[right + 1]) - int(std::lround(255 * (.2 + .4 * radial)))) <= 2);
            CHECK(effects.selection.receivers == 2 && effects.selection.selected == budget + 1 &&
                  effects.selection.max_selected == budget && effects.selection.dropped == 40 - budget);
            std::cout << (model ? "Models" : "World batches") << ": " << budget
                      << " per receiver, distant light retained\n";
        }
    return 0;
}
