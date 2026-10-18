#pragma once
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
        Count
    };
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
        // Standalone renderer: owns its pipelines and a depth target sized by prepare().
        explicit WorldRenderer(SDL_GPUDevice* device, DrawStyle style = DrawStyle::Opaque)
            : device_(device), style_(style), own_pipelines_(device), pipelines_(&own_pipelines_)
        {
        }
        // Scene renderer: records into externally owned passes/targets with shared pipelines.
        WorldRenderer(SDL_GPUDevice* device, DrawStyle style, WorldPipelines& shared)
            : device_(device), style_(style), own_pipelines_(device), pipelines_(&shared), external_targets_(true)
        {
        }
        ~WorldRenderer();
        WorldRenderer(const WorldRenderer&) = delete;
        WorldRenderer& operator=(const WorldRenderer&) = delete;
        bool prepare(const WorldMesh&, std::uint64_t revision, SDL_GPUTextureFormat, Uint32 width, Uint32 height,
                     std::string& error, bool dynamic = false);
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
            return depth_;
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
        SDL_GPUDevice* device_ = nullptr;
        DrawStyle style_;
        WorldPipelines own_pipelines_;
        WorldPipelines* pipelines_ = nullptr;
        bool external_targets_ = false;
        std::vector<WorldAlphaBatch> alpha_order_;
        std::vector<WorldDraw> visible_;
        SDL_GPUBuffer* vertices_ = nullptr;
        SDL_GPUTransferBuffer* vertex_transfer_ = nullptr;
        Uint32 vertex_capacity_ = 0;
        bool upload_vertices(const WorldMesh&, std::string&);
        SDL_GPUTexture* depth_ = nullptr;
        std::vector<SDL_GPUTexture*> textures_;
        SDL_GPUSampler* sampler_ = nullptr;
        bool upload_materials(const WorldMesh&, std::string&);
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
