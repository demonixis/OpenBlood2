// GI lots G3/G4: the F8 overlay's gizmos, status lookup, placement editing and headless ImGui drawing
// (gi_contracts.md).
#include "app/gi_overlay.h"
#include "engine/config.h"
#include "game/server_adapter.h"
#include "scratch_dir.h"
#include <imgui.h>
#include <iostream>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    // A headless ImGui frame (no platform or renderer backend).
    struct Frame
    {
        Frame()
        {
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = {1280, 720};
            io.DeltaTime = 1.f / 60;
            io.Fonts->AddFontDefault();
            io.Fonts->Build();
        }
        ~Frame()
        {
            ImGui::DestroyContext();
        }
        template <class F> void run(F&& body)
        {
            ImGui::NewFrame();
            body();
            ImGui::Render();
        }
    };

    bool cell()
    {
        WorldModel primary; // No tree: Unknown, magenta.
        std::vector<LineVertex> lines;
        GiTools::cell_gizmo(primary, {10, -10, 200}, lines);
        CHECK(lines.size() == 24);
        float lo[3]{1e9f, 1e9f, 1e9f}, hi[3]{-1e9f, -1e9f, -1e9f};
        for (const auto& v : lines)
        {
            CHECK(v.color[0] == 1 && v.color[1] == 0 && v.color[2] == 1 && v.color[3] == 1);
            for (int i = 0; i < 3; ++i)
            {
                lo[i] = std::min(lo[i], v.position[i]);
                hi[i] = std::max(hi[i], v.position[i]);
            }
        }
        // The cell [0,128) x [-128,0) x [128,256).
        CHECK(lo[0] == 0 && hi[0] == 128 && lo[1] == -128 && hi[1] == 0 && lo[2] == 128 && hi[2] == 256);
        return true;
    }

    // Without a world: Closed clears the gizmos and draws nothing; Open and Pinned draw a frame.
    bool draw()
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        MenuInput input;
        GiTools tools;
        std::vector<LineVertex> gizmos{LineVertex{}};
        Frame frame;
        bool closed = true;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos); });
        CHECK(!closed && gizmos.empty() && ImGui::GetDrawData()->TotalVtxCount == 0);
        input.gi = GiOverlay::Open;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos); });
        CHECK(!closed && gizmos.empty() && ImGui::GetDrawData()->TotalVtxCount > 0);
        CHECK(tools.summary() == "no world" && tools.status().roots.front().source == "user");
        input.gi = GiOverlay::Pinned;
        for (int i = 0; i < 2; ++i) // Auto-sized windows are measured, hidden, on their first frame.
            frame.run([&] { closed = tools.draw(input, game, config, gizmos); });
        CHECK(!closed && ImGui::GetDrawData()->TotalVtxCount > 0);
        // Every panel draws without a world; the placement is not editable.
        input.gi = GiOverlay::Open;
        for (const auto panel : {GiTools::Panel::Status, GiTools::Panel::Runtime, GiTools::Panel::LightProbes,
                                 GiTools::Panel::ReflectionProbes, GiTools::Panel::Bake})
        {
            tools.select = panel;
            frame.run([&] { closed = tools.draw(input, game, config, gizmos); });
            CHECK(!closed && !tools.select && ImGui::GetDrawData()->TotalVtxCount > 0);
        }
        CHECK(!tools.placement().edit && gizmos.empty());
        CHECK(!tools.place_light_probes() && !tools.add_reflection({}) && !tools.save_placement());
        return true;
    }

    std::size_t count_color(const std::vector<LineVertex>& lines, std::array<float, 4> color)
    {
        std::size_t n = 0;
        for (const auto& v : lines)
            n += v.color == color;
        return n;
    }

    // Private, after `retail`: 01_SubwayA's placement in a scratch --gi-placement-dir. Missing gives
    // the defaults (1,031 probes, as the audit), edits stay unsaved until Save, a foreign file waits
    // for "Start from defaults", and edits survive closing and reopening the overlay.
    bool placement(GameSession& game, Config& config, const std::filesystem::path& scratch)
    {
        config.gi_placement_dir = scratch / "placement";
        GiTools tools;
        tools.refresh(game, config);
        const auto& p = tools.placement();
        CHECK(p.status == GiPlacementStatus::Missing && p.edit && !p.dirty && p.error.empty());
        CHECK(p.stats.probes == 1031 && p.lattice.spacing == 128 && p.lattice.probes.size() == 1031);
        CHECK(p.directory == config.gi_placement_dir && !p.rejected.empty());
        const auto at = p.lattice.probes.front().position;
        std::vector<LineVertex> lines;
        tools.placement_gizmos(at, lines);
        const auto probes = lines.size();
        CHECK(probes > 0 && probes % 6 == 0 && probes <= GiTools::max_probe_markers * 6);
        tools.show_rejected = true;
        lines.clear();
        tools.placement_gizmos(p.rejected.front().position, lines);
        CHECK(count_color(lines, {1, .2f, 1, 1}) >= 6); // At least the rejected node itself.
        tools.show_rejected = false;

        // A manual reflection probe at the first probe; Save writes the canonical file.
        CHECK(tools.add_reflection(at));
        CHECK(p.edit->reflections.size() == 1 && !p.edit->reflections[0].automatic && p.dirty);
        CHECK(tools.selected_reflection == 0 && p.edit->reflections[0].projection.contains(at));
        lines.clear();
        tools.placement_gizmos(at, lines);
        CHECK(lines.size() == probes + 2 * 24 + 6 && count_color(lines, {1, 1, .2f, 1}) == 24 + 6);
        CHECK(tools.save_placement() && p.status == GiPlacementStatus::Ready && !p.dirty);
        const auto file = config.gi_placement_dir / "worlds" / "01_subwaya.json";
        CHECK(std::filesystem::exists(file));
        CHECK(!tools.move_reflection(0, {1e5f, 1e5f, 1e5f}) && !p.detail.empty()); // Void: refused, unchanged.
        CHECK(!p.dirty && p.edit->reflections.size() == 1);

        // Automatic placement keeps the manual probe first; deleting marks the placement dirty.
        CHECK(tools.place_reflection_probes() && p.dirty);
        const auto placed = p.edit->reflections.size();
        CHECK(placed > 1 && placed <= 42 && !p.edit->reflections[0].automatic && p.stats.manual == 1);
        CHECK(p.edit->reflections[1].automatic && p.stats.automatic == placed - 1);
        tools.delete_reflection(0);
        CHECK(p.edit->reflections.size() == placed - 1 && tools.selected_reflection == -1);

        // An exclude volume at the first probe removes nodes; deleting it restores the lattice.
        tools.add_volume(false, at);
        CHECK(p.edit->volumes.size() == 1 && p.stats.excluded > 0 && p.stats.probes < 1031 && p.error.empty());
        tools.delete_volume(0);
        CHECK(p.edit->volumes.empty() && p.stats.probes == 1031 && p.stats.excluded == 0);

        // Revert drops edits; reset gives defaults (unsaved).
        tools.revert_placement();
        CHECK(p.status == GiPlacementStatus::Ready && !p.dirty && p.edit->reflections.size() == 1);
        tools.reset_placement();
        CHECK(p.dirty && p.edit->reflections.empty() && p.stats.probes == 1031);
        tools.revert_placement();

        // Every panel draws with the placement loaded.
        MenuInput input;
        input.gi = GiOverlay::Open;
        Frame frame;
        for (const auto panel : {GiTools::Panel::LightProbes, GiTools::Panel::ReflectionProbes})
        {
            tools.select = panel;
            frame.run([&] { tools.draw(input, game, config, lines); });
            CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        }

        // Unsaved edits survive closing and reopening the overlay.
        tools.add_volume(true, at);
        CHECK(p.dirty && p.edit->volumes.size() == 1);
        input.gi = GiOverlay::Closed;
        frame.run([&] { tools.draw(input, game, config, lines); });
        input.gi = GiOverlay::Open;
        frame.run([&] { tools.draw(input, game, config, lines); });
        CHECK(p.dirty && p.edit->volumes.size() == 1);
        tools.revert_placement();

        // A file made for another DAT is Foreign: nothing to edit until "Start from defaults".
        auto foreign = *p.edit;
        foreign.world[0] ^= 1;
        std::string error;
        CHECK(save_gi_placement(config.gi_placement_dir, foreign, error));
        tools.revert_placement();
        CHECK(p.status == GiPlacementStatus::Foreign && !p.edit && !p.detail.empty());
        tools.reset_placement();
        CHECK(p.edit && p.dirty && p.status == GiPlacementStatus::Foreign && p.edit->world != foreign.world);
        CHECK(tools.save_placement() && p.status == GiPlacementStatus::Ready);
        return true;
    }

    // Private: 01_SubwayA's status follows the --gi-dir folder (missing, stale, ready), and the
    // Open window draws its status table.
    bool retail(const std::filesystem::path& data, const std::filesystem::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        Config config;
        config.data_dir = data;
        config.gi_dir = scratch;
        GameSession game;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error) && game.request_world("01_SubwayA", error));
        GiTools tools;
        tools.refresh(game, config);
        const auto& status = tools.status();
        CHECK(status.error.empty() && status.expected && status.world_path == "worlds/01_subwaya.dat");
        CHECK(status.roots.front().source == "--gi-dir" && status.lookup.chosen == -1);
        CHECK(tools.summary() == "no baked file");
        GiFile file;
        file.key.world = status.expected->world;
        file.key.albedo = status.expected->albedo;
        file.key.world_path = status.world_path;
        file.key.settings = sha256({});
        const auto path = scratch / status.file_name;
        file.key.albedo[0] ^= 1;
        CHECK(save_gi_file(path, file, error));
        tools.refresh(game, config);
        CHECK(tools.summary() == "stale file (albedo)");
        file.key.albedo[0] ^= 1;
        CHECK(save_gi_file(path, file, error));
        tools.refresh(game, config);
        CHECK(tools.status().lookup.chosen == 0 && tools.summary() == "ready file, not applied (GI rendering: lot G8)");
        MenuInput input;
        input.gi = GiOverlay::Open;
        std::vector<LineVertex> gizmos;
        {
            Frame frame; // One ImGui context at a time: `placement` makes its own.
            frame.run([&] { tools.draw(input, game, config, gizmos); });
            CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        }
        CHECK(gizmos.empty()); // No player: no camera, no cell.
        return placement(game, config, scratch) && Testing::remove_scratch_dir(scratch);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    bool ok = false;
    if (name == "cell")
        ok = cell();
    else if (name == "draw")
        ok = draw();
    else if (name == "retail" && argc == 4)
        ok = retail(path_from_utf8(argv[2]), path_from_utf8(argv[3]));
    else
        return 2;
    return ok ? 0 : 1;
}
