#pragma once
#include "engine/world_renderer.h"

namespace OpenBlood2
{
    // GPU-skinned presentation of a ModelView in gpu_skinning mode. Each shared immutable
    // asset's static corner stream uploads once into the registry and is shared by every
    // ModelRenderer using it; a frame stages only node palettes and packed deformation.
    // The registry's owner flushes staged uploads. Records into externally owned passes.
    class ModelRenderer
    {
      public:
        ModelRenderer(DrawStyle style, WorldPipelines& pipelines, GpuRegistry& registry)
            : style_(style), pipelines_(&pipelines), registry_(&registry)
        {
        }
        ~ModelRenderer();
        ModelRenderer(const ModelRenderer&) = delete;
        ModelRenderer& operator=(const ModelRenderer&) = delete;
        // Stages new assets, skins appended since the last view revision, and this frame's
        // palette/deformation. Nothing reaches the GPU until the registry is flushed.
        bool prepare(const ModelView&, SDL_GPUTextureFormat, Uint32 width, Uint32 height, std::string& error);
        // Opaque records non-translucent instances (all of them for SpriteAlpha); Translucent
        // records translucent instances far-to-near. Bounded instances outside the frustum
        // are skipped unless close_view. Validation precedes any encoding.
        bool record(const WorldRecordTarget&, const ModelView&, std::span<const WorldDraw>, DVector eye, DRotation view,
                    std::string& error, WorldPass = WorldPass::Opaque, const PointLight& light = {},
                    bool close_view = false);
        void clear_geometry();
        WorldViewStats stats;
        std::size_t staged_bytes = 0;   // Last prepare(): new assets, palettes and deformation.
        std::size_t asset_vertices = 0; // Static corners of the assets this renderer drew last.
        static constexpr std::size_t asset_limit = 512, asset_vertex_limit = 2000000;

      private:
        struct Visible
        {
            std::size_t draw = 0;
            double depth = 0;
        };
        DrawStyle style_;
        WorldPipelines* pipelines_ = nullptr;
        GpuRegistry* registry_ = nullptr;
        std::vector<GpuRegistry::Shared> instance_assets_; // Per view instance, by value.
        BufferHandle palette_, deformation_;
        std::vector<TextureHandle> textures_;
        std::vector<std::string> texture_paths_;
        SDL_GPUSampler *sampler_ = nullptr, *lightmap_sampler_ = nullptr; // Owned by the registry.
        std::uint64_t revision_ = UINT64_MAX;
        Uint32 width_ = 0, height_ = 0;
        std::vector<Visible> visible_;
    };
} // namespace OpenBlood2
