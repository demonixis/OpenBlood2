#include "engine/world_model.h"
#include "game/game_settings.h"
#include "game/original/SoccerObjects.h"
#include "game/original/Spawner.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
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
    bool match(const char* root, const char* map)
    {
        Config config;
        config.data_dir = root;
        config.nightmares = true;
        GameSession game;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        const bool loaded = game.request_world(map, error);
        if (!loaded)
            std::cerr << error << '\n';
        CHECK(loaded);

        SoccerBall* ball = nullptr;
        SoccerGoal* goals[2]{};
        Spawner* spawn = nullptr;
        for (const auto& entry : game.entries)
        {
            auto* r = game.objects.get(entry.object);
            if (!r || r->life != Life::Alive)
                continue;
            if (auto* actor = dynamic_cast<SoccerBall*>(r->game.get()))
                ball = actor;
            if (auto* goal = dynamic_cast<SoccerGoal*>(r->game.get()))
                goals[goal->team - 1] = goal;
            if (auto* spawner = dynamic_cast<Spawner*>(r->game.get()); spawner && same_name(r->state.m_Name, "spawn"))
                spawn = spawner;
        }
        CHECK(ball && goals[0] && goals[1] && spawn);
        const auto original = ball->m_hObject;
        auto* r = game.objects.get(original);
        CHECK(r->model && r->model_body && r->state.m_Flags & FLAG_GRAVITY &&
              same_name(r->state.m_Filename, "models_ao/worldobjects_ao/headball.abc"));

        // The world inspector uses local custom rules. Connect one checked team
        // member to exercise the object's server scoring without a lobby.
        game.server.net_game_type = GAMETYPE_SOCCER;
        auto client = game.server.connect_client();
        CHECK(client && game.server.AssignClientTeam(client, 1));
        ObjectCreateStruct create;
        create.m_Pos = r->state.m_Pos;
        create.m_Pos.y += 100;
        create.m_Flags = FLAG_SOLID;
        const auto player_handle = game.objects.create(game.objects.find_class("CPlayerObj"), create);
        auto* player = dynamic_cast<CPlayerObj*>(game.server.HandleToObject(player_handle));
        CHECK(player && game.server.SetClientUserData(client, player_handle) == DE_OK);
        player->SetClient(client);
        game.server.SetNextUpdate(player_handle, 0);
        const auto step = [&]
        {
            if (game.step(1. / 60))
                return true;
            for (const auto& diagnostic : game.step_errors())
                std::cerr << diagnostic << '\n';
            return false;
        };

        // An untouched ball crossing the line never scores.
        CHECK(game.objects.engine_message(goals[1]->m_hObject, MID_TOUCHNOTIFY, original));
        CHECK(!goals[1]->scores && game.server.IsObjectAlive(original));
        DVector kick{150, 0, 0};
        CHECK(game.server.SetVelocity(player_handle, &kick) == DE_OK);
        CHECK(game.objects.engine_message(original, MID_TOUCHNOTIFY, player_handle));
        CHECK(ball->last_player == player_handle && game.server.GetVelocity(original, &kick) == DE_OK &&
              kick.y > 100 && ball->expires >= game.objects.time() + 59);
        const auto rotation = r->state.m_Rotation;
        CHECK(step());
        CHECK(r->state.m_Rotation.m_Spin != rotation.m_Spin);

        // Cross the authored goal volume through the same swept collision
        // path as a moving ball. Start just outside its narrow horizontal face.
        const auto* target = game.objects.get(goals[1]->m_hObject);
        const auto end = object_bounds_center(*target);
        auto from = end;
        const auto horizontal = target->dimensions.x < target->dimensions.z ? 0 : 2;
        if (horizontal == 0)
            from.x += target->dimensions.x + r->dimensions.x + 8;
        else
            from.z += target->dimensions.z + r->dimensions.z + 8;
        auto velocity = vector_subtract(end, from);
        CHECK(game.server.TeleportObject(original, &from) == DE_OK &&
              game.server.SetVelocity(original, &velocity) == DE_OK);
        auto destination = end;
        if (game.server.MoveObject(original, &destination) != DE_OK)
            for (const auto& diagnostic : game.objects.diagnostics())
                std::cerr << diagnostic << '\n';
        if (game.server.IsObjectAlive(original) || !goals[1]->score_error.empty())
            std::cerr << "Goal " << goals[1]->scores << ", ball alive " << game.server.IsObjectAlive(original)
                      << ", score error: " << goals[1]->score_error
                      << ", ball position " << property_text(r->state.m_Pos)
                      << ", ball half " << property_text(r->dimensions)
                      << ", goal center " << property_text(end) << '\n';
        CHECK(!game.server.IsObjectAlive(original) && goals[1]->scores == 1 && goals[1]->score_error.empty());
        CHECK(player->m_Frags == 5 && game.server.team_frags[0] == 5 && spawn->spawned == 1);
        const auto replacement = spawn->last_child;
        CHECK(replacement != original && game.server.IsObjectAlive(replacement));
        CHECK(same_name(game.objects.get(replacement)->state.m_Filename, "models_ao/worldobjects_ao/soccerball.abc"));

        // Let the replacement expire without a player touching it.
        DVector stopped{};
        CHECK(game.server.SetVelocity(player_handle, &stopped) == DE_OK);
        const auto player_flags = game.server.GetObjectFlags(player_handle);
        game.server.SetObjectFlags(player_handle, player_flags & ~FLAG_SOLID);
        for (unsigned frame = 0; frame < 3650 && game.server.IsObjectAlive(replacement); ++frame)
            CHECK(step());
        CHECK(!game.server.IsObjectAlive(replacement) && spawn->spawned == 1);
        CHECK(step() && spawn->spawned == 1);
        CHECK(step() && spawn->spawned == 2);
        game.server.SetObjectFlags(player_handle, player_flags);
        auto* respawned = dynamic_cast<SoccerBall*>(game.server.HandleToObject(spawn->last_child));
        CHECK(respawned && !respawned->last_player);
        CHECK(game.objects.engine_message(respawned->m_hObject, MID_TOUCHNOTIFY, player_handle));
        CHECK(game.objects.engine_message(goals[0]->m_hObject, MID_TOUCHNOTIFY, respawned->m_hObject));
        CHECK(goals[0]->scores == 1 && goals[0]->score_error.empty() && player->m_Frags == 0 &&
              game.server.team_frags[0] == 0 && spawn->spawned == 3);
        CHECK(goals[0]->effect_error.empty() && goals[1]->effect_error.empty());
        for (unsigned frame = 0; frame < 30; ++frame)
            CHECK(step());
        return game.unload_world() && game.server.disconnect_client(client);
    }
} // namespace

int main(int argc, char** argv)
{
    return argc == 3 && match(argv[2], argv[1]) ? 0 : 1;
}
