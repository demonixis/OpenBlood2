#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "camera_fixture.h"
#include "engine/capture.h"
#include "engine/collision.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/Gib.h"
#include <algorithm>
#include <cmath>
#include <iostream>
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
    bool render(Runtime& runtime, WorldRenderer& renderer, const WorldMesh& mesh, std::uint64_t revision,
                const std::vector<WorldDraw>& draws, DVector eye, DRotation camera, Capture& capture,
                std::vector<std::uint8_t>& pixels, std::string& error)
    {
        if ((!capture.texture && !capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300)) ||
            !renderer.prepare(mesh, revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 400, 300, error, true))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        if (!commands)
            return false;
        if (!renderer.draw(commands, capture.texture, mesh, draws, eye, camera, false, error))
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
    if (argc != 3)
        return 2;
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    std::cout << "GPU driver: " << SDL_GetGPUDeviceDriver(runtime.gpu) << "\n";
    WorldRenderer renderer(runtime.gpu, DrawStyle::SpriteAlpha);
    GameSession game;
    CHECK(game.initialize());
    Config cfg;
    cfg.data_dir = argv[2];
    CHECK(game.resources.mount_game(cfg, error));
    CHECK(game.request_world("01_subwaya", error) && game.enter_player(error) &&
          test_camera::complete_subway_intro(game));
    auto* player = game.server_shell.Player(game.local_client);
    CHECK(player->m_InventoryMgr.ObtainWeapon(WEAP_SHOTGUN, 3) == InventoryResult::Ok);
    CHECK(game.select_weapon(3, error));
    auto& weapon = game.client_shell.weapons[0];
    CHECK(weapon.flash_error.empty());
    weapon.animation.animation = animation_index(*weapon.model, "static_model");
    weapon.animation.time = 0;
    // Isolate the real view-weapon presentation path from unrelated world sprites.
    for (auto id : game.objects.snapshot())
        if (auto* r = game.objects.get(id); r && r->state.m_ObjectType == OT_SPRITE)
            r->state.m_Flags &= ~FLAG_VISIBLE;
    game.server.reset_world_time();
    game.client_shell.view.position = game.client_shell.camera_position = {};
    game.client_shell.view.rotation = {};
    game.client_shell.rotation = {};
    game.client_shell.weapons_visible = true;
    Capture capture;
    std::vector<std::uint8_t> baseline, pixels, primary;
    SpriteView scene;
    CHECK(update_sprite_scene(game, scene, error));
    CHECK(render(runtime, renderer, scene.mesh, scene.revision, scene.draws, {}, {}, capture, baseline, error));
    std::filesystem::create_directories(argv[1]);
    for (unsigned mode = 1; mode <= 2; ++mode)
    {
        weapon.flash_visible = mode;
        CHECK(update_sprite_scene(game, scene, error));
        CHECK(scene.entries.back().range != UINT32_MAX && scene.entries.back().error.empty());
        CHECK(render(runtime, renderer, scene.mesh, scene.revision, scene.draws, {}, {}, capture, pixels, error));
        unsigned changed = 0;
        for (unsigned i = 0; i < 400 * 300; ++i)
            if (pixels[i * 4] != baseline[i * 4] || pixels[i * 4 + 1] != baseline[i * 4 + 1] ||
                pixels[i * 4 + 2] != baseline[i * 4 + 2])
                ++changed;
        CHECK(changed > 100);
        if (mode == 1)
            primary = pixels;
        else
            CHECK(primary != pixels);
        CHECK(capture.save(
            (std::filesystem::path(argv[1]) / (mode == 1 ? "primary.png" : "secondary.png")).string().c_str()));
        std::cout << "Muzzle " << mode << ": " << changed << " changed pixels\n";
    }
    weapon.flash_visible = 0;
    ShotgunEffectSettings shot;
    shot.source = {0, 50, 0};
    shot.destination = {0, 50, 100};
    shot.normal = {0, 0, -1};
    shot.forward = {0, 0, 1};
    shot.surface = SURFTYPE_STONE;
    shot.eject = true;
    CHECK(game.server.PublishShotgunEffects(player->m_hObject, shot, error) && error.empty());
    game.server.shotgun_effects.advance(game.objects.time() + .02, game.server);
    for (auto& c : game.server.shotgun_effects.casings)
        c.rotation = rotation_from_euler({.6f, .7f, .2f});
    ModelView models;
    CHECK(update_model_scene(game, models, error));
    std::vector<WorldDraw> casing_draws;
    for (const auto& entry : models.entries)
        if (entry.label == "Models/Ammo/sshell.abc")
        {
            CHECK(entry.error.empty() && entry.triangles > 0);
            for (const auto& draw : models.world_draws)
                if (draw.range == entry.range)
                    casing_draws.push_back(draw);
        }
    CHECK(casing_draws.size() == 2);
    DVector low{1e6f, 1e6f, 1e6f}, high{-1e6f, -1e6f, -1e6f};
    for (const auto& draw : casing_draws)
        for (const auto& vertex :
             std::span(models.mesh.vertices)
                 .subspan(models.mesh.ranges[draw.range].first, models.mesh.ranges[draw.range].count))
        {
            const auto point = vector_add(draw.position, rotate_vector(draw.rotation, vertex.position));
            low.x = std::min(low.x, point.x);
            low.y = std::min(low.y, point.y);
            low.z = std::min(low.z, point.z);
            high.x = std::max(high.x, point.x);
            high.y = std::max(high.y, point.y);
            high.z = std::max(high.z, point.z);
        }
    const float radius = std::max({high.x - low.x, high.y - low.y, high.z - low.z, 2.f});
    const DVector eye{(low.x + high.x) * .5f, (low.y + high.y) * .5f, (low.z + high.z) * .5f - radius * 1.5f};
    WorldRenderer casing_renderer(runtime.gpu, DrawStyle::Opaque);
    CHECK(
        render(runtime, casing_renderer, models.mesh, models.revision, casing_draws, eye, {}, capture, pixels, error));
    CHECK(capture.save((std::filesystem::path(argv[1]) / "casings.png").string().c_str()));
    CHECK(pixels != baseline);
    CHECK(capture.save((std::filesystem::path(argv[1]) / "casings.png").string().c_str()));
    CHECK(game.unload_world());
    return 0;
}
