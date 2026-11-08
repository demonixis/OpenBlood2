#pragma once
#include "app/gi_bake_process.h"
#include "app/menu_input.h"
#include "engine/config.h"
#include "engine/gi_file.h"
#include "engine/gi_placement.h"
#include "engine/gi_reflection_runtime.h"
#include "engine/gi_runtime.h"
#include "engine/line_renderer.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OpenBlood2
{
    class GameSession;
    struct WorldMesh;
    struct ModelLightingStats;
    // What the Runtime panel (lots G8, G9) reads and controls. Null members leave it unavailable.
    struct GiRuntimeLink
    {
        bool* enabled = nullptr;              // Options > Display GLOBAL ILLUMINATION (saved with the preferences).
        GiComposition* composition = nullptr; // ScenePresenter::gi_composition.
        std::uint64_t* reload = nullptr;      // ScenePresenter::gi_reload.
        const GiRuntimeStatus* status = nullptr;
        const WorldMesh* mesh = nullptr;
        double mesh_build_ms = 0;
        GiProbeSettings* probes = nullptr;                   // ScenePresenter::gi_probes (lot G9).
        const GiWorldLight* light = nullptr;                 // The applied file, or null.
        const ModelLightingStats* model_lighting = nullptr;  // The last frame's model light.
        // Reflection probes (lot G12).
        int* reflections = nullptr;                     // Options > Display REFLECTION PROBES (0-2, saved).
        GiReflectionView* reflection_view = nullptr;    // ScenePresenter::gi_reflection_view.
        const GiReflectionStats* reflection_stats = nullptr;
        bool enhanced = false; // The enhanced renderer is selected (the probes need it).
    };
    // The F8 GI tools overlay (gi_contracts.md lots G3, G4, G8, G9, G10), built with BLOOD2_GI_TOOLS on desktop.
    // It shares UiRuntime's ImGui context and only reads the session: no frame changes while
    // Closed, and its gizmos go through the scene's existing line renderer. Placement edits stay in
    // the overlay (Dirty) until saved to the placement directory.
    class GiTools
    {
      public:
        // Draws the Open window or the Pinned strip into the current ImGui frame and fills
        // `gizmos` (cleared while Closed). The lookup is recomputed only when the world changes,
        // on Refresh, or when the overlay leaves Closed. Returns true when its own window closed it.
        bool draw(MenuInput& input, GameSession&, const Config&, std::vector<LineVertex>& gizmos,
                  const GiRuntimeLink& runtime = {});
        // Forgets the cached lookup (world unload or a new GI directory). Placement edits survive.
        void forget();

        // Status snapshot (also what the tests inspect).
        struct Status
        {
            std::string world_path, file_name, error;
            std::optional<GiExpectation> expected;
            GiLookup lookup;
            std::vector<GiRoot> roots;
        };
        const Status& status() const
        {
            return status_;
        }
        bool camera_cell = true; // Gizmo: the 128-unit lattice cell at the camera, by point contents.
        static constexpr float cell_size = 128;
        // The twelve edges of the lattice cell containing `position`, coloured by the root-BSP
        // contents there: green Empty, red Solid, magenta Unknown.
        static void cell_gizmo(const WorldModel& primary, DVector position, std::vector<LineVertex>&);
        // Recomputes the status for the session's current world, and reloads the placement when the
        // world changed since it was loaded.
        void refresh(const GameSession&, const Config&);
        // One-line summary for the Pinned strip and the Status header.
        std::string summary() const;
        // The Runtime panel's state line: whether the world draws GI, and why not.
        std::string runtime_summary() const;

        // Panels, in tab order. Setting `select` makes the next Open frame show that panel.
        enum class Panel
        {
            Status,
            Runtime,
            LightProbes,
            ReflectionProbes,
            Bake
        };
        std::optional<Panel> select;

        // Placement (lot G4). The scene keeps references to the session's world and collision; it is
        // rebuilt whenever the world changes.
        struct Placement
        {
            std::filesystem::path directory;
            GiPlacementStatus status = GiPlacementStatus::Missing;
            std::string detail;           // Load failure, foreign world, last save or edit error.
            std::optional<GiPlacement> edit; // Empty while a foreign or invalid file waits for a decision.
            bool dirty = false;           // Edited since loaded or saved.
            GiLightLattice lattice;
            GiPlacementStats stats;
            std::vector<GiRejectedNode> rejected;
            std::string error; // Last resolve or placement failure; empty when the lattice is current.
            double resolve_ms = 0;
        };
        const Placement& placement() const
        {
            return placement_;
        }
        // Editing, as the panels call it. Each edit marks the placement dirty; light-probe edits
        // re-resolve the lattice. Reflection edits make the probe manual. They use the scene built by
        // the last refresh, so call them only after draw or refresh saw the current world.
        bool place_light_probes();
        bool place_reflection_probes();
        bool add_reflection(DVector capture);
        bool move_reflection(std::size_t index, DVector capture); // Refits its boxes around the point.
        void delete_reflection(std::size_t index);
        void add_volume(bool include, DVector center);
        void delete_volume(std::size_t index);
        void reset_placement(); // Defaults for the current world (the file is replaced only when saved).
        bool save_placement();
        void revert_placement(); // Reloads the file, dropping edits.
        // Markers: light probes (green, relocated yellow) and rejected nodes (magenta) within
        // `radius` of the camera, reflection probes (orange influence, blue projection, white
        // capture; the selected one yellow) and volumes (green include, red exclude).
        void placement_gizmos(DVector camera, std::vector<LineVertex>&) const;
        bool show_light_probes = true, show_rejected = false, show_reflections = true, show_volumes = true;
        float marker_radius = 1536;
        static constexpr std::size_t max_probe_markers = 4096;
        // "Show baked SH samples" (lot G9): the applied file's probes within marker_radius as spheres
        // whose segments take min(1, k E(direction)) (at least a dim grey), invalid probes magenta.
        bool show_baked_probes = false;
        static constexpr std::size_t max_sh_markers = 1024;
        static constexpr float sh_radius = 10;
        static void baked_probe_gizmos(const GiProbeField&, float k, DVector camera, float radius,
                                       std::vector<LineVertex>&);
        int selected_reflection = -1;

        // Bake (lot G10, decision 10): the current world baked by a child process of this binary
        // (`--bake-gi`), polled every draw (also while Closed), cancelled with SIGTERM (forced on
        // Windows). A finished bake reloads at the next frame boundary (the presenter's lookup,
        // through the runtime link) when the world's DAT digest still matches the one it started for.
        struct Bake
        {
            GiBakeProcess process;
            // Preset (Preview, Final, Ultra or Custom with its settings, lot GQ1), emission scale (lot
            // GE1) and direct light (re-baked, lot GD2's default, or the original lightmaps). Ultra with
            // emission at scale 1 by default (Gate H); the Custom fields start at Final's values and
            // survive switching presets.
            GiBakeSettings settings{.quality = GiQuality::Ultra,
                                    .emissive = gi_bake_settings_limits::default_emissive};
            bool suspend_scene = false; // "Suspend scene rendering during bake".
            std::string world;          // The world path being (or last) baked.
            Sha256Digest world_digest{};
            std::filesystem::path out_dir, file; // user_directory()/gi and the expected file.
            std::string notice;                  // The last outcome, or why a bake did not start.
            bool reload_pending = false;         // Finished for the current world; waiting for a link.
            bool reloaded = false;               // The last bake was handed to the presenter.
            // Frame times between draws while the bake runs (logged when it ends).
            std::uint64_t frames = 0, last_frame_ns = 0;
            double frame_sum_ms = 0, frame_max_ms = 0;
        };
        Bake bake;
        std::filesystem::path bake_executable; // Empty: this executable (tests use a stub).
        // Saves a dirty placement, then starts the child for the current world. False with
        // bake.notice when nothing started.
        bool start_bake(const GameSession&, const Config&);
        void cancel_bake();
        void stop_bake(); // Cancels and waits (quit).
        bool suspend_scene() const
        {
            return bake.suspend_scene && bake.process.running();
        }
        // Toggles a world model in the placement's excluded_models (marks it dirty).
        bool exclude_model(std::string_view name, bool excluded);
        // Desktop GI tool builds: copies the last baked file into the checkout's data/gi/baked.
        bool copy_bake_to_project(std::string& error);

      private:
        Status status_;
        GiRuntimeLink runtime_; // The last draw's link.
        void runtime_panel();
        void reflection_section();
        std::uint64_t world_revision_ = UINT64_MAX;
        bool stale_ = true;
        Placement placement_;
        GiPlacementScene scene_;
        bool scene_ready_ = false;
        std::string placement_world_;
        Sha256Digest placement_digest_{};
        std::uint64_t placement_revision_ = UINT64_MAX;
        void load_placement(const GameSession&, const Config&);
        void read_placement_file();
        std::string placement_summary() const;
        void status_panel(const GameSession&, const Config&);
        void light_panel(const GameSession&);
        void baked_probe_panel(const GameSession&);
        void reflection_panel(const GameSession&);
        void volume_table();
        void update_bake(const GameSession&);
        void bake_panel(const GameSession&, const Config&);
    };
} // namespace OpenBlood2
