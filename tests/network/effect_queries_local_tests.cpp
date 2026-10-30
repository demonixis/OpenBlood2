#include "../brush_fixture.h"
#include "../model_fixture.h"
#include "client/effect_queries.h"
#include "client/effect_services.h"
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
    // Compare the unchanged primary-only trajectories through both service backends.
    // Real resource/output creation is covered by the independently linked services test.
    OriginalEffects old_effects, new_effects;
    EffectServices old_services{game.server, old_effects, nullptr, nullptr, nullptr, 0, .01};
    EffectServices new_services{client, new_effects, nullptr, nullptr, nullptr, 0, .01};
    FleshGib gib;
    gib.model = model;
    gib.position = {0, 2, 0};
    gib.velocity = {30, -100, 0};
    gib.sound_path = "expected-unavailable.wav";
    old_effects.flesh_gibs.active.push_back(gib);
    new_effects.flesh_gibs.active.push_back(gib);
    ShotgunCasing casing;
    casing.model = model;
    casing.position = {0, 3, 0};
    casing.velocity = {20, -100, 0};
    casing.expires = 20;
    old_effects.shotgun_effects.casings.push_back(casing);
    new_effects.shotgun_effects.casings.push_back(casing);
    CParticleExplosionFX old_blood, new_blood;
    old_blood.position = old_blood.previous = {0, 1, 0};
    old_blood.velocity = {10, -100, 0};
    new_blood = old_blood;
    std::minstd_rand old_random{1}, new_random{1};
    const auto equal = [](DVector a, DVector b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    for (unsigned tick = 1; tick <= 120; ++tick)
    {
        old_services.time = new_services.time = tick * .01;
        CHECK(old_effects.flesh_gibs.advance(old_services.time, old_services));
        old_effects.shotgun_effects.advance(old_services.time, old_services);
        CHECK(old_blood.Update(old_services.time, .01, old_services, old_random, 16384));
        const auto calls = game.server.service_calls;
        CHECK(new_effects.flesh_gibs.advance(new_services.time, new_services));
        new_effects.shotgun_effects.advance(new_services.time, new_services);
        CHECK(new_blood.Update(new_services.time, .01, new_services, new_random, 16384));
        CHECK(game.server.service_calls == calls);
        const auto& a = old_effects.flesh_gibs.active.front();
        const auto& b = new_effects.flesh_gibs.active.front();
        CHECK(equal(a.position, b.position) && equal(a.velocity, b.velocity) && equal(a.angles, b.angles) &&
              a.impacts == b.impacts && a.resting == b.resting && a.splat_error == b.splat_error);
        const auto& c = old_effects.shotgun_effects.casings.front();
        const auto& d = new_effects.shotgun_effects.casings.front();
        CHECK(equal(c.position, d.position) && equal(c.velocity, d.velocity) && c.impacts == d.impacts &&
              c.resting == d.resting && c.pitch == d.pitch && c.yaw == d.yaw && c.sound_error == d.sound_error);
        CHECK(equal(old_blood.position, new_blood.position) && equal(old_blood.velocity, new_blood.velocity) &&
              old_blood.impacts == new_blood.impacts && old_blood.emitted == new_blood.emitted &&
              old_blood.sound_resource == new_blood.sound_resource && old_blood.alpha == new_blood.alpha &&
              old_random == new_random);
    }
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
