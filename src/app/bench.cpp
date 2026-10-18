#include "app/bench.h"
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "engine/world_shaders.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    namespace
    {
        constexpr unsigned warmup_frames = 30; // First uploads and pipeline creation are reported separately.
        struct Series
        {
            std::vector<double> values;
            void add(double value)
            {
                values.push_back(value);
            }
            double mean() const
            {
                double sum = 0;
                for (auto value : values)
                    sum += value;
                return values.empty() ? 0 : sum / values.size();
            }
            double percentile(double p) const
            {
                if (values.empty())
                    return 0;
                auto sorted = values;
                std::sort(sorted.begin(), sorted.end());
                return sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(p * sorted.size()))];
            }
        };
        double milliseconds(Uint64 from, Uint64 to)
        {
            return static_cast<double>(to - from) / 1e6;
        }
        struct Device
        {
            SDL_GPUDevice* gpu = nullptr;
            SDL_GPUTexture* target = nullptr;
            ~Device()
            {
                if (target)
                    SDL_ReleaseGPUTexture(gpu, target);
                if (gpu)
                    SDL_DestroyGPUDevice(gpu);
                SDL_Quit();
            }
        };
    } // namespace

    int run_bench(const Options& options, const Config& config)
    {
        const unsigned frames = options.frame_limit ? options.frame_limit : 600;
        Device device;
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            spdlog::error("Bench SDL initialization: {}", SDL_GetError());
            return 3;
        }
        device.gpu = SDL_CreateGPUDevice(WorldShaderFormats, false, nullptr);
        constexpr auto format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        if (!device.gpu || !SDL_GPUTextureSupportsFormat(device.gpu, format, SDL_GPU_TEXTURETYPE_2D,
                                                         SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
        {
            spdlog::error("Bench GPU device: {}", SDL_GetError());
            return 3;
        }
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width = options.bench_width;
        info.height = options.bench_height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        device.target = SDL_CreateGPUTexture(device.gpu, &info);
        if (!device.target)
        {
            spdlog::error("Bench target: {}", SDL_GetError());
            return 3;
        }
        spdlog::info("Bench GPU: {} ({}), offscreen {}x{}, {} measured frames after {} warm-up",
                     SDL_GetGPUDeviceDriver(device.gpu),
                     SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                           "unreported"),
                     options.bench_width, options.bench_height, frames, warmup_frames);
        std::string error;
        auto game = std::make_unique<GameSession>();
        if (!game->initialize() || !game->resources.mount_game(config, error) ||
            !game->request_world(options.world, error) || !game->run_world(error))
        {
            spdlog::error("Bench session: {}", error);
            return 5;
        }
        {
            ScenePresenter presenter(device.gpu);
            Series step, world, models, effects, record, gpu, total, draws, culled;
            std::size_t passes = 0, max_draws = 0;
            const auto base = game->client_shell.view.rotation;
            for (unsigned frame = 0; frame < frames + warmup_frames; ++frame)
            {
                const auto begin = SDL_GetTicksNS();
                if (!step_app_session(*game, 1.0 / 60))
                {
                    spdlog::error("Bench simulation step failed at frame {}", frame);
                    return 5;
                }
                const auto stepped = SDL_GetTicksNS();
                if (!presenter.update(*game, error))
                {
                    spdlog::error("Bench {}", error);
                    return 5;
                }
                if (options.bench_sweep)
                {
                    // Presentation-only yaw sweep around the current view; simulation never reads it.
                    const double yaw = 3.14159265358979 * frame / (frames + warmup_frames);
                    game->client_shell.view.rotation =
                        compose_rotation({{0, float(std::sin(yaw)), 0}, float(std::cos(yaw))}, base);
                }
                auto* commands = SDL_AcquireGPUCommandBuffer(device.gpu);
                if (!commands)
                {
                    spdlog::error("Bench command buffer: {}", SDL_GetError());
                    return 3;
                }
                const auto updated = SDL_GetTicksNS();
                if (presenter.render_world && !presenter.record(*game, commands, device.target, format,
                                                                options.bench_width, options.bench_height, error))
                {
                    spdlog::error("Bench {}", error);
                    SDL_SubmitGPUCommandBuffer(commands);
                    return 3;
                }
                auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
                const auto submitted = SDL_GetTicksNS();
                if (!fence || !SDL_WaitForGPUFences(device.gpu, true, &fence, 1))
                {
                    spdlog::error("Bench GPU completion: {}", SDL_GetError());
                    return 3;
                }
                SDL_ReleaseGPUFence(device.gpu, fence);
                const auto completed = SDL_GetTicksNS();
                if (frame < warmup_frames)
                    continue;
                step.add(milliseconds(begin, stepped));
                world.add(presenter.stats.world_ns / 1e6);
                models.add(presenter.stats.models_ns / 1e6);
                effects.add(presenter.stats.effects_ns / 1e6);
                record.add(milliseconds(updated, submitted));
                gpu.add(milliseconds(submitted, completed));
                total.add(milliseconds(begin, completed));
                draws.add(double(presenter.stats.draw_calls));
                culled.add(double(presenter.stats.culled));
                passes = std::max(passes, presenter.stats.passes);
                max_draws = std::max(max_draws, presenter.stats.draw_calls);
            }
            auto row = [](const char* name, const Series& s)
            {
                spdlog::info("  {:<34} mean {:8.3f}  p50 {:8.3f}  p95 {:8.3f}  max {:8.3f}", name, s.mean(),
                             s.percentile(.5), s.percentile(.95), s.percentile(1));
            };
            spdlog::info("Bench {} (ms per frame):", game->world_path);
            row("simulation step", step);
            row("world/sky draw lists", world);
            row("models (CPU pose/vertices)", models);
            row("polygrids/sprites/particles", effects);
            row("record (uploads/encode/submit)", record);
            row("GPU completion wait", gpu);
            row("total serialized frame", total);
            spdlog::info(
                "Counters: {:.0f} draw calls mean ({} max), {:.0f} culled draw records mean, {} render passes, "
                "{} world draws, {} skinned model instances, {} skinned asset vertices, {} model upload bytes/frame, "
                "{} sprite draws, {} PolyGrid surfaces, {} world triangles",
                draws.mean(), max_draws, culled.mean(), passes, presenter.world_draws.size(),
                presenter.model_scene.instances.size() + presenter.explosion_model_scene.instances.size(),
                presenter.model_renderer.asset_vertices() + presenter.explosion_model_renderer.asset_vertices(),
                presenter.model_renderer.uploaded_bytes + presenter.explosion_model_renderer.uploaded_bytes,
                presenter.sprite_scene.draws.size(), presenter.polygrid_scene.draws.size(),
                presenter.world_renderer.stats.triangles);
        }
        game->unload_world();
        return 0;
    }
} // namespace OpenBlood2
