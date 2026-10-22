#pragma once
#include "engine/gpu_registry.h"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <span>
#include <string>

namespace OpenBlood2
{
    // Display post-processing (roadmap M6). Anti-aliasing applies to both renderers; the HDR
    // scene, bloom and tonemapping apply to the enhanced renderer only, so the original renderer
    // keeps its output unless anti-aliasing is selected.
    enum class Antialiasing
    {
        Off,
        Fxaa,
        Msaa2,
        Msaa4
    };
    enum class Tonemap
    {
        Off, // Clamp, as the original framebuffer.
        Neutral,
        Filmic
    };
    struct PostSettings
    {
        Antialiasing antialiasing = Antialiasing::Off;
        bool bloom = true;
        Tonemap tonemap = Tonemap::Neutral;
        bool operator==(const PostSettings&) const = default;
    };
    // Scene pass attachments of one frame, chosen by PostProcess::begin. Null resolve textures
    // mean single-sampled targets; a null glow means no glow target.
    struct SceneTarget
    {
        SDL_GPUTexture *color = nullptr, *resolve = nullptr;
        SDL_GPUTexture *glow = nullptr, *glow_resolve = nullptr;
        Uint32 layer = 0;
    };
    // The frame planned by the last begin() and recorded by finish(). Inspection/measurement only.
    struct PostStats
    {
        bool hdr = false, bloom = false, fxaa = false;
        Tonemap tonemap = Tonemap::Off;
        Uint32 samples = 1, requested_samples = 1; // MSAA: the device may support fewer.
        std::size_t bloom_levels = 0, passes = 0, targets = 0;
        std::uint64_t frames = 0;
    };
    inline constexpr SDL_GPUTextureFormat scene_hdr_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    // Owns the intermediate targets and pipelines between the scene pass and the output:
    // MSAA color/glow targets resolved at the end of the scene pass, the HDR scene, the bloom
    // chain, the composite (bloom and tonemapping) and FXAA. When nothing applies, the scene
    // pass draws straight into the output, as before post-processing.
    class PostProcess
    {
      public:
        explicit PostProcess(GpuRegistry&);
        ~PostProcess();
        PostProcess(const PostProcess&) = delete;
        PostProcess& operator=(const PostProcess&) = delete;
        // Plans one frame into an output of this format and size and prepares its targets and
        // pipelines (all post pipelines for an output format are created together).
        bool begin(SDL_GPUTextureFormat output, Uint32 width, Uint32 height, bool enhanced, const PostSettings&,
                   std::string& error);
        SDL_GPUTextureFormat scene_format() const
        {
            return scene_format_;
        }
        SDL_GPUSampleCount sample_count() const
        {
            return samples_;
        }
        bool glow() const
        {
            return bloom_;
        }
        // The scene pass attachments for this frame's output (valid after begin()).
        SceneTarget scene_target(SDL_GPUTexture* output) const;
        // Records the passes after the scene pass into output; none when the scene pass wrote it.
        bool finish(SDL_GPUCommandBuffer*, SDL_GPUTexture* output, std::string& error);
        PostStats stats;
        // Replacement policy: bloom strength over the whole chain, the coarsest level count and the
        // filmic exposure that keeps mid-grey (0.18) at 0.18 through the ACES fit.
        static constexpr float bloom_strength = 1.5f, filmic_exposure = 2.2f;
        static constexpr std::size_t bloom_level_limit = 6;

      private:
        bool prepare_pipelines(SDL_GPUTextureFormat output, std::string& error);
        bool target(TextureHandle&, SDL_GPUTextureFormat, Uint32 width, Uint32 height, SDL_GPUSampleCount,
                    std::string& error);
        void release_targets();
        bool draw(SDL_GPUCommandBuffer*, SDL_GPUTexture* target, bool load, SDL_GPUGraphicsPipeline*,
                  std::span<SDL_GPUTexture* const> sources, std::span<SDL_GPUSampler* const> samplers,
                  const void* uniform, Uint32 uniform_bytes, std::string& error);
        GpuRegistry& registry_;
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUGraphicsPipeline *down_ = nullptr, *up_ = nullptr, *composite_ = nullptr, *fxaa_ = nullptr;
        SDL_GPUTextureFormat pipeline_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        // Frame plan.
        SDL_GPUTextureFormat output_format_ = SDL_GPU_TEXTUREFORMAT_INVALID, scene_format_ = output_format_;
        SDL_GPUSampleCount samples_ = SDL_GPU_SAMPLECOUNT_1;
        bool hdr_ = false, bloom_ = false, fxaa_pass_ = false;
        Tonemap tonemap_ = Tonemap::Off;
        Uint32 width_ = 0, height_ = 0;
        struct
        {
            SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
            Uint32 requested = 0;
            bool glow = false;
            SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;
        } supported_; // Last MSAA support query.
        // Targets, kept while the plan and size stay the same.
        TextureHandle multisample_, scene_, multisample_glow_, glow_, display_;
        std::array<TextureHandle, bloom_level_limit> levels_{};
        std::array<std::array<Uint32, 2>, bloom_level_limit> level_sizes_{};
        std::size_t level_count_ = 0;
    };
} // namespace OpenBlood2
