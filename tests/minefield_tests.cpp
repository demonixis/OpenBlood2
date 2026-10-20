#include "brush_fixture.h"
#include "game/original/Minefield.h"
#include "player_fixture.h"
#include <cmath>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* fixture, const char* data)
    {
        GameSession g;
        CHECK(test_player::setup(g, fixture));
        std::string error;
        CHECK(g.enter_player(error));
        if (data)
        {
            Config config;
            config.data_dir = data;
            CHECK(g.resources.mount_game(config, error));
        }
        g.world.models.push_back(test_brush::box("mines", {-500, 0, -500}, {500, 500, 500}));
        auto h =
            g.objects.create(g.objects.find_class("Minefield"), {}, Creation::World, {{"Name", std::string("mines")}});
        CHECK(h);
        auto* mine = dynamic_cast<Minefield*>(g.objects.get(h)->game.get());
        CHECK(mine && mine->info.code == CC_DAMAGE && mine->damage == 0 && !mine->show_surface);
        CHECK(mine->sprite.empty() && mine->surface.scale_min[0] == 15 && mine->surface.scale_max[1] == 25);
        auto character = [&](DVector pos)
        {
            ObjectCreateStruct c;
            c.m_Pos = pos;
            auto object = g.objects.create(g.objects.find_class("CBaseCharacter"), c);
            auto* body = dynamic_cast<CBaseCharacter*>(g.objects.get(object)->game.get());
            body->m_damage.Init(object);
            body->m_damage.SetHitPoints(100);
            body->m_damage.SetArmorPoints(100);
            DVector dims{1, 1, 1};
            g.server.SetObjectDims(object, &dims);
            return body;
        };
        auto* air = character({100, 100, 0});
        auto* ground = character({200, 1.001f, 0});
        ContainerPhysics cp;
        cp.object = air->m_hObject;
        cp.velocity = {1, 2, 3};
        cp.acceleration = {4, 5, 6};
        cp.gravity = true;
        cp.seconds = .01f;
        mine->info.viscosity = .7f;
        mine->info.current = {100, 100, 100};
        mine->damage = 100;
        mine->UpdatePhysics(cp);
        CHECK(!air->m_damage.IsDead() && !mine->explosions && !mine->physics_calls);
        CHECK(cp.velocity.x == 1 && cp.acceleration.y == 5 && cp.gravity); // No base volume physics.
        mine->info.hidden = true;                                          // Original override does not use Hidden.
        cp.object = ground->m_hObject;
        mine->UpdatePhysics(cp);
        CHECK(ground->m_damage.IsDead() && ground->m_damage.m_fArmorPoints == 0);
        CHECK(ground->m_damage.last_damage.attacker == ground->m_hObject &&
              ground->m_damage.last_damage.type == DamageType::Death && ground->m_damage.last_damage.amount == 500);
        CHECK(!air->m_damage.IsDead() && mine->explosions == 1 && mine->damage_events == 1); // No radial damage.
        mine->UpdatePhysics(cp);
        CHECK(mine->explosions == 1); // Already dead does not explode again.
        auto* player = g.server_shell.Player(g.local_client);
        DVector pos{0, 200, 0};
        CHECK(g.server.TeleportObject(player->m_hObject, &pos) == DE_OK);
        CHECK(g.server.GetWorldTime() < 2);
        cp.object = player->m_hObject;
        mine->UpdatePhysics(cp);
        CHECK(player->m_damage.IsDead() && player->protected_volume_hits == 0 && mine->explosions == 2);
        CHECK(player->m_damage.last_damage.attacker == player->m_hObject);
        auto message = g.server.StartMessageToObject(air, h, 3510);
        CHECK(message && g.server.EndMessage(message) == DE_OK);
        CHECK(air->m_damage.IsDead() && mine->explosions == 3); // Explicit MID_INCONTAINER bypasses support.
        cp.object = g.objects.create(g.objects.find_class("BaseClass"), {});
        mine->UpdatePhysics(cp);
        CHECK(mine->explosions == 3 && mine->error.empty());
        CHECK(g.server.scene_explosions.size() == 3);
        for (const auto& report : g.server.scene_explosions)
        {
            CHECK(report.type == 3 && report.targets.delivered == 0);
            if (data)
            {
                if (!report.sound_error.empty())
                    std::cerr << report.sound_error << '\n';
                CHECK(report.voice && report.sound_error.empty());
                CHECK(report.model_event && report.sprite_event && report.ring_event && report.light_event);
                CHECK(report.model_error.empty() && report.sprite_error.empty() && report.ring_error.empty() &&
                      report.light_error.empty());
            }
            else
                CHECK(!report.voice && !report.sound_error.empty()); // No fabricated synthetic sound.
        }
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.message_count() &&
              !g.server.string_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc >= 2 && run(argv[1], argc > 2 ? argv[2] : nullptr) ? 0 : 1;
}
