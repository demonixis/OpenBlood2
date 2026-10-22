#pragma once
#include "engine/gpu_registry.h"
#include "engine/model_view.h"
#include "engine/normal_maps.h"
#include "engine/surface_materials.h"
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
        ShadowCaster, // Skinned caster heights into a shadow_tile_format tile (depth-tested).
        // Enhanced renderer only (null with SurfaceShading::Original): world faces and lit models.
        SurfaceOpaque,
        SurfaceAlpha,
        SkinnedSurfaceOpaque,
        SkinnedSurfaceAlpha,
        Count
    };
    // Fragment shading of world faces and models. Original keeps the original shaders, which
    // stay the selectable reference; Standard and High select the enhanced (PBR) shaders.
    enum class SurfaceShading
    {
        Original,
        Standard,
        High
    };
    // Enhanced fragment uniform slot 2 (std140), pushed once per record: camera and light grid.
    struct SurfaceFrameUniform
    {
        std::array<float, 4> eye{}, grid_origin{}; // grid_origin.w: 1 / cell size.
        std::array<std::uint32_t, 4> grid_size{};  // Cells per axis; w: cell count.
    };
    static_assert(sizeof(SurfaceFrameUniform) == 48);
    // Enhanced fragment uniform slot 3 (std140), pushed when the material changes.
    struct SurfaceMaterialUniform
    {
        // Roughness, metallic, dielectric F0 scale, flags: 1 lit model, 2 normal map,
        // 4 original shading (unlit/flat models).
        std::array<float, 4> material{.8f, 0, 1, 0};
        // Emission strength and luminance threshold (SurfaceMaterial::emissive*); zero: none.
        std::array<float, 4> emission{};
        bool operator==(const SurfaceMaterialUniform&) const = default;
    };
    static_assert(sizeof(SurfaceMaterialUniform) == 32);
    // Enhanced-renderer state shared by the scene renderers that opt in (world faces and
    // lit models), owned by the presenter. Renderers resolve per-material parameters and
    // normal maps again whenever `revision` changes.
    struct SurfaceScene
    {
        const SurfaceManifest* manifest = nullptr;
        NormalMapCache* normal_maps = nullptr; // Null: generated normal maps disabled.
        BufferHandle lights, cells;            // Light grid storage buffers (LightGrid layout).
        SurfaceFrameUniform frame;             // eye is filled per record.
        std::uint64_t revision = 0;
    };
    // Bloom scene passes (post-processing): color target 1 holds the linear light that emission adds,
    // premultiplied by coverage. Only the enhanced renderer writes it.
    inline constexpr SDL_GPUTextureFormat scene_glow_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    // Vertical model shadow tiles store caster world heights; uncovered texels keep the clear value.
    inline constexpr SDL_GPUTextureFormat shadow_tile_format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    inline constexpr float shadow_tile_clear = -1e30f;
    inline constexpr std::size_t shadow_caster_limit = 32;
    // Fragment uniform slot 0 of both world fragment shaders (std140), pushed per draw in full:
    // modulation (alpha, lightmap scale, shadow receiver, per-pixel model light), the radial
    // point light, then the per-pixel model light (ambient + count, color + scale, 3 float4 per
    // source; zero for world geometry).
    struct FragmentLightUniform
    {
        std::array<float, 4> modulation{}, position_radius{}, light_color{}, model_ambient{}, model_color{};
        std::array<std::array<float, 4>, 12> model_sources{};
        std::array<float, 4> scene_color{1, 1, 1, 1};
    };
    static_assert(sizeof(FragmentLightUniform) == 288);
    // Fragment uniform slot 1 of both world fragment shaders (std140), pushed once per record.
    struct ShadowUniform
    {
        std::array<float, 4> info{}; // Caster count, strength, fade range below the feet, atlas texel.
        std::array<float, 4> eye{};
        std::array<float, 4> bounds{}; // Union of caster boxes: min x, min z, max x, max z.
        std::array<float, 4> height{}; // Union top.
        // Per caster: tile (u0, v0, size, top), footprint at the top (min x, min z, 1/size x, 1/size z),
        // (feet, bias, slope x, slope z).
        std::array<std::array<float, 4>, 3 * shadow_caster_limit> casters{};
    };
    // Conservative bounding-sphere test against the world_view_uniform projection.
    struct ViewFrustum
    {
        DVector eye{}, right{}, up{}, forward{};
        double aspect = 1, near = 1, far = 65536;
        // XR eyes: planes from tangent-space half angles instead of the 90-degree/aspect pair.
        bool asymmetric = false;
        double tan_left = -1, tan_right = 1, tan_down = -1, tan_up = 1;
    };
    ViewFrustum view_frustum(DVector eye, DRotation view, double aspect, double near = 1, double far = 65536);
    // XR eye view: an asymmetric projection replacing the desktop {1, aspect} * view_scale terms
    // (offsets go to the right/up w components, zero on desktop) and its culling tangents.
    struct EyeProjection
    {
        float scale_x = 1, scale_y = 1, offset_x = 0, offset_y = 0;
        double tan_left = -1, tan_right = 1, tan_down = -1, tan_up = 1;
    };
    ViewFrustum eye_frustum(DVector eye, DRotation view, const EyeProjection&, double near = 1, double far = 65536);
    bool sphere_visible(const ViewFrustum&, DVector center, double radius);
    // Every world/model/sprite/sky pipeline for one color format, created together
    // so no pipeline is compiled lazily during gameplay. One set may be shared by
    // all renderers recording into the same scene pass. A glow set adds color target 1
    // (scene_glow_format) to every scene pipeline; a multisampled set matches MSAA targets.
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
        // No-op when already prepared for this format, shading, glow target and sample count;
        // failure releases every pipeline. The format-only overload keeps the other choices.
        bool prepare(SDL_GPUTextureFormat, std::string& error);
        bool prepare(SDL_GPUTextureFormat, SurfaceShading, std::string& error);
        bool prepare(SDL_GPUTextureFormat, SurfaceShading, bool glow, SDL_GPUSampleCount, std::string& error);
        SurfaceShading shading() const
        {
            return shading_;
        }
        SDL_GPUGraphicsPipeline* get(WorldPipeline kind) const
        {
            return pipelines_[static_cast<std::size_t>(kind)];
        }
        SDL_GPUTextureFormat format() const
        {
            return format_;
        }
        bool glow() const
        {
            return glow_;
        }
        SDL_GPUSampleCount samples() const
        {
            return samples_;
        }
        std::uint64_t creations = 0; // Complete pipeline-set creations, for inspection.

      private:
        void release();
        SDL_GPUDevice* device_ = nullptr;
        std::array<SDL_GPUGraphicsPipeline*, static_cast<std::size_t>(WorldPipeline::Count)> pipelines_{};
        SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        SurfaceShading shading_ = SurfaceShading::Original;
        bool glow_ = false;
        SDL_GPUSampleCount samples_ = SDL_GPU_SAMPLECOUNT_1;
    };
    // Destination of record(): an already begun render pass whose color/depth
    // formats match the prepared pipelines. Depth range selects a viewport slice.
    struct WorldRecordTarget
    {
        SDL_GPUCommandBuffer* commands = nullptr;
        SDL_GPURenderPass* pass = nullptr;
        bool letterbox = false;
        float min_depth = 0, max_depth = 1;
        // Camera zoom: 1/tan(fovX/2) over the 90-degree default. View models keep 1; culling
        // stays at 90 degrees, which contains every narrower zoomed view.
        float view_scale = 1;
        // Vertical model shadow tiles and their uniform; receivers ignore them when null.
        SDL_GPUTexture* shadow_atlas = nullptr;
        const ShadowUniform* shadows = nullptr;
        DVector scene_color{1, 1, 1}; // Original SetGlobalLightScale, scene only.
        // XR eye: every draw (sky and close included) uses this projection and its frustum.
        const EyeProjection* eye = nullptr;
    };
    // Per-material enhanced parameters and normal maps of one renderer, parallel to its
    // material textures; resolved again when the materials or the SurfaceScene revision change.
    struct SurfaceMaterials
    {
        std::vector<SurfaceMaterialUniform> uniforms;
        std::vector<TextureHandle> normals;
        std::uint64_t revision = UINT64_MAX;
        SDL_GPUSampler* sampler = nullptr; // Registry-owned trilinear repeat sampler.
        void clear()
        {
            uniforms.clear();
            normals.clear();
            revision = UINT64_MAX;
        }
        // Resolves every material not yet resolved (or all after a revision change).
        bool refresh(const SurfaceScene&, GpuRegistry&, std::span<const WorldMaterial>, std::string& error);
        // Binds the light grid storage buffers and pushes the frame uniform for this eye.
        static bool bind_scene(const SurfaceScene&, const GpuRegistry&, SDL_GPUCommandBuffer*, SDL_GPURenderPass*,
                               DVector eye);
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
        bool receive_shadows = false; // Opaque/translucent world passes read the shadow tiles.
        // Enhanced renderer state; world faces use it when the pipelines are not Original.
        const SurfaceScene* surface = nullptr;
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
        SDL_GPUSampler *sampler_ = nullptr, *lightmap_sampler_ = nullptr, *shadow_sampler_ = nullptr; // Registry-owned.
        Uint32 width_ = 0, height_ = 0;
        std::uint64_t revision_ = UINT64_MAX;
        SurfaceMaterials surface_materials_;
        bool enhanced() const
        {
            return surface && pipelines_->shading() != SurfaceShading::Original && style_ == DrawStyle::Opaque;
        }
        // Checks every input and selects visible_/alpha_order_ before any pass is begun.
        bool validate(const WorldMesh&, std::span<const WorldDraw>, DVector eye, DRotation view, WorldPass,
                      double surface_time, const PointLight&, bool letterbox, bool close_view, std::string& error,
                      const EyeProjection* eye_projection = nullptr);
        // Encodes the draws selected by the preceding successful validate().
        void encode(const WorldRecordTarget&, const WorldMesh&, DVector eye, DRotation view, WorldPass,
                    double surface_time, const PointLight&, bool close_view);
    };
} // namespace OpenBlood2
