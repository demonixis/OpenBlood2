#include "app/sprite_scene.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "weapon_sprite_fixture.h"
#include <cmath>
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
    bool render(Runtime& runtime, WorldRenderer& renderer, SpriteView& scene, DVector eye, DRotation camera,
                Capture& capture, std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, scene.mesh, scene.draws, eye, camera, false, error))
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 400;
        region.h = 300;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 400;
        target.rows_per_layer = 300;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(runtime.gpu))
            return false;
        auto* data = static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        if (!data)
            return false;
        pixels.assign(data, data + 400 * 300 * 4);
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    Runtime runtime;
    std::string error;
    CHECK(runtime.initialize(true, error));
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    Capture capture;
    SpriteView scene;
    Resources resources;
    CHECK(test_weapon_sprite::mount(resources, argv[1], error));
    SpriteStore store(resources);
    SmokeImpactSettings settings;
    settings.position = {0, 0, 2};
    settings.scale1 = settings.scale2 = {.25f, .5f, 0};
    settings.duration = 1;
    settings.alpha = .65f;
    settings.fade = true;
    settings.color = {.4f, .8f, .6f};
    CSmokeImpactFX smoke;
    CHECK(smoke.Init(settings, error) && smoke.CreateObject(store, 0, error) && smoke.Update(0) && smoke.Update(.5));
    std::vector<std::uint8_t> white, tinted;
    for (bool tint : {false, true})
    {
        CHECK(scene.begin_frame({}, {}, error) &&
              scene.add(smoke.sprite.get(), smoke.track, {smoke.position, {}}, smoke.scale, smoke.flags, {}, resources,
                        error, tint ? smoke.alpha : 1, tint ? smoke.color : DVector{1, 1, 1}));
        CHECK(scene.entries.back().error.empty() &&
              render(runtime, renderer, scene, {}, {}, capture, tint ? tinted : white, error));
    }
    unsigned verified = 0;
    for (unsigned i = 0; i < 400 * 300; ++i)
    {
        const auto k = i * 4;
        if (white[k] == 255 && white[k + 1] == 0 && white[k + 2] == 0)
        {
            CHECK(std::abs(int(tinted[k]) - (255 * smoke.color.x * smoke.alpha + 6 * (1 - smoke.alpha))) <= 2);
            CHECK(std::abs(int(tinted[k + 1]) - 9 * (1 - smoke.alpha)) <= 2 &&
                  std::abs(int(tinted[k + 2]) - 13 * (1 - smoke.alpha)) <= 2);
            ++verified;
        }
    }
    CHECK(verified > 100 && capture.save((std::filesystem::path(argv[1]) / "smoke-tint-alpha.png").c_str()));
    std::cout << "Smoke RGB / original fade: " << verified << " exact opaque-texel blend checks\n";
    if (argc == 3)
    {
        GameSession g;
        Config c;
        c.data_dir = argv[2];
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto h = g.server_shell.Player(g.local_client)->m_hObject;
            CWeaponFX request;
            request.m_nAmmoType = AMMO_BULLET;
            request.m_eSurfaceType = SURFTYPE_WOOD;
            request.m_vDestPos = {0, 5000, 0};
            std::minstd_rand random(cycle + 1);
            CHECK(request.CreateSprites(g.server, h, {0, 0, -1}, random) && g.step(.01));
            CHECK(g.server.weapon_sprites.flashes.size() == 1 && g.server.weapon_sprites.smoke.size() == 1);
            CHECK(g.server.weapon_sprites.smoke.front().id > previous);
            previous = g.server.weapon_sprites.smoke.front().id;
            for (bool flash : {true, false})
            {
                const auto& f = g.server.weapon_sprites.flashes.front();
                const auto& s = g.server.weapon_sprites.smoke.front();
                const auto pos = flash ? f.settings.position : s.position;
                const auto eye = vector_add(pos, {0, 0, -35});
                CHECK(scene.begin_frame(eye, {}, error));
                CHECK(scene.add(flash ? f.sprite.get() : s.sprite.get(), flash ? f.track : s.track, {pos, {}},
                                flash ? f.scale : s.scale, flash ? f.flags : s.flags, {}, g.resources, error,
                                flash ? f.alpha : s.alpha, flash ? DVector{1, 1, 1} : s.color));
                CHECK(scene.entries[0].error.empty() && scene.draws.size() == 1);
                std::vector<std::uint8_t> pixels;
                CHECK(render(runtime, renderer, scene, eye, {}, capture, pixels, error));
                unsigned ink = 0;
                for (unsigned k = 0; k < pixels.size(); k += 4)
                    if (pixels[k] != 6 || pixels[k + 1] != 9 || pixels[k + 2] != 13)
                        ++ink;
                CHECK(ink > 50 && ink < 400 * 300);
                CHECK(capture.save(
                    (std::filesystem::path(argv[1]) / (flash ? "private-flash.png" : "private-smoke.png")).c_str()));
                std::cout << "Private cycle " << cycle << (flash ? " flash " : " smoke ") << scene.entries[0].frame_path
                          << " changed pixels " << ink << "\n";
            }
            CHECK(g.leave_player() && g.step(.2) && g.server.weapon_sprites.flashes.empty() &&
                  !g.server.weapon_sprites.smoke.empty());
            CHECK(g.unload_world() && g.server.weapon_sprites.smoke.empty());
            scene.clear();
            renderer.clear_geometry();
            CHECK(update_sprite_scene(g, scene, error) && scene.entries.empty());
        }
    }
    return 0;
}
