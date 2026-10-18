#pragma once
#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "app/polygrid_scene.h"
#include "app/sprite_scene.h"
#include "engine/model_renderer.h"
#include "engine/world_renderer.h"

namespace OpenBlood2
{
    // Vertical FLAG_SHADOW model shadows of the last update(). Inspection/measurement only.
    struct ShadowStats
    {
        bool available = false; // Atlas formats supported by the device.
        std::size_t candidates = 0, casters = 0, beyond_limit = 0, distant = 0, unbounded = 0, outside = 0;
    };
    // Counters of the most recently recorded frame. Inspection/measurement only.
    struct PresentationStats
    {
        std::uint64_t frames = 0;
        std::size_t passes = 0, draw_calls = 0, culled = 0;
        // CPU wall time of the last update() stages and record(), in nanoseconds.
        std::uint64_t world_ns = 0, models_ns = 0, effects_ns = 0, record_ns = 0;
    };
    // Read-only presentation snapshots and their GPU renderers, shared by the
    // application and the measurement tool. update() reads current checked session
    // state without advancing simulation; record() encodes one presented frame.
    // All in-world presentation is recorded into one scene render pass sharing one
    // depth target and one pipeline set. View models use a separate near depth slice.
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
        WorldPipelines pipelines; // Declared before the renderers that borrow it.
        WorldRenderer world_renderer;
        ModelRenderer model_renderer, explosion_model_renderer; // GPU skinning of both model views.
        WorldRenderer polygrid_renderer, sprite_renderer;
        ModelView model_scene, explosion_model_scene;
        ModelLighting model_lighting; // Static light objects shading model_scene instances.
        // Vertical model shadows: FLAG_SHADOW casters nearest the camera, one tile each.
        bool model_shadows = true;
        // Original "LightMap" setting (off at low detail): a change rebuilds the world mesh.
        bool lightmaps = true;
        bool per_pixel_lighting = false; // Model light per pixel instead of per vertex.
        float shadow_aspect = 16.f / 9;  // Viewport aspect for caster culling; record() keeps it current.
        std::vector<ModelRenderer::ShadowCaster> shadow_casters;
        ShadowUniform shadow_uniform;
        ShadowStats shadow_stats;
        static constexpr Uint32 shadow_atlas_size = 512, shadow_tile = 128;
        // Replacement policy: footprint margin, distance limit, strength (fraction of the baked
        // light removed), fade range below the feet and height bias, in world units.
        static constexpr float shadow_margin = 8, shadow_distance = 2048, shadow_strength = .6f, shadow_fade = 96,
                               shadow_bias = 2, shadow_elevation = 40; // Minimum light elevation, degrees.
        PolyGridView polygrid_scene;
        SpriteView sprite_scene;
        PresentationStats stats;

      private:
        void select_shadow_casters(const GameSession&);
        bool record_shadow_tiles(SDL_GPUCommandBuffer*, std::string& error);
        TextureHandle shadow_atlas_, shadow_depth_;
        bool built_lightmaps_ = true;
        TextureHandle depth_;
        Uint32 depth_width_ = 0, depth_height_ = 0;
    };
} // namespace OpenBlood2
