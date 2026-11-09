#include "brush_fixture.h"
#include "game/original/CameraObj.h"
#include "game/original/CraneObjects.h"
#include "game/original/DestructableBrush.h"
#include "game/original/LegacyLines.h"
#include "game/server_adapter.h"
#include <iostream>
#include <numbers>
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
    bool setup(GameSession& game, const char* path)
    {
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        WorldModel floor;
        test_brush::face(floor, {{-2048, 0, -2048}, {-2048, 0, 2048}, {2048, 0, 2048}, {2048, 0, -2048}}, {0, 1, 0});
        CHECK(game.collision.build(floor, error));
        game.world.models.push_back(std::move(floor));
        game.world.min = {-2048, 0, -2048};
        game.world.max = {2048, 2048, 2048};
        game.server.collision_ready = true;
        return true;
    }

    HOBJECT brush(GameSession& g, const char* type, const char* name, DVector min, DVector max, DVector origin,
                  Properties props = {})
    {
        auto shape = test_brush::box(name, min, max);
        shape.translation = origin;
        g.world.models.push_back(std::move(shape));
        props.emplace("Name", std::string(name));
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }

    bool close(DVector a, DVector b)
    {
        auto delta = vector_subtract(a, b);
        return vector_dot(delta, delta) < .01f;
    }

    bool crane(const char* path)
    {
        GameSession game;
        std::string error;
        CHECK(setup(game, path));
        const auto crane_handle = brush(game, "CDestructableBrush", "crane", {-20, 400, -20}, {20, 440, 20},
                                       {0, 400, 0}, {{"Destructable", false}});
        const auto control = brush(game, "CraneControl", "controls", {450, 0, -50}, {550, 128, 50}, {500, 64, 0});
        auto* controller = dynamic_cast<CraneControl*>(game.server.HandleToObject(control));
        CHECK(crane_handle && controller && controller->locked && controller->info.code == CC_CRANECONTROL);
        const auto ball_handle = game.objects.create(game.objects.find_class("WreckingBall"));
        auto* ball = dynamic_cast<WreckingBall*>(game.server.HandleToObject(ball_handle));
        CHECK(ball && game.objects.get(ball_handle)->model && game.objects.advance(.02));
        CHECK(ball->crane == crane_handle && ball->chain && close(ball->anchor, {0, 730, 180}));
        CHECK(close(game.objects.get(ball_handle)->state.m_Pos, {0, 150, 180}));

        ObjectCreateStruct state;
        state.m_Pos = {500, 42, 0};
        state.m_Flags = FLAG_SOLID;
        const auto player_handle = game.objects.create(game.objects.find_class("CPlayerObj"), state);
        auto* player = dynamic_cast<CPlayerObj*>(game.server.HandleToObject(player_handle));
        CHECK(player && game.server.IsObjectAlive(player_handle));
        game.server.SetNextUpdate(player_handle, 0);
        player->crane_turn = 1;
        CHECK(game.server.damage_containers() && controller->yaw == .03f && controller->control_error.empty());
        CHECK(game.objects.get(crane_handle)->mover_moves == 1);
        auto* camera = dynamic_cast<CameraObj*>(game.server.HandleToObject(controller->camera));
        CHECK(camera && camera->GetLinkObject() == ball_handle && !camera->IsActive());
        CHECK(game.objects.advance(.02) && ball->anchor.x < -5.f);
        CHECK(game.publish_client_world(error));
        const auto& lines = game.client_session.effects.frame().lines;
        CHECK(lines.size() == 1 && lines[0].id.kind == EffectKind::CastLine &&
              close(lines[0].geometry.from, ball->anchor) &&
              close(lines[0].geometry.to, game.objects.get(ball_handle)->state.m_Pos));

        DVector outside{700, 42, 0};
        CHECK(game.server.TeleportObject(player_handle, &outside) == DE_OK);
        CHECK(game.server.damage_containers() && controller->yaw == .03f);
        const auto target = brush(game, "CDestructableBrush", "wall", {180, 100, 160}, {200, 220, 200},
                                 {190, 160, 180}, {{"HitPoints", 100.f}});
        auto* wall = dynamic_cast<CDestructableBrush*>(game.server.HandleToObject(target));
        CHECK(wall && game.objects.engine_message(ball_handle, MID_TOUCHNOTIFY, target) && wall->m_damage.IsDead());
        const auto chain = ball->chain;
        CHECK(game.objects.remove(crane_handle) && game.objects.finish_frame() && game.objects.advance(.02));
        CHECK(!game.server.IsObjectAlive(ball_handle) && !game.server.IsObjectAlive(chain));
        CHECK(game.publish_client_world(error) && game.client_session.effects.frame().lines.empty());
        return game.unload_world();
    }

    bool debug_line(const char* path)
    {
        GameSession game;
        std::string error;
        CHECK(setup(game, path));
        ObjectCreateStruct create;
        create.m_Pos = {20, 100, 30};
        const auto handle = game.objects.create(game.objects.find_class("CClientDebugLine"), create);
        auto* line = dynamic_cast<CClientDebugLine*>(game.server.HandleToObject(handle));
        CHECK(line && line->Setup({10, 80, 20}, {15, 90, 23}));
        CHECK(game.objects.advance(.01) && !game.server.IsObjectAlive(handle) && game.publish_client_world(error));
        const auto& lines = game.client_session.effects.frame().lines;
        CHECK(lines.size() == 1 && lines[0].id.kind == EffectKind::DetachedLine);
        CHECK(close(lines[0].geometry.from, {30, 120, 40}) && close(lines[0].geometry.to, {25, 110, 37}));
        CHECK(close(lines[0].geometry.start_color, {1, 0, 0}) && lines[0].geometry.end_alpha == 1);
        CHECK(game.objects.advance(20) && game.publish_client_world(error) &&
              game.client_session.effects.frame().lines.empty());
        return game.unload_world();
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "crane" ? crane(argv[2]) : mode == "debug" && debug_line(argv[2])) ? 0 : 1;
}
