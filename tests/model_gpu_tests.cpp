#include "app/model_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
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
WorldMesh quad(float z, float radius, DVector color, float x = 0)
{
    WorldMesh m;
    for (auto p : {DVector{-radius, -radius, z}, DVector{radius, -radius, z}, DVector{radius, radius, z},
                   DVector{-radius, -radius, z}, DVector{radius, radius, z}, DVector{-radius, radius, z}})
        m.vertices.push_back({{p.x + x, p.y, p.z}, color, {}});
    m.ranges.push_back({0, 6, {}, "quad", {}});
    return m;
}
int main(int argc, char** argv)
{
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    WorldRenderer world(runtime.gpu), models(runtime.gpu);
    WorldDraw draw;
    auto background = quad(4, 2, {0, 0, 1});
    for (int frame = 0; frame < 4; ++frame)
    {
        const Uint32 width = frame == 2 ? 240 : 320, height = frame == 2 ? 320 : 240;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height));
        auto mesh =
            quad(frame == 1 || frame == 2 ? 2 : 6, frame == 1 || frame == 2 ? 1 : 3, {1, 0, 0}, frame == 2 ? 1.5f : 0);
        CHECK(world.prepare(background, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, error));
        CHECK(models.prepare(mesh, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, error, true));
        CHECK(models.stats.uploads == 1); // Animated geometry updates do not reload textures.
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        CHECK(world.draw(commands, capture.texture, background, std::span(&draw, 1), {}, {}, false, error));
        CHECK(models.draw(commands, capture.texture, mesh, std::span(&draw, 1), {}, {}, false, error,
                          frame == 3 ? DrawLoad::ClearDepth : DrawLoad::Load,
                          frame == 3 ? nullptr : world.depth_target()));
        auto* copy = SDL_BeginGPUCopyPass(commands);
        CHECK(copy);
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
        CHECK(SDL_SubmitGPUCommandBuffer(commands));
        CHECK(SDL_WaitForGPUIdle(runtime.gpu));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        auto rgb = [&](Uint32 x, Uint32 y, bool red)
        {
            auto* p = pixels + (y * width + x) * 4;
            return p[0] == (red ? 255 : 0) && p[1] == 0 && p[2] == (red ? 0 : 255) && p[3] == 255;
        };
        const bool correct = rgb(width / 2, height / 2, frame == 1 || frame == 3) &&
                             (frame != 2 || rgb(width * 7 / 8, height / 2, true));
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        if (argc == 2)
            CHECK(capture.save(argv[1]));
        CHECK(correct);
    }
    // Render two rigid node groups through ModelView, then hide each and restore.
    // Pixels, not only counters, prove the visibility mask reaches SDL_GPU.
    {
        Model model;
        ModelAnimation animation;
        animation.duration = 100;
        animation.keys.resize(2);
        animation.keys[1].time = 100;
        for (unsigned side = 0; side < 2; ++side)
        {
            ModelNode node;
            node.name = side ? "right" : "left";
            node.index = side;
            model.nodes.push_back(node);
            NodeAnimation track;
            track.frames.resize(2);
            animation.nodes.push_back(track);
            auto mesh = quad(3, .75f, {1, 1, 1}, side ? 1.5f : -1.5f);
            for (const auto& v : mesh.vertices)
            {
                ModelVertex vertex;
                vertex.position = v.position;
                vertex.node = side;
                model.vertices.push_back(vertex);
            }
            for (unsigned triangle = 0; triangle < 2; ++triangle)
            {
                ModelFace face;
                for (unsigned c = 0; c < 3; ++c)
                    face.vertices[c] = side * 6 + triangle * 3 + c;
                model.faces.push_back(face);
            }
        }
        model.animations.push_back(animation);
        Resources resources;
        ModelView scene;
        models.clear_geometry();
        for (unsigned frame = 0; frame < 4; ++frame)
        {
            const std::array<std::uint8_t, 2> hidden{std::uint8_t(frame == 1 || frame == 2), std::uint8_t(frame == 2)};
            scene.begin_frame();
            CHECK(scene.add(&model, {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "node pixel probe", resources,
                            error, hidden));
            CHECK(scene.entries[0].hidden_triangles == (frame == 1 ? 2 : frame == 2 ? 4 : 0));
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
            CHECK(models.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error,
                                 true));
            auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(commands &&
                  models.draw(commands, capture.texture, scene.mesh, scene.world_draws, {}, {}, false, error));
            auto* copy = SDL_BeginGPUCopyPass(commands);
            CHECK(copy);
            SDL_GPUTextureRegion region{};
            region.texture = capture.texture;
            region.w = 320;
            region.h = 240;
            region.d = 1;
            SDL_GPUTextureTransferInfo target{};
            target.transfer_buffer = capture.transfer;
            target.pixels_per_row = 320;
            target.rows_per_layer = 240;
            SDL_DownloadFromGPUTexture(copy, &region, &target);
            SDL_EndGPUCopyPass(copy);
            CHECK(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(runtime.gpu));
            auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
            CHECK(pixels);
            bool valid = true;
            for (unsigned side = 0; side < 2; ++side)
            {
                const auto* pixel = pixels + (120 * 320 + (side ? 240 : 80)) * 4;
                valid &= hidden[side] ? pixel[0] == 6 && pixel[1] == 9 && pixel[2] == 13
                                      : pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 255;
            }
            SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
            CHECK(valid);
        }
        std::cout << "Per-node GPU pixels: two visible / left hidden / all hidden / restored; dynamic upload without "
                     "texture revision\n";
    }
    if (argc == 3)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = argv[2];
        CHECK(game.resources.mount_game(config, error));
        CHECK(game.request_world("01_SubwayA", error));
        CHECK(game.enter_player(error) && game.step(1. / 60));
        PlayerRestoreState restored;
        CHECK(game.capture_player(restored, error));
        const auto previous_player = game.client_shell.player;
        CHECK(game.leave_player() && !game.objects.get(previous_player));
        CHECK(game.restore_player(restored, error) && game.client_shell.restored_entry);
        CHECK(game.client_shell.player != previous_player && game.restoration.restored == 1);
        CHECK(game.server_shell.Player(game.local_client)->respawns == 0);
        std::cout << "Restored original player/client association before private GPU rendering\n";
        game.client_shell.player_visible = true;
        game.client_shell.weapons_visible = false;
        ModelView scene;
        CHECK(update_model_scene(game, scene, error));
        CHECK(scene.world_draws.size() == 27);
        const auto* player = game.objects.get(game.client_shell.player);
        CHECK(player);
        DVector eye = player->state.m_Pos;
        eye.y += 20;
        eye.z -= 120;
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 500));
        models.clear_geometry();
        CHECK(models.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 500, error, true));
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        CHECK(models.draw(commands, capture.texture, scene.mesh, scene.world_draws, eye, {}, false, error));
        auto* copy = SDL_BeginGPUCopyPass(commands);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 400;
        region.h = 500;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 400;
        target.rows_per_layer = 500;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(commands));
        CHECK(SDL_WaitForGPUIdle(runtime.gpu));
        auto* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(pixels);
        std::size_t filled = 0;
        for (std::size_t i = 0; i < 400 * 500; ++i)
            filled += pixels[i * 4] != 6 || pixels[i * 4 + 1] != 9 || pixels[i * 4 + 2] != 13;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(capture.save(argv[1]));
        CHECK(filled > 1000);
        std::cout << "Private isolated Caleb/held Beretta diagnostic: " << filled << " non-background pixels, "
                  << scene.mesh.vertices.size() << " vertices\n";
        CHECK(game.unload_world());
        scene.clear();
        models.clear_geometry();
    }
    models.clear_geometry();
    world.clear_geometry();
    std::cout << "Dynamic vertices, unchanged texture revision, shared world/model occlusion, resize and close-view "
                 "depth isolation pass\n";
    return 0;
}
