#pragma once
#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "app/polygrid_scene.h"
#include "app/sprite_scene.h"
#include "engine/light_grid.h"
#include "engine/line_renderer.h"
#include "engine/model_renderer.h"
#include "engine/post_process.h"
#include "engine/surface_report.h"
#include "engine/world_renderer.h"
#include <functional>
#include <optional>
#include <span>

namespace OpenBlood2
{
    // Vertical FLAG_SHADOW model shadows of the last update(). Inspection/measurement only.
    struct ShadowStats
    {
        bool available = false; // Atlas formats supported by the device.
        std::size_t candidates = 0, casters = 0, beyond_limit = 0, distant = 0, unbounded = 0, outside = 0;
    };
    // Enhanced renderer state of the last update(). Inspection/measurement only.
    struct SurfaceStats
    {
        std::size_t grid_lights = 0, grid_cells = 0, grid_dropped = 0;
        float grid_cell_size = 0;
        std::uint64_t grid_build_ns = 0;
    };
    // Counters of the most recently recorded frame. Inspection/measurement only.
    struct PresentationStats
    {
        std::uint64_t frames = 0;
        std::size_t passes = 0, draw_calls = 0, culled = 0;
        // CPU wall time of the last update() stages and record(), in nanoseconds.
        std::uint64_t world_ns = 0, models_ns = 0, effects_ns = 0, record_ns = 0;
    };
    // One XR eye: camera and asymmetric projection, recorded into colour layer `index` of the target.
    struct StereoEye
    {
        DVector position{};
        DRotation rotation{};
        EyeProjection projection;
    };
    // Read-only presentation snapshots and their GPU renderers, shared by the
    // application and the measurement tool. update() reads current checked session
    // state without advancing simulation; record() encodes one presented frame.
    // All in-world presentation is recorded into one scene render pass sharing one
    // depth target and one pipeline set. View models use a separate near depth slice.
    // On desktop, post-processing (MSAA, the enhanced HDR scene, reflections, ambient occlusion,
    // bloom, tonemapping, FXAA) surrounds that pass; XR eyes keep the plain single-sampled pass.
    class ScenePresenter
    {
      public:
        explicit ScenePresenter(SDL_GPUDevice*);
        ~ScenePresenter();
        ScenePresenter(const ScenePresenter&) = delete;
        ScenePresenter& operator=(const ScenePresenter&) = delete;
        // Rebuilds world geometry after a world revision change, then refreshes all
        // presentation snapshots. Errors carry the failing stage as a prefix.
        bool update(GameSession&, std::string& error);
        // Encodes the in-world viewport into target. Requires a successful update().
        bool record(GameSession&, SDL_GPUCommandBuffer*, SDL_GPUTexture* target, SDL_GPUTextureFormat, Uint32 width,
                    Uint32 height, std::string& error);
        // XR: one scene pass per eye into layer i of a layered target, sharing this frame's uploads
        // and shadow tiles. No letterbox; view_scale is ignored. Requires a successful update().
        bool record_eyes(GameSession&, SDL_GPUCommandBuffer*, SDL_GPUTexture* target, SDL_GPUTextureFormat,
                         Uint32 width, Uint32 height, std::span<const StereoEye> eyes, std::string& error);
        // XR head camera for update() (sprites face it; the sky and shadow casters follow it) and
        // the matching view-weapon placement. Empty: the game's camera, as on desktop.
        std::optional<CameraView> camera_override;
        std::optional<std::array<ViewWeaponCamera, 2>> weapon_cameras; // {left, right}
        // XR without a compositor fade: screen fades scale the scene light (1 on desktop).
        float scene_fade = 1;
        // XR: recorded at the end of each eye's scene pass (laser, reticle).
        std::function<void(SDL_GPURenderPass*, SDL_GPUCommandBuffer*, std::size_t eye)> eye_overlay;
        // XR menus: hand models at the controllers (fully bright), drawn by record_overlay_eyes
        // while draw_hands is set; the scene then omits the view weapons. Filled by the XR frontend
        // before recording; record_eyes or record_empty_eyes prepares them.
        ModelView hand_scene;
        bool draw_hands = false;
        // XR without a world: prepares the frame (hands included) and clears each eye dark.
        bool record_empty_eyes(SDL_GPUCommandBuffer*, SDL_GPUTexture* target, SDL_GPUTextureFormat, Uint32 width,
                               Uint32 height, std::span<const StereoEye> eyes, std::string& error);
        // XR overlay layer, after this frame's record_eyes or record_empty_eyes: a transparent clear,
        // then hand_scene (view-model depth slice) and eye_overlay, one pass per eye.
        bool record_overlay_eyes(SDL_GPUCommandBuffer*, SDL_GPUTexture* target, Uint32 width, Uint32 height,
                                 std::span<const StereoEye> eyes, std::string& error);

