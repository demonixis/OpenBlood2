#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "game/local_scene.h"
#include "game/localized_font.h"
#include "game/localized_signs.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <iostream>
#include <memory>

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
    bool download(SDL_GPUDevice* gpu, SDL_GPUCommandBuffer* commands, Capture& capture, std::vector<Uint8>& pixels)
    {
        auto* copy = SDL_BeginGPUCopyPass(commands);
        if (!copy)
            return false;
        SDL_GPUTextureRegion source{};
        source.texture = capture.texture;
        source.w = capture.width;
        source.h = capture.height;
        source.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = capture.width;
        target.rows_per_layer = capture.height;
        SDL_DownloadFromGPUTexture(copy, &source, &target);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(commands) || !SDL_WaitForGPUIdle(gpu))
            return false;
        const auto* data = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu, capture.transfer, false));
        if (!data)
            return false;
        pixels.assign(data, data + std::size_t(capture.width) * capture.height * 4);
        SDL_UnmapGPUTransferBuffer(gpu, capture.transfer);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    const bool demo = std::string_view(argv[1]) == "demo";
    const bool billboard = std::string_view(argv[1]) == "billboard";
    std::string error;
    Runtime runtime;
    CHECK(runtime.initialize(true, error));
    LocalizedFont font;
    CHECK(font.load(error));
    Config config;
    config.data_dir = path_from_utf8(argv[2]);
    config.demo = demo;
    auto game = std::make_unique<GameSession>();
    CHECK(game->initialize() && game->resources.mount_game(config, error) &&
          game->request_world(billboard ? "02_CityHub" : first_level(demo), error) && game->run_world(error));
    for (int i = 0; i < 4; ++i)
        CHECK(step_app_session(*game, 1.0 / 60));
    CHECK(game->publish_client_world(error));
    const auto time = game->objects.time();
    auto scene = local_scene(*game);
    // A green EXIT sign, SubwayA's No Spitting sign, or CityHub's six-tile billboard.
    const std::string_view path = billboard ? "textures/blueyellow/boardf1.dtx"
                                : demo    ? "textures/reds/fexit.dtx"
                                          : "textures/blueyellow/nospitting2.dtx";
    scene.view = {};
    scene.view.position = billboard ? DVector{-2944, 1504, -960} : demo ? DVector{-450, 2704, -2192}
                                                                      : DVector{20, 216, -3288};
    scene.view.rotation = rotation_from_euler({0, billboard ? 0.f : demo ? -1.570796327f : 1.570796327f, 0});
    scene.weapons_visible = false;
    Texture original;
    CHECK(read_texture_resource(game->resources, path, original, error));
    constexpr auto format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    Capture capture;
    CHECK(capture.initialize(runtime.gpu, format, 960, 720));
    Localization catalog;
    ScenePresenter presenter(runtime.gpu);
    presenter.model_shadows = false;
    std::vector<Uint8> baseline, pixels;
    const auto render = [&](bool use_original)
    {
        presenter.set_texture_decoration(sign_texture_decoration(catalog, font));
        if (!presenter.update(scene, error) || !presenter.render_world || !presenter.rebuilt)
            return false;
        const auto material = std::ranges::find(presenter.world_mesh.materials, path, &WorldMaterial::path);
        if (material == presenter.world_mesh.materials.end())
            return false;
        const auto& image = material->image;
        if (image.width != original.width || image.height != original.height || image.mipmaps != original.mipmaps ||
            (image.rgba == original.rgba) != use_original || (use_original && image.mip_rgba != original.mip_rgba))
            return false;
        auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        return commands && presenter.record(scene, commands, capture.texture, format, capture.width, capture.height,
                                             error) &&
               download(runtime.gpu, commands, capture, pixels);
    };
    CHECK(catalog.load(Language::English, error) && render(true));
    baseline = pixels;
    const auto output = path_from_utf8(argv[3]);
    std::filesystem::create_directories(output);
    CHECK(capture.save(path_text(output / "en.png").c_str()));
    for (const auto language : languages().subspan(1))
    {
        CHECK(catalog.load(language, error) && render(false) && pixels != baseline && game->objects.time() == time);
        CHECK(capture.save(path_text(output / (std::string(language_tag(language)) + ".png")).c_str()));
    }
    CHECK(catalog.load_catalog(Language::French, "[signs]\nother_sign=Autre panneau", error) &&
          render(true) && pixels == baseline);
    CHECK(catalog.load(Language::English, error) && render(true) && pixels == baseline && game->objects.time() == time);
    return 0;
}
