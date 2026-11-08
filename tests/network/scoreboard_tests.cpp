#include "../resource_fixture.h"
#include "../string_resource_fixture.h"
#include "app/scoreboard.h"
#include "audio_fixture.h"
#include "client/network_player_events.h"
#include "client/network_speech.h"
#include <iostream>
#if BLOOD2_SCOREBOARD_GPU
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#endif
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    const Joined owner{7, 10, {0, 1}};
    PlayerRoster initial()
    {
        return {{{10, {0, 1}, "A%s", 0, 0}, {11, {1, 1}, "B", 1, 0}, {12, {2, 1}, "C", 2, 0}}};
    }
    bool state()
    {
        std::string error;
        NetworkPlayerEvents inbox;
        CHECK(inbox.begin(owner));
        std::uint64_t sequence = 0;
        const auto send = [&](PrivateValue value)
        {
            return inbox.receive({owner, ++sequence, 0, std::move(value)}, error) ==
                   NetworkPlayerEvents::Reception::Applied;
        };
        const auto ids = [&]
        {
            std::vector<std::uint64_t> result;
            for (const auto& p : inbox.scores())
                result.push_back(p.player);
            return result;
        };
        CHECK(send(initial()) && ids() == std::vector<std::uint64_t>({10, 11, 12}));
        CHECK(send(FragEvent{{1, 1}, {2, 1}, 1, 0}) && ids() == std::vector<std::uint64_t>({12, 10, 11}));
        CHECK(send(FragEvent{{1, 1}, {0, 1}, 1, 0}) && ids() == std::vector<std::uint64_t>({12, 10, 11}));
        CHECK(send(FragEvent{{2, 1}, {2, 1}, 0, 0}) && ids() == std::vector<std::uint64_t>({10, 12, 11}));
        auto roster = initial();
        roster.players[0].frags = 1;
        roster.players.push_back({13, {3, 1}, "D", 3, 0});
        CHECK(send(roster) && ids() == std::vector<std::uint64_t>({10, 12, 11, 13}));
        CHECK(send(FragEvent{{0, 1}, {0, 1}, 0, 0}) && ids() == std::vector<std::uint64_t>({10, 12, 11, 13}));
        CHECK(send(FragEvent{{2, 1}, {2, 1}, -1, 0}) && ids() == std::vector<std::uint64_t>({10, 11, 13, 12}));
        roster.players[0].frags = 0;
        roster.players[2].frags = -1;
        roster.players.erase(roster.players.begin() + 1);
        CHECK(send(roster) && ids() == std::vector<std::uint64_t>({10, 13, 12}));
        roster.players.push_back({14, {1, 2}, "B", 1, 0});
        CHECK(send(roster) && ids() == std::vector<std::uint64_t>({10, 13, 14, 12}));
        const auto before = inbox.scores();
        auto invalid = roster;
        invalid.players.back().player = 11;
        CHECK(inbox.receive({owner, sequence + 1, 0, invalid}, error) == NetworkPlayerEvents::Reception::Refused &&
              inbox.scores() == before);
        CHECK(inbox.receive({owner, sequence, 0, roster}, error) == NetworkPlayerEvents::Reception::Ignored &&
              inbox.scores() == before);
        CHECK(inbox.begin(owner) && inbox.scores().empty());
        sequence = 0;
        roster = initial();
        roster.players[0].frags = 5;
        roster.players[1].frags = -1;
        roster.players[2].frags = 5;
        CHECK(send(roster) && ids() == std::vector<std::uint64_t>({10, 12, 11}));
        std::cout << "Source stable-tie ranking, gain/suicide, late roster, leave/reconnect, duplicate/refused "
                     "atomicity and reset passed\n";
        return true;
    }
    bool controls()
    {
        std::string error;
        ScoreboardControl scores;
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = 3;
        event.key.scancode = SDL_SCANCODE_TAB;
        scores.event(event, 3, true);
        CHECK(scores.visible);
        event.key.repeat = true;
        scores.event(event, 3, true);
        CHECK(scores.visible);
        event.key.repeat = false;
        scores.event(event, 4, true);
        CHECK(scores.visible);
        scores.event(event, 3, false);
        CHECK(scores.visible);
        scores.event(event, 3, true);
        CHECK(!scores.visible);
        event = {};
        event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        event.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
        scores.event(event, 3, true);
        CHECK(scores.visible);
        scores.event(event, 3, false);
        CHECK(scores.visible);
        scores.receive({owner, 1, 0, PlayerDead{}});
        CHECK(scores.visible && scores.dead);
        scores.event(event, 3, true);
        CHECK(!scores.visible && scores.dead);
        CameraState living;
        living.alive = true;
        scores.receive({owner, 2, 0, living});
        CHECK(!scores.visible && !scores.dead);
        scores.event(event, 3, true);
        CHECK(scores.visible);
        scores.receive({owner, 3, 0, living});
        CHECK(scores.visible); // Ordinary live snapshots do not hide a manually opened table.
        const auto column = scores.team_column;
        scores.event(event, 3, true);
        CHECK(!scores.visible && scores.team_column == column);
        scores.event(event, 3, true);
        CHECK(scores.visible && scores.team_column == column % 2 + 1);
        return true;
    }
    bool notices(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        test_resource::Directory dir;
        dir.files["cres.dll"] = test_strings::pe({{25, u"Bienvenue %1!s! / 100%%"}, {26, u"%2!n!"}}).b;
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        const auto [off, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, off);
        rez.patch(135, size);
        {
            std::ofstream f(root / "strings.rez", std::ios::binary);
            f.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
            CHECK(f.good());
        }
        CHECK(resources.mount(root / "strings.rez", error));
        NetworkAudio audio;
        NetworkSpeech speech(audio, resources);
        CHECK(audio.begin(owner) && speech.begin(owner));
        auto roster = initial();
        CHECK(speech.receive({owner, 1, 0, roster}, 1, false, roster.players, error));
        CHECK(speech.joined_notices == 3 && speech.notices().size() == 3 &&
              speech.notices()[0].text == "Bienvenue A%s / 100%");
        CHECK(speech.receive({owner, 1, 0, roster}, 1, false, roster.players, error) && speech.joined_notices == 3);
        CHECK(speech.receive({owner, 2, 0, roster}, 2, false, roster.players, error) && speech.notices().size() == 3);
        roster.players.pop_back();
        CHECK(speech.receive({owner, 3, 0, roster}, 3, false, roster.players, error));
        CHECK(speech.left_notices == 1 && speech.notices().back().text == "C has left the game." &&
              speech.announcer.fallback_texts == 1);
        roster.players.push_back({13, {2, 2}, "C%1!s!", 3, 0});
        CHECK(speech.receive({owner, 4, 0, roster}, 4, false, roster.players, error));
        CHECK(speech.joined_notices == 4 && speech.notices().size() == 4 &&
              speech.notices().back().text == "Bienvenue C%1!s! / 100%");
        CHECK(speech.update(6, error) && speech.notices().size() == 2);
        CHECK(speech.update(9, error) && speech.notices().empty());
        CHECK(!audio.local_started && !audio.started && !speech.communication.received);
        std::string text;
        CHECK(speech.announcer.player_notice("A%2!s!", true, resources, text, 2) &&
              text == "A%2!s! has joined the game on Team Blue."); // Original missing-team-name fallback typo.
        CHECK(!speech.announcer.player_notice("A", true, resources, text, 3));
        speech.clear();
        CHECK(speech.notices().empty());
        std::cout << "Localized arrival/departure, literal percent names, source fallback, shared four-line expiry and "
                     "no audio passed\n";
        return true;
    }
    bool assets(const std::filesystem::path& root)
    {
        std::string error;
        Config config;
        config.data_dir = root;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ClientFragTable table(resources);
        auto rows = initial().players;
        rows.push_back({13, {3, 1}, "Gabriella", 3, -3});
        CHECK(table.compose(rows, 640, 480, error) && table.displayed == 4);
        const auto first = table.surface().rgba;
        const auto rev = table.revision;
        CHECK(table.compose(rows, 640, 480, error) && table.revision == rev);
        CHECK(table.x == (640 - int(table.surface().width)) / 2 && table.y == (480 - int(table.surface().height)) / 2);
        rows[0].frags = 3;
        CHECK(table.compose(rows, 640, 480, error) && table.revision == rev + 1 && table.surface().rgba != first);
        CHECK(table.compose(rows, 320, 240, error) && table.displayed > 0 && table.displayed <= 4);
        CHECK(!table.compose(rows, 0, 240, error));
        CHECK(table.compose(rows, 32, 32, error) && !table.displayed && table.surface().rgba.empty());
        Resources missing;
        ClientFragTable absent(missing);
        CHECK(!absent.compose(rows, 640, 480, error) && !error.empty());
        ClientAnnouncer announcer;
        std::string text;
        CHECK(announcer.player_notice("A%s", true, resources, text) && text == "A%s has joined the game.");
        CHECK(announcer.player_notice("A%s", false, resources, text) && text == "A%s has left the game." &&
              !announcer.fallback_texts);
        CHECK(announcer.team_name(1, resources) == "Team Blue" && announcer.team_name(2, resources) == "Team Red");
        CHECK(announcer.player_notice("A%2!s!", true, resources, text, 2) &&
              text == "A%2!s! has joined the game on Team Red." && !announcer.fallback_texts);
        for (unsigned i = 0; i < rows.size(); ++i)
            rows[i].team = DBYTE(i % 2 + 1);
        CHECK(table.compose_teams(rows, {8, -3}, 0, 640, 480, error) && table.displayed == 6);
        const auto team_image = table.surface().rgba;
        const auto team_revision = table.revision;
        CHECK(table.compose_teams(rows, {8, -3}, 0, 640, 480, error) && table.revision == team_revision);
        unsigned labels = 0;
        table.set_team_label([&](DBYTE team, HudSurface& surface, const HudFont& font, const HudRect& row)
        {
            ++labels;
            font.draw(surface, team == 1 ? "EQUIPE BLEUE" : "EQUIPE ROUGE", row.left, row.top, 0);
            return true;
        });
        CHECK(table.compose_teams(rows, {8, -3}, 0, 640, 480, error) && table.revision != team_revision &&
              table.surface().rgba != team_image && labels == 2);
        table.set_team_label({});
        CHECK(table.compose_teams(rows, {8, -3}, 0, 640, 480, error) && table.surface().rgba == team_image);
        CHECK(table.compose_teams(rows, {9, -3}, 0, 640, 480, error) && table.surface().rgba != team_image);
        CHECK(table.compose_teams(rows, {9, -3}, 0, 320, 240, error) && !table.displayed);
        CHECK(table.compose_teams(rows, {9, -3}, 1, 320, 240, error) && table.displayed == 3);
        const auto blue = table.surface().rgba;
        CHECK(table.compose_teams(rows, {9, -3}, 2, 320, 240, error) && table.displayed == 3 &&
              table.surface().rgba != blue);
        CHECK(table.compose_teams(rows, {9, -3}, 1, 32, 32, error) && !table.displayed && table.surface().rgba.empty());
        CHECK(!table.compose_teams(rows, {1000001, 0}, 1, 640, 480, error));
        CHECK(!table.compose_teams(rows, {0, 0}, 3, 640, 480, error));
        rows[0].team = 0;
        CHECK(!table.compose_teams(rows, {0, 0}, 1, 640, 480, error));
        std::cout << "Retail Team titles/portraits/totals, localized notices, column switching and bounded tiny "
                     "viewports passed\n";
        std::cout << "Retail frag PCX/FNT composition, cache revision, signed scores, small-screen mode, tiny-window "
                     "safety and exact localized notices passed\n";
        return true;
    }
