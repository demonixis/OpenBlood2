#include "app/sprite_scene.h"
#include "energy_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << " " << SDL_GetError() << "\n";                            \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    GameSession game;
    if (argc == 3)
    {
        Config c;
        c.data_dir = argv[2];
        CHECK(game.initialize() && game.resources.mount_game(c, error));
        WorldModel floor;
        floor.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(game.collision.build(floor, error));
        game.server.collision_ready = true;
    }
    else
        CHECK(test_player::setup(game, argv[1]) && test_energy::mount(game.resources, argv[1], error));
    auto* owner = test_energy::actor(game, {0, 50, 0});
    CHECK(owner);
    auto* projectile = test_energy::projectile(game, owner->m_hObject, {0, 50, 20}, {0, 0, 1}, 100);
    CHECK(projectile);
    auto h = projectile->m_hObject;
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    SpriteView scene;
    std::vector<Uint8> blank;
    unsigned previous = 0;
    for (unsigned stage = 0; stage < 3; ++stage)
    {
        CHECK(game.objects.advance(stage == 0 ? .005 : .01));
        if (stage == 2)
            CHECK(game.server.advance_projectiles(.1));
        auto* record = game.objects.get(h);
        CHECK(record && record->sprite);
        CHECK(scene.begin_frame({0, 50, 0}, {}, error));
        CHECK(scene.add(record->sprite.get(), record->sprite_track, {record->state.m_Pos, record->state.m_Rotation},
                        record->state.m_Scale, record->state.m_Flags, record->id, game.resources, error));
        CHECK(scene.entries.size() == 1 && scene.entries[0].error.empty());
        CHECK(stage == 0 ? scene.draws.empty() : scene.draws.size() == 1);
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240));
        CHECK(
            renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 320, 240, error, true));
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(commands);
        CHECK(renderer.draw(commands, capture.texture, scene.mesh, scene.draws, {0, 50, 0}, {}, false, error));
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
        unsigned changed = 0;
        if (stage == 0)
            blank.assign(pixels, pixels + 320 * 240 * 4);
        else
            for (unsigned i = 0; i < 320 * 240; ++i)
                if (!std::equal(pixels + i * 4, pixels + i * 4 + 3, blank.data() + i * 4))
                    ++changed;
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(stage == 0 || changed > 20);
        if (stage == 2)
            CHECK(changed < previous);
        previous = changed;
        std::error_code ec;
        std::filesystem::create_directories(argv[1], ec);
        CHECK(!ec);
        CHECK(capture.save((std::filesystem::path(argv[1]) / ("energy-" + std::to_string(stage) + ".png")).c_str()));
        std::cout << "Stage " << stage << ": scale " << record->state.m_Scale.x << ", z " << record->state.m_Pos.z
                  << ", changed pixels " << changed << "\n";
    }
    CHECK(game.objects.remove(h) && game.objects.finish_frame());
    CHECK(update_sprite_scene(game, scene, error) && scene.entries.empty());
    renderer.clear_geometry();
    CHECK(game.unload_world());
    return 0;
}
