#include "app/save_flow.h"
#include "engine/atomic_file.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/weapon_messages.h"
#include "save_rewrites.h"
#include "scratch_dir.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

// Whole-world certification (save_load_contracts lot 14, private data). Every single-player
// world through the real slot files: at entry and after combat, save, load, compare the object
// census, save again with the same bytes apart from the listed rewrites, then run 30 seconds.
// Restart after a load returns to the level-entry state.
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
    std::filesystem::path root, data_dir;
    constexpr double step_seconds = .05;
    constexpr int continue_steps = 600; // 30 seconds.

    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    bool mount(GameSession& g)
    {
        Config config;
        config.data_dir = data_dir;
        std::string error;
        if (g.initialize() && g.resources.mount_game(config, error))
            return true;
        std::cerr << "mount: " << error << '\n';
        return false;
    }
    std::vector<std::string> world_names()
    {
        std::vector<std::string> names;
        GameSession g;
        if (!mount(g))
            return names;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (path.starts_with("worlds/") && path.ends_with(".dat"))
                names.push_back(path.substr(7, path.size() - 11));
        }
        return names;
    }
    bool decode(const SaveImage& image, std::vector<SaveEngineRecord>& records)
    {
        SaveError error;
        if (decode_save_records(image.objects, records, error))
            return true;
        std::cerr << "decode: " << error.text << '\n';
        return false;
    }
    // A save right after a load reproduces OBJS, LINK and ATCH apart from the listed rewrites.
    bool same_image(const SaveImage& a, const SaveImage& b, std::string& why)
    {
        std::vector<SaveEngineRecord> x, y;
        if (!decode(a, x) || !decode(b, y))
        {
            why = "undecodable OBJS";
            return false;
        }
        save_rewrites::apply(x);
        save_rewrites::apply(y);
        if (x.size() != y.size())
        {
            why = "records " + std::to_string(x.size()) + " vs " + std::to_string(y.size());
            return false;
        }
        for (std::size_t i = 0; i < x.size(); ++i)
            if (!(x[i] == y[i]))
            {
                why = "record " + std::to_string(i) + " (" + x[i].class_name + " " + x[i].name + ") differs";
                if (x[i].payload != y[i].payload)
                {
                    std::size_t k = 0;
                    while (k < std::min(x[i].payload.size(), y[i].payload.size()) && x[i].payload[k] == y[i].payload[k])
                        ++k;
                    why += " in its payload at byte " + std::to_string(k) + " of " +
                           std::to_string(x[i].payload.size());
                }
                else
                    why += " in its engine record";
                return false;
            }
        if (a.links != b.links || a.attachments != b.attachments)
        {
            why = a.links != b.links ? "LINK differs" : "ATCH differs";
            return false;
        }
        return true;
    }
    // Live objects by class: the saved (Original, USRFLG_SAVEABLE) classes, which must match after a
    // load, and the authored always-load ones, which come back from the DAT in their authored state
    // (original rule 2: consumed Keys reappear, SkyPointers that removed themselves after binding
    // return until their first update, the runtime SmellHint trail is lost). Those are only reported.
    std::map<std::string, unsigned> census(GameSession& g, bool saved)
    {
        std::map<std::string, unsigned> out;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive)
            {
                const auto support = save_support(r->type);
                if (saved ? support == SaveSupport::Original && (r->user_flags & USRFLG_SAVEABLE)
                          : support == SaveSupport::AlwaysLoad && r->reason == Creation::World)
                    ++out[r->type->name];
            }
        return out;
    }
    std::string census_difference(const std::map<std::string, unsigned>& a, const std::map<std::string, unsigned>& b)
    {
        std::string text;
        std::map<std::string, std::pair<unsigned, unsigned>> all;
        for (const auto& [k, v] : a)
            all[k].first = v;
        for (const auto& [k, v] : b)
            all[k].second = v;
        for (const auto& [k, v] : all)
            if (v.first != v.second)
                text += (text.empty() ? "" : ", ") + k + " " + std::to_string(v.first) + "->" +
                        std::to_string(v.second);
        return text;
    }
    // Runtime health after a load: every step succeeds, no NPC suspends and no service is missing.
    bool run_on(GameSession& g, int steps, std::string& why)
    {
        for (int i = 0; i < steps; ++i)
            if (!g.step(step_seconds))
            {
                why = "step " + std::to_string(i) + " failed";
                for (const auto& line : g.step_errors())
                    why += "; " + line;
                for (const auto& line : g.objects.diagnostics())
                    why += "; " + line;
                return false;
            }
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive)
                if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()); ai && ai->runtime_failed)
                {
                    why = std::string("NPC ") + r->type->name + " " + r->state.m_Name + " failed: " + ai->runtime_error;
                    return false;
                }
        if (!g.server.unavailable_services.empty())
        {
            why = "unavailable service " + g.server.unavailable_services.begin()->first;
            return false;
        }
        return true;
    }
    struct WorldResult
    {
        std::string name, blocker, failure, notes;
        unsigned cycles = 0, records = 0;
        std::uint64_t bytes = 0;
    };
    // One certification cycle through slot 0: save, load, census, save again, run on.
    bool cycle(std::unique_ptr<GameSession>& game, SaveSlots& slots, const char* phase, WorldResult& result)
    {
        std::string error;
        const auto before = census(*game, true), authored = census(*game, false);
        if (!slots.save(*game, 0, error))
        {
            result.blocker = std::string(phase) + ": " + error;
            return false;
        }
        result.bytes = std::max(result.bytes, slots.report.last_bytes);
        SaveContainer file;
        SaveError fault;
        GameSave first;
        if (!read_save_file(slots.path(0), file, fault) || !game_save_from(file, first, error))
        {
            result.failure = std::string(phase) + ": the written slot does not read back: " + fault.text + error;
            return false;
        }
        result.records = std::max(result.records, first.image.records);
        if (slots.load(game, 0, error) != SaveSlots::LoadResult::Loaded)
        {
            result.failure = std::string(phase) + ": " + error;
            return false;
        }
        if (const auto difference = census_difference(before, census(*game, true)); !difference.empty())
        {
            result.failure = std::string(phase) + ": saved-object census differs after the load: " + difference;
            return false;
        }
        if (const auto difference = census_difference(authored, census(*game, false)); !difference.empty())
            result.notes += std::string(result.notes.empty() ? "" : "; ") + phase + " always-load " + difference;
        GameSave again;
        std::string why;
        if (!game->save_game(again, error))
        {
            result.failure = std::string(phase) + ": the loaded game cannot be saved again: " + error;
            return false;
        }
        if (!same_image(first.image, again.image, why))
        {
            result.failure = std::string(phase) + ": the save after the load differs: " + why;
            return false;
        }
        if (!run_on(*game, continue_steps, why))
        {
            result.failure = std::string(phase) + ": after the load, " + why;
            return false;
        }
        ++result.cycles;
        return true;
    }
    bool boss(std::string_view type)
    {
        return type.find("Gideon") != type.npos || type.starts_with("AncientOne") || type == "Naga" ||
               type == "DrudgePriest";
    }
    // Combat by the player: the three nearest live non-boss NPCs take player-attributed damage, two
    // lethal and one wounding, then the world runs three seconds (corpses, drops, gibs, effects).
    unsigned combat(GameSession& g)
    {
        auto* p = player(g);
        if (!p)
            return 0;
        DVector origin;
        g.server.GetObjectPos(p->m_hObject, &origin);
        std::vector<std::pair<float, HOBJECT>> targets;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive)
                if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get());
                    ai && !ai->m_damage.IsDead() && !boss(r->type->name))
                {
                    const auto d = vector_subtract(r->state.m_Pos, origin);
                    targets.emplace_back(vector_dot(d, d), r->handle);
                }
        std::sort(targets.begin(), targets.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        unsigned hits = 0;
        for (std::size_t i = 0; i < std::min<std::size_t>(3, targets.size()); ++i)
        {
            auto* ai = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(targets[i].second));
            DVector at;
            g.server.GetObjectPos(targets[i].second, &at);
            const float amount = i < 2 ? 10000.f : std::max(1.f, ai->m_damage.m_fHitPoints * .3f);
            auto message = g.server.StartMessageToObject(p, targets[i].second, damage_message);
            if (message && g.server.WriteDamage(message, {{0, 0, 1}, at, amount, DamageType::Normal, p->m_hObject}) ==
                               DE_OK &&
                g.server.EndMessage(message) == DE_OK)
                ++hits;
        }
        for (int i = 0; i < 60; ++i)
            g.step(step_seconds);
        return hits;
    }
    bool audit_world(const std::string& name, unsigned difficulty, WorldResult& result)
    {
        result.name = name;
        auto game = std::make_unique<GameSession>();
        if (!mount(*game))
            return false;
        std::string error;
        if (!GameSession::start_new_game(game, name, static_cast<DBYTE>(difficulty), 0, error))
        {
            result.failure = "the new game failed: " + error.substr(0, 200);
            return false;
        }
        auto* p = player(*game);
        if (!p)
        {
            result.failure = "no player after the new game";
            return false;
        }
        // Keeps the player alive through the audit, so a hostile world never ends it early; the
        // flag is saved and restored like any other CDestructable field.
        p->m_damage.m_bGodMode = true;
        SaveSlots slots(root / "worlds" / name);
        // The level-entry snapshot, as the application takes it (it finishes the entry frame).
        if (!slots.capture_level_entry(*game, error))
        {
            result.blocker = "level entry: " + error;
            return false;
        }
        if (!cycle(game, slots, "entry", result))
            return false;
        const unsigned hits = combat(*game);
        if (game->pending_level_change)
        {
            result.blocker = "combat requested a level change";
            return false;
        }
        if (!player(*game) || player(*game)->m_damage.IsDead())
        {
            result.failure = "the player died during combat";
            return false;
        }
        if (!cycle(game, slots, hits ? "combat" : "combat (no NPC)", result))
            return false;
        return true;
    }

    bool worlds(unsigned difficulty, std::string_view only)
    {
        const auto names = world_names();
        CHECK(!names.empty());
        std::vector<WorldResult> results;
        unsigned single = 0, certified = 0;
        for (const auto& name : names)
        {
            if (!only.empty() && name != only)
                continue;
            if (name.starts_with("multi/"))
                continue; // Multiplayer maps: saves are out of scope (D12).
            ++single;
            WorldResult result;
            const bool ok = audit_world(name, difficulty, result);
            certified += ok;
            std::cout << name << ": "
                      << (ok            ? "certified, " + std::to_string(result.records) + " records, " +
                                              std::to_string(result.bytes) + " bytes" +
                                              (result.notes.empty() ? "" : " (" + result.notes + ")")
                          : !result.blocker.empty() ? "BLOCKED " + result.blocker
                                                    : "FAILED " + result.failure)
                      << std::endl;
            results.push_back(std::move(result));
        }
        std::cout << certified << " of " << single << " single-player worlds certified at difficulty " << difficulty
                  << " (" << names.size() << " mounted)\n";
        bool clean = true;
        for (const auto& r : results)
            if (!r.failure.empty() || !r.blocker.empty())
            {
                std::cout << "  " << r.name << ": " << (r.failure.empty() ? r.blocker : r.failure) << '\n';
                clean = false;
            }
        return clean && single > 0;
    }

    // Restart after a load (original rule 11): SubwayA's level-entry snapshot, a slot saved later,
    // then Restart from the loaded slot returns to the entry state, repeatably.
    bool restart()
    {
        auto game = std::make_unique<GameSession>();
        CHECK(mount(*game));
        std::string error;
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 1, 0, error));
        SaveFlow flow;
        flow.slots = SaveSlots(root / "restart");
        if (!flow.level_entered(*game, true, error))
        {
            std::cerr << "level entry: " << error << '\n';
            return false;
        }
        CHECK(flow.slots.level_entry());
        SaveContainer entry_file;
        SaveError fault;
        GameSave entry;
        CHECK(read_save_file(flow.slots.path(SLOT_CURRENT), entry_file, fault) &&
              game_save_from(entry_file, entry, error));
        CHECK(entry_file.meta.kind == SaveKind::LevelEntry && entry_file.meta.world_path == "worlds/01_subwaya.dat");
        const auto entry_position = game->objects.get(player(*game)->m_hObject)->state.m_Pos;
        player(*game)->m_damage.m_bGodMode = true;
        std::string why;
        CHECK(run_on(*game, 200, why)); // Ten seconds of the intro and the train.
        player(*game)->m_damage.m_fHitPoints = 42;
        CHECK(flow.slots.save(*game, 1, error));
        CHECK(run_on(*game, 100, why));
        // Load the slot, then Restart: the start of the slot's level, as carried into it.
        CHECK(flow.slots.load(game, 1, error) == SaveSlots::LoadResult::Loaded);
        CHECK(player(*game)->m_damage.m_fHitPoints == 42);
        SaveContainer slot_file;
        CHECK(read_save_file(flow.slots.path(1), slot_file, fault) && slot_file.level_entry);
        CHECK(flow.slots.level_entry() && *flow.slots.level_entry() == *slot_file.level_entry);
        std::cout << "Slot 1 saved at " << slot_file.meta.simulated_seconds << " s; level entry at "
                  << entry_file.meta.simulated_seconds << " s\n";
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            if (flow.slots.load(game, SLOT_CURRENT, error) != SaveSlots::LoadResult::Loaded)
            {
                std::cerr << "restart: " << error << '\n';
                return false;
            }
            const auto at = game->objects.get(player(*game)->m_hObject)->state.m_Pos;
            CHECK(at.x == entry_position.x && at.y == entry_position.y && at.z == entry_position.z);
            CHECK(player(*game)->m_damage.m_fHitPoints != 42);
            GameSave again;
            CHECK(game->save_game(again, error));
            if (!same_image(entry.image, again.image, why))
            {
                std::cerr << "restart differs from the level entry: " << why << '\n';
                return false;
            }
            CHECK(run_on(*game, 100, why));
        }
        std::cout << "Restart after a load returned to SubwayA's level entry twice\n";
        return true;
    }
    // Control run: the same world for the audit's duration with no save or load.
    bool baseline(const std::string& name)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(mount(*game));
        std::string error, why;
        CHECK(GameSession::start_new_game(game, name, 1, 0, error));
        player(*game)->m_damage.m_bGodMode = true;
        const bool ok = run_on(*game, 2 * continue_steps + 60, why);
        std::cout << name << " without a load: " << (ok ? "ran " : "FAILED: " + why) << '\n';
        return ok;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    data_dir = argv[3];
    if (!OpenBlood2::Testing::reset_scratch_dir(root))
        return 2;
    if (mode == "worlds")
        return worlds(argc > 4 ? static_cast<unsigned>(std::stoul(argv[4])) : 1, argc > 5 ? argv[5] : "") ? 0 : 1;
    if (mode == "baseline" && argc > 4)
        return baseline(argv[4]) ? 0 : 1;
    if (mode == "restart")
        return restart() ? 0 : 1;
    return 2;
}
