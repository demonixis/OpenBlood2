// Display post-processing (roadmap M6) on retail worlds. With anti-aliasing off the original
// renderer keeps its exact frame whatever the bloom/tonemap settings; MSAA and FXAA only move
// edge pixels, and FXAA leaves flat areas untouched. With the enhanced renderer, the neutral
// tonemap keeps every value below its compression start and never brightens, bloom only adds
// light and adds none without emission, and returning to the plain settings restores the frame
// and releases the intermediate targets. Ambient occlusion only darkens, and only the
// enhanced renderer's opaque, non-emissive light. Screen-space reflections stay small with the
// built-in materials and show on glossy ones.
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/capture.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <regex>
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
    struct Difference
    {
        double mean = 0, moved = 0;             // Mean channel difference; share of pixels moved by more than 16.
        std::size_t brighter = 0, darker = 0;   // Pixels whose channel sum rose/fell by more than 3.
        int most_brighter = 0, most_darker = 0; // Largest single-channel rise/fall.
    };
    Difference compare(const std::vector<Uint8>& a, const std::vector<Uint8>& b)
    {
        Difference result;
        std::uint64_t sum = 0, moved = 0;
        for (std::size_t i = 0; i < a.size(); i += 4)
        {
            int total = 0, largest = 0;
            for (int c = 0; c < 3; ++c)
            {
                const int d = int(b[i + c]) - int(a[i + c]);
                sum += unsigned(std::abs(d));
                total += d;
                largest = std::max(largest, std::abs(d));
                result.most_brighter = std::max(result.most_brighter, d);
                result.most_darker = std::max(result.most_darker, -d);
            }
            moved += largest > 16;
            result.brighter += total > 3;
            result.darker += total < -3;
        }
        const auto pixels = double(a.size() / 4);
        result.mean = double(sum) / (pixels * 3);
        result.moved = double(moved) / pixels;
        return result;
    }
    // B8G8R8A8 luma, as FXAA computes it from display values.
    float luma(const std::vector<Uint8>& p, std::size_t i)
    {
        return (0.299f * p[i + 2] + 0.587f * p[i + 1] + 0.114f * p[i]) / 255.f;
    }
    std::ostream& operator<<(std::ostream& out, const Difference& d)
    {
        return out << "mean " << d.mean << ", moved " << d.moved << ", brighter " << d.brighter << ", darker "
                   << d.darker;
    }
} // namespace
int main(int argc, char** argv)
{
    std::string error;
    if (argc != 2)
        return 2;
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    auto* gpu = SDL_CreateGPUDevice(WorldShaderFormats, true, nullptr);
    CHECK(gpu);
    std::cout << "GPU=" << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu), SDL_PROP_GPU_DEVICE_NAME_STRING, "?")
              << " backend=" << SDL_GetGPUDeviceDriver(gpu) << '\n';
    const PostSettings plain{Antialiasing::Off, false, Tonemap::Off, AmbientOcclusion::Off, Reflections::Off};
    for (const char* world : {"02_CityHub", "01_SubwayA"})
    {
        Config config;
        config.data_dir = argv[1];
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize() && game->resources.mount_game(config, error) && game->request_world(world, error) &&
              game->run_world(error));
        for (int i = 0; i < 30; ++i)
            CHECK(step_app_session(*game, 1.0 / 60));
        Capture capture;
        CHECK(capture.initialize(gpu, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, 960, 540));
        const auto width = std::size_t(capture.width), height = std::size_t(capture.height);
        ScenePresenter presenter(gpu);
        const auto& stats = presenter.post_process.stats;

        // Original renderer: bloom and tonemap never apply, the scene pass writes the output.
        std::vector<Uint8> original, same, msaa, fxaa, again;
        CHECK(frame(gpu, presenter, *game, capture, original, error));
        CHECK(!stats.hdr && !stats.bloom && !stats.fxaa && stats.samples == 1 && stats.passes == 0 &&
              stats.targets == 0);
        presenter.post = plain;
        CHECK(frame(gpu, presenter, *game, capture, same, error) && same == original);
        presenter.post.antialiasing = Antialiasing::Msaa4;
        CHECK(frame(gpu, presenter, *game, capture, msaa, error));
        const auto msaa_samples = stats.samples;
        const auto msaa_difference = compare(original, msaa);
        CHECK(stats.requested_samples == 4 && stats.passes == 0 && !stats.hdr);
        CHECK(msaa_samples == 1 ? msaa == original : msaa != original);
        CHECK(msaa_difference.mean < 1.5 && msaa_difference.moved < .03);
        presenter.post.antialiasing = Antialiasing::Fxaa;
        CHECK(frame(gpu, presenter, *game, capture, fxaa, error));
        CHECK(stats.fxaa && stats.passes == 1 && stats.targets == 1);
        const auto fxaa_difference = compare(original, fxaa);
        CHECK(fxaa != original && fxaa_difference.mean < 2 && fxaa_difference.moved < .03);
        // FXAA keeps every pixel whose local contrast stays under its edge threshold.
        std::size_t flat = 0;
        for (std::size_t y = 1; y + 1 < height; ++y)
            for (std::size_t x = 1; x + 1 < width; ++x)
            {
                const auto i = (y * width + x) * 4;
                const float m = luma(original, i), n = luma(original, i - width * 4), s = luma(original, i + width * 4),
                            w = luma(original, i - 4), e = luma(original, i + 4);
                const float highest = std::max({m, n, s, w, e}), lowest = std::min({m, n, s, w, e});
                if (highest - lowest >= std::max(.0312f, highest * .125f) - .004f)
                    continue;
                ++flat;
                for (int c = 0; c < 3; ++c)
                    CHECK(std::abs(int(fxaa[i + c]) - int(original[i + c])) <= 1);
            }
        CHECK(flat > width * height / 4);
        presenter.post = {};
        CHECK(frame(gpu, presenter, *game, capture, again, error) && again == original && stats.targets == 0);
        std::cout << world << ": original MSAA " << msaa_samples << "x (" << msaa_difference << "), FXAA ("
                  << fxaa_difference << "), " << flat << " flat pixels unchanged\n";

        // Enhanced renderer.
        presenter.shading = SurfaceShading::Standard;
        std::vector<Uint8> direct, neutral, bloom, filmic, combined, restored;
        presenter.post = plain;
        CHECK(frame(gpu, presenter, *game, capture, direct, error));
        CHECK(!stats.hdr && stats.passes == 0 && stats.targets == 0 && !presenter.pipelines.aux());
        presenter.post.tonemap = Tonemap::Neutral;
        CHECK(frame(gpu, presenter, *game, capture, neutral, error));
        CHECK(stats.hdr && !stats.bloom && stats.tonemap == Tonemap::Neutral && stats.passes == 1 &&
              presenter.pipelines.format() == scene_hdr_format && !presenter.pipelines.aux());
        // Neutral compresses only above 0.76 linear (about 225 in display values) and never brightens.
        // The HDR target does not clamp a translucent draw's colour before blending, so highlights
        // carried by enhanced translucent faces (coverage raised to the reflection) keep their
        // energy: a bounded share of pixels may be brighter.
        std::size_t kept = 0, compressed = 0, unclamped = 0;
        for (std::size_t i = 0; i < direct.size(); i += 4)
        {
            const bool low = std::max({direct[i], direct[i + 1], direct[i + 2]}) < 220;
            bool differs = false;
            for (int c = 0; c < 3; ++c)
                differs |= int(neutral[i + c]) > int(direct[i + c]) + 1 ||
                           (low && std::abs(int(neutral[i + c]) - int(direct[i + c])) > 1);
            unclamped += differs;
            kept += low && !differs;
            compressed +=
                !low && (neutral[i] != direct[i] || neutral[i + 1] != direct[i + 1] || neutral[i + 2] != direct[i + 2]);
        }
        CHECK(unclamped < width * height / 100 && kept > width * height / 2);
        presenter.post.bloom = true;
        CHECK(frame(gpu, presenter, *game, capture, bloom, error));
        CHECK(stats.bloom && presenter.pipelines.aux() && stats.bloom_levels == PostProcess::bloom_level_limit &&
              stats.passes == 2 * stats.bloom_levels);
        const auto levels = stats.bloom_levels;
        // Bloom only adds light.
        const auto bloom_difference = compare(neutral, bloom);
        CHECK(bloom_difference.most_darker <= 1 && bloom_difference.brighter > 50);
        presenter.post.tonemap = Tonemap::Filmic;
        CHECK(frame(gpu, presenter, *game, capture, filmic, error));
        const auto filmic_difference = compare(bloom, filmic);
        CHECK(stats.tonemap == Tonemap::Filmic && filmic_difference.mean > 1);
        // MSAA resolves the three scene targets, FXAA follows the composite.
        presenter.post = {Antialiasing::Msaa4, true, Tonemap::Neutral, AmbientOcclusion::Off, Reflections::Off};
        CHECK(frame(gpu, presenter, *game, capture, combined, error));
        CHECK((presenter.pipelines.samples() != SDL_GPU_SAMPLECOUNT_1) == (stats.samples > 1) &&
              stats.targets == (stats.samples > 1 ? 12u : 9u));
        const auto combined_difference = compare(bloom, combined);
        CHECK(combined_difference.mean < 1.5 && combined_difference.moved < .03);
        presenter.post.antialiasing = Antialiasing::Fxaa;
        CHECK(frame(gpu, presenter, *game, capture, combined, error));
        CHECK(stats.fxaa && stats.hdr && stats.passes == 2 * stats.bloom_levels + 1);
        // Ambient occlusion only darkens, by a bounded amount, at both qualities.
        std::vector<Uint8> occluded, occluded_low, everything;
        presenter.post = {Antialiasing::Off, false, Tonemap::Neutral, AmbientOcclusion::High, Reflections::Off};
        CHECK(frame(gpu, presenter, *game, capture, occluded, error));
        // The depth pass, the pyramid levels above 0, GTAO, two blurs and the composite; the pyramid
        // and its separate levels add depth_levels targets.
        CHECK(stats.ambient_occlusion == AmbientOcclusion::High && presenter.pipelines.aux() && !stats.bloom &&
              stats.depth_levels == PostProcess::depth_level_limit && stats.passes == 4 + stats.depth_levels &&
              stats.targets == 6 + stats.depth_levels);
        const auto occlusion_difference = compare(neutral, occluded);
        CHECK(occlusion_difference.most_brighter <= 1 && occlusion_difference.darker > width * height / 50 &&
              occlusion_difference.mean < 8);
        presenter.post.ambient_occlusion = AmbientOcclusion::Low;
        CHECK(frame(gpu, presenter, *game, capture, occluded_low, error));
        const auto low_difference = compare(neutral, occluded_low);
        CHECK(stats.ambient_occlusion == AmbientOcclusion::Low && low_difference.most_brighter <= 1 &&
              low_difference.darker > width * height / 50 && occluded_low != occluded);
        // Reflections replace the environment light of the environment specular: small with the
        // built-in materials, which have few glossy surfaces.
        std::vector<Uint8> reflected, glossy, glossy_reflected;
        presenter.post = {Antialiasing::Off, false, Tonemap::Neutral, AmbientOcclusion::Off, Reflections::High};
        CHECK(frame(gpu, presenter, *game, capture, reflected, error));
        CHECK(stats.reflections == Reflections::High && presenter.pipelines.reflections() &&
              stats.passes == 4 + stats.depth_levels && stats.targets == 8 + stats.depth_levels);
        const auto reflection_difference = compare(neutral, reflected);
        CHECK(reflection_difference.mean < 2 && reflection_difference.moved < .02);
        // Every effect at once.
        presenter.post = {Antialiasing::Msaa4, true, Tonemap::Filmic, AmbientOcclusion::High, Reflections::High};
        CHECK(frame(gpu, presenter, *game, capture, everything, error));
        CHECK(stats.passes == 2 * stats.bloom_levels + 6 + stats.depth_levels &&
              stats.targets == (stats.samples > 1 ? 21u : 17u) + stats.depth_levels);
        presenter.post.antialiasing = Antialiasing::Fxaa;
        CHECK(frame(gpu, presenter, *game, capture, everything, error));
        CHECK(stats.passes == 2 * stats.bloom_levels + 7 + stats.depth_levels);
        // With every material glossy, the reflections show.
        const std::string mirror = std::regex_replace(std::string(builtin_surface_manifest()),
                                                      std::regex(R"("roughness": [0-9.]+)"), "\"roughness\": 0.1");
        CHECK(presenter.surface_manifest.parse(mirror, error));
        presenter.post = {Antialiasing::Off, false, Tonemap::Neutral, AmbientOcclusion::Off, Reflections::Off};
        CHECK(frame(gpu, presenter, *game, capture, glossy, error));
        presenter.post.reflections = Reflections::High;
        CHECK(frame(gpu, presenter, *game, capture, glossy_reflected, error));
        const auto glossy_difference = compare(glossy, glossy_reflected);
        CHECK(glossy_difference.brighter + glossy_difference.darker > width * height / 100);
        CHECK(presenter.surface_manifest.parse_builtin(error));
        // Without emission the glow target stays empty: bloom adds nothing.
        const std::string unlit =
            std::regex_replace(std::string(builtin_surface_manifest()),
                               std::regex(R"(, "emissive": [0-9.]+, "emissive_threshold": [0-9.]+)"), "");
        CHECK(unlit.find("emissive\"") == std::string::npos && presenter.surface_manifest.parse(unlit, error));
        std::vector<Uint8> dark_neutral, dark_bloom;
        presenter.post = {Antialiasing::Off, false, Tonemap::Neutral, AmbientOcclusion::Off, Reflections::Off};
        CHECK(frame(gpu, presenter, *game, capture, dark_neutral, error));
        presenter.post.bloom = true;
        CHECK(frame(gpu, presenter, *game, capture, dark_bloom, error));
        const auto dark_difference = compare(dark_neutral, dark_bloom);
        CHECK(dark_difference.most_brighter <= 1 && dark_difference.most_darker <= 1);
        CHECK(presenter.material_report.emissive == 0 && presenter.surface_manifest.parse_builtin(error));
        // The plain settings return to the direct frame and release every intermediate target.
        presenter.post = plain;
        CHECK(frame(gpu, presenter, *game, capture, restored, error) && restored == direct && stats.targets == 0);
        presenter.shading = SurfaceShading::Original;
        presenter.post = {};
        CHECK(frame(gpu, presenter, *game, capture, again, error) && again == original);
        std::cout << world << ": neutral kept " << kept << " pixels, compressed " << compressed << ", " << unclamped
                  << " unclamped translucent; bloom (" << bloom_difference << ", " << levels << " levels); filmic ("
                  << filmic_difference << "); MSAA with bloom (" << combined_difference << "); occlusion high ("
                  << occlusion_difference << "), low (" << low_difference << "); reflections (" << reflection_difference
                  << "), glossy (" << glossy_difference << ")\n";
        game->unload_world();
    }
    SDL_DestroyGPUDevice(gpu);
    SDL_Quit();
    std::cout << "GPU post-processing leaves the original frame intact and only adds what each effect claims\n";
    return 0;
}
