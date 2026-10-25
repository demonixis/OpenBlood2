#include "app/sprite_scene.h"
#include "brush_fixture.h"
#include "game/original/VolumeBrush.h"
#include "weapon_sprite_fixture.h"
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
        return std::abs(a - b) < .0001f;
    }
    SmokeImpactSettings smoke()
    {
        SmokeImpactSettings s;
        s.position = {1, 2, 3};
        s.normal = {0, 0, 1};
        s.offset = 3;
        s.scale1 = {.1f, .1f, 0};
        s.scale2 = {.35f, .35f, 0};
        s.duration = 1;
        s.alpha = .65f;
        s.fade = true;
        s.move = true;
        s.color = {1, .8f, .6f};
        s.velocity = {1, 0, 0};
        s.deceleration = {.1f, 0, 0};
        s.gravity = {0, .1f, 0};
        return s;
    }
    bool formulas(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_weapon_sprite::mount(r, root, error));
        SpriteStore store(r);
        CSmokeImpactFX f;
        auto s = smoke();
        CHECK(f.Init(s, error) && f.CreateObject(store, 10, error));
        CHECK(f.position.z == 6 && f.alpha == 1 && f.color.y == 1 && f.updates == 0);
        CHECK(f.Update(10.5) && f.start_time == 10.5 && near(f.position.x, 2.1f) && near(f.alpha, .65f));
        CHECK(near(f.color.y, .8f) && near(f.color.z, .6f) && near(f.position.y, 2.1f));
        CHECK(f.Update(10.5005) && f.updates == 1 && near(f.position.x, 2.1f));
        CHECK(f.Update(11) && near(f.scale.x, .225f) && near(f.alpha, .325f) && near(f.position.x, 3.3f));
        CHECK(!f.Update(11.5) && f.error.empty() && near(f.scale.x, .35f) && f.alpha == 0 && near(f.position.x, 4.6f));
        s = smoke();
        s.update_scale = false;
        s.rotate = true;
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, error) && f.Update(.1) && f.Update(.6));
        CHECK(f.scale_changes && near(f.scale.x, .225f) && !(f.flags & FLAG_ROTATEABLESPRITE));
        s.scale1 = {1, 1, 0};
        s.scale2 = {1, 1, 5};
        s.color = {};
        s.move = false;
        s.fade = false;
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, error) && f.Update(.1) && f.Update(.6));
        CHECK(!f.scale_changes && f.scale.z == 0 && f.color.y == 1 && near(f.alpha, .65f) && f.position.y == 2);
        s.scale1 = {1, 1, 0};
        s.scale2 = {.5f, .5f, 0};
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, error) && f.Update(.1) && f.scale.x == .5f);
        return true;
    }
    bool delay(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_weapon_sprite::mount(r, root, error));
        SpriteStore store(r);
        CSmokeImpactFX f;
        auto s = smoke();
        s.delay = .25f;
        CHECK(f.Init(s, error) && f.CreateObject(store, 0, error) && f.Update(0));
        CHECK(f.first_update && f.start_delay == 0 && f.alpha == 0 && f.position.x == 1);
        CHECK(f.Update(.1) && f.start_delay == .1 && f.Update(.36));
        CHECK(f.delay == 0 && f.first_update && f.alpha == 0); // Zero sentinel rearmed; activation returns early.
        CHECK(f.Update(.4) && f.start_time == .4 && !f.first_update && near(f.alpha, .65f));
        CHECK(f.Update(1.39) && !f.Update(1.41) && f.alpha == 0);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources r;
        std::string error;
        CHECK(test_weapon_sprite::mount(r, root, error, false));
        SpriteStore store(r);
        CSmokeImpactFX f;
        auto s = smoke();
        for (auto value : {0.f, -1.f, 601.f, NAN})
        {
            s.duration = value;
            CHECK(!f.Init(s, error) && !error.empty());
        }
        s = smoke();
        s.alpha = 2;
        CHECK(!f.Init(s, error));
        s = smoke();
        s.color.x = NAN;
        CHECK(!f.Init(s, error));
        s = smoke();
        s.resource = "";
        CHECK(f.Init(s, error) && !f.CreateObject(store, 0, error));
        s = smoke();
        CHECK(f.Init(s, error) && f.CreateObject(store, 1, error) && !f.Update(.5) && !f.error.empty());
        WeaponSprites effects;
        WeaponSpriteSettings setting;
        setting.smoke = smoke();
        setting.flash.emplace();
        CHECK(effects.publish(setting, "source", store, 0, error) && !error.empty() && effects.smoke.size() == 1 &&
              effects.flashes.empty());
        CHECK(effects.failed == 1 && effects.started == 1 && effects.reports.back().smoke &&
              !effects.reports.back().flash);
        CHECK(!effects.advance(-1) && effects.smoke.empty() && effects.failed == 2 && !effects.update_error.empty());
        effects.clear();
        setting.flash.reset();
        for (unsigned i = 0; i < 256; ++i)
            CHECK(effects.publish(setting, "source", store, 0, error) && error.empty());
        CHECK(effects.publish(setting, "source", store, 0, error) && !error.empty() && effects.started == 256 &&
              effects.failed == 1);
        CHECK(effects.smoke.size() == 256 && effects.reports.size() == 64 && effects.advance(.1) &&
              effects.advance(1.2));
        CHECK(effects.expired == 256 && effects.smoke.empty());
        auto id = effects.reports.back().id;
        effects.clear();
        CHECK(effects.publish(setting, "source", store, 0, error) > id && effects.smoke.front().id > 256);
        return true;
    }
    bool selection(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_weapon_sprite::mount(g.resources, root, error));
        CHECK(g.enter_player(error));
        auto h = g.server_shell.Player(g.local_client)->m_hObject;
        CWeaponFX f;
        f.m_vDestPos = {10, 40, 30};
        std::minstd_rand rng(9);
        for (auto type : {AMMO_BULLET, AMMO_SHELL, AMMO_BMG})
            for (auto material : {SURFTYPE_WOOD, SURFTYPE_STONE})
            {
                f.m_nAmmoType = type;
                f.m_eSurfaceType = material;
                auto expected = rng;
                auto flash = std::uniform_int_distribution<int>(1, 3)(expected);
                auto gravity = std::uniform_real_distribution<float>(.05f, .1f)(expected);
                auto resource = std::uniform_int_distribution<int>(0, 2)(expected) + 1;
                CHECK(f.CreateSprites(g.server, h, {0, 0, 2}, rng) && rng == expected);
                const auto& report = g.server.weapon_sprites.reports.back();
                CHECK(report.flash && report.smoke &&
                      report.settings.flash->resource == "Sprites/impact0" + std::to_string(flash) + ".spr");
                CHECK(report.settings.smoke->resource == "Sprites/smokepuff" + std::to_string(resource) + ".spr");
                CHECK(report.settings.smoke->gravity.y == gravity && report.settings.smoke->move);
                CHECK(report.settings.flash->position.z == 31 && g.server.weapon_sprites.smoke.back().position.z == 33);
                CHECK(report.settings.smoke->color.x == (material == SURFTYPE_WOOD ? 1 : 0));
            }
        for (auto material : {SURFTYPE_BUILDING, SURFTYPE_METAL, SURFTYPE_UNKNOWN, SURFTYPE_GLASS, SURFTYPE_LIQUID})
        {
            f.m_eSurfaceType = material;
            auto old = rng;
            CHECK(f.CreateSprites(g.server, h, {0, 0, 1}, rng) && !f.sprite_event && old == rng);
        }
        f.m_eSurfaceType = SURFTYPE_WOOD;
        f.m_nAmmoType = AMMO_NONE;
        auto old = rng;
        CHECK(f.CreateSprites(g.server, h, {0, 0, 1}, rng) && !f.sprite_event && old == rng);
        f.m_nAmmoType = AMMO_BULLET;
        CHECK(!f.CreateSprites(g.server, h, {}, rng) && rng == old && !f.sprite_error.empty());
        CHECK(f.CreateSprites(g.server, h, {0, -1, 0}, rng) && !g.server.weapon_sprites.smoke.back().settings.move);
        return true;
    }
    bool containers(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_weapon_sprite::mount(g.resources, root, error) &&
              g.enter_player(error));
        auto h = g.server_shell.Player(g.local_client)->m_hObject;
        auto a = test_brush::box("A", {-10, 10, -10}, {10, 50, 10});
        auto b = a;
        b.name = "B";
        g.world.models.push_back(a);
        g.world.models.push_back(b);
        auto first = g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                                      {{"Name", std::string("A")}, {"Hidden", true}});
        auto second =
            g.objects.create(g.objects.find_class("Water"), {}, Creation::World, {{"Name", std::string("B")}});
        CHECK(first && second);
        CWeaponFX f;
        f.m_nAmmoType = AMMO_BULLET;
        f.m_eSurfaceType = SURFTYPE_STONE;
        f.m_vDestPos = {0, 30, 0};
        std::minstd_rand rng(1);
        ContainerCode code;
        CHECK(g.server.GetPointContainerCode(f.m_vDestPos, code) && code == CC_WATER);
        CHECK(g.server.GetPointContainerCode(f.m_vDestPos, code, true) && code == CC_NOTHING);
        CHECK(f.CreateSprites(g.server, h, {0, 1, 0},
                              rng)); // Hidden first volume masks visible second, source one-result query.
        g.server.SetObjectUserFlags(first, FLAG_VISIBLE);
        auto before = rng;
        CHECK(!f.CreateSprites(g.server, h, {0, 1, 0}, rng) && rng == before &&
              f.sprite_error.find("bubbles") != std::string::npos);
        CHECK(g.server.weapon_sprites.reports.back().settings.bubbles_unavailable &&
              !g.server.weapon_sprites.reports.back().smoke);
        CHECK(g.objects.remove(first));
        g.objects.finish_frame();
        CHECK(!f.CreateSprites(g.server, h, {0, 1, 0}, rng));
        g.server.SetObjectUserFlags(second, 0);
        CHECK(f.CreateSprites(g.server, h, {0, 1, 0}, rng));
        return true;
    }
    bool session(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && test_weapon_sprite::mount(g.resources, root, error) &&
              g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        pos.y += p->eye_level;
        WorldModel wall;
        test_brush::face(wall,
                         {{pos.x - 200, pos.y - 200, pos.z + 40},
                          {pos.x + 200, pos.y - 200, pos.z + 40},
                          {pos.x + 200, pos.y + 200, pos.z + 40},
                          {pos.x - 200, pos.y + 200, pos.z + 40}},
                         {0, 0, -1});
        wall.surfaces.back().flags = 1;
        wall.surfaces.back().texture_flags = SURFTYPE_WOOD;
        CHECK(g.collision.build(wall, error));
        CHECK(g.server_shell.OnMessage(g.local_client, WeaponFire{pos, {0, 0, 1}, 42, WEAP_BERETTA, false, false}) ==
              FireResult::Fired);
        auto* w = p->m_InventoryMgr.GetWeapon(1);
        CHECK(w->impacts.size() == 1 && w->impacts[0].effect.sprite_event && w->impacts[0].effect.sprite_error.empty());
        CHECK(g.server.weapon_sprites.started == 3 && g.server.weapon_sprites.failed == 0);
        CHECK(near(g.server.weapon_sprites.smoke.front().position.z, pos.z + 40 - 4.25f));
        CHECK(near(g.server.weapon_sprites.flashes.front().settings.position.z, pos.z + 40 - 2.25f));
        CHECK(g.step(.1));
        const auto updates = g.server.weapon_sprites.smoke.front().updates;
        SpriteView view;
        for (int i = 0; i < 3; ++i)
        {
            CHECK(update_sprite_scene(g, view, error) && view.draws.size() == 3);
            CHECK(g.server.weapon_sprites.smoke.front().updates == updates);
        }
        CHECK(std::any_of(view.mesh.vertices.begin(), view.mesh.vertices.end(),
                          [](const auto& v) { return near(v.color.y, .8f); }));
        CHECK(g.leave_player() && g.server.weapon_sprites.smoke.size() == 2 && g.step(.1) &&
              g.server.weapon_sprites.flashes.empty());
        CHECK(g.step(.5) && g.step(.5) && g.step(.5) && g.server.weapon_sprites.smoke.empty() &&
              g.server.weapon_sprites.expired == 3);
        CHECK(g.unload_world() && g.server.weapon_sprites.reports.empty() && g.server.string_count() == 0 &&
              g.server.message_count() == 0);
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        std::uint64_t previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto h = g.server_shell.Player(g.local_client)->m_hObject;
            CWeaponFX fx;
            fx.m_nAmmoType = AMMO_BULLET;
            fx.m_eSurfaceType = SURFTYPE_WOOD;
            fx.m_vDestPos = {0, 5000, 0};
            std::minstd_rand random(1 + cycle);
            for (unsigned i = 0; i < 32; ++i)
                CHECK(fx.CreateSprites(g.server, h, {0, 1, 0}, random));
            CHECK(g.server.weapon_sprites.started == 64 && g.server.weapon_sprites.failed == 0 && g.step(.05));
            SpriteView view;
            CHECK(update_sprite_scene(g, view, error));
            unsigned count = 0;
            for (const auto& e : view.entries)
                if (e.weapon_effect)
                {
                    CHECK(e.error.empty() && e.range != UINT32_MAX && e.width > 0);
                    ++count;
                }
            CHECK(count == 64 && g.server.weapon_sprites.smoke.front().id > previous);
            previous = g.server.weapon_sprites.smoke.back().id;
            const auto before = g.server.weapon_sprites.smoke.front();
            const auto time = g.objects.time();
            WorldMessage destination;
            CHECK(original_world_request("02_cityhub", 0, LOADTYPE_NEW_LEVEL, 0, destination));
            CHECK(g.rehearse_destination(destination, "01_egressa", error) && g.destination_trial.cleanup_complete);
            const auto& after = g.server.weapon_sprites.smoke.front();
            CHECK(g.objects.time() == time && g.server.weapon_sprites.started == 64 && after.id == before.id &&
                  after.updates == before.updates && after.track.frame == before.track.frame &&
                  after.alpha == before.alpha && after.position.y == before.position.y &&
                  after.sprite == before.sprite);
            CHECK(g.leave_player() && g.step(.1) && g.step(.1));
            CHECK(g.server.weapon_sprites.flashes.empty() && g.server.weapon_sprites.smoke.size() == 32);
            CHECK(g.unload_world() && g.server.weapon_sprites.smoke.empty() && g.server.weapon_sprites.reports.empty());
            view.clear();
            CHECK(update_sprite_scene(g, view, error) && view.entries.empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
            std::cout
                << "Private cycle " << cycle
                << ": 32 original diagnostic wood recipes, 64 actual SPR/DTX quads; source removal and unload passed\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "formulas"     ? formulas(argv[2])
            : mode == "delay"      ? delay(argv[2])
            : mode == "failures"   ? failures(argv[2])
            : mode == "selection"  ? selection(argv[2])
            : mode == "containers" ? containers(argv[2])
            : mode == "session"    ? session(argv[2])
                                   : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
