// GI lots G3/G4/G8/G9/G10: the F8 overlay's gizmos, status lookup, placement editing, runtime panels,
// the F8 bake (a fake baker, then this build's blood2) and headless ImGui drawing (gi_contracts.md).
#include "app/gi_overlay.h"
#include "app/model_lighting.h"
#include "engine/config.h"
#include "engine/gi_bake.h"
#include "engine/user_directory.h"
#include "game/server_adapter.h"
#include "scratch_dir.h"
#include <imgui.h>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

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
        // The F8 bake (lot G10) needs a world; nothing starts.
        CHECK(!tools.start_bake(game, config) && tools.bake.notice == "No world loaded.");
        CHECK(tools.bake.process.state == GiBakeProcess::State::Idle && !tools.suspend_scene());
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
        // The Runtime panel (lot G8) with the presenter's controls, then without them.
        bool enabled = false;
        GiComposition composition;
        std::uint64_t reload = 0;
        GiRuntimeStatus status;
        const GiRuntimeLink link{&enabled, &composition, &reload, &status, nullptr, 0};
        tools.select = GiTools::Panel::Runtime;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos, link); });
        CHECK(!closed && ImGui::GetDrawData()->TotalVtxCount > 0);
        CHECK(tools.runtime_summary().find("off (Options > Display") != std::string::npos);
        enabled = true;
        status.state = GiRuntimeState::NoWorld;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos, link); });
        CHECK(tools.runtime_summary() == "GI rendering: no world; the original lightmaps are in use.");
        CHECK(composition == GiComposition{} && reload == 0);
        // Lot G9: the probe controls, the Light probes panel's baked samples and their gizmos.
        GiProbeSettings probe_settings;
        GiWorldLight light;
        {
            GiLightLattice lattice;
            lattice.spacing = 64;
            lattice.dims = {2, 1, 1};
            lattice.probes = {{0, lattice.node_position(0)}, {1, lattice.node_position(1)}};
            GiProbes probes;
            probes.sh.assign(54, 0.f);
            probes.sh[0] = probes.sh[1] = probes.sh[2] = 1;
            probes.valid = {1, 0};
            std::string error;
            CHECK(light.probes.build(lattice, probes, error));
            light.has_probes = true;
        }
        ModelLightingStats lighting;
        lighting.probe_lit = 3;
        status.state = GiRuntimeState::Applied;
        status.file = "x.ob2gi";
        status.probes = 2;
        status.valid_probes = 1;
        const GiRuntimeLink probe_link{&enabled, &composition, &reload,         &status,
                                       nullptr,  0,            &probe_settings, &light, &lighting};
        for (const auto panel : {GiTools::Panel::Runtime, GiTools::Panel::LightProbes})
        {
            tools.select = panel;
            frame.run([&] { closed = tools.draw(input, game, config, gizmos, probe_link); });
            CHECK(!closed && ImGui::GetDrawData()->TotalVtxCount > 0);
        }
        tools.show_baked_probes = true;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos, probe_link); });
        CHECK(composition == GiComposition{} && probe_settings == GiProbeSettings{} && reload == 0);
        std::vector<LineVertex> sh_lines;
        GiTools::baked_probe_gizmos(light.probes, 2, {0, 0, 0}, 1536, sh_lines);
        CHECK(sh_lines.size() == 2 * 3 * 12 * 2);
        // The valid probe: k E = 2 Y00, a .56 grey; the invalid one magenta.
        CHECK(std::abs(sh_lines.front().color[0] - 2 * 0.28209479f) < 1e-5f && sh_lines.front().color[0] ==
                                                                               sh_lines.front().color[1]);
        CHECK(sh_lines.back().color[1] < sh_lines.back().color[0] && sh_lines.back().color[2] > .5f);
        sh_lines.clear();
        GiTools::baked_probe_gizmos(light.probes, 2, {1000, 0, 0}, 100, sh_lines);
        CHECK(sh_lines.empty());
        tools.show_baked_probes = false;
        frame.run([&] { closed = tools.draw(input, game, config, gizmos); });
        CHECK(tools.runtime_summary() == "GI rendering: unavailable here.");
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


    // Draws the overlay every `frame_ms` (the game's per-frame poll) until `until` holds.
    template <class Until>
    bool run_until(GiTools& tools, GameSession& game, const Config& config, MenuInput& input,
                   const GiRuntimeLink& link, int timeout_ms, int frame_ms, Until until)
    {
        std::vector<LineVertex> lines;
        Frame frame;
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            frame.run([&] { tools.draw(input, game, config, lines, link); });
            if (until())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(frame_ms));
        }
        return false;
    }
    bool same_bytes(const std::filesystem::path& a, const std::filesystem::path& b)
    {
        std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
        const std::vector<char> va{std::istreambuf_iterator<char>(fa), {}}, vb{std::istreambuf_iterator<char>(fb), {}};
        return fa.good() == fb.good() && !va.empty() && va == vb;
    }

    // Private, after `placement` (lot G10): the F8 bake of 01_SubwayA. The fake baker checks the child's
    // arguments, the reload hand-off, cancel, a world change (NEW_LEVEL) and quit during a bake; then
    // this build's blood2 bakes Preview (frame times logged), its bytes equal an in-process bake, and a
    // cancelled Final exits 12 without touching the file.
    bool bake(GameSession& game, Config& config, const std::filesystem::path& scratch,
              const std::filesystem::path& stub, const std::filesystem::path& blood2)
    {
        config.gi_dir.clear(); // The lookup below searches the user directory first.
        std::filesystem::path out;
        std::string error;
        CHECK(user_directory(out, error));
        out /= "gi";
        GiTools tools;
        tools.refresh(game, config);
        CHECK(tools.placement().status == GiPlacementStatus::Ready);
        bool enabled = true;
        GiComposition composition;
        std::uint64_t reload = 0;
        GiRuntimeStatus runtime;
        const GiRuntimeLink link{&enabled, &composition, &reload, &runtime, nullptr, 0};
        MenuInput input;
        input.gi = GiOverlay::Open;
        tools.select = GiTools::Panel::Bake;
        const auto running = [&] { return tools.bake.process.running(); };
        const auto done = [&] { return !running(); };
        const auto progressed = [&] { return !tools.bake.process.progress.stage.empty() || !running(); };
        using State = GiBakeProcess::State;

        CHECK(tools.bake.process.state == State::Idle && !tools.suspend_scene());

        // Excluded models (case-insensitive, validated); Bake saves the edited placement first.
        std::string model;
        for (std::size_t m = 1; m < game.world.models.size() && model.empty(); ++m)
            model = game.world.models[m].name;
        const auto& placed = tools.placement();
        const auto excluded = placed.edit->excluded_models;
        CHECK(!model.empty() && tools.exclude_model(model, true) && placed.dirty);
        CHECK(placed.edit->excluded_models.size() == excluded.size() + 1);
        auto lower = model;
        for (auto& c : lower)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        CHECK(tools.exclude_model(lower, true) && placed.edit->excluded_models.size() == excluded.size() + 1);
        CHECK(!tools.exclude_model("bad\"name", true) && placed.edit->excluded_models.size() == excluded.size() + 1);
        CHECK(placed.detail.rfind("Not excluded: ", 0) == 0);
        CHECK(tools.exclude_model(lower, false) && placed.edit->excluded_models == excluded && placed.dirty);

        // The fake baker (Preview): the child's arguments, the reload at the next frame.
        tools.bake_executable = stub;
        tools.bake.final_quality = false;
        tools.bake.suspend_scene = true;
        CHECK(tools.start_bake(game, config) && running() && tools.suspend_scene() && !placed.dirty);
        CHECK(!tools.start_bake(game, config) && tools.bake.notice == "A bake is already running.");
        CHECK(run_until(tools, game, config, input, link, 20000, 16, done));
        CHECK(tools.bake.process.state == State::Finished && tools.bake.process.exit_code == 0);
        CHECK(reload == 1 && tools.bake.reloaded && !tools.bake.reload_pending && !tools.suspend_scene());
        CHECK(tools.bake.notice.rfind("worlds/01_subwaya.dat baked (exit 0) in ", 0) == 0);
        CHECK(tools.bake.notice.find("; frames during the bake: ") != std::string::npos && tools.bake.frames > 0);
        CHECK(tools.bake.out_dir == out && tools.bake.file.parent_path().parent_path() == out);
        {
            std::ifstream in(out / "args.txt");
            std::vector<std::string> args;
            for (std::string line; std::getline(in, line);)
                args.push_back(line);
            const unsigned threads = std::thread::hardware_concurrency();
            CHECK(args == gi_bake_arguments(config, "worlds/01_subwaya.dat", "preview", config.gi_placement_dir, out,
                                            threads > 2 ? threads - 2 : 1));
        }

        // Cancel (Final), with the Pinned strip drawn while it runs.
        tools.bake.final_quality = true;
        CHECK(tools.start_bake(game, config));
        CHECK(run_until(tools, game, config, input, link, 20000, 16, progressed) && running());
        input.gi = GiOverlay::Pinned;
        CHECK(run_until(tools, game, config, input, link, 1000, 0, [] { return true; }));
        CHECK(ImGui::GetCurrentContext() == nullptr); // run_until owns its context.
        input.gi = GiOverlay::Open;
        tools.cancel_bake();
        CHECK(run_until(tools, game, config, input, link, 20000, 16, done));
        CHECK(tools.bake.process.state == State::Cancelled && tools.bake.process.exit_code == 12 && reload == 1);
        CHECK(tools.bake.notice.find(" cancelled (exit 12)") != std::string::npos);

        // Quit during a bake: stop_bake cancels and waits.
        CHECK(tools.start_bake(game, config));
        CHECK(run_until(tools, game, config, input, link, 20000, 16, progressed) && running());
        tools.stop_bake();
        CHECK(tools.bake.process.state == State::Cancelled && !running());

        if (!blood2.empty())
        {
            // This build's blood2: an F8 Preview bake of 01_SubwayA at about 60 draws per second.
            tools.bake_executable = blood2;
            tools.bake.final_quality = false;
            tools.bake.suspend_scene = false;
            CHECK(tools.start_bake(game, config));
            CHECK(run_until(tools, game, config, input, link, 300000, 16, done));
            const auto& p = tools.bake.process;
            std::cout << "F8 bake: " << tools.bake.notice << '\n';
            CHECK(p.state == State::Finished && p.exit_code == 0 && p.result.at("status") == "baked");
            CHECK(p.progress.stage == "probes" && p.progress.done == p.progress.total && p.progress.total == 1031);
            CHECK(p.result.at("file") == path_text(tools.bake.file) && std::filesystem::is_regular_file(tools.bake.file));
            CHECK(reload == 2 && tools.bake.reloaded && tools.bake.frames > 0);
            tools.refresh(game, config);
            const auto& lookup = tools.status().lookup;
            CHECK(lookup.chosen >= 0 && lookup.candidates[lookup.chosen].path == path_text(tools.bake.file));

            // The same bytes as an in-process bake (another job count).
            GiBakeRequest request;
            request.world = game.world_path;
            CHECK(make_gi_bake_settings("preview", 0, 0, -1, request.settings, error));
            request.placement_dir = config.gi_placement_dir;
            request.out_dir = scratch / "in-process";
            request.jobs = 3;
            const auto local = bake_gi_world(game.resources, request);
            CHECK(local.exit_code == 0 && same_bytes(local.file, tools.bake.file));

            // A cancelled Final exits 12 and leaves the Preview file as it was.
            tools.bake.final_quality = true;
            CHECK(tools.start_bake(game, config));
            CHECK(run_until(tools, game, config, input, link, 300000, 16, progressed) && running());
            tools.cancel_bake();
            CHECK(run_until(tools, game, config, input, link, 60000, 16, done));
            std::cout << "F8 cancel: " << tools.bake.notice << '\n';
            CHECK(p.state == State::Cancelled && p.exit_code == 12 && reload == 2);
            CHECK(same_bytes(local.file, tools.bake.file));
        }

        // NEW_LEVEL during a bake (last: it changes the session's world): the result is kept for its
        // own world and not reloaded into the new one.
        tools.bake_executable = stub;
        tools.bake.final_quality = true;
        const auto before = reload;
        CHECK(tools.start_bake(game, config));
        CHECK(run_until(tools, game, config, input, link, 20000, 16, progressed) && running());
        CHECK(game.request_world("02_CityHub", error) && game.world_path == "worlds/02_cityhub.dat");
        CHECK(run_until(tools, game, config, input, link, 20000, 16, done));
        CHECK(tools.bake.process.state == State::Finished && reload == before && !tools.bake.reload_pending);
        CHECK(tools.bake.notice.find("the world changed, so it applies when that world loads") != std::string::npos);

        std::vector<LineVertex> lines;
        Frame frame;
        frame.run([&] { tools.draw(input, game, config, lines); }); // Drops the link to these locals.
        return Testing::remove_scratch_dir(out);
    }

    // Private: 01_SubwayA's status follows the --gi-dir folder (missing, stale, ready), and the
    // Open window draws its status table.
    bool retail(const std::filesystem::path& data, const std::filesystem::path& scratch,
                const std::filesystem::path& stub, const std::filesystem::path& blood2)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        // The test's own user directory (CTest): an earlier F8 bake left a Ready file in its gi folder.
        std::filesystem::path user_gi;
        std::string user_error;
        CHECK(user_directory(user_gi, user_error) && Testing::reset_scratch_dir(user_gi / "gi"));
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
        CHECK(tools.status().lookup.chosen == 0 && tools.summary() == "ready file");
        {
            // The summary follows the presenter's runtime state (lot G8).
            bool enabled = false;
            GiComposition composition;
            std::uint64_t reload = 0;
            GiRuntimeStatus runtime;
            const GiRuntimeLink link{&enabled, &composition, &reload, &runtime, nullptr, 0};
            MenuInput closed_input;
            std::vector<LineVertex> lines;
            Frame frame;
            frame.run([&] { tools.draw(closed_input, game, config, lines, link); });
            CHECK(tools.summary() == "ready file, GI off (Options > Display)");
            enabled = true;
            runtime.state = GiRuntimeState::Failed;
            runtime.detail = "test";
            CHECK(tools.summary() == "ready file, not applied (failed)");
            CHECK(tools.runtime_summary() == "GI rendering: failed (test); the original lightmaps are in use.");
            runtime.state = GiRuntimeState::Applied;
            runtime.file = "x.ob2gi";
            CHECK(tools.summary() == "ready file, applied" &&
                  tools.runtime_summary() == "GI rendering: applied from x.ob2gi.");
            frame.run([&] { tools.draw(closed_input, game, config, lines); }); // Drops the link to these locals.
        }
        MenuInput input;
        input.gi = GiOverlay::Open;
        std::vector<LineVertex> gizmos;
        {
            Frame frame; // One ImGui context at a time: `placement` makes its own.
            frame.run([&] { tools.draw(input, game, config, gizmos); });
            CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        }
        CHECK(gizmos.empty()); // No player: no camera, no cell.
        return placement(game, config, scratch) && (stub.empty() || bake(game, config, scratch, stub, blood2)) &&
               Testing::remove_scratch_dir(scratch);
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
    else if (name == "retail" && (argc == 4 || argc == 6))
        ok = retail(path_from_utf8(argv[2]), path_from_utf8(argv[3]), argc == 6 ? path_from_utf8(argv[4]) : "",
                    argc == 6 ? path_from_utf8(argv[5]) : "");
    else
        return 2;
    return ok ? 0 : 1;
}
