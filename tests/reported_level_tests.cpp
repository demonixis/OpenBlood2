#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <iostream>

using namespace OpenBlood2;
namespace
{
    bool check(const char* root, const char* world)
    {
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        std::string error;
        const auto step_failure = [&]
        {
            for (const auto& why : game->step_errors())
                std::cerr << why << '\n';
            for (const auto& why : game->objects.diagnostics())
                std::cerr << "Object: " << why << '\n';
            for (const auto& why : game->trace)
                std::cerr << "Session: " << why << '\n';
        };
        if (!game->initialize() || !game->resources.mount_game(config, error))
        {
            std::cerr << world << ": initialization/mount failed: " << error << '\n';
            return false;
        }
        // Preserve per-source diagnostics before start_new_game discards a rejected candidate.
        if (!game->request_world(std::string_view(world) == "collection" ? "18_temple" : world, error))
        {
            std::cerr << world << ": " << error << '\n';
            return false;
        }
        for (std::size_t i = 0; i < game->entries.size(); ++i)
            if (game->entries[i].state == EntryState::Failed || game->entries[i].state == EntryState::Unsupported)
                std::cout << world << " entry=" << i << " class=" << game->world.objects[i].class_name << " "
                          << entry_state_name(game->entries[i].state) << ": " << game->entries[i].detail << '\n';
        if (std::string_view(world) == "collection")
        {
            unsigned collected = 0;
            for (std::size_t i = 0; i < game->entries.size(); ++i)
                if (game->entries[i].state == EntryState::CollectedPickup)
                {
                    ++collected;
                    if (game->world.objects[i].class_name != "DieBugDieAmmoPU" || game->entries[i].object ||
                        game->entries[i].detail.find("acknowledged") == std::string::npos)
                        return false;
                }
            unsigned acknowledgements = 0;
            for (auto id : game->objects.snapshot())
                if (auto* ai = dynamic_cast<AI_Mgr*>(game->objects.get(id)->game.get()))
                    acknowledgements += ai->ammo_collections;
            return collected == 1 && acknowledgements >= 1 && game->unload_world();
        }
        if (!game->unload_world())
            return false;
        for (DBYTE difficulty : {1, 0, 2})
        {
            if (!GameSession::start_new_game(game, world, difficulty, 0, error, true))
            {
                std::cerr << world << ": " << error << '\n';
                return false;
            }
            for (double dt : {0., 1e-8, 1e-6, 1e-4, 1. / 144})
                if (!game->step(dt))
                {
                    std::cerr << world << ": initial step dt=" << dt << " failed\n";
                    step_failure();
                    return false;
                }
            game->client_shell.movement_input.forward = true;
            for (unsigned i = 0; i < 180; ++i)
            {
                if (i == 30)
                    game->client_shell.movement_input.forward = false;
                if (!game->step(1. / 60))
                {
                    std::cerr << world << ": simulation step " << i << " failed\n";
                    step_failure();
                    return false;
                }
            }
            // step() can suspend an NPC with a durable diagnostic; that is still a failure.
            if (!game->run_world(error))
            {
                std::cerr << world << ": " << error << '\n';
                for (auto id : game->objects.snapshot())
                    if (auto* r = game->objects.get(id))
                        if (auto* ai = dynamic_cast<AI_Mgr*>(r->game.get()); ai && ai->runtime_failed)
                            std::cerr << "NPC " << r->state.m_Name << " model=" << r->state.m_Filename
                                      << " requested_clip=" << ai->requested_animation
                                      << " clips=" << (r->model ? r->model->animations.size() : 0)
                                      << " state=" << ai->m_nState << " metacmd=" << ai->Metacmd << '\n';
                return false;
            }
            const auto* player = game->server_shell.Player(game->local_client);
            std::cout << world << " start=" << player->entry_start_name
                      << " multiplayer-inspection=" << player->entry_start_multiplayer << ' ';
            std::cout << world << " difficulty=" << unsigned(difficulty) << " passed" << std::endl;
        }
        const bool clean = game->unload_world() && game->objects.snapshot().empty() && !game->server.string_count() &&
                           !game->server.message_count() && !game->server.list_count();
        if (!clean)
            std::cerr << world << ": unload/ownership cleanup failed\n";
        return clean;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    if (std::string_view(argv[1]) != "audit")
        return check(argv[2], argv[1]) ? 0 : 1;
    Config config;
    config.data_dir = argv[2];
    Resources resources;
    std::string error;
    if (!resources.mount_game(config, error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    unsigned passed = 0, failed = 0;
    for (const auto& [path, resource] : resources.index())
    {
        (void)resource;
        if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
            continue;
        const auto world = path.substr(7, path.size() - 11);
        const bool ok = check(argv[2], world.c_str());
        ok ? ++passed : ++failed;
        std::cout << "AUDIT " << world << ' ' << (ok ? "PASS" : "FAIL") << std::endl;
    }
    std::cout << "AUDIT TOTAL passed=" << passed << " failed=" << failed << '\n';
    return failed ? 1 : 0;
}