#if BLOOD2_SCOREBOARD_GPU
    bool gpu(const std::filesystem::path& root, const std::filesystem::path& output)
    {
        std::string error;
        Config config;
        config.data_dir = root;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        Runtime runtime;
        CHECK(runtime.initialize(true, error));
        UiRuntime ui(runtime);
        CHECK(ui.initialize(error));
        ClientFragTable table(resources);
        ScoreboardRenderer renderer(runtime.gpu);
        auto rows = initial().players;
        rows[0].frags = 25;
        rows[1].frags = 12;
        rows[2].frags = 0;
        rows.push_back({13, {3, 1}, "Gabriella with a deliberately long name", 3, -3});
        std::error_code ec;
        std::filesystem::create_directories(output, ec);
        CHECK(!ec);
        struct Scene
        {
            unsigned width, height;
            DBYTE team;
        };
        for (const auto [width, height, team] :
             {Scene{640, 480, 0}, Scene{320, 240, 0}, Scene{640, 480, 1}, Scene{320, 240, 1}, Scene{320, 240, 2}})
        {
            if (team)
            {
                for (unsigned i = 0; i < rows.size(); ++i)
                    rows[i].team = DBYTE(i % 2 + 1);
                CHECK(table.compose_teams(rows, {40, -5}, team, width, height, error));
            }
            else
                CHECK(table.compose(rows, width, height, error));
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::GetIO().DisplaySize = {float(width), float(height)};
            ImGui::GetIO().DisplayFramebufferScale = {1, 1};
            ImGui::NewFrame();
            CHECK(renderer.draw(table, error));
            ImGui::GetForegroundDrawList()->AddText({1, 0}, IM_COL32_WHITE, "A%s has joined the game.");
            ImGui::Render();
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                     height));
            auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(cmd);
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
            SDL_GPUColorTargetInfo color{};
            color.texture = capture.texture;
            color.load_op = SDL_GPU_LOADOP_CLEAR;
            color.store_op = SDL_GPU_STOREOP_STORE;
            color.clear_color = {.05f, .06f, .08f, 1};
            auto* pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
            CHECK(pass);
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
            SDL_EndGPURenderPass(pass);
            auto* copy = SDL_BeginGPUCopyPass(cmd);
            CHECK(copy);
            SDL_GPUTextureRegion region{};
            region.texture = capture.texture;
            region.w = width;
            region.h = height;
            region.d = 1;
            SDL_GPUTextureTransferInfo target{};
            target.transfer_buffer = capture.transfer;
            target.pixels_per_row = width;
            target.rows_per_layer = height;
            SDL_DownloadFromGPUTexture(copy, &region, &target);
            SDL_EndGPUCopyPass(copy);
            CHECK(SDL_SubmitGPUCommandBuffer(cmd));
            CHECK(capture.save(path_text(output / (std::string(team ? "team-" : "scores-") + std::to_string(width) +
                                                   (team ? "-" + std::to_string(team) : "") + ".png"))
                                   .c_str()));
        }
        return true;
    }
#endif
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode = argv[1];
    if (mode == "state")
        return state() ? 0 : 1;
    if (mode == "controls")
        return controls() ? 0 : 1;
    if (argc == 3 && mode == "notices")
        return notices(argv[2]) ? 0 : 1;
    if (argc == 3 && mode == "assets")
        return assets(argv[2]) ? 0 : 1;
#if BLOOD2_SCOREBOARD_GPU
    if (argc == 4 && mode == "gpu")
        return gpu(argv[2], argv[3]) ? 0 : 1;
#endif
    return 2;
}
