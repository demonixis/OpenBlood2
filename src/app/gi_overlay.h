#pragma once
#include "app/menu_input.h"
#include "engine/config.h"
#include "engine/gi_file.h"
#include "engine/gi_placement.h"
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
    // What the Runtime panel (lot G8) reads and controls. Null members leave it unavailable.
    struct GiRuntimeLink
    {
        bool* enabled = nullptr;              // Options > Display GLOBAL ILLUMINATION (saved with the preferences).
        GiComposition* composition = nullptr; // ScenePresenter::gi_composition.
        std::uint64_t* reload = nullptr;      // ScenePresenter::gi_reload.
        const GiRuntimeStatus* status = nullptr;
        const WorldMesh* mesh = nullptr;
        double mesh_build_ms = 0;
    };
    // The F8 GI tools overlay (gi_contracts.md lots G3, G4, G8), built with BLOOD2_GI_TOOLS on desktop.
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
        int selected_reflection = -1;

      private:
        Status status_;
        GiRuntimeLink runtime_; // The last draw's link.
        void runtime_panel();
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
        void reflection_panel(const GameSession&);
        void volume_table();
    };
} // namespace OpenBlood2
