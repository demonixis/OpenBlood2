#include "brush_fixture.h"
#include "game/original/CalebAI.h"
#include "game/original/SparksObj.h"
#include "game/original/trigger_helpers.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <iostream>
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
    bool mount(GameSession& g, const char* path)
    {
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        return true;
    }
    bool sparks(const char* path)
    {
        GameSession g;
        CHECK(mount(g, path));
        Properties props{{"Name", std::string("emitter")},
                         {"Pos", DVector{100, 200, 300}},
                         {"Rotation", EulerAngles{{0, 1.5707963f, 0}}},
                         {"DelaySecsMin", .1f},
                         {"DelaySecsMax", .1f},
                         {"SparkCountMin", 12.9f},
                         {"SparkCountMax", 12.9f},
                         {"Type", std::int32_t(100)},
                         {"SparkSound", std::string("Sounds\\Weapons\\c4\\explosion_1.wav")}};
        auto h = g.objects.create(g.objects.find_class("SparksObj"), {}, Creation::World, props);
        CHECK(h);
        auto* s = dynamic_cast<SparksObj*>(g.objects.get(h)->game.get());
        CHECK(s && s->on && s->next_delay == .1f && s->sound == "Sounds\\Weapons\\c4\\explosion_1.wav");
        CHECK(g.objects.advance(.1) && s->bursts == 0); // Strict greater-than deadline.
        CHECK(g.objects.advance(.011));
        if (!s->effect || !s->voice)
            std::cerr << "Spark bursts=" << s->bursts << " " << s->error << " " << s->sound_error << '\n';
        CHECK(s->bursts == 1 && s->effect && s->voice);
        CHECK(s->error.empty() && s->sound_error.empty() && g.server.sparks.active.size() == 1);
        const auto& fx = g.server.sparks.active.front();
        CHECK(fx.particles.size() == 12 && fx.settings.count == 12 && fx.settings.gravity == -500);
        CHECK(fx.settings.position.x == 0 && fx.settings.position.y == 0 && fx.settings.position.z == 0);
        CHECK(fx.settings.color1.y == 200 && fx.settings.color2.y == 200); // Source Type lookup bug retained.
        CHECK(std::abs(fx.settings.direction.x - 1) < .001f && s->sound_position.x == 100);
        const auto original_start = s->start_time;
        auto trigger = [&](const char* value)
        {
            auto text = g.server.CreateString(value);
            auto message = g.server.StartMessageToObject(s, h, 1000);
            g.server.WriteToMessageHString(message, text);
            g.server.EndMessage(message);
            g.server.FreeString(text);
        };
        trigger("off");
        CHECK(s->on);
        trigger("OFFsuffix");
        CHECK(!s->on && s->start_time == original_start);
        CHECK(g.objects.advance(.2) && s->bursts == 1 && s->on_time > original_start);
        trigger("SWITCHsuffix");
        CHECK(s->on && s->start_time == original_start);
        CHECK(g.objects.advance(.011) && s->bursts == 2); // Off did not reset burst timer.
        trigger("OFF");
        CHECK(g.objects.advance(.011));
        s->max_seconds = .02f;
        CHECK(g.objects.advance(.03)); // Expiry is checked before off-clock refresh.
        CHECK(!g.objects.get(h));
        CHECK(g.server.sparks.active.size() == 2 && g.server.message_count() == 0 && g.server.string_count() == 0);
        CHECK(g.server.sparks.advance(.9) && g.server.sparks.active.empty()); // Effects outlive source, then expire.
        props["SparkDuration"] = 0.0f;
        CHECK(!g.objects.create(g.objects.find_class("SparksObj"), {}, Creation::World, props));
        props["SparkDuration"] = .4f;
        props["SparkCountMax"] = 256.0f;
        CHECK(!g.objects.create(g.objects.find_class("SparksObj"), {}, Creation::World, props));
        props["SparkCountMax"] = 12.9f;
        props["SparkSound"] = std::string("missing.wav");
        auto missing = g.objects.create(g.objects.find_class("SparksObj"), {}, Creation::World, props);
        CHECK(missing && g.objects.advance(.111));
        auto* bad = dynamic_cast<SparksObj*>(g.objects.get(missing)->game.get());
        CHECK(bad->effect && !bad->voice && !bad->sound_error.empty());
        WorldValidation report;
        std::string error;
        CHECK(!check_world_initialization(g, report, error) && error.starts_with("SparksObj emitter"));
        CHECK(g.unload_world() && g.server.sparks.active.empty() && g.server.string_count() == 0 &&
              g.server.message_count() == 0);
        return true;
    }
    bool caleb(const char* path)
    {
        GameSession g;
        CHECK(mount(g, path));
        std::string error;
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        auto create = [&]
        {
            return g.objects.create(g.objects.find_class("CalebAI"), {}, Creation::String,
                                    {{"Name", std::string("Caleb")},
                                     {"Pos", DVector{0, 100, 0}},
                                     {"AIState", std::string("PASSIVE")},
                                     {"AIWeapon", std::string("MINIGUN")}});
        };
        auto h = create(), other = create();
        CHECK(h && other);
        auto* a = dynamic_cast<CalebAI*>(g.objects.get(h)->game.get());
        auto* b = dynamic_cast<CalebAI*>(g.objects.get(other)->game.get());
        if (a)
            std::cerr << "Caleb initialized=" << a->runtime_initialized << " " << a->runtime_error << '\n';
        for (const auto& diagnostic : g.objects.diagnostics())
            std::cerr << diagnostic << '\n';
        CHECK(a && b && a->runtime_initialized && !a->runtime_failed);
        CHECK(a->npc_type == AI_Mgr::NPCType::Caleb && !a->m_bCabal && a->m_dwFlags == 0);
        CHECK(a->m_fHearingDist == 0 && a->m_fSeeingDist == 1000 && a->m_fSensingDist == 200);
        CHECK(a->m_fWalkSpeed == 75 && a->m_fRunSpeed == 180 && a->m_fRollSpeed == 3.5f);
        CHECK(a->npc_weapons[0] == "NONE"); // The source singular AIWeapon property is not read by AI_Mgr.
        CHECK(std::string(g.objects.get(h)->state.m_Filename) == "Models/Characters/caleb.abc");
        CHECK(std::string(g.objects.get(h)->state.m_SkinName) == "Skins/Characters/caleb1.dtx");
        CHECK(a->AnimSound() && a->AnimSound() == b->AnimSound());
        CHECK(a->AnimSound()->GetSoundRoot() == "sounds/chosen/caleb");
        CHECK(a->m_damage.m_fHitPoints >= 30 && a->m_damage.m_fHitPoints <= 31 && a->npc_armor == 0);
        CHECK(g.objects.advance(.01) && a->runtime_updates > 0 && !a->runtime_failed && !b->runtime_failed);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        CHECK(b->AnimSound() && !b->runtime_failed);
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count() &&
              !g.server.message_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "sparks" ? sparks(argv[2]) : caleb(argv[2])) ? 0 : 1;
}
