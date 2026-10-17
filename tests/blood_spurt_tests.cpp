#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Gib.h"
#include <cmath>
#include <limits>
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
        return std::abs(a - b) < .0001f;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root, false));
        CHECK(test_blood_spurt::mount(g.resources, root, error));
        CHECK(test_blood_spurt::floor(g, error));
        return true;
    }
    bool formulas(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        BloodSpurtSettings s;
        s.position = {0, 1000, 0};
        s.min_velocity = s.max_velocity = {60, 0, 0};
        CParticleExplosionFX f;
        std::minstd_rand random(1);
        CHECK(f.CreateObject(s, g.resources, g.server, 0, random, error));
        CHECK(f.Update(0, .1, g.server, random, 16384));
        CHECK(f.particles.size() == 12 && f.position.x == 6 && f.position.y == 1000 && f.velocity.y == -20);
        for (const auto& p : f.particles)
            CHECK(std::abs(p.position.x) <= .5 && std::abs(p.position.y) <= .5 && p.lifetime == 10 &&
                  near(p.color.x, 128.f / 255) && p.velocity.y >= -10 && p.velocity.y <= -5);
        auto old = f.particles[0];
        CHECK(f.Update(.1, .1, g.server, random, 16384));
        CHECK(f.particles.size() == 24 && f.position.x == 12 && f.position.y == 998 && f.velocity.y == -40);
        CHECK(near(f.particles[0].position.y, old.position.y + old.velocity.y * .1f) &&
              near(f.particles[0].velocity.y, old.velocity.y - 10));
        for (int step = 0; step < 6; ++step)
            for (int i = 0; i < 2; ++i)
            {
                const auto& p = f.particles[12 + step * 2 + i];
                CHECK(std::abs(p.position.x - step) <= .5 && near(p.lifetime, 10 + step * .1f / 6));
            }
        f.emitting = false;
        CHECK(f.Update(.5, .4, g.server, random, 16384) && f.alpha == 1);
        CHECK(f.Update(1.25, .75, g.server, random, 16384) && near(f.alpha, .5));
        CHECK(f.Update(2, .75, g.server, random, 16384) && f.alpha == 0);
        CHECK(!f.Update(2.001, .001, g.server, random, 16384) && f.error.empty());
        // Source clients explicitly Update on creation; server SFX decoding waits for the manager.
        CHECK(g.server.PublishClientBloodSpurt(s, "immediate", true, error) && error.empty());
        CHECK(g.server.blood_spurts.active.back().updates == 1 && g.server.blood_spurts.live_particles() == 12);
        CHECK(g.server.blood_spurts.advance(g.objects.time(), g.server) &&
              g.server.blood_spurts.live_particles() == 12);
        CHECK(g.server.PublishClientBloodSpurt(s, "deferred", false, error) && error.empty());
        CHECK(g.server.blood_spurts.active.back().first_update &&
              g.server.blood_spurts.active.back().particles.empty());
        CHECK(g.server.blood_spurts.advance(g.objects.time(), g.server) &&
              g.server.blood_spurts.live_particles() == 24);
        return true;
    }
    bool impact(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand rng(2);
        BloodSpurtSettings s;
        s.position = {0, 1, 0};
        s.min_velocity = s.max_velocity = {0, -20, 0};
        CParticleExplosionFX fx;
        CHECK(fx.CreateObject(s, g.resources, g.server, 0, rng, error) && fx.Update(.1, .1, g.server, rng, 16384));
        CHECK(!fx.emitting && fx.impacts == 1 && fx.splat_event && fx.splat_error.empty() && fx.sound_error.empty());
        CHECK(near(fx.position.y, .3f) && g.server.blood_splats.active.size() == 1);
        const auto splat = g.server.blood_splats.active.front();
        CHECK(splat.settings.scale >= 0 && splat.settings.scale <= .1 && splat.settings.grow_scale == .01f &&
              near(splat.position.y, .3f));
        CHECK(fx.Update(.2, .1, g.server, rng, 16384) && fx.emitted == 12 && fx.impacts == 1);
        // Client fragment uses its larger original splat recipe and is never parented to the mark.
        FleshGib fragment;
        fragment.position = {50, 1, 0};
        fragment.velocity = {0, -20, 0};
        CHECK(g.models.load("models/characters/caleb.abc", fragment.model, error));
        fragment.sound_path = "sounds/gibs/flesh/gib_impact1.wav";
        g.server.flesh_gibs.active.push_back(fragment);
        CHECK(g.server.flesh_gibs.advance(.1, g.server));
        const auto& f = g.server.flesh_gibs.active.front();
        CHECK(f.impacts == 1 && f.splat_event && f.splat_error.empty() && f.error.empty());
        const auto& large = g.server.blood_splats.reports.back();
        CHECK(large.settings.scale >= .04f && large.settings.scale <= .24f && large.settings.grow_scale == .01f);
        g.server.flesh_gibs.clear();
        CHECK(g.server.blood_splats.active.size() == 2);
        return true;
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand random(1);
        auto source = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(source);
        CWeaponFX fx;
        fx.m_vDestPos = {0, 100, 0};
        fx.m_eSurfaceType = SURFTYPE_STONE;
        CHECK(fx.SelectBloodSpurt(100, random) && !fx.blood_requested && fx.blood_roll == -1);
        fx.m_eSurfaceType = SURFTYPE_FLESH;
        unsigned selected = 0, declined = 0;
        for (int i = 0; i < 64; ++i)
        {
            CHECK(fx.SelectBloodSpurt(25, random));
            CHECK(fx.blood_requested == (fx.blood_roll == 0));
            fx.blood_requested ? ++selected : ++declined;
        }
        CHECK(selected && declined && fx.SelectBloodSpurt(100, random) && fx.blood_roll >= 0 && fx.blood_requested);
        CHECK(fx.CreateBloodSpurt(g.server, source, {0, 0, 2}, 100, random));
        CHECK(fx.blood_events[0] && fx.blood_events[1] && g.server.blood_spurts.active.size() == 2);
        const auto& a = g.server.blood_spurts.active[0];
        const auto& b = g.server.blood_spurts.active[1];
        CHECK(a.texture == b.texture && a.updates == 1 && b.updates == 1);
        CHECK(a.settings.min_velocity.x == -20 && a.settings.min_velocity.y == 40 && a.settings.min_velocity.z == 10);
        CHECK(b.settings.min_velocity.z == -270 && b.settings.max_velocity.z == -280 && b.settings.lifetime == 1.2f &&
              b.settings.fade_time == .8f);
        CHECK(g.objects.remove(source) && g.objects.finish_frame() && g.server.blood_spurts.active.size() == 2);
        CHECK(!fx.CreateBloodSpurt(g.server, source, {0, 0, 1}, 100, random) && !fx.blood_error.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        BloodSpurtSettings s;
        s.position = {0, 1000, 0};
        s.texture = 4;
        CHECK(g.server.PublishClientBloodSpurt(s, "invalid", true, error) && !error.empty());
        s.texture = 1;
        s.fade_time = 2;
        CHECK(g.server.PublishClientBloodSpurt(s, "fade", true, error) && !error.empty());
        s.fade_time = .5f;
        s.position.x = std::numeric_limits<float>::infinity();
        CHECK(g.server.PublishClientBloodSpurt(s, "position", true, error) && !error.empty());
        s.position.x = 0;
        CHECK(!g.server.PublishBloodSpurt(nullptr, s, error) && !error.empty());
        for (int i = 0; i < 128; ++i)
            CHECK(g.server.PublishClientBloodSpurt(s, "capacity", true, error) && error.empty());
        CHECK(g.server.PublishClientBloodSpurt(s, "overflow", true, error) && !error.empty() &&
              g.server.blood_spurts.active.size() == 128);
        CHECK(!g.server.blood_spurts.advance(-1, g.server) && g.server.blood_spurts.active.empty());
        std::minstd_rand rng(1);
        CParticleExplosionFX fx;
        CHECK(fx.CreateObject(s, g.resources, g.server, 0, rng, error));
        CHECK(!fx.Update(0, 0, g.server, rng, 11) && !fx.error.empty() && fx.particles.empty());
        Resources missing;
        BloodSpurts effects;
        CHECK(effects.publish(s, "missing", false, missing, g.server, 0, error) && !error.empty() &&
              effects.active.empty());
        CHECK(g.unload_world() && g.server.blood_spurts.reports.empty() && g.server.blood_splats.reports.empty());
        return true;
    }
    bool private_world(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        std::uint64_t last = 0;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.objects.clear() && test_blood_spurt::floor(g, error));
            auto h = g.objects.create(
                g.objects.find_class("CultistAI"), {}, Creation::World,
                {{"Pos", DVector{0, 100, 0}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
            auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
            CHECK(a && a->RuntimeReady() && a->m_MoveObj.Update());
            // Real original weapon -> NPC contact -> damage and client blood, on a labelled diagnostic floor.
            auto shooter_handle = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                                   {{"Pos", DVector{-200, 100, 0}},
                                                    {"AIState", std::string("IDLE")},
                                                    {"AIWeapon1", std::string("BERETTA")}});
            auto* shooter = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(shooter_handle));
            CHECK(shooter && shooter->RuntimeReady());
            auto* weapon = shooter->m_InventoryMgr.CurrentWeapon();
            CHECK(weapon);
            shooter->m_InventoryMgr.damage_multiplier = 3;
            a->m_damage.SetArmorPoints(0);
            a->m_damage.SetHitPoints(10000);
            const auto actual_target = g.objects.get(h)->state.m_Pos;
            const auto shot_origin = vector_add(actual_target, {-200, 0, 0});
            bool blood = false;
            for (unsigned shot = 0; shot < 32 && !blood; ++shot)
            {
                CHECK(weapon->Fire({shot_origin, {1, 0, 0}, DBYTE(42 + shot), WEAP_BERETTA, false, false}) ==
                      FireResult::Fired);
                CHECK(weapon->last_shot.damage_delivered && !weapon->impacts.empty());
                const auto& hit = weapon->impacts.front();
                CHECK(hit.hit.object == h && hit.effect.blood_roll >= 0);
                if (hit.effect.blood_requested)
                {
                    CHECK(hit.effect.blood_events[0] && hit.effect.blood_events[1] && hit.effect.blood_error.empty());
                    const auto& exit = g.server.blood_spurts.active.back().settings;
                    CHECK(near(exit.min_velocity.x, -2.5f * weapon->last_shot.damage * hit.hit.normal.x - 20));
                    blood = true;
                }
            }
            CHECK(blood && a->m_damage.m_fHitPoints < 10000);
            a->m_damage.SetHitPoints(100);
            CHECK(g.objects.remove(shooter_handle) && g.objects.finish_frame());
            auto limb = CreateLimb(h, 4, {0, 0, 100}, error);
            CHECK(limb);
            auto* gib = dynamic_cast<CGib*>(g.server.HandleToObject(limb));
            CHECK(gib);
            DVector pos{200, 10, 0}, v{10, -400, 0};
            CHECK(g.server.TeleportObject(limb, &pos) == DE_OK && g.server.SetVelocity(limb, &v) == DE_OK);
            gib->last_position = pos;
            gib->bounces = 2;
            CHECK(g.objects.engine_message(limb, MID_TOUCHNOTIFY, nullptr) && gib->add_blood_spurt &&
                  !gib->spurt_event);
            CHECK(g.objects.engine_message(limb, MID_UPDATE, nullptr) && !gib->add_blood_spurt && gib->spurt_event &&
                  gib->spurt_error.empty());
            CHECK(g.server.blood_spurts.active.back().first_update);
            DVector actual;
            CHECK(g.server.GetVelocity(limb, &actual) == DE_OK);
            CHECK(near(g.server.blood_spurts.active.back().settings.min_velocity.x, actual.x * .06f));
            a->m_damage.SetArmorPoints(0);
            auto m = g.server.StartMessageToObject(a, h, damage_message);
            CHECK(m && g.server.WriteDamage(m, {{0, 0, 1}, {0, 100, 0}, 500, DamageType::Explode, h, true}) == DE_OK &&
                  g.server.EndMessage(m) == DE_OK);
            CHECK(a->m_bRemoveMe && a->gib_error.empty() && !g.server.flesh_gibs.active.empty());
            for (const auto& f : g.server.flesh_gibs.active)
                CHECK(f.spurt_event && f.spurt_error.empty());
            CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && g.objects.remove(limb) &&
                  g.objects.finish_frame());
            CHECK(!g.server.IsObjectAlive(h) && !g.server.IsObjectAlive(limb));
            CHECK(g.server.blood_spurts.active.front().id > last);
            last = g.server.blood_spurts.active.back().id;
            std::size_t peak = 0;
            for (int frame = 0; frame < 110; ++frame)
            {
                CHECK(g.objects.advance(.02) && g.server.flesh_gibs.advance(g.objects.time(), g.server) &&
                      g.server.blood_spurts.advance(g.objects.time(), g.server) &&
                      g.server.blood_splats.advance(g.objects.time()));
                peak = std::max(peak, g.server.blood_spurts.live_particles());
            }
            CHECK(peak > 12 && g.server.blood_spurts.active.empty() && g.server.blood_splats.active.size() > 0);
            unsigned impacts = 0;
            for (const auto& r : g.server.blood_spurts.reports)
            {
                CHECK(r.error.empty() && r.splat_error.empty() && r.ended);
                impacts += r.impacts;
            }
            CHECK(impacts > 0);
            for (unsigned variant = 1; variant <= 3; ++variant)
            {
                BloodSpurtSettings s;
                s.position = {0, 1000, 0};
                s.texture = variant;
                CHECK(g.server.PublishClientBloodSpurt(s, "private texture", true, error) && error.empty());
            }
            SpriteView view;
            CHECK(update_sprite_scene(g, view, error));
            unsigned drops = 0;
            for (const auto& e : view.entries)
                if (e.blood_spurt)
                {
                    CHECK(e.error.empty() && e.width > 0);
                    ++drops;
                }
            CHECK(drops == 36);
            std::cout << "Private cycle " << cycle << ": original death/limb/fragment callers, peak " << peak
                      << ", emitter impacts " << impacts << ", splats " << g.server.blood_splats.active.size()
                      << ", three real drop textures\n";
            CHECK(g.unload_world() && g.server.blood_spurts.active.empty() && g.server.blood_splats.active.empty() &&
                  g.server.flesh_gibs.active.empty() && !g.server.message_count() && !g.server.string_count());
            CHECK(update_sprite_scene(g, view, error) && view.entries.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    return (mode == "formulas"   ? formulas(argv[2])
            : mode == "impact"   ? impact(argv[2])
            : mode == "weapon"   ? weapon(argv[2])
            : mode == "failures" ? failures(argv[2])
                                 : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