        bool render_world = false;      // Entered player and loaded geometry at the last update().
        bool rebuilt = false;           // The last update() rebuilt or released world geometry.
        bool resources_changed = false; // The last update() rebuilt geometry or admitted new materials.
        // Viewport depth slice reserved for view models; the scene uses [this, 1].
        static constexpr float view_model_depth = 1.f / 256;
        WorldMesh world_mesh;
        std::uint64_t mesh_revision = UINT64_MAX;
        std::vector<WorldDraw> world_draws, sky_draws;
        DVector sky_eye{};
        GpuRegistry registry;     // Owns every presentation GPU resource; declared before its users.
        PostProcess post_process; // Desktop record() only: targets and passes around the scene pass.
        PostSettings post;        // Display settings for record(); all but AA need the enhanced renderer.
        LineRenderer ballistic_lines;
        std::vector<LineVertex> ballistic_vertices;
        unsigned ballistic_draws = 0;
        WorldPipelines pipelines; // Declared before the renderers that borrow it.
        WorldRenderer world_renderer;
        ModelRenderer model_renderer, explosion_model_renderer; // GPU skinning of both model views.
        ModelRenderer hand_renderer;                            // hand_scene (original shading).
        WorldRenderer polygrid_renderer, sprite_renderer, sky_sprite_renderer;
        ModelView model_scene, explosion_model_scene;
        ModelLighting model_lighting; // Static light objects shading model_scene instances.
        // Vertical model shadows: FLAG_SHADOW casters nearest the camera, one tile each.
        bool model_shadows = true;
        // Original "LightMap" setting (off at low detail): a change rebuilds the world mesh.
        bool lightmaps = true;
        bool per_pixel_lighting = false; // Model light per pixel instead of per vertex.
        // Renderer selection: Original keeps the original shaders; Standard/High enable the
        // physically based world and model shading, optionally with generated normal maps.
        SurfaceShading shading = SurfaceShading::Original;
        bool normal_maps = false;
        SurfaceManifest surface_manifest; // Built-in config/materials.json unless replaced before update().
        NormalMapCache normal_map_cache;  // Survives world changes; generated maps are content-addressed.
        LightGrid light_grid;
        SurfaceStats surface_stats;
        // Resolved enhanced materials of the current world (roadmap 5.5), rebuilt when the world
        // or the material settings change; empty while the original renderer is selected.
        SurfaceReport material_report;
        float shadow_aspect = 16.f / 9; // Viewport aspect for caster culling; record() keeps it current.
        std::vector<ModelRenderer::ShadowCaster> shadow_casters;
        ShadowUniform shadow_uniform;
        ShadowStats shadow_stats;
        static constexpr Uint32 shadow_atlas_size = 512, shadow_tile = 128;
        // Replacement policy: footprint margin, distance limit, strength (fraction of the baked
        // light removed), fade range below the feet and height bias, in world units.
        static constexpr float shadow_margin = 8, shadow_distance = 2048, shadow_strength = .6f, shadow_fade = 96,
                               shadow_bias = 2, shadow_elevation = 40; // Minimum light elevation, degrees.
        PolyGridView polygrid_scene;
        SpriteView sprite_scene, sky_sprite_scene;
        struct SkyDraw
        {
            bool sprite = false;
            std::size_t index = 0;
        };
        std::vector<SkyDraw> sky_order; // Interleaved authored slots, never distance sorted.
        PresentationStats stats;

      private:
        const CameraView& camera(const GameSession&) const;
        // Pipelines, renderer uploads, shadow tiles and a depth target with `layers` layers.
        bool prepare_depth(Uint32 width, Uint32 height, Uint32 layers, SDL_GPUSampleCount, std::string& error);
        bool prepare_frame(SDL_GPUCommandBuffer*, SDL_GPUTextureFormat, Uint32 width, Uint32 height, Uint32 layers,
                           bool aux, bool reflections, SDL_GPUSampleCount, std::string& error);
        // One scene render pass into colour/depth layer `target.layer`.
        bool record_view(GameSession&, SDL_GPUCommandBuffer*, const SceneTarget&, DVector eye, DRotation rotation,
                         bool letterbox, const EyeProjection*, std::string& error);
        void finish_frame(std::uint64_t started);
        void select_shadow_casters(const GameSession&);
        bool record_shadow_tiles(SDL_GPUCommandBuffer*, std::string& error);
        TextureHandle shadow_atlas_, shadow_depth_;
        bool built_lightmaps_ = true;
        bool prepare_surface(GameSession&, std::string& error);
        SurfaceScene surface_scene_;
        bool manifest_loaded_ = false;
        std::uint64_t grid_revision_ = UINT64_MAX;
        SurfaceShading applied_shading_ = SurfaceShading::Original;
        bool applied_normal_maps_ = false;
        std::uint64_t applied_manifest_ = 0;
        std::uint64_t report_mesh_ = UINT64_MAX, report_surface_ = UINT64_MAX;
        TextureHandle depth_;
        Uint32 depth_width_ = 0, depth_height_ = 0, depth_layers_ = 0;
        SDL_GPUSampleCount depth_samples_ = SDL_GPU_SAMPLECOUNT_1;
    };
} // namespace OpenBlood2
