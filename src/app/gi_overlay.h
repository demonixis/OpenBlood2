#pragma once
#include "app/menu_input.h"
#include "engine/config.h"
#include "engine/gi_file.h"
#include "engine/line_renderer.h"
#include <optional>
#include <string>
#include <vector>

namespace OpenBlood2
{
    class GameSession;
    // The F8 GI tools overlay (gi_contracts.md lot G3), built with BLOOD2_GI_TOOLS on desktop.
    // It shares UiRuntime's ImGui context and only reads the session: no frame changes while
    // Closed, and its gizmos go through the scene's existing line renderer.
    class GiTools
    {
      public:
        // Draws the Open window or the Pinned strip into the current ImGui frame and fills
        // `gizmos` (cleared while Closed). The lookup is recomputed only when the world changes,
        // on Refresh, or when the overlay leaves Closed. Returns true when its own window closed it.
        bool draw(MenuInput& input, GameSession&, const Config&, std::vector<LineVertex>& gizmos);
        // Forgets the cached lookup (world unload or a new GI directory).
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
        // Recomputes the status for the session's current world.
        void refresh(const GameSession&, const Config&);
        // One-line summary for the Pinned strip and the Status header.
        std::string summary() const;

      private:
        Status status_;
        std::uint64_t world_revision_ = UINT64_MAX;
        bool stale_ = true;
        void status_panel(const GameSession&, const Config&);
    };
} // namespace OpenBlood2
