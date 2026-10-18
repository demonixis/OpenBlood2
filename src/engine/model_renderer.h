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
        // One shadow tile: the caster instance, its pose, the tile viewport in pixels, its world
        // box (y from the feet to the top), the slopes projecting a point along the light onto
        // the box top (x' = x + slope.x * (y - top)) and the projected footprint at that top.
        struct ShadowCaster
        {
            std::uint32_t instance = 0;
            DVector position{};
            DRotation rotation{{0, 0, 0}, 1};
            std::array<float, 4> viewport{};
            DVector min{}, max{};
            std::array<float, 2> slope{};
            std::array<float, 4> footprint{}; // Min x, min z, max x, max z.
        };
        // Records caster heights with an orthographic projection along the light (vertical when
        // the slopes are zero) into the current shadow tile pass (shadow_tile_format color and a
        // depth target). Requires a prepare() for this view.
        bool record_shadows(SDL_GPUCommandBuffer*, SDL_GPURenderPass*, const ModelView&, std::span<const ShadowCaster>,
                            std::string& error);
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
        SDL_GPUSampler *sampler_ = nullptr, *lightmap_sampler_ = nullptr, *shadow_sampler_ = nullptr; // Registry-owned.
        std::uint64_t revision_ = UINT64_MAX;
        Uint32 width_ = 0, height_ = 0;
        std::vector<Visible> visible_;
    };
} // namespace OpenBlood2
