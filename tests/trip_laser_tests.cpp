// Original TripLaser: beam endpoints, 10 Hz first-hit damage, commands and flicker.
#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/TripLaser.h"
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
    HOBJECT laser(GameSession& g, Properties extra)
    {
        extra.try_emplace("Name", std::string("beam1"));
        extra.try_emplace("Pos", DVector{0, 40, 0});
        extra.try_emplace("Dims", DVector{1, 1, 52}); // 05_CDM beam: 104-unit beam along forward.
        extra.try_emplace("Color", DVector{0, 128, 255});
        extra.try_emplace("Damage", 1000.0f);
        extra.try_emplace("StartOn", true);
        return g.objects.create(g.objects.find_class("TripLaser"), {}, Creation::World, extra);
    }
    bool trigger(GameSession& g, HOBJECT h, const char* text)
    {
        auto name = g.server.CreateString(g.objects.get(h)->state.m_Name), command = g.server.CreateString(text);
        SendTriggerMsgToObjects(g.server.HandleToObject(h), name, command);
        g.server.FreeString(name);
        g.server.FreeString(command);
        return true;
    }
    bool beam(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = laser(g, {{"Sound", std::string("sounds\\ambient\\forcefieldhum.wav")}});
        CHECK(h);
        auto* t = dynamic_cast<TripLaser*>(g.objects.get(h)->game.get());
        CHECK(t && t->error.empty() && t->m_hSound);
        CHECK(std::abs(t->m_vEndPoints[0].z + 52) < .01f && std::abs(t->m_vEndPoints[1].z - 52) < .01f);
        DVector dims;
        CHECK(g.server.GetObjectDims(h, &dims) == DE_OK && dims.x == 1 && dims.y == 1 && std::abs(dims.z - 52) < .01f);
        const auto flags = g.server.GetObjectFlags(h);
        CHECK((flags & (FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_NOLIGHT)) == (FLAG_VISIBLE | FLAG_TOUCH_NOTIFY | FLAG_NOLIGHT));
        CHECK(!(flags & FLAG_SOLID));
        // A Cultist standing across the beam is the first object on it.
        auto victim = g.objects.create(
            g.objects.find_class("CultistAI"), {}, Creation::World,
            {{"Pos", DVector{0, 40, 10}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
        auto* ai = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(victim));
        CHECK(ai && ai->RuntimeReady());
        const float before = ai->m_damage.m_fHitPoints;
        // Source quirk: at time zero `now <= m_fDamageTime` (both zero) skips the contact.
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, victim) && t->touches == 1 && !t->beam_hits);
        CHECK(g.objects.advance(.05) && g.objects.engine_message(h, MID_TOUCHNOTIFY, victim));
        // 1000 Damage * DAMAGE_UPDATE_DELTA = 100 per contact.
        CHECK(t->beam_hits == 1 && t->damage_messages == 1 && ai->m_damage.m_fHitPoints < before);
        // Damage is applied at most ten times a second.
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, victim) && t->touches == 3 && t->damage_messages == 1);
        CHECK(g.objects.advance(.11) && g.objects.engine_message(h, MID_TOUCHNOTIFY, victim));
        CHECK(t->damage_messages == 2 || !g.server.IsObjectAlive(victim));
        // An object the beam segment does not hit first receives nothing.
        auto aside = g.objects.create(
            g.objects.find_class("CultistAI"), {}, Creation::World,
            {{"Pos", DVector{300, 40, 0}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
        const auto hits = t->beam_hits, touches = t->touches;
        CHECK(aside && g.objects.advance(.11) && g.objects.engine_message(h, MID_TOUCHNOTIFY, aside));
        CHECK(t->touches == touches + 1 && t->beam_hits == hits);
        // TRIGGEROFF stops the loop and flickers once before hiding again.
        CHECK(trigger(g, h, "triggeroff") && !t->m_bTriggered && !t->m_hSound && !(g.server.GetObjectFlags(h) & FLAG_VISIBLE));
        CHECK(g.objects.advance(.6) && (g.server.GetObjectFlags(h) & FLAG_VISIBLE) && t->flickers == 1);
        CHECK(g.objects.advance(.4) && !(g.server.GetObjectFlags(h) & FLAG_VISIBLE) && t->flickers == 2);
        CHECK(trigger(g, h, "TRIGGER") && t->m_bTriggered && t->m_hSound &&
              (g.server.GetObjectFlags(h) & FLAG_TOUCH_NOTIFY));
        // KILL removes the beam after its final flicker.
        CHECK(trigger(g, h, "KILL") && t->m_bRemove);
        for (int i = 0; i < 20 && g.server.IsObjectAlive(h); ++i)
            CHECK(g.objects.advance(.1));
        CHECK(!g.server.IsObjectAlive(h) && g.objects.finish_frame());
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "beam")
        return beam(argv[2]) ? 0 : 1;
    return 2;
}
