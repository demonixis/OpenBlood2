#include "app/sprite_scene.h"
#include "blood_spurt_fixture.h"
#include "brush_fixture.h"
#include "camera_fixture.h"
#include "game/original/VolumeBrush.h"
#include "weapon_sprite_fixture.h"
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
    struct BallisticWeapon : CWeapon
    {
        explicit BallisticWeapon(DBYTE type) : CWeapon(type) {}
    };
    struct SaveProbe : B2BaseClass
    {
        DDWORD EngineMessageFn(DDWORD id, void*, DFLOAT) override
        {
            if (id == MID_SAVEOBJECT)
            {
                std::string error;
                ShotgunEffectSettings s;
                GetServerDE()->PublishShotgunEffects(m_hObject, s, error);
            }
            return 1;
        }
    };
    bool setup(GameSession& g, const std::filesystem::path& root, bool missing = false)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(test_weapon_sprite::mount(g.resources, root, error));
        std::vector<std::uint8_t> skin, model, spr;
        CHECK(g.resources.read("alpha.dtx", skin, error));
        CHECK(g.resources.read("models/characters/caleb.abc", model, error));
        CHECK(g.resources.read("sprites/impact01.spr", spr, error));
        test_player::Directory dir;
        if (!missing)
            dir.children["models"].children["ammo"].files["sshell.abc"] = model;
        dir.children["skins"].children["ammo"].files["sshell.dtx"] = skin;
        if (!missing)
            dir.children["models"].children["ammo"].files["bshell.abc"] = model;
        dir.children["skins"].children["ammo"].files["bshell.dtx"] = skin;
        dir.children["spritetextures"].files["drop32_1.dtx"] = skin;
        dir.children["spritetextures"].children["particles"].files["particlebubble.dtx"] = skin;
        for (const auto* name : {"blthole", "blthole2", "blthole3", "blthole4", "blthole5", "woodsplint1",
                                 "woodsplint2", "woodsplint3", "stonechip1", "stonechip2", "stonechip3", "ripple"})
            dir.children["sprites"].files[std::string(name) + ".spr"] = spr;
        for (const auto* material : {"stone", "wood", "metal", "water", "glass", "plastic", "terrain"})
            for (int i = 1; i <= 2; ++i)
                dir.children["sounds"]
                    .children["weapons"]
                    .children["shelldrops"]
                    .children[material]
                    .files["shell" + std::to_string(i) + ".wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        const auto path = root / "shotgun.rez";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        CHECK(g.resources.mount(path, error));
        return true;
    }
    ShotgunEffectSettings recipe()
    {
        ShotgunEffectSettings s;
        s.source = {0, 80, 0};
        s.destination = {0, 80, 100};
        s.forward = {0, 0, 1};
        s.normal = {0, 0, -1};
        s.surface = SURFTYPE_WOOD;
        return s;
    }
    bool publish(GameSession& g, ShotgunEffectSettings s, HOBJECT h)
    {
        std::string error;
        CHECK(g.server.PublishShotgunEffects(h, s, error));
        if (!error.empty())
            std::cerr << error << '\n';
        CHECK(error.empty());
        return true;
    }
    bool recipes(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(owner);
        auto& fx = g.server.shotgun_effects;
        auto s = recipe();
        CHECK(publish(g, s, owner));
        CHECK(fx.casings.empty() && fx.reports.back().smoke == 0 && fx.reports.back().fragments >= 1 &&
              fx.reports.back().marks == 1);
        s.eject = true;
        CHECK(publish(g, s, owner));
        CHECK(fx.casings.size() == 2 && fx.reports.back().smoke);
        for (const auto& c : fx.casings)
            CHECK(c.velocity.x >= 50 && c.velocity.x <= 70 && c.velocity.y >= 60 && c.velocity.y <= 90 &&
                  c.velocity.z >= 10 && c.velocity.z <= 25 && c.bounces == 2 && !c.visible);
        const auto& smoke = g.server.weapon_sprites.reports.back().settings.smoke;
        CHECK(smoke && smoke->alpha == .25f && smoke->duration == 1 && smoke->scale1.x == .04f &&
              smoke->scale2.x == .25f);
        s.left = true;
        CHECK(publish(g, s, owner));
        CHECK(fx.casings.back().velocity.x <= -50);
        for (auto material : {SURFTYPE_GLASS, SURFTYPE_SKY, SURFTYPE_FLESH, SURFTYPE_UNKNOWN})
        {
            s.surface = material;
            s.eject = false;
            CHECK(publish(g, s, owner));
            CHECK(fx.reports.back().marks == 0 && fx.reports.back().fragments == 0);
        }
        s.surface = SURFTYPE_WOOD;
        for (int i = 0; i < 20; ++i)
            CHECK(publish(g, s, owner));
        CHECK(std::count_if(fx.sprites.begin(), fx.sprites.end(),
                            [](const auto& f) { return f.kind == ShotgunSpriteKind::Mark; }) == 11);
        static ClassInfo probe{"ShotgunSaveProbe",
                               nullptr,
                               []() -> std::unique_ptr<BaseClass> { return std::make_unique<SaveProbe>(); },
                               {}};
        probe.parent = g.objects.find_class("B2BaseClass");
        CHECK(g.objects.register_class(probe));
        auto saved = g.objects.create(&probe, {});
        CHECK(saved);
        std::vector<std::uint8_t> bytes;
        SavePayloadReport report;
        const auto before = fx.started;
        CHECK(!g.server.save_object_payload(saved, 0, {}, bytes, report) && fx.started == before);
        return true;
    }
    bool ballistics(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        auto& fx = g.server.shotgun_effects;
        auto s = recipe();
        s.ammo = AMMO_BULLET;
        s.eject = true;
        s.source = {0, 200, 0};
        s.normal = {0, -1, 0}; // The original tracer ray follows the normal, not the shot.
        CHECK(publish(g, s, owner));
        CHECK(fx.casings.size() == 1 && fx.casings[0].scale == 1.5f &&
              fx.casings[0].skin_path == "Skins/Ammo/bshell.dtx");
        CHECK(fx.sprites[0].scale.x == .05f && fx.reports.back().tracers == 1);
        const auto tracer = fx.tracers[0];
        CHECK(tracer.forward.y == -1 && tracer.forward.z == 0 && tracer.expires == .1);
        const auto& smoke = g.server.weapon_sprites.reports.back().settings.smoke;
        CHECK(smoke && smoke->duration == 1.5f && smoke->alpha == .5f && smoke->scale1.x == .05f &&
              smoke->velocity.z == 2);
        fx.advance(.1, g.server);
        CHECK(fx.tracers.size() == 1 && fx.tracers[0].center.y == tracer.center.y);
        fx.advance(.101, g.server);
        CHECK(fx.tracers.empty());
        fx.clear();
        s.source.y = 80; // Ray hit too close: deliberately no tracer.
        CHECK(publish(g, s, owner));
        CHECK(fx.tracers.empty());
        fx.clear();
        s.ammo = AMMO_BMG;
        s.eject = false;
        CHECK(publish(g, s, owner));
        CHECK(fx.casings.empty() && fx.tracers.empty() && fx.reports.back().smoke == 0 &&
              fx.sprites[0].scale.x == .035f);
        for (auto type : {WEAP_BERETTA, WEAP_SUBMACHINEGUN, WEAP_ASSAULTRIFLE, WEAP_SNIPERRIFLE, WEAP_MINIGUN})
        {
            BallisticWeapon weapon(type);
            weapon.BeginFire(false);
            CHECK(weapon.eject_pending == (weapon.data.m_nAmmoType == AMMO_BULLET));
        }
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        auto& inventory = player->m_InventoryMgr;
        for (auto type : {WEAP_BERETTA, WEAP_SUBMACHINEGUN, WEAP_ASSAULTRIFLE, WEAP_SNIPERRIFLE, WEAP_MINIGUN})
            for (bool left : {false, true})
            {
                fx.clear();
                BallisticWeapon gun(type);
                gun.Init(player->m_hObject, &inventory, left);
                inventory.SetAmmoCount(gun.GetAmmoType(), 100);
                WeaponFire request{{500, 300, 0}, {0, -1, 0}, 42, type, false, left};
                CHECK(gun.Fire(request) == FireResult::Fired);
                const bool effects = type != WEAP_MINIGUN;
                CHECK(!fx.reports.empty() == effects);
                CHECK(fx.casings.size() == unsigned(effects && gun.GetAmmoType() == AMMO_BULLET));
                if (effects)
                {
                    CHECK(fx.reports[0].settings.eject == (gun.GetAmmoType() == AMMO_BULLET));
                    CHECK(fx.reports[0].settings.left == left);
                }
                fx.clear();
                BallisticWeapon miss(type);
                miss.Init(player->m_hObject, &inventory, left);
                request.direction = {0, 1, 0};
                CHECK(miss.Fire(request) == FireResult::Fired && fx.reports.empty());
            }
        s.ammo = AMMO_BULLET;
        s.eject = true;
        s.normal = {0, 0, -1};
        g.world.models.push_back(test_brush::box("water", {-20, 50, -20}, {20, 110, 120}));
        CHECK(g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                               {{"Name", std::string("water")}, {"ShowSurface", false}}));
        CHECK(publish(g, s, owner));
        CHECK(fx.reports.back().bubbles == 2 && g.server.sparks.reports.back().settings.count == 4);
        g.server.reset_world_time();
        CHECK(fx.casings.empty() && fx.tracers.empty());
        return true;
    }
    bool lifecycle(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        auto s = recipe();
        s.eject = true;
        CHECK(publish(g, s, owner));
        auto& fx = g.server.shotgun_effects;
        const auto old = fx.casings[0];
        fx.advance(.1, g.server);
        CHECK(fx.casings[0].visible &&
              std::abs(fx.casings[0].position.y - (old.position.y + old.velocity.y * .1f)) < .001f);
        CHECK(std::abs(fx.casings[0].velocity.y - (old.velocity.y - 25)) < .001f);
        for (int i = 6; i <= 1000; ++i)
            fx.advance(i * .02, g.server);
        CHECK(fx.casings.size() == 2 && fx.casings[0].resting && fx.casings[0].impacts >= 1 &&
              fx.casings[0].impacts <= 2 && fx.casings[0].sound_error.empty());
        fx.advance(20.01, g.server);
        CHECK(fx.casings.empty());
        CHECK(std::none_of(fx.sprites.begin(), fx.sprites.end(),
                           [](const auto& f) { return f.kind != ShotgunSpriteKind::Mark; }));
        fx.clear();
        for (int i = 0; i < 60; ++i)
            CHECK(publish(g, s, owner));
        CHECK(fx.casings.size() == 100 && fx.reports.size() == 60);
        const auto id = fx.casings.back().id;
        g.server.reset_world_time();
        CHECK(fx.casings.empty() && fx.sprites.empty() && fx.reports.empty());
        CHECK(publish(g, s, owner));
        CHECK(fx.casings[0].id > id);
        return true;
    }
    bool liquids(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        auto s = recipe();
        s.eject = true;
        g.world.models.push_back(test_brush::box("water", {-20, 50, -20}, {20, 110, 120}));
        CHECK(g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                               {{"Name", std::string("water")}, {"ShowSurface", false}}));
        CHECK(publish(g, s, owner));
        auto& fx = g.server.shotgun_effects;
        CHECK(fx.reports.back().bubbles == 2 && fx.reports.back().smoke == 0 && fx.casings[0].liquid);
        s.surface = SURFTYPE_LIQUID;
        s.color1 = {10, 20, 30};
        s.color2 = {40, 50, 60};
        CHECK(publish(g, s, owner));
        CHECK(fx.reports.back().splashes == 1 && fx.reports.back().smoke && fx.splashes[0].particles.size() == 4);
        fx.advance(.01, g.server);
        CHECK(fx.splashes[0].particles.size() == 8);
        for (int i = 2; i <= 120; ++i)
            fx.advance(i * .01, g.server);
        CHECK(fx.splashes.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, true));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {});
        auto s = recipe();
        s.eject = true;
        std::string error;
        CHECK(g.server.PublishShotgunEffects(owner, s, error) && !error.empty());
        CHECK(g.server.shotgun_effects.casings.empty() && g.server.shotgun_effects.reports.back().fragments > 0);
        s.normal = {};
        CHECK(g.server.PublishShotgunEffects(owner, s, error) && error == "Invalid ballistic effect payload");
        CHECK(g.server.shotgun_effects.failed == 2);
        return true;
    }
    bool retail(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        CHECK(g.request_world("01_subwaya", error) && g.enter_player(error) && test_camera::complete_subway_intro(g));
        for (auto type : {WEAP_BERETTA, WEAP_SUBMACHINEGUN, WEAP_SNIPERRIFLE, WEAP_ASSAULTRIFLE, WEAP_MINIGUN})
            for (bool left : {false, true})
            {
                const auto* data = weapon_definition(type);
                if (left && (!data->m_szLeftViewModelFilename || !data->m_szLeftViewModelFilename[0]))
                    continue;
                CViewWeapon check;
                CHECK(check.Create(type, left, g.models, g.resources, error));
                CHECK(check.flash_error.empty());
                CHECK(bool(check.flashes[0]) == bool(data->m_szFlashSprite && data->m_szFlashSprite[0]));
                CHECK(bool(check.flashes[1]) == bool(data->m_szAltFlashSprite && data->m_szAltFlashSprite[0]));
            }
        auto* player = g.server_shell.Player(g.local_client);
        auto& inv = player->m_InventoryMgr;
        CHECK(inv.ObtainWeapon(WEAP_SHOTGUN, 3) == InventoryResult::Ok);
        inv.SetAmmoCount(AMMO_SHELL, 30);
        CHECK(g.select_weapon(3, error));
        auto& view = g.client_shell.weapons[0];
        CHECK(view.flash_error.empty() && view.flashes[0] && view.flashes[1]);
        auto* gun = inv.CurrentWeapon();
        auto shoot = [&](bool alternate)
        {
            const auto shots = gun->shots;
            g.client_shell.weapon_input = {};
            g.client_shell.weapon_input.primary = !alternate;
            g.client_shell.weapon_input.alternate = alternate;
            for (int i = 0; i < 300 && gun->shots == shots; ++i)
                CHECK(g.step(1.0 / 60));
            CHECK(gun->shots == shots + 1 && gun->last_pellets == 16);
            g.client_shell.weapon_input = {};
            return true;
        };
        CHECK(shoot(false));
        CHECK(view.flash_visible == 1 && inv.GetAmmoCount(AMMO_SHELL) == 29);
        CHECK(g.server.shotgun_effects.casings.empty());
        SpriteView sprites;
        CHECK(update_sprite_scene(g, sprites, error));
        CHECK(std::any_of(sprites.entries.begin(), sprites.entries.end(),
                          [](const auto& e) { return e.frame_path.find("shotgun") != std::string::npos; }));
        for (int i = 0; i < 180; ++i)
            CHECK(g.step(1.0 / 60));
        CHECK(view.flash_visible == 0 && view.second_barrel);
        CHECK(shoot(true)); // Second barrel converts alternate to primary.
        CHECK(view.flash_visible == 1 && inv.GetAmmoCount(AMMO_SHELL) == 28);
        for (int i = 0; i < 240; ++i)
            CHECK(g.step(1.0 / 60));
        CHECK(!view.second_barrel && view.animation_sound_error.empty());
        CHECK(shoot(true));
        CHECK(view.flash_visible == 2 && inv.GetAmmoCount(AMMO_SHELL) == 26);
        CHECK(g.server.shotgun_effects.casings.size() == 2);
        // A tracked right hand moves the flash with the same arm/model pose; no headset required.
        std::array<ViewWeaponCamera, 2> cameras;
        ViewArmPlacement placement;
        placement.grip = {{0, 0, 0}, {}};
        placement.scale = view_model_meters * 48;
        placement.shoulders = {DVector{-.17f * 48, .25f * 48, -.3f * 48}, DVector{.17f * 48, .25f * 48, -.3f * 48}};
        placement.poles = {DVector{-.45f, -1, -.35f}, DVector{.45f, -1, -.35f}};
        cameras[1].arms = placement;
        CHECK(update_sprite_scene(g, sprites, error, nullptr, &cameras));
        auto muzzle_draw = [&](const SpriteView& scene) -> DVector
        {
            for (const auto& entry : scene.entries)
                if (entry.frame_path.find("shotgun") != std::string::npos && entry.range != UINT32_MAX)
                    for (const auto& draw : scene.draws)
                        if (draw.range == entry.range)
                            return draw.position;
            return {1e6f, 1e6f, 1e6f};
        };
        const auto before = muzzle_draw(sprites);
        CHECK(before.x < 1e5f);
        cameras[1].arms->grip.position.x += 10;
        CHECK(update_sprite_scene(g, sprites, error, nullptr, &cameras));
        CHECK(std::abs(muzzle_draw(sprites).x - before.x - 10) < .001f);
        // Every authored material asset is resolved, not merely indexed.
        auto effect = recipe();
        for (auto material :
             {SURFTYPE_WOOD, SURFTYPE_STONE, SURFTYPE_METAL, SURFTYPE_PLASTIC, SURFTYPE_CLOTH, SURFTYPE_LIQUID})
        {
            effect.surface = material;
            CHECK(publish(g, effect, player->m_hObject));
        }
        for (const auto& r : g.server.shotgun_effects.reports)
        {
            if (!r.error.empty())
                std::cerr << r.error << '\n';
            CHECK(r.error.empty());
        }
        CHECK(g.unload_world());
        CHECK(g.server.shotgun_effects.casings.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    return (mode == "recipes"      ? recipes(argv[2])
            : mode == "ballistics" ? ballistics(argv[2])
            : mode == "lifecycle"  ? lifecycle(argv[2])
            : mode == "liquids"    ? liquids(argv[2])
            : mode == "failures"   ? failures(argv[2])
            : mode == "retail"     ? retail(argv[2])
                                   : false)
               ? 0
               : 1;
}
