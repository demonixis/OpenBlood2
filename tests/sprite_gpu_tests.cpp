#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "sprite_view_fixture.h"
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << " " << SDL_GetError() << "\n";                         \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
namespace
{
    WorldMesh quad(float z, DVector color)
    {
        WorldMesh mesh;
        for (auto p : {DVector{-20, 20, z}, DVector{20, 20, z}, DVector{20, -20, z}, DVector{-20, 20, z},
                       DVector{20, -20, z}, DVector{-20, -20, z}})
            mesh.vertices.push_back({p, color, {}});
        mesh.ranges.push_back({0, 6, {}, "solid", {}});
        return mesh;
    }
    bool download(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* commands, Capture& capture)
    {
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
            return false;
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = capture.width;
        region.h = capture.height;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = capture.width;
        target.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        return SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(gpu);
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    WorldRenderer world(runtime.gpu), sprites(runtime.gpu, DrawStyle::SpriteAlpha), overlay(runtime.gpu);
    auto background = quad(4, {0, 0, 1});
    auto after = quad(2.5f, {1, 1, 0});
    WorldDraw draw;
    Resources resources;
    CHECK(test_sprite_view::mount(resources, argv[1], error));
    auto sprite = test_sprite_view::manifest();
    SpriteView scene;
    for (unsigned frame = 0; frame < 6; ++frame)
    {
        const unsigned width = frame == 4 ? 400 : 320, height = frame == 4 ? 300 : 240;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height));
        CHECK(scene.begin_frame({}, {}, error));
        CHECK(scene.add(&sprite, {}, {{0, 0, frame == 1 ? 6.f : 2.f}, {}}, {.25f, .5f, 1},
                        FLAG_VISIBLE | FLAG_SPRITECHROMAKEY, {}, resources, error, frame == 5 ? .5f : 1.f));
        if (frame == 2)
        {
            // Deliberately append farther sprite last, scaled to the same screen extent.
            CHECK(scene.add(&sprite, {}, {{0, 0, 3}, {}}, {.375f, .75f, 1}, FLAG_VISIBLE, {}, resources, error));
            scene.sort();
            CHECK(scene.draws[0].position.z == 3);
        }
        if (frame == 4)
            sprites.clear_geometry();
        CHECK(world.prepare(background, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, error));
        CHECK(sprites.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, error,
                              true));
        CHECK(overlay.prepare(after, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, error));
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        CHECK(world.draw(commands, capture.texture, background, std::span(&draw, 1), {}, {}, false, error));
        CHECK(sprites.draw(commands, capture.texture, scene.mesh, scene.draws, {}, {}, false, error, DrawLoad::Load,
                           world.depth_target()));
        if (frame == 3)
            CHECK(overlay.draw(commands, capture.texture, after, std::span(&draw, 1), {}, {}, false, error,
                               DrawLoad::Load, world.depth_target()));
        CHECK(download(runtime.gpu, commands, capture));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        bool valid = true;
        for (unsigned col = 0; col < 4; ++col)
        {
            const auto* pixel = pixels + (height / 2 * width + width * (5 + 2 * col) / 16) * 4;
            const int green = frame == 2 ? 199 : frame == 5 ? 68 : 136, blue = frame == 2 ? 56 : frame == 5 ? 187 : 119;
            const int red = frame == 3 ? 255 : (frame != 1 && col == 2 ? (frame == 5 ? 128 : 255) : 0);
            const int g = frame == 3 ? 255 : (frame != 1 && col == 1 ? green : 0);
            const int b = frame == 1 || col == 0 ? 255 : (col == 1 ? blue : frame == 5 ? 128 : 0);
            const int expected_b = frame == 3 ? 0 : b;
            if (std::abs(int(pixel[0]) - red) > 1 || std::abs(int(pixel[1]) - g) > 1 ||
                std::abs(int(pixel[2]) - expected_b) > 1 || pixel[3] != 255)
            {
                valid = false;
                std::cerr << "frame " << frame << " column " << col << " RGB " << int(pixel[0]) << "," << int(pixel[1])
                          << "," << int(pixel[2]) << "\n";
            }
        }
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(valid);
        CHECK(sprites.stats.uploads == (frame >= 4 ? 2 : 1));
        CHECK(capture.save((std::filesystem::path(argv[1]) / "alpha.png").c_str()));
    }
    std::cout << "Pixels: zero/partial/opaque/opaque-black DTX alpha, per-instance opacity, overlap sort, world "
                 "occlusion, no depth writes, "
                 "resize and reupload passed\n";
    if (argc == 3)
    {
        GameSession game;
        Config config;
        config.data_dir = argv[2];
        CHECK(game.initialize() && game.resources.mount_game(config, error) && game.request_world("01_SubwayA", error));
        CHECK(update_sprite_scene(game, scene, error) && scene.entries.size() == 12);
        const auto id = scene.entries.front().object;
        const auto* record = game.objects.get(id);
        CHECK(record && record->sprite);
        const auto original = record->state;
        auto normal = rotate_vector(original.m_Rotation, {0, 0, 1});
        const DVector eye{original.m_Pos.x - 200 * normal.x, original.m_Pos.y - 200 * normal.y,
                          original.m_Pos.z - 200 * normal.z};
        scene.clear();
        CHECK(scene.begin_frame(eye, original.m_Rotation, error));
        CHECK(scene.add(record->sprite.get(), record->sprite_track, {original.m_Pos, original.m_Rotation},
                        original.m_Scale, original.m_Flags, id, game.resources, error));
        CHECK(scene.draws.size() == 1 && scene.entries[0].zero_alpha > 0 && scene.entries[0].partial_alpha > 0);
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300));
        CHECK(sprites.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true));
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        CHECK(sprites.draw(commands, capture.texture, scene.mesh, scene.draws, eye, original.m_Rotation, false, error));
        CHECK(download(runtime.gpu, commands, capture));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        unsigned ink = 0, clear = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (pixels[i * 4] == 6 && pixels[i * 4 + 1] == 9 && pixels[i * 4 + 2] == 13)
                ++clear;
            else
                ++ink;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(ink > 50 && clear > 100000);
        CHECK(capture.save((std::filesystem::path(argv[1]) / "bloodsplat.png").c_str()));
        CHECK(game.unload_world());
        scene.clear();
        sprites.clear_geometry();
        CHECK(update_sprite_scene(game, scene, error) && scene.draws.empty() && scene.entries.empty());
        std::cout << "Private authored sprite pose/scale: " << ink << " non-background pixels, " << clear
                  << " clear; unload emptied CPU/GPU geometry\n";
    }
    return 0;
}
