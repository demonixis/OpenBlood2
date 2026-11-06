// GI lot G3: the F8 overlay's gizmo, status lookup and headless ImGui drawing (gi_contracts.md).
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
        Frame frame;
        frame.run([&] { tools.draw(input, game, config, gizmos); });
        CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        CHECK(gizmos.empty()); // No player: no camera, no cell.
        return Testing::remove_scratch_dir(scratch);
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
