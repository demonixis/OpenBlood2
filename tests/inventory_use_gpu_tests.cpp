#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include "game/original/BoneLeech.h"
#include <SDL3/SDL.h>
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
    bool frame(SDL_GPUDevice* gpu, ScenePresenter& presenter, GameSession& game, Capture& capture,
               std::vector<Uint8>& pixels, std::string& error)
    {
        if (!presenter.update(game, error) || !presenter.render_world)
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(gpu);
        if (!commands || !presenter.record(game, commands, capture.texture, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                           capture.width, capture.height, error))
            return false;
        auto* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion source{};
        source.texture = capture.texture;
        source.w = capture.width;
        source.h = capture.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = capture.transfer;
        destination.pixels_per_row = capture.width;
        destination.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &source, &destination);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(gpu))
            return false;
        auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, capture.transfer, false));
        if (!bytes)
            return false;
        pixels.assign(bytes, bytes + std::size_t(capture.width) * capture.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, capture.transfer);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 3)
        return 2;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(gpu);
    std::cout << "GPU=" << SDL_GetGPUDeviceDriver(gpu) << '\n';
    {
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) &&
              game->request_world("02_CityHub", error) && game->run_world(error));
        for (int i = 0; i < 30; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        ScenePresenter presenter(gpu);
        presenter.model_shadows = false;
        const std::filesystem::path root = argv[2];
        std::filesystem::create_directories(root);
        std::vector<Uint8> plain, night, eye, blind, close;
        CHECK(frame(gpu, presenter, *game, capture, plain, error) && capture.save((root / "plain.png").c_str()));
        auto* p = game->server_shell.Player(game->local_client);
        auto& c = game->client_shell;
        CHECK(p && p->m_InventoryMgr.AddItem(3, 1) == InventoryResult::Ok && p->OnCommandItem(3));
        CHECK(game->pump_player_messages(error));
        CHECK(frame(gpu, presenter, *game, capture, night, error) && capture.save((root / "night.png").c_str()));
        std::uint64_t normal_red = 0, night_red = 0, normal_blue = 0, night_blue = 0, night_green = 0;
        for (std::size_t i = 0; i < plain.size(); i += 4)
        {
            normal_blue += plain[i];
            night_blue += night[i];
            normal_red += plain[i + 2];
            night_red += night[i + 2];
            night_green += night[i + 1];
        }
        CHECK(night_red * 4 < normal_red && night_blue * 4 < normal_blue && night_green > night_red * 2);
        bool glow = false;
        for (const auto& entry : presenter.model_scene.entries)
            if (entry.object && entry.range < presenter.model_scene.instances.size())
                if (const auto* r = game->objects.get(entry.object); r && (r->user_flags & (1u << 10)))
                {
                    const auto& instance = presenter.model_scene.instances[entry.range];
                    CHECK(instance.material == 0 && instance.light.ambient.y == 1 && instance.light.ambient.x == 0);
                    glow = true;
                }
        CHECK(glow);
        CHECK(p->OnCommandItem(3) && game->pump_player_messages(error));
        CHECK(p->m_InventoryMgr.AddItem(4, 1) == InventoryResult::Ok && p->OnCommandItem(4));
        for (int i = 0; i < 20; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        CHECK(p->OnCommandItem(4));
        CHECK(game->pump_player_messages(error) && c.seeing_eye && !c.weapons_visible);
        CHECK(frame(gpu, presenter, *game, capture, eye, error) && capture.save((root / "eye.png").c_str()));
        std::size_t green = 0, colored = 0;
        for (std::size_t i = 0; i < eye.size(); i += 4)
        {
            green += eye[i + 1];
            colored += eye[i] + eye[i + 2];
        }
        CHECK(colored > 10000 && green < colored / 20);
        CHECK(p->OnCommandItem(4) && game->pump_player_messages(error));
        c.blind_until = game->server.GetTime() + 10;
        CHECK(frame(gpu, presenter, *game, capture, blind, error));
        std::size_t black = 0;
        for (std::size_t i = 0; i < blind.size(); i += 4)
            black += !blind[i] && !blind[i + 1] && !blind[i + 2];
        CHECK(black > blind.size() / 4 * 9 / 10);
        c.blind_until = 0;
        ObjectCreateStruct create;
        create.m_Pos = c.position;
        auto h = game->objects.create(game->objects.find_class("BoneLeech"), create, Creation::Runtime, {});
        auto* b = h ? dynamic_cast<BoneLeech*>(game->server.HandleToObject(h)) : nullptr;
        CHECK(b && b->RequestAttach(p->m_hObject) && b->AI_STATE_EnemyAttach());
        CHECK(game->pump_player_messages(error) && c.creature == h);
        CHECK(frame(gpu, presenter, *game, capture, close, error) && capture.save((root / "leech.png").c_str()));
        bool shown = false;
        for (const auto& entry : presenter.model_scene.entries)
            if (const auto* r = game->objects.get(entry.object); r && r->handle == h)
                shown = entry.visible && entry.close && entry.triangles;
        CHECK(shown && close != plain);
        CHECK(game->unload_world());
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    return 0;
}
