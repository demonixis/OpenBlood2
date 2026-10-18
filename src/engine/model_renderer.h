#pragma once
#include "engine/world_renderer.h"

namespace OpenBlood2
{
    // GPU-skinned presentation of a ModelView in gpu_skinning mode. Each shared immutable
    // asset's static corner stream uploads once; a frame uploads only node palettes and
    // packed deformation, in one copy submission. Records into externally owned passes.
    class ModelRenderer
    {
      public:
        ModelRenderer(SDL_GPUDevice* device, DrawStyle style, WorldPipelines& pipelines)
            : device_(device), style_(style), pipelines_(&pipelines)
        {
        }
        ~ModelRenderer();
        ModelRenderer(const ModelRenderer&) = delete;
        ModelRenderer& operator=(const ModelRenderer&) = delete;
        // Uploads new assets, skins on a view revision change, and this frame's palette/deformation.
        // Uploads into `frame` when given (before its render passes), else a dedicated submission.
        bool prepare(const ModelView&, SDL_GPUTextureFormat, Uint32 width, Uint32 height, std::string& error,
                     SDL_GPUCommandBuffer* frame = nullptr);
        // Opaque records non-translucent instances (all of them for SpriteAlpha); Translucent
        // records translucent instances far-to-near. Bounded instances outside the frustum
        // are skipped unless close_view. Validation precedes any encoding.
        bool record(const WorldRecordTarget&, const ModelView&, std::span<const WorldDraw>, DVector eye, DRotation view,
                    std::string& error, WorldPass = WorldPass::Opaque, const PointLight& light = {},
                    bool close_view = false);
        void clear_geometry();
        WorldViewStats stats;
        std::size_t assets() const
        {
            return assets_.size();
        }
        std::size_t asset_vertices() const
        {
            return asset_vertices_;
        }
        std::size_t uploaded_bytes = 0; // Last prepare(): new assets, palettes and deformation.
        static constexpr std::size_t asset_limit = 256, asset_vertex_limit = 2000000;

      private:
        struct Asset
        {
            std::weak_ptr<const Model> owner;
            SDL_GPUBuffer* buffer = nullptr;
            std::uint32_t vertices = 0;
        };
        struct Visible
        {
            std::size_t draw = 0;
            double depth = 0;
        };
        bool reserve(SDL_GPUBuffer*& buffer, Uint32& capacity, Uint32 bytes, std::string& error);
        void release_asset(Asset&);
        SDL_GPUDevice* device_ = nullptr;
        DrawStyle style_;
        WorldPipelines* pipelines_ = nullptr;
        std::map<const Model*, Asset> assets_;
        std::size_t asset_vertices_ = 0;
        std::vector<std::vector<SkinnedVertex>> pending_;
        SDL_GPUBuffer *palette_ = nullptr, *deformation_ = nullptr;
        Uint32 palette_capacity_ = 0, deformation_capacity_ = 0;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        Uint32 transfer_capacity_ = 0;
        std::vector<SDL_GPUTexture*> textures_;
        SDL_GPUSampler* sampler_ = nullptr;
        std::uint64_t revision_ = UINT64_MAX;
        Uint32 width_ = 0, height_ = 0;
        std::vector<Visible> visible_;
    };
} // namespace OpenBlood2
