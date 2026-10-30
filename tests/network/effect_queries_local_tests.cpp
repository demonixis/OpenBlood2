#include "../brush_fixture.h"
#include "../model_fixture.h"
#include "client/effect_queries.h"
#include "game/server_adapter.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    std::string error;
    GameSession game;
    CHECK(game.initialize());
    auto floor = test_brush::box("floor", {-500, -10, -500}, {500, 0, 500});
    for (auto& surface : floor.surfaces)
        surface.flags = 1;
    CHECK(game.collision.build(floor, error));
    game.server.collision_ready = true;
    ObjectCreateStruct state;
    state.m_Pos = {0, 50, 10};
    state.m_Flags = FLAG_SOLID | FLAG_RAYHIT;
    auto obstacle = game.objects.create(game.objects.find_class("BaseClass"), state);
    CHECK(obstacle);
    game.objects.get(obstacle)->dimensions = {5, 5, 1};
    CHECK(game.local_movement.publish_collision());
    ClientEffectQueries client(&game.client_session.collision);
    auto model = std::make_shared<Model>();
    CHECK(read_model(test_model::abc().b, *model, error));
    for (const auto position : {DVector{0, 5, 0}, DVector{0, 50, 0}})
    {
        ExplosionFragSettings settings;
        settings.position = position;
        settings.gravity = 1;
        settings.duration = 2;
        settings.fade_time = .5;
        CExplosionFragFX authoritative;
        CHECK(authoritative.Init(settings, error));
        authoritative.model = model;
        authoritative.velocity = position.y == 5 ? DVector{0, -4, 0} : DVector{0, 0, 4};
        auto replica = authoritative;
        for (unsigned tick = 0; tick < 120; ++tick)
        {
            const double now = double(tick) / 60;
            const auto live = authoritative.Update(now, game.server);
            const auto calls = game.server.service_calls;
            CHECK(replica.Update(now, client) == live && game.server.service_calls == calls);
            CHECK(replica.position.x == authoritative.position.x && replica.position.y == authoritative.position.y &&
                  replica.position.z == authoritative.position.z && replica.velocity.x == authoritative.velocity.x &&
                  replica.velocity.y == authoritative.velocity.y && replica.velocity.z == authoritative.velocity.z &&
                  replica.impacts == authoritative.impacts && replica.last_query == authoritative.last_query &&
                  replica.last_hit.object == authoritative.last_hit.object && replica.alpha == authoritative.alpha);
        }
    }
    const auto previous = game.objects.get(obstacle)->id;
    CHECK(game.objects.remove(obstacle));
    CHECK(game.local_movement.publish_collision());
    ClientSegmentHit hit;
    CHECK(client.effect_segment({0, 50, 0}, {0, 50, 20}, hit) == SegmentResult::Miss && !hit.object);
    CHECK(!game.client_session.collision.get(previous));
    return 0;
}
