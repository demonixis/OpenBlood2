#include "game/game_settings.h"
#include "game/original/GameStartPoint.h"
#include "net/transport.h"
#include "player_fixture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>

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
    bool capacity(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        for (auto mode : {GAMETYPE_SINGLE, GAMETYPE_ACTION, GAMETYPE_CUSTOM, GAMETYPE_COOP, GAMETYPE_SOCCER,
                          GAMETYPE_BLOODBATH, GAMETYPE_TEAMPLAY, GAMETYPE_CTF})
        {
            game.server.net_game_type = mode;
            const auto capacity = network_game_type(mode) ? Net::max_players : std::size_t{1};
            std::vector<HCLIENT> clients;
            for (std::size_t i = 0; i < capacity; ++i)
            {
                const auto client = game.server.connect_client();
                CHECK(client && std::find(clients.begin(), clients.end(), client) == clients.end());
                clients.push_back(client);
            }
            CHECK(!game.server.connect_client() && game.server.client_count() == capacity);
            const auto old = clients.front();
            CHECK(game.server.disconnect_client(old));
            clients.front() = game.server.connect_client();
            CHECK(clients.front() && clients.front() != old && !game.server.disconnect_client(old));
            CHECK(game.server.SetClientUserData(old, nullptr) == DE_INVALIDPARAMS);
            for (auto client : clients)
                CHECK(game.server.disconnect_client(client));
            CHECK(game.server.client_count() == 0);
        }
        return true;
    }

    bool ownership(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        const auto first = game.server.connect_client();
        const auto second = game.server.connect_client();
        CHECK(first && second && first != second);
        auto* player = game.server_shell.OnClientEnterWorld(first);
        CHECK(player);
        const auto object = player->m_hObject;
        const auto id = game.objects.get(object)->id;
        CHECK(game.server.SetClientUserData(second, object) == DE_ERROR);
        // The registry also protects the window before a restored player binds its client field.
        player->SetClient(nullptr);
        CHECK(game.server.SetClientUserData(second, object) == DE_ERROR);
        player->SetClient(first);
        CHECK(!game.server.disconnect_client(first));
        std::vector<PlayerEvent> events;
        CHECK(game.server.take_client_events(first, events));
        CHECK(game.server.SendToClient(first, PlayerRotation{.1f, .2f}) == DE_OK);
        CHECK(game.server.SendToClient(second, PlayerRotation{.3f, .4f}) == DE_INVALIDPARAMS);
        auto* other = game.server_shell.OnClientEnterWorld(second);
        CHECK(other && other != player);
        CHECK(game.server.take_client_events(second, events));
        CHECK(game.server.SendToClient(second, PlayerRotation{.3f, .4f}) == DE_OK);
        CHECK(game.server.take_client_events(first, events) && events.size() == 1);
        CHECK(std::get<PlayerRotation>(events.front()).yaw == .2f);
        CHECK(game.server.take_client_events(second, events) && events.size() == 1);
        CHECK(std::get<PlayerRotation>(events.front()).yaw == .4f);
        // A full private queue does not consume another player's allowance.
        for (unsigned i = 0; i < 256; ++i)
            CHECK(game.server.SendToClient(first, PlayerInitRequest{}) == DE_OK);
        CHECK(game.server.SendToClient(first, PlayerInitRequest{}) == DE_ERROR);
        CHECK(game.server.SendToClient(second, PlayerRotation{.5f, .6f}) == DE_OK);
        game.server_shell.OnClientExitWorld(first);
        CHECK(game.objects.finish_frame() && !game.objects.get(id));
        CHECK(game.server.disconnect_client(first));
        CHECK(game.server_shell.Player(second) == other && other->GetClient() == second);
        CHECK(game.server.take_client_events(second, events) && events.size() == 1);
        const auto replacement = game.server.connect_client();
        CHECK(replacement && replacement != first);
        CHECK(game.server.SendToClient(first, PlayerInitRequest{}) == DE_INVALIDPARAMS);
        CHECK(!game.server.take_client_events(replacement, events));
        CHECK(game.server.SetClientUserData(replacement, object) == DE_INVALIDPARAMS);
        CHECK(game.server.disconnect_client(replacement));
        game.server_shell.OnClientExitWorld(second);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(second));
        CHECK(game.unload_world() && game.server.client_count() == 0);
        CHECK(!game.server.string_count() && !game.server.list_count());
        return true;
    }

    bool bloodbath_overlap(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        std::vector<HCLIENT> clients;
        std::vector<DVector> positions;
        for (unsigned i = 0; i < 5; ++i)
        {
            auto client = game.server.connect_client();
            CHECK(client);
            clients.push_back(client);
            std::minstd_rand expected(game.server.save_clock().generator);
            std::uniform_int_distribution<int>(0, 0)(expected);
            auto* player = game.server_shell.OnClientEnterWorld(client);
            CHECK(player);
            CHECK(std::minstd_rand(game.server.save_clock().generator) == expected);
            const auto* record = game.objects.get(player->m_hObject);
            CHECK(record && record->placement_nudges == (i ? 1 : 0));
            CHECK(std::abs(record->state.m_Pos.y - 41.001f) < .01f);
            const auto delta = vector_subtract(record->state.m_Pos, DVector{32, 100, 64});
            CHECK(std::hypot(delta.x, delta.z) <= 24.01f);
            for (const auto previous : positions)
                CHECK(std::abs(previous.x - record->state.m_Pos.x) >= 24 ||
                      std::abs(previous.z - record->state.m_Pos.z) >= 24);
            positions.push_back(record->state.m_Pos);
        }
        // All four nearby exits are occupied: fail without another marker draw,
        // crossing another body, moving existing players or leaking new children.
        const auto count = game.objects.snapshot().size();
        auto client = game.server.connect_client();
        CHECK(client);
        std::minstd_rand expected(game.server.save_clock().generator);
        std::uniform_int_distribution<int>(0, 0)(expected);
        std::string error;
        CHECK(!game.server_shell.OnClientEnterWorld(client, &error));
        CHECK(error == "Occupied multiplayer start has no clear bounded placement");
        CHECK(std::minstd_rand(game.server.save_clock().generator) == expected);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        CHECK(game.objects.snapshot().size() == count);
        for (unsigned i = 0; i < clients.size(); ++i)
        {
            const auto* record = game.objects.get(game.server_shell.Player(clients[i])->m_hObject);
            CHECK(record && record->state.m_Pos.x == positions[i].x && record->state.m_Pos.y == positions[i].y &&
                  record->state.m_Pos.z == positions[i].z);
        }
        // With another marker, the occupied selection moves on to it in list
        // order: still one random draw, the existing players untouched.
        CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                  {{"Name", std::string("spare")},
                                   {"Multiplayer", true},
                                   {"Pos", DVector{-400, 100, -400}},
                                   {"Rotation", EulerAngles{{0, .5f, 0}}}}));
        client = game.server.connect_client();
        CHECK(client);
        auto* list = game.server.FindNamedObjects("Blood2StartPoint");
        CHECK(list && list->m_nInList == 2);
        const bool spare_first =
            game.server.GetStringData(
                dynamic_cast<GameStartPoint*>(game.server.HandleToObject(list->m_pFirstLink->m_hObject))->GetName()) ==
            std::string("spare");
        game.server.RelinquishList(list);
        bool retried = false;
        for (unsigned attempt = 0; attempt < 32 && !retried; ++attempt)
        {
            expected = std::minstd_rand(game.server.save_clock().generator);
            const auto selected = std::uniform_int_distribution<int>(0, 1)(expected);
            auto* player = game.server_shell.OnClientEnterWorld(client, &error);
            CHECK(player && std::minstd_rand(game.server.save_clock().generator) == expected);
            const auto* record = game.objects.get(player->m_hObject);
            CHECK(record && std::abs(record->state.m_Pos.x + 400) < .01f &&
                  std::abs(record->state.m_Pos.z + 400) < .01f);
            CHECK(player->entry_start_name == "spare");
            retried = player->entry_start_retries == 1;
            CHECK(retried == ((selected == 0) != spare_first));
            game.server_shell.OnClientExitWorld(client);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
            client = game.server.connect_client();
            CHECK(client);
        }
        CHECK(retried && game.server.disconnect_client(client));
        for (unsigned i = 0; i < clients.size(); ++i)
        {
            const auto* record = game.objects.get(game.server_shell.Player(clients[i])->m_hObject);
            CHECK(record && record->state.m_Pos.x == positions[i].x && record->state.m_Pos.z == positions[i].z);
        }
        // The placement service does not expand the solo policy.
        game.server.net_game_type = GAMETYPE_SINGLE;
        CHECK(game.server.ResolveInitialActorOverlap(game.server_shell.Player(clients[0])->m_hObject) ==
              DE_INVALIDPARAMS);
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        for (auto joined : clients)
        {
            game.server_shell.OnClientExitWorld(joined);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(joined));
        }
        CHECK(game.unload_world());
        return true;
    }

    bool bloodbath_starts(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root, false));
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        const auto client = game.server.connect_client();
        CHECK(client);
        const auto before_empty = game.server.save_clock().generator;
        CHECK(!game.server_shell.OnClientEnterWorld(client));
        CHECK(game.objects.finish_frame());
        CHECK(game.server.save_clock().generator == before_empty && !game.server.list_count());
        std::array<DVector, 3> positions{{{-200, 100, -200}, {0, 100, 0}, {200, 100, 200}}};
        std::array<const char*, 3> names{{"solo", "multi-a", "multi-b"}};
        for (unsigned i = 0; i < positions.size(); ++i)
            CHECK(game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                      {{"Name", std::string(names[i])},
                                       {"Multiplayer", i != 0},
                                       {"Pos", positions[i]},
                                       {"Rotation", EulerAngles{{0, float(i) * .5f, 0}}}}));
        auto* player = game.server_shell.OnClientEnterWorld(client);
        CHECK(player);
        // Source selects by the actual named-list order, including the non-MP marker.
        auto* list = game.server.FindNamedObjects("Blood2StartPoint");
        CHECK(list && list->m_nInList == 3);
        std::vector<GameStartPoint*> starts;
        for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
            starts.push_back(dynamic_cast<GameStartPoint*>(game.server.HandleToObject(link->m_hObject)));
        game.server.RelinquishList(list);
        std::array<bool, 3> seen{};
        for (unsigned i = 0; i < 64; ++i)
        {
            std::minstd_rand expected(game.server.save_clock().generator);
            const auto selected = std::uniform_int_distribution<int>(0, 2)(expected);
            auto after_draw = expected;
            const auto next = after_draw();
            CHECK(player->GoToStartPoint(nullptr));
            CHECK(player->entry_start_name == game.server.GetStringData(starts[selected]->GetName()));
            CHECK(player->entry_start_multiplayer == bool(starts[selected]->IsMultiplayer()));
            CHECK(!player->entry_start_fallback);
            DVector marker{}, actual{};
            CHECK(game.server.GetObjectPos(starts[selected]->m_hObject, &marker) == DE_OK);
            CHECK(game.server.GetObjectPos(player->m_hObject, &actual) == DE_OK);
            CHECK(actual.x == marker.x && actual.z == marker.z && std::abs(actual.y - 41.001f) < .01f);
            std::minstd_rand actual_rng(game.server.save_clock().generator);
            CHECK(actual_rng() == next); // Exactly the original one random selection, no retries.
            std::vector<PlayerEvent> events;
            CHECK(game.server.take_client_events(client, events));
            CHECK(!game.server.list_count());
            seen[selected] = true;
        }
        CHECK(seen[0] && seen[1] && seen[2]);
        game.server_shell.OnClientExitWorld(client);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        CHECK(game.unload_world() && !game.server.string_count() && !game.server.list_count());
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    return (name == "capacity"            ? capacity(argv[2])
            : name == "ownership"         ? ownership(argv[2])
            : name == "bloodbath_overlap" ? bloodbath_overlap(argv[2])
                                          : name == "bloodbath_starts" && bloodbath_starts(argv[2]))
               ? 0
               : 1;
}
