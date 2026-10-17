#include "app/sprite_scene.h"
#include "blood_splat_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/ClientSplatFX.h"
#include "game/original/Corpse.h"
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
    bool alignment()
    {
        for (auto forward :
             {DVector{0, 1, 0}, DVector{0, -1, 0}, DVector{0, 0, 1}, DVector{0, 0, -1}, DVector{1, 2, 3}})
        {
            DVector f, right;
            bool fallback = false;
            DRotation q;
            CHECK(alignment_axes(forward, forward, f, right, fallback) && fallback &&
                  align_rotation(forward, forward, q));
            auto actual = rotate_vector(q, {0, 0, 1}), r = rotate_vector(q, {1, 0, 0});
            CHECK(near(vector_dot(actual, f), 1) && near(vector_dot(r, right), 1));
        }
        DRotation q;
        CHECK(!align_rotation({}, {0, 1, 0}, q) && !align_rotation({1, 0, 0}, {}, q));
        return true;
    }
    bool formulas(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_blood_splat::mount(resources, root, error));
        SpriteStore store(resources);
        BloodSplatSettings s;
        s.resource = "sprites/blood1.spr";
        CBloodSplatFX f;
        std::minstd_rand random(1), expected(1);
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, random, error) && f.grow_time == 4);
        CHECK(f.Update(.1, random));
        std::uniform_real_distribution<float> grow(1, 1 + .02f * .1f * 30 * .1f);
        CHECK(near(f.scale.x, .1f * grow(expected)) && near(f.scale.y, .1f * grow(expected)) &&
              near(f.scale.z, .1f * grow(expected)));
        CHECK(near(f.grow_time, 3.9f));
        const auto before = f.scale;
        CHECK(f.Update(1.1, random));
        std::uniform_real_distribution<float> capped(1, 1 + .02f * .25f * 30 * .25f);
        CHECK(near(f.scale.x, before.x * capped(expected)) && near(f.scale.y, before.y * capped(expected)) &&
              near(f.scale.z, before.z * capped(expected)));
        CHECK(near(f.grow_time, 3.65f) && !f.Term() && f.shrinking && f.grow_time == 0);
        CHECK(f.Update(3.1, random) && f.alpha == 1 && near(f.grow_time, 2));
        CHECK(f.Update(6.1, random) && near(f.alpha, .6f) && near(f.grow_time, 5));
        CHECK(!f.Update(6.2, random) && f.error.empty());
        s.grow_scale = 0;
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, random, error) && f.Update(50, random) &&
              f.Update(100, random) && f.scale.x == .1f && f.alpha == 1);
        CHECK(!f.Term() && f.Update(101, random) && f.scale.x == .1f && f.alpha == 1 && f.Update(102, random) &&
              near(f.alpha, .8f));
        s.grow_scale = .02f;
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, random, error));
        for (int i = 1; i <= 17; ++i)
            CHECK(f.Update(i * .25, random));
        auto stopped = f.scale;
        CHECK(f.grow_time <= 0 && f.Update(6, random) && f.scale.x == stopped.x);
        return alignment();
    }
    bool region(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_blood_splat::mount(resources, root, error));
        SpriteStore store(resources);
        BloodSplats effects;
        BloodSplatSettings s;
        s.resource = "sprites/blood1.spr";
        for (unsigned i = 0; i < 11; ++i)
        {
            s.position = {float(i) * .5f, 0, 0};
            CHECK(effects.publish(s, "region", store, 0, error) && error.empty());
        }
        CHECK(effects.active.size() == 11 && effects.started == 11 && effects.reused == 0);
        CHECK(effects.advance(.1));
        const auto original = effects.active[2];
        s.position = {1.1f, 0, 0};
        s.scale = 1;
        s.grow_scale = 0;
        s.resource = "missing.spr";
        CHECK(effects.publish(s, "reuse", store, .1, error) && error.empty() && effects.active.size() == 11);
        CHECK(effects.reports.back().reused && effects.reports.back().effect == original.id && effects.reused == 1);
        auto& moved = effects.active[2];
        CHECK(moved.position.x == 1.1f && moved.scale.x == original.scale.x && moved.resource == original.resource &&
              moved.grow_time == original.grow_time && moved.updates == original.updates);
        // Exactly 10 units away is outside (squared distance strictly <100).
        effects.clear();
        s = {};
        s.resource = "sprites/blood1.spr";
        for (int i = 0; i < 11; ++i)
            CHECK(effects.publish(s, "same", store, 0, error) && error.empty());
        s.position = {10, 0, 0};
        CHECK(effects.publish(s, "boundary", store, 0, error) && error.empty() && effects.active.size() == 12);
        // Scan stops at 11, although the twelfth object is now the nearest.
        s.position = {9, 0, 0};
        CHECK(effects.publish(s, "early", store, 0, error) &&
              effects.reports.back().effect == effects.active.front().id);
        CHECK(effects.active.front().position.x == 9 && effects.active.back().position.x == 10);
        CHECK(effects.terminate(effects.active.front().id));
        s.position = {9, 0, 0};
        CHECK(effects.publish(s, "shrinking reuse", store, 0, error) && effects.active.front().shrinking);
        auto id = effects.active.back().id;
        effects.clear();
        CHECK(effects.publish(s, "reload", store, 0, error) && effects.active.front().id > id);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_blood_splat::mount(resources, root, error));
        SpriteStore store(resources);
        BloodSplats effects;
        BloodSplatSettings s;
        s.position.x = std::numeric_limits<float>::infinity();
        CHECK(effects.publish(s, "invalid", store, 0, error) && !error.empty() && effects.active.empty());
        s = {};
        s.resource = "";
        CHECK(effects.publish(s, "empty", store, 0, error) && !error.empty());
        s.resource = "missing.spr";
        CHECK(effects.publish(s, "missing", store, 0, error) && !error.empty());
        s = {};
        s.rotation.m_Spin = 0;
        CHECK(effects.publish(s, "rotation", store, 0, error) && !error.empty());
        s = {};
        s.grow_scale = .2f;
        CHECK(effects.publish(s, "growth", store, 0, error) && !error.empty());
        s = {};
        CHECK(!effects.publish(s, std::string(101, 'x'), store, 0, error) && !error.empty());
        for (unsigned i = 0; i < 256; ++i)
        {
            s.position = {float(i) * 30, 0, 0};
            CHECK(effects.publish(s, "capacity", store, 0, error) && error.empty());
        }
        s.position.x += 30;
        CHECK(effects.publish(s, "overflow", store, 0, error) && !error.empty() && effects.active.size() == 256 &&
              effects.reports.size() == 64);
        CHECK(!effects.advance(-1) && effects.active.empty() && !effects.update_error.empty());
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        std::string error;
        CHECK(test_blood_splat::mount(g.resources, root, error));
        auto h = g.objects.create(g.objects.find_class("CClientSplatFX"), {});
        auto* splat = dynamic_cast<CClientSplatFX*>(g.server.HandleToObject(h));
        CHECK(splat && splat->Setup({0, 1, 0}, {0, 1, 0}, .1f, .02f) && splat->event &&
              g.server.blood_splats.started == 1);
        CHECK(g.objects.get(h)->state.m_Flags & FLAG_FORCECLIENTUPDATE);
        const auto id = g.server.blood_splats.active.front().id;
        CHECK(g.objects.advance(.001) && !g.server.IsObjectAlive(h) && g.server.blood_splats.active.size() == 1);
        BloodSplatSettings s;
        CHECK(!g.server.PublishBloodSplat(h, s, error) && !error.empty());
        CHECK(!g.objects.create(g.objects.find_class("CClientSplatFX"), {}, Creation::World));
        CHECK(g.server.blood_splats.advance(g.objects.time()));
        SpriteView view;
        auto callbacks = g.server.blood_splats.active.front().updates;
        CHECK(update_sprite_scene(g, view, error) && view.entries.size() == 1 && view.entries[0].error.empty() &&
              view.entries[0].blood_splat == id);
        CHECK(update_sprite_scene(g, view, error) && g.server.blood_splats.active.front().updates == callbacks);
        CHECK(g.unload_world() && g.server.blood_splats.active.empty() && g.server.blood_splats.reports.empty());
        CHECK(update_sprite_scene(g, view, error) && view.entries.empty() && g.server.message_count() == 0 &&
              g.server.string_count() == 0);
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
            CHECK(g.request_world("01_SubwayA", error));
            // Real model/death code on labelled diagnostic primary-world floor.
            CHECK(g.objects.clear());
            WorldModel floor;
            floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
            floor.surfaces.resize(1);
            floor.surfaces[0].flags = 1;
            floor.polygons = {{0, {0, 1, 2, 3}}};
            CHECK(g.collision.build(floor, error));
            g.server.collision_ready = true;
            auto h = g.objects.create(
                g.objects.find_class("CultistAI"), {}, Creation::World,
                {{"Pos", DVector{0, 100, 0}}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
            auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
            CHECK(a && a->RuntimeReady() && a->m_MoveObj.Update());
            a->m_dwFlags |= 0x800;
            a->m_damage.SetHitPoints(0);
            a->m_damage.node_hit = 2;
            CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState() && a->DispatchState() && a->last_corpse);
            auto body = a->last_corpse;
            auto* corpse = dynamic_cast<CCorpse*>(g.server.HandleToObject(body));
            CHECK(corpse);
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
            for (int frame = 0; frame < 600 && !corpse->animation_done; ++frame)
                CHECK(g.server.advance_models(.02) && g.server.advance_physics(.02) && g.objects.advance(.02) &&
                      g.server.blood_splats.advance(g.objects.time()));
            CHECK(corpse->animation_done && corpse->pool_requested && corpse->pool_event &&
                  corpse->pool_error.empty() && g.server.blood_splats.active.size() == 1);
            const auto pool = g.server.blood_splats.active.front();
            CHECK(pool.id > last && near(pool.position.y, .1f));
            last = pool.id;
            CHECK(g.objects.remove(body) && g.objects.advance(.01) && g.server.blood_splats.active.size() == 1);
            // Each original random choice has a real SPR and DTX frame.
            for (int variant = 1; variant <= 3; ++variant)
            {
                BloodSplatSettings s;
                s.position = {float(variant) * 30, .1f, 0};
                s.resource = "sprites/blood" + std::to_string(variant) + ".spr";
                CHECK(g.server.blood_splats.publish(s, "private variant", g.sprites, g.objects.time(), error) &&
                      error.empty());
            }
            SpriteView view;
            CHECK(update_sprite_scene(g, view, error) && view.entries.size() == 4 && view.draws.size() == 4);
            for (const auto& e : view.entries)
                CHECK(e.error.empty() && e.width > 0 && e.height > 0 && e.rotatable);
            for (int frame = 0; frame < 210; ++frame)
                CHECK(g.objects.advance(.02) && g.server.blood_splats.advance(g.objects.time()));
            CHECK(g.server.blood_splats.active.size() == 4 && g.server.blood_splats.active.front().grow_time <= 0);
            std::cout << "Private cycle " << cycle
                      << ": original Cultist death -> corpse -> pool, real 3 SPR/DTX variants, source-independent "
                         "growth and unload\n";
            CHECK(g.unload_world() && g.server.blood_splats.active.empty() && !g.server.message_count() &&
                  !g.server.string_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "formulas"   ? formulas(argv[2])
            : mode == "region"   ? region(argv[2])
            : mode == "failures" ? failures(argv[2])
            : mode == "session"  ? session(argv[2])
                                 : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
