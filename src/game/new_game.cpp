#include "game/original_globals.h"
#include "game/server_adapter.h"
#include <limits>
namespace OpenBlood2
{
    bool GameSession::start_new_game(std::unique_ptr<GameSession>& current, std::string_view world, DBYTE difficulty,
                                     DBYTE character, std::string& error)
    {
        error.clear();
        if (!current || current->objects.busy() || ServerAdapter::active_ != &current->server ||
            g_pServerDE != &current->server || !current->server.bound_ || character != 0 || difficulty > 2 ||
            world.empty() || world.size() > 255 || world.find('\0') != world.npos ||
            current->world_revision == std::numeric_limits<std::uint64_t>::max())
        {
            error = "New game requires an active idle owner and supported solo profile";
            return false;
        }
        WorldMessage request;
        const std::string path(world);
        if (!original_world_request(path.c_str(), 0, LOADTYPE_NEW_GAME, difficulty, request))
        {
            error = "Invalid new-game request";
            return false;
        }
        auto& source = *current;
        OriginalGlobals source_globals;
        source_globals.exchange();
        source.server.bound_ = false;
        ServerAdapter::active_ = nullptr;
        g_pServerDE = nullptr;
        auto destination = std::make_unique<GameSession>();
        destination->resources = source.resources;
        const bool ready =
            destination->initialize() && destination->load_world(request, error) && destination->run_world(error);
        auto restore_source = [&]
        {
            source_globals.exchange();
            ServerAdapter::active_ = &source.server;
            g_pServerDE = &source.server;
            source.server.bound_ = true;
        };
        if (!ready)
        {
            destination.reset();
            const bool clean = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
            restore_source();
            if (error.empty())
                error = "New-game preparation failed";
            if (!clean)
                error += "; destination cleanup invariant failed";
            return false;
        }
        OriginalGlobals destination_globals;
        destination_globals.exchange();
        destination->server.bound_ = false;
        restore_source();
        if (!source.server.audio.transfer_output_to(destination->server.audio, error))
        {
            source_globals.exchange();
            source.server.bound_ = false;
            destination_globals.exchange();
            ServerAdapter::active_ = &destination->server;
            g_pServerDE = &destination->server;
            destination->server.bound_ = true;
            destination.reset();
            restore_source();
            return false;
        }
        destination->world_revision = source.world_revision + 1;
        current.reset();
        const bool clean = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
        destination_globals.exchange();
        ServerAdapter::active_ = &destination->server;
        g_pServerDE = &destination->server;
        destination->server.bound_ = true;
        current = std::move(destination);
        if (!clean)
        {
            error = "New-game source cleanup invariant failed";
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
