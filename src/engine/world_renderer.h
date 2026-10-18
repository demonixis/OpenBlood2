#pragma once
#include "engine/gpu_registry.h"
#include "engine/model_view.h"
#include "engine/point_light.h"
#include "engine/world_view.h"
#include <SDL3/SDL_gpu.h>

namespace OpenBlood2
{
    enum class DrawLoad
    {
        Clear,
        Load,
        ClearDepth
    };
    enum class DrawStyle
    {
        Opaque,
        SpriteAlpha
    };
    enum class WorldPass
    {
        Opaque,
        Translucent,
        Sky,
        OpaqueWithSky
    };
    enum class WorldPipeline
    {
        Opaque,
        Alpha,
        Sky,
        SkyAlpha,
        SkinnedOpaque, // GPU-skinned ABC models (SkinnedVertex input, node palette storage).
        SkinnedAlpha,
        Count
    };
    // Conservative bounding-sphere test against the world_view_uniform projection.
    struct ViewFrustum
    {
        DVector eye{}, right{}, up{}, forward{};
        double aspect = 1, near = 1, far = 65536;
    };
    ViewFrustum view_frustum(DVector eye, DRotation view, double aspect, double near = 1, double far = 65536);
    bool sphere_visible(const ViewFrustum&, DVector center, double radius);
    // Every world/model/sprite/sky pipeline for one color format, created together
    // so no pipeline is compiled lazily during gameplay. One set may be shared by
    // all renderers recording into the same scene pass.
    class WorldPipelines
    {
      public:
        explicit WorldPipelines(SDL_GPUDevice* device) : device_(device) {}
        ~WorldPipelines()
        {
            release();
        }
        WorldPipelines(const WorldPipelines&) = delete;
        WorldPipelines& operator=(const WorldPipelines&) = delete;
        // No-op when already prepared for this format; failure releases every pipeline.
        bool prepare(SDL_GPUTextureFormat, std::string& error);
        SDL_GPUGraphicsPipeline* get(WorldPipeline kind) const
        {
            return pipelines_[static_cast<std::size_t>(kind)];
        }
        SDL_GPUTextureFormat format() const
        {
            return format_;
        }
        std::uint64_t creations = 0; // Complete pipeline-set creations, for inspection.

      private:
        void release();
        SDL_GPUDevice* device_ = nullptr;
        std::array<SDL_GPUGraphicsPipeline*, static_cast<std::size_t>(WorldPipeline::Count)> pipelines_{};
        SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    };
    // Destination of record(): an already begun render pass whose color/depth
    // formats match the prepared pipelines. Depth range selects a viewport slice.
    struct WorldRecordTarget
    {
        SDL_GPUCommandBuffer* commands = nullptr;
        SDL_GPURenderPass* pass = nullptr;
        bool letterbox = false;
        float min_depth = 0, max_depth = 1;
    };
    class WorldRenderer
    {
      public:
        // Standalone renderer: owns its pipelines, GPU registry and a depth target sized by
        // prepare(), and flushes its uploads in prepare().
        explicit WorldRenderer(SDL_GPUDevice* device, DrawStyle style = DrawStyle::Opaque)
            : style_(style), own_pipelines_(device), pipelines_(&own_pipelines_), own_registry_(device),
              registry_(&own_registry_)
        {
        }
        // Scene renderer: records into externally owned passes/targets with shared pipelines
        // and registry; the registry's owner flushes the staged uploads.
        WorldRenderer(DrawStyle style, WorldPipelines& pipelines, GpuRegistry& registry)
            : style_(style), own_pipelines_(registry.device()), pipelines_(&pipelines),
              own_registry_(registry.device()), registry_(&registry), external_targets_(true)
        {
        }
        ~WorldRenderer();
        WorldRenderer(const WorldRenderer&) = delete;
        WorldRenderer& operator=(const WorldRenderer&) = delete;
        // Stages geometry/material uploads in the registry. An owned registry is flushed into
        // `frame` when given (before its render passes), else in a dedicated submission.
        bool prepare(const WorldMesh&, std::uint64_t revision, SDL_GPUTextureFormat, Uint32 width, Uint32 height,
                     std::string& error, bool dynamic = false, SDL_GPUCommandBuffer* frame = nullptr);
        // Begins and ends one render pass around record(). Validation failures begin no pass.
        bool draw(SDL_GPUCommandBuffer*, SDL_GPUTexture*, const WorldMesh&, std::span<const WorldDraw>, DVector eye,
                  DRotation view, bool letterbox, std::string& error, DrawLoad = DrawLoad::Clear,
                  SDL_GPUTexture* shared_depth = nullptr, WorldPass = WorldPass::Opaque, double surface_time = 0,
                  const PointLight& light = {});
        // Encodes into target.pass. close_view selects the near-plane view-model projection
        // without illumination, as DrawLoad::ClearDepth does for draw(). Draws whose
        // bounded ranges lie outside the view frustum are skipped and counted.
        bool record(const WorldRecordTarget&, const WorldMesh&, std::span<const WorldDraw>, DVector eye, DRotation view,
                    std::string& error, WorldPass = WorldPass::Opaque, double surface_time = 0,
                    const PointLight& light = {}, bool close_view = false);
        SDL_GPUTexture* depth_target() const
        {
            return registry_->get(depth_);
        }
        void clear_geometry();
        WorldViewStats stats;
        Uint32 width() const
        {
            return width_;
        }
        Uint32 height() const
        {
            return height_;
        }

      private:
        DrawStyle style_;
        WorldPipelines own_pipelines_;
        WorldPipelines* pipelines_ = nullptr;
        GpuRegistry own_registry_;
        GpuRegistry* registry_ = nullptr;
        bool external_targets_ = false;
        std::vector<WorldAlphaBatch> alpha_order_;
        std::vector<WorldDraw> visible_;
        BufferHandle vertices_;
        bool upload_vertices(const WorldMesh&, std::string&);
        TextureHandle depth_;
        std::vector<TextureHandle> textures_;
        std::vector<std::string> texture_paths_;
        SDL_GPUSampler *sampler_ = nullptr, *lightmap_sampler_ = nullptr; // Owned by the registry.
        Uint32 width_ = 0, height_ = 0;
        std::uint64_t revision_ = UINT64_MAX;
        // Checks every input and selects visible_/alpha_order_ before any pass is begun.
        bool validate(const WorldMesh&, std::span<const WorldDraw>, DVector eye, DRotation view, WorldPass,
                      double surface_time, const PointLight&, bool letterbox, bool close_view, std::string& error);
        // Encodes the draws selected by the preceding successful validate().
        void encode(const WorldRecordTarget&, const WorldMesh&, DVector eye, DRotation view, WorldPass,
                    double surface_time, const PointLight&, bool close_view);
    };
} // namespace OpenBlood2
