#pragma once
#include "app/model_scene.h"
#include "app/polygrid_scene.h"
#include "app/sprite_scene.h"
#include "engine/world_renderer.h"

namespace OpenBlood2
{
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
        WorldPipelines pipelines; // Declared before the renderers that borrow it.
        WorldRenderer world_renderer, model_renderer, explosion_model_renderer, polygrid_renderer, sprite_renderer;
        ModelView model_scene, explosion_model_scene;
        PolyGridView polygrid_scene;
        SpriteView sprite_scene;
        PresentationStats stats;

      private:
        SDL_GPUDevice* device_ = nullptr;
        SDL_GPUTexture* depth_ = nullptr;
        Uint32 depth_width_ = 0, depth_height_ = 0;
    };
} // namespace OpenBlood2
