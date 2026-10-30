#include "../brush_fixture.h"
#include "../explosion_sprite_fixture.h"
#include "client/effect_scene.h"
#include "game/original/VolumeBrush.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool local(const std::filesystem::path& root)
    {
        std::string error;
        ClientEffects retained;
        Resources resources;
        SpriteView before;
        std::uint64_t world = 0;
        {
            GameSession g;
            CHECK(g.initialize());
            CHECK(test_explosion_sprite::mount(g.resources, root, error));
            CHECK(test_explosion_sprite::mount(resources, root / "client", error));
            auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
            CHECK(owner && g.server.PublishExplosionSprites(owner, 2, {0, 0, 50}, {0, 1, 0}, error) && error.empty());
            SparkSettings sparks;
            sparks.position = {0, 0, 40};
            sparks.direction = {0, 20, 0};
            sparks.color1 = sparks.color2 = {1, .5f, 0};
            sparks.count = 4;
            sparks.duration = 1;
            sparks.radius = 100;
            sparks.texture = "alpha.dtx";
            CHECK(g.server.PublishSparks(owner, sparks, error) && error.empty());
            CHECK(g.server.explosion_sprites.advance(.1) && g.server.sparks.advance(.1));
            g.world.models.push_back(test_brush::box("water", {-10, -10, -10}, {10, 10, 10}));
            auto water = g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                                          {{"Name", std::string("water")},
                                           {"ShowSurface", false},
                                           {"FogEnable", true},
                                           {"FogNearZ", -20.f},
                                           {"FogFarZ", 100.f},
                                           {"FogColor", DVector{64, 128, 255}}});
            CHECK(water);
            g.client_shell.flashlight.source = water;
            g.client_shell.flashlight.owner = owner;
            g.client_shell.flashlight.visible = true;
            g.client_shell.flashlight.light = {{1, 2, 3}, {.8f, .8f, .8f}, 70};
            CHECK(g.publish_client_world(error));
            CHECK(g.client_session.effects.frame().flashlight.radius == 70);
            auto frame = g.client_session.effects.frame();
            world = frame.world;
            CHECK(frame.billboards.size() >= 3 && frame.volumes.size() == 1 && g.local_effects.shape_copies == 1);
            CHECK(frame.volumes[0].shape != g.objects.get(water)->container());
            CHECK(retained.begin(world) && retained.receive(std::move(frame), error));
            const auto time = g.objects.time();
            const auto calls = g.server.service_calls;
            CHECK(g.local_effects.publish(g, g.client_session.effects, error) && g.local_effects.shape_copies == 1);
            CHECK(g.objects.time() == time && g.server.service_calls == calls);
            CHECK(before.begin_frame({}, {}, error) &&
                  append_effect_sprites(retained, before, {}, true, resources, error));
            CHECK(!before.mesh.vertices.empty() && g.server.sparks.active[0].particles.size() == 4);
            const auto& copied = std::get<EffectParticles>(retained.frame().billboards.back().value);
            CHECK(copied.particles.data() != g.server.sparks.active[0].particles.data());
            const auto position = copied.particles[0].position;
            CHECK(g.server.sparks.advance(.5));
            CHECK(copied.particles[0].position.x == position.x && copied.particles[0].position.y == position.y);
            CHECK(g.objects.remove(owner) && g.publish_client_world(error));
            CHECK(g.client_session.effects.frame().flashlight.radius == 0 && retained.frame().flashlight.radius == 70);
            CHECK(g.unload_world() && g.client_session.effects.frame().billboards.empty());
            CHECK(g.client_session.effects.frame().world != world);
        }
        SpriteView after;
        CHECK(after.begin_frame({}, {}, error) && append_effect_sprites(retained, after, {}, true, resources, error));
        CHECK(after.mesh.vertices.size() == before.mesh.vertices.size() &&
              after.entries.size() == before.entries.size());
        for (std::size_t i = 0; i < after.mesh.vertices.size(); ++i)
        {
            const auto a = after.mesh.vertices[i].position, b = before.mesh.vertices[i].position;
            CHECK(a.x == b.x && a.y == b.y && a.z == b.z);
        }
        CHECK(gather_scene_effects(retained, {}, {}).uniform.fog_color[0] == 64.f / 255);
        CHECK(retained.begin(world + 1) && retained.frame().billboards.empty());
        return true;
    }
    bool private_worlds(const std::filesystem::path& root)
    {
        std::string error;
        for (auto name : {"01_SubwayA", "02_CityHub"})
        {
            GameSession g;
            Config c;
            c.data_dir = root;
            WorldMessage message;
            CHECK(g.initialize() && g.resources.mount_game(c, error) && g.client_shell.StartNewWorld(name, message) &&
                  g.load_world(message, error) && g.enter_player(error));
            for (unsigned frame = 0; frame < 180; ++frame)
            {
                CHECK(g.step(1. / 60) && g.publish_client_world(error));
                const auto& fx = g.client_session.effects.frame();
                CHECK(fx.world == g.client_session.world.frame().world && fx.time == g.server.world_time());
                CHECK(g.effects_replica_error.empty() && g.audio_replica_error.empty());
            }
            CHECK(g.unload_world() && g.client_session.effects.frame().volumes.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 3 && (std::string_view(argv[1]) == "local" ? local(argv[2]) : private_worlds(argv[2])) ? 0 : 1;
}
