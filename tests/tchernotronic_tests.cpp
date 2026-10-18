// Original Tchernotronic: non-player TRIGGER, animation, grenade explosion, delayed destroy trigger.
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Tchernotronic.h"
#include "game/original/trigger_helpers.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(GameSession& g, const char* root)
    {
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-2000, 0, -2000}, {2000, 0, -2000}, {2000, 0, 2000}, {-2000, 0, 2000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    bool trigger(GameSession& g, HOBJECT from, const char* target, const char* text)
    {
        auto name = g.server.CreateString(target), command = g.server.CreateString(text);
        SendTriggerMsgToObjects(g.server.HandleToObject(from), name, command);
        g.server.FreeString(name);
        g.server.FreeString(command);
        return true;
    }
    Tchernotronic* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<Tchernotronic*>(r->game.get()) : nullptr;
    }
    bool explode(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto first = g.objects.create(g.objects.find_class("Tchernotronic"), {}, Creation::World,
                                      {{"Name", std::string("tech1")},
                                       {"Pos", DVector{0, 60, 0}},
                                       {"DestroyTriggerTarget", std::string("tech2")},
                                       {"DestroyTriggerMessage", std::string("trigger")}});
        auto second = g.objects.create(g.objects.find_class("Tchernotronic"), {}, Creation::World,
                                       {{"Name", std::string("tech2")}, {"Pos", DVector{1500, 60, 0}}});
        auto* t = get(g, first);
        CHECK(t && get(g, second) && g.objects.get(first)->model && g.objects.get(first)->model_error.empty());
        // B2BaseClass::InitialUpdate adds DONTFOLLOWSTANDING after the subclass flags.
        CHECK(g.server.GetObjectFlags(first) == (FLAG_VISIBLE | FLAG_SOLID | FLAG_DONTFOLLOWSTANDING) &&
              (g.server.GetObjectUserFlags(first) & (1u << 3)));
        CHECK(t->m_nRestAnim != DDWORD(-1) && t->m_nMovingAnim != DDWORD(-1));
        DVector dims, authored;
        CHECK(g.server.GetObjectDims(first, &dims) == DE_OK &&
              g.server.GetModelAnimUserDims(first, &authored, t->m_nRestAnim) == DE_OK && dims.x == authored.x &&
              dims.y == authored.y && dims.z == authored.z);
        // Waiting to be triggered: no update scheduled, rest clip kept.
        CHECK(g.objects.advance(1) && g.server.advance_models(1) && !t->explosions);
        // Players cannot trigger it; other senders can, case-insensitively.
        auto player =
            g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::World, {{"Pos", DVector{0, 60, -1000}}});
        CHECK(player && trigger(g, player, "tech1", "TRIGGER") && t->ignored_player_triggers == 1 && !t->triggers);
        auto victim = g.objects.create(
            g.objects.find_class("CultistAI"), {}, Creation::World,
            {{"Pos", DVector{0, 60, 120}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
        auto* ai = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(victim));
        CHECK(ai && ai->RuntimeReady());
        const float before = ai->m_damage.m_fHitPoints;
        ObjectCreateStruct sender_state;
        auto sender = g.objects.create(g.objects.find_class("BaseClass"), sender_state);
        CHECK(trigger(g, sender, "tech1", "trigger") && t->triggers == 1 &&
              g.server.GetModelAnimation(first) == t->m_nMovingAnim);
        // The animation plays once; when it is done the machine explodes.
        int frames = 0;
        for (; frames < 2000 && !t->explosions; ++frames)
            CHECK(g.server.advance_models(.01) && g.objects.advance(.01));
        CHECK(t->explosions == 1 && frames > 1);
        CHECK(t->explosion.type == 3 && t->explosion.radius == 200 && t->explosion.damage == 100 &&
              t->explosion.sound == "Sounds\\Weapons\\c4\\explosion_1.wav");
        CHECK(t->targets.in_radius >= 1 && t->targets.delivered >= 1);
        CHECK(!g.server.IsObjectAlive(victim) || ai->m_damage.m_fHitPoints < before);
        // Half a second later it sends its destroy trigger and removes itself.
        const float exploded = g.server.GetTime();
        for (int i = 0; i < 100 && g.server.IsObjectAlive(first); ++i)
            CHECK(g.server.advance_models(.01) && g.objects.advance(.01));
        CHECK(!g.server.IsObjectAlive(first) && g.server.GetTime() - exploded >= .5f);
        CHECK(g.objects.finish_frame() && !g.objects.get(first));
        auto* next = get(g, second);
        CHECK(next && next->triggers == 1 && g.server.GetModelAnimation(second) == next->m_nMovingAnim);
        std::cout << "explosion after " << frames << " frames, targets=" << next->triggers << "\n";
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "explode")
        return explode(argv[2]) ? 0 : 1;
    return 2;
}
