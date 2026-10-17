#include "game/original/CultistAI.h"
#include "player_fixture.h"
#include <cmath>
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .003f;
    }
    bool same(DVector a, DVector b)
    {
        return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
    }
    bool damage(GameSession& g, HOBJECT attacker, float amount, DVector direction, DamageType type = DamageType::Normal)
    {
        auto h = g.client_shell.player;
        DVector pos;
        CHECK(g.server.GetObjectPos(h, &pos) == DE_OK);
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(attacker), h, damage_message);
        CHECK(m && g.server.WriteDamage(m, {direction, pos, amount, type, attacker}) == DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        for (int i = 0; i < 120; ++i)
            CHECK(g.step(1. / 120));
        return true;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(other && p->m_damage.apply_damage_physics && p->m_damage.m_fMass == 250);
        DVector velocity{3, 4, 5};
        CHECK(g.server.SetVelocity(p->m_hObject, &velocity) == DE_OK);
        p->m_damage.m_fResistance = .5f;
        p->m_damage.SetArmorPoints(100);
        const float hp = p->m_damage.m_fHitPoints;
        CHECK(damage(g, other, 10, {0, 0, 2}));
        CHECK(p->m_damage.damage_impulses == 1 && same(r->velocity, {3, 4, 55}));
        CHECK(same(p->m_damage.last_impulse, {0, 0, 50}) && near(p->m_damage.m_fHitPoints, hp - 1.5f));
        // Additional hits accumulate once in the shared body, before armor absorption.
        CHECK(damage(g, other, 10, {2, 0, 0}) && same(r->velocity, {53, 4, 55}));
        auto count = p->m_damage.damage_impulses;
        velocity = r->velocity;
        auto health = p->m_damage.m_fHitPoints;
        CHECK(damage(g, p->m_hObject, 10, {0, 1, 0}, DamageType::Explode));
        CHECK(same(r->velocity, velocity) && p->m_damage.damage_impulses == count && p->m_damage.m_fHitPoints < health);
        CHECK(damage(g, other, 1, {.099f, 0, 0}) && p->m_damage.damage_impulses == count);
        // Exactly .1 is admitted by the original squared direction threshold.
        CHECK(damage(g, other, 1, {.1f, 0, 0}) && p->m_damage.damage_impulses == ++count);
        velocity = r->velocity;
        p->m_damage.m_bGodMode = true;
        CHECK(damage(g, other, 5, {1, 0, 0}) && same(r->velocity, velocity));
        p->m_damage.m_bGodMode = false;
        p->m_damage.m_bNighInvulnerable = true;
        CHECK(damage(g, other, 10, {0, 1, 0}) && near(r->velocity.y, velocity.y + 2.5f));
        CHECK(p->m_damage.damage_impulses == ++count);
        p->m_damage.m_bNighInvulnerable = false;
        p->m_damage.apply_damage_physics = false;
        velocity = r->velocity;
        CHECK(damage(g, other, 1, {1, 0, 0}) && same(r->velocity, velocity));
        p->m_damage.apply_damage_physics = true;
        // A refused out-of-bounds impulse cannot corrupt motion or suppress health damage.
        p->m_damage.m_fMass = .000001f;
        health = p->m_damage.m_fHitPoints;
        CHECK(damage(g, other, 1, {1, 0, 0}));
        CHECK(!p->m_damage.last_impulse_ok && same(r->velocity, velocity) && p->m_damage.damage_impulses == count &&
              p->m_damage.m_fHitPoints < health);
        p->m_damage.m_fMass = 250;
        CHECK(damage(g, other, 1, {0, 1, 0}, DamageType::Death));
        CHECK(p->m_damage.IsDead() && same(r->velocity, velocity) && p->m_damage.damage_impulses == count);
        CHECK(g.unload_world() && g.objects.record_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        DVector reference{};
        for (int hz : {30, 60, 120})
        {
            GameSession g;
            CHECK(setup(g, root / std::to_string(hz)));
            auto* p = g.server_shell.Player(g.local_client);
            auto* r = g.objects.get(p->m_hObject);
            auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
            CHECK(other);
            const auto start = r->state.m_Pos;
            CHECK(damage(g, other, 10, {1, 0, 0}) && damage(g, other, 5, {0, 0, 1}));
            CHECK(same(r->velocity, {100, 0, 50}));
            CHECK(g.server.advance_physics(.1) && same(r->state.m_Pos, start) && same(r->velocity, {100, 0, 50}));
            const auto moves = p->movement_messages;
            for (int i = 0; i < hz / 2; ++i)
                CHECK(g.step(1. / hz));
            auto displacement = vector_subtract(r->state.m_Pos, start);
            CHECK(p->m_damage.damage_impulses == 2 && p->movement_messages - moves == 60);
            CHECK(displacement.x > 5 && displacement.x < 7 && displacement.z > 2.5f && displacement.z < 3.5f);
            if (hz == 30)
                reference = displacement;
            else
                CHECK(same(displacement, reference));
            std::cout << hz << " Hz: displacement=" << displacement.x << ',' << displacement.y << ',' << displacement.z
                      << " velocity=" << r->velocity.x << ',' << r->velocity.y << ',' << r->velocity.z << '\n';
            // Door's once-consumed push composes with a hit rather than replacing it.
            DVector zero{};
            CHECK(g.server.SetVelocity(p->m_hObject, &zero) == DE_OK && damage(g, other, 1, {1, 0, 0}));
            p->door_push_pending = true;
            p->door_push_velocity = {0, 0, -500};
            CHECK(p->ApplyDoorPush() && same(r->velocity, {10, 0, -500}));
            CHECK(p->ApplyDoorPush() && same(r->velocity, {10, 0, -500}));
            // A hit still passes through the real swept solver; it cannot push
            // the shared player through a nearby solid wall.
            CHECK(g.server.SetVelocity(p->m_hObject, &zero) == DE_OK);
            const auto from = r->state.m_Pos;
            ObjectCreateStruct wall;
            wall.m_Pos = vector_add(from, {r->dimensions.x + 2, 0, 0});
            wall.m_Flags = FLAG_SOLID;
            auto obstacle = g.objects.create(g.objects.find_class("BaseClass"), wall);
            DVector dims{1, 1000, 1000};
            CHECK(obstacle && g.server.SetObjectDims(obstacle, &dims) == DE_OK && damage(g, other, 10, {1, 0, 0}));
            for (int i = 0; i < hz / 2; ++i)
                CHECK(g.step(1. / hz));
            CHECK(r->state.m_Pos.x > from.x + .9f && r->state.m_Pos.x <= from.x + 1.001f && r->velocity.x == 0);
            CHECK(g.unload_world() && g.objects.record_count() == 0);
        }
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(other && damage(g, other, 10, {1, 0, 0}));
        PlayerRestoreState saved;
        std::string error;
        CHECK(g.capture_player(saved, error) && saved.damage.apply_damage_physics && saved.velocity.x == 100);
        const auto old = p->m_hObject;
        CHECK(g.leave_player() && g.restore_player(saved, error));
        p = g.server_shell.Player(g.local_client);
        CHECK(p && p->m_hObject != old && p->m_damage.apply_damage_physics && p->m_damage.damage_impulses == 0);
        CHECK(same(g.objects.get(p->m_hObject)->velocity, {}));                // Original load/force-pos resets motion.
        CHECK(g.step(.05) && same(g.objects.get(p->m_hObject)->velocity, {})); // No queued replay.
        CHECK(damage(g, other, 1, {1, 0, 0}) && near(g.objects.get(p->m_hObject)->velocity.x, 10));
        CHECK(g.unload_world() && !g.server.client_count() && !g.server.message_count() &&
              g.objects.record_count() == 0);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-2000, 0, -2000}, {2000, 0, -2000}, {2000, 0, 2000}, {-2000, 0, 2000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "diagnostic-npc-player-impulse.dat";
        g.state = SessionState::Inspection;
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
        auto npc =
            g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World, {{"Pos", DVector{0, 100, 0}}});
        CHECK(npc && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* ai = dynamic_cast<CultistAI*>(g.server.HandleToObject(npc));
        CHECK(ai);
        // No direct damage, relocation, state/health edit or input: original AI acquires and fires.
        for (int i = 0; i < 600; ++i)
            CHECK(g.step(1. / 60));
        auto* r = g.objects.get(p->m_hObject);
        CHECK(ai->RuntimeReady() && ai->fire_shots > 0 && p->m_damage.damage_impulses > 0);
        CHECK(p->m_damage.m_fHitPoints < 100 && std::hypot(r->state.m_Pos.x, r->state.m_Pos.z - 200) > 1);
        CHECK(p->m_damage.last_impulse_ok);
        std::cout << "Original Cultist shots=" << ai->fire_shots << " impulses=" << p->m_damage.damage_impulses
                  << " player HP=" << p->m_damage.m_fHitPoints << " position=" << r->state.m_Pos.x << ','
                  << r->state.m_Pos.y << ',' << r->state.m_Pos.z << '\n';
        CHECK(g.leave_player() && g.unload_world() && g.objects.record_count() == 0 &&
              g.server.audio.voices().empty() && g.server.message_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "rules"     ? rules(argv[2])
            : mode == "motion"  ? motion(argv[2])
            : mode == "restore" ? restore(argv[2])
                                : mode == "private" && private_data(argv[2]))
               ? 0
               : 1;
}
