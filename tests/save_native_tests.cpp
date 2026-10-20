#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/FlagObjects.h"
#include "game/original/PolyGrid.h"
#include "game/original/VolumeBrush.h"
#include "game/world_validation.h"
#include "save_world_fixture.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

// Native state of save/load (save_load_contracts lot 13): the engine-implemented classes, the
// sky and PolyGrid republication after a load, the VoiceMgr and CommLink client state across a
// load, and the CTF refusal path, through GameSession::save_game/load_game.
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    std::filesystem::path root;
    using save_world::object;
    using save_world::text;
    using save_world::vector;
    constexpr const char* current = "Save\\Current\\Current.sav";
    constexpr DDWORD save_flags = SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS;

    bool write_fixture()
    {
        // An authored PolyGrid (always-load, published by its own initial update) beside the player.
        // Zero area: its default water sprite is not in the fixture, which only such a grid tolerates.
        return save_world::write_archive(
            root / "native.rez",
            {{"native", save_world::dat({object("WorldProperties", {text("Name", "WorldProperties0")}),
                                         object("GameStartPoint", {text("Name", "start"), vector("Pos", {0, 10, 0})}),
                                         object("PolyGrid", {text("Name", "pool"), vector("Pos", {0, 1, 0}),
                                                             vector("Dims", {0, 0, 32})})})}});
    }
    bool setup(std::unique_ptr<GameSession>& game)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "native.rez", error));
        CHECK(game->request_world("native", error));
        if (!game->run_world(error))
        {
            std::cerr << "run_world: " << error << '\n';
            for (const auto& service : game->server.unavailable_services)
                std::cerr << "  unavailable: " << service.first << '\n';
            return false;
        }
        return true;
    }
    bool load(std::unique_ptr<GameSession>& game, const GameSave& save)
    {
        std::string error;
        if (GameSession::load_game(game, save, error))
            return true;
        std::cerr << "load refused at " << game->game_load.stage << ": " << error << '\n';
        return false;
    }
    bool steps(GameSession& g, unsigned n, double dt = .05)
    {
        for (unsigned i = 0; i < n; ++i)
            if (!g.step(dt))
            {
                for (const auto& d : g.objects.diagnostics())
                    std::cerr << d << '\n';
                return false;
            }
        return true;
    }
    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    bool contains(const std::string& text, std::string_view part)
    {
        if (text.find(part) != std::string::npos)
            return true;
        std::cerr << "missing \"" << part << "\" in: " << text << '\n';
        return false;
    }
    std::size_t live(GameSession& g, std::string_view type)
    {
        std::size_t n = 0;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && r->type->name == type)
                ++n;
        return n;
    }

    bool declarations()
    {
        // Every engine-implemented class (the engineobjects.c built-ins and StartPoint) is the
        // original CF_ALWAYSLOAD or not saveable: none is left Unsupported, and none needs a
        // payload. BaseClass is never saveable.
        GameSession g;
        CHECK(g.initialize());
        const std::map<std::string, SaveSupport> native{
            {"BaseClass", SaveSupport::NotSaved}, {"Model", SaveSupport::AlwaysLoad},
            {"Container", SaveSupport::AlwaysLoad}, {"Sprite", SaveSupport::AlwaysLoad},
            {"Sound", SaveSupport::AlwaysLoad}, {"DemoSkyWorldModel", SaveSupport::AlwaysLoad},
            {"SkyPointer", SaveSupport::AlwaysLoad}, {"StartPoint", SaveSupport::AlwaysLoad}};
        std::size_t seen = 0;
        for (auto type : g.objects.classes())
            if (const auto it = native.find(type->name); it != native.end())
            {
                CHECK(save_support(type) == it->second && type->payload_version == 0);
                ++seen;
            }
        CHECK(seen == native.size());
        // The always-load classes the original engine has but no shipped world places stay
        // unregistered: WorldModel, FogSphere (and the editor-only World and Brush).
        for (const char* absent : {"WorldModel", "FogSphere", "World", "Brush"})
            CHECK(!g.objects.find_class(absent));
        // The CTF classes are NotSaved: the original never marks them saveable.
        for (auto type : g.objects.classes())
            if (type->name == "FlagObject" || type->name == "FlagStand")
                CHECK(save_support(type) == SaveSupport::NotSaved);
        return true;
    }

    bool polygrid()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        CHECK(steps(*game, 5));
        CHECK(game->server.polygrids.active.size() == 1 && live(*game, "PolyGrid") == 1);
        std::string error;
        CHECK(check_restored_publications(*game, error));
        // A PolyGrid is always-load: the shell's list skips it, the image holds only the player.
        SaveObjectsReport listed;
        std::vector<HOBJECT> all{player(*game)->m_hObject};
        for (auto id : game->objects.snapshot())
            if (const auto* r = game->objects.get(id); r && r->life == Life::Alive && r->type->name == "PolyGrid")
                all.push_back(r->handle);
        CHECK(game->server.save_objects(current, all, 0, save_flags, listed) && listed.always_load == 1 &&
              listed.saved == 1);
        GameSave save;
        CHECK(game->save_game(save, error));
        CHECK(save.image.classes.size() == 1 && save.image.classes.front().name == "CPlayerObj");
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(load(game, save));
            // The restore-mode world recreated the authored PolyGrid, which published once.
            CHECK(game->server.polygrids.active.size() == 1 && live(*game, "PolyGrid") == 1);
            CHECK(game->server.polygrids.started == 1 && game->server.polygrids.failed == 0);
            CHECK(check_restored_publications(*game, error));
            CHECK(steps(*game, 10) && game->server.polygrids.active.size() == 1);
            CHECK(game->save_game(save, error));
        }
        // The checks catch a duplicated publication and a duplicated sky registration.
        auto& grids = game->server.polygrids.active;
        grids.push_back({grids.front().owner, {}});
        CHECK(!check_restored_publications(*game, error) && contains(error, "PolyGrid published twice"));
        grids.pop_back();
        CHECK(check_restored_publications(*game, error));
        const auto* record = game->objects.get(player(*game)->m_hObject);
        game->server.sky.registered.push_back(record->id);
        game->server.sky.registered.push_back(record->id);
        CHECK(!check_restored_publications(*game, error) && contains(error, "Sky object registered twice"));
        game->server.sky.registered.clear();
        game->server.sky.objects[3] = record->id;
        CHECK(!check_restored_publications(*game, error) && contains(error, "unregistered"));
        game->server.sky.objects[3] = {};
        CHECK(check_restored_publications(*game, error));
        // A record of an engine-implemented always-load class is refused by name, never restored:
        // it is not in the build's Original schema.
        for (const char* type : {"PolyGrid", "Sound", "SkyPointer", "Model"})
        {
            auto bad = save;
            std::vector<SaveEngineRecord> records;
            SaveError codec;
            CHECK(decode_save_records(bad.image.objects, records, codec) && records.size() == 1);
            records.front().class_name = type;
            CHECK(encode_save_records(records, bad.image.objects, codec));
            bad.image.classes = bad.fingerprint.classes = {{type, 1}};
            CHECK(!GameSession::load_game(game, bad, error) && contains(error, type) &&
                  game->game_load.stage == "fingerprint" &&
                  game->server.polygrids.active.size() == 1);
        }
        return true;
    }

    bool voices()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        CHECK(steps(*game, 5));
        auto& source = game->client_shell.voice_manager;
        source.played[0][2] = source.played[3][8] = true;
        source.on = false;
        source.groups[1][4].last = 7;
        source.SetNextPlayTime(100); // A long cooldown in the source's clock.
        std::string error;
        GameSave save;
        CHECK(game->save_game(save, error));
        CHECK(load(game, save));
        const auto& restored = game->client_shell.voice_manager;
        // StartNewGame's Reset: every unique flag is clear and the next play time is the
        // restored session's own (no voice played since the load).
        for (const auto& character : restored.played)
            CHECK(std::none_of(character.begin(), character.end(), [](bool b) { return b; }));
        CHECK(restored.next_play_time == 0 && !restored.voice);
        // The client process continues: on/off state and per-group last choices are kept.
        CHECK(!restored.on && restored.groups[1][4].last == 7);
        // The communication panel starts idle, with no retained voice.
        const auto& link = game->client_shell.communication;
        CHECK(link.level == 0 && link.text_level == 0 && !link.voice);
        // The cooldown is checked against the restored clock.
        game->client_shell.voice_manager.on = true;
        CHECK(game->client_shell.voice_manager.IsOkToPlaySound(false));
        game->client_shell.voice_manager.SetNextPlayTime(5);
        CHECK(!game->client_shell.voice_manager.IsOkToPlaySound(false) &&
              game->client_shell.voice_manager.reason == "cooldown");
        CHECK(game->client_shell.voice_manager.next_play_time > game->server.GetTime());
        return true;
    }

    bool ctf()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        CHECK(steps(*game, 2));
        std::string error;
        GameSave save;
        const auto revision = game->server.save_store.revision();
        auto* p = player(*game);
        p->ctf_team = 1;
        CHECK(!game->save_game(save, error) && contains(error, "CTF games cannot be saved") &&
              contains(error, "CTF team"));
        p->ctf_team = 0;
        game->server.set_game_settings(1, GAMETYPE_CTF);
        CHECK(!game->save_game(save, error) && contains(error, "CTF games cannot be saved"));
        game->server.set_game_settings(1, GAMETYPE_BLOODBATH);
        CHECK(!game->save_game(save, error) && contains(error, "single-player game types"));
        CHECK(game->server.save_store.revision() == revision); // Nothing was written.
        game->server.set_game_settings(1, GAMETYPE_SINGLE);
        CHECK(game->save_game(save, error));
        // A CTF image is refused at admission, naming CTF.
        for (auto& variable : save.image.console)
            if (variable.name == "GAME_GAMETYPE")
                variable.value = "2";
        CHECK(!GameSession::load_game(game, save, error) && contains(error, "CTF saves cannot be loaded") &&
              game->game_load.stage == "admission");
        return true;
    }

    // Real worlds: what still blocks a whole save of each world, the CTF refusal on the CTF maps,
    // whole restores of every world that saves, and sky/PolyGrid republication for every world
    // with a sky or a PolyGrid through an image of every Original object (the player included).
    bool private_worlds(const std::filesystem::path& data)
    {
        std::vector<std::string> worlds;
        {
            GameSession g;
            CHECK(g.initialize());
            Config config;
            config.data_dir = data;
            std::string error;
            CHECK(g.resources.mount_game(config, error));
            for (const auto& [path, resource] : g.resources.index())
            {
                (void)resource;
                if (path.starts_with("worlds/") && path.ends_with(".dat"))
                    worlds.push_back(path.substr(7, path.size() - 11));
            }
        }
        unsigned entered = 0, whole = 0, ctf_refused = 0, ctf_maps = 0, published = 0;
        for (const auto& name : worlds)
        {
            auto game = std::make_unique<GameSession>();
            CHECK(game->initialize());
            Config config;
            config.data_dir = data;
            std::string error;
            CHECK(game->resources.mount_game(config, error));
            if (!game->request_world(name, error))
            {
                std::cout << name << ": not entered (" << error.substr(0, 120) << ")\n";
                continue;
            }
            game->server_shell.inspection_start = true; // Multiplayer maps: the local inspection marker.
            if (!game->enter_player(error))
            {
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
                continue;
            }
            ++entered;
            CHECK(steps(*game, 30));
            const bool ctf_map = g_ctf.flags[0] || g_ctf.flags[1];
            GameSave save;
            const bool saved = game->save_game(save, error);
            std::cout << name << ": " << (saved ? "saved whole" : error) << '\n';
            if (ctf_map)
            {
                ++ctf_maps;
                CHECK(!saved && contains(error, "CTF games cannot be saved"));
                ++ctf_refused;
                // The player's CTF team is cleared below so its image can still test the sky.
                if (auto* p = player(*game))
                    p->ctf_team = 0;
            }
            else
                CHECK(saved || contains(error, "cannot be saved: "));
            if (saved)
            {
                for (int cycle = 0; cycle < 2; ++cycle)
                {
                    const auto before = save.image.objects;
                    CHECK(load(game, save));
                    CHECK(check_restored_publications(*game, error));
                    GameSave again;
                    CHECK(game->save_game(again, error));
                    // After the first load the listed rewrites are applied; later loads reproduce bytes.
                    if (cycle)
                        CHECK(again.image.objects == before);
                    CHECK(steps(*game, 30) && game->server.unavailable_services.empty());
                    CHECK(game->save_game(save, error));
                }
                ++whole;
            }
            // Sky and PolyGrid republication through an image of every Original object.
            const auto sky_before = game->server.sky.registered.size();
            const auto grids_before = game->server.polygrids.active.size();
            if (!sky_before && !grids_before)
                continue;
            std::size_t surfaces = 0;
            std::vector<HOBJECT> covered;
            for (auto id : game->objects.snapshot())
                if (const auto* r = game->objects.get(id); r && r->life == Life::Alive)
                {
                    if (save_support(r->type) == SaveSupport::Original && (r->user_flags & USRFLG_SAVEABLE))
                        covered.push_back(r->handle);
                    if (const auto* v = dynamic_cast<const VolumeBrush*>(r->game.get()); v && v->surface_object)
                        ++surfaces;
                }
            SaveObjectsReport report;
            CHECK(game->server.save_objects(current, covered, 0, save_flags, report));
            GameSave image{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
            SaveError fingerprint;
            CHECK(build_fingerprint(game->resources, image.world_path, image.image.classes, image.fingerprint,
                                    fingerprint));
            if (!load(game, image))
                return false;
            std::size_t restored_surfaces = 0;
            for (auto id : game->objects.snapshot())
                if (const auto* r = game->objects.get(id); r && r->life == Life::Alive)
                    if (const auto* v = dynamic_cast<const VolumeBrush*>(r->game.get()); v && v->surface_object)
                        ++restored_surfaces;
            // Every surface reappears once and every PolyGrid publishes once; the sky holds the
            // same objects, the always-load pointers binding again with no duplicate.
            CHECK(restored_surfaces == surfaces && game->server.polygrids.active.size() == grids_before);
            CHECK(check_restored_publications(*game, error));
            CHECK(steps(*game, 10) && game->server.unavailable_services.empty());
            CHECK(game->server.sky.registered.size() == sky_before &&
                  game->server.polygrids.active.size() == grids_before && check_restored_publications(*game, error));
            std::cout << "  republished " << sky_before << " sky objects and " << grids_before << " PolyGrids ("
                      << surfaces << " volume surfaces)\n";
            ++published;
        }
        std::cout << entered << " of " << worlds.size() << " worlds entered; " << whole << " saved whole; "
                  << ctf_refused << " of " << ctf_maps << " CTF maps refused; " << published
                  << " worlds republished their sky and PolyGrids\n";
        CHECK(entered == worlds.size() && ctf_maps > 0 && ctf_refused == ctf_maps && published > 0);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    if (mode == "private")
        return private_worlds(argv[2]) ? 0 : 1;
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (!write_fixture())
        return 1;
    const std::map<std::string_view, std::function<bool()>> cases{
        {"declarations", declarations}, {"polygrid", polygrid}, {"voices", voices}, {"ctf", ctf}};
    const auto it = cases.find(mode);
    return it != cases.end() && it->second() ? 0 : 1;
}
