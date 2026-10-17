#include "brush_fixture.h"
#include "camera_fixture.h"
#include "flare_fixture.h"
#include "game/original/CultistAI.h"
#include "game/original/HandWeaponModel.h"
#include <functional>
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
        return std::abs(a - b) < .002f;
    }
    struct Target : CBaseCharacter
    {
        HOBJECT remove_on_damage = nullptr;
        unsigned touches = 0;
        DDWORD EngineMessageFn(DDWORD m, void* d, float f) override
        {
            if (m == MID_INITIALUPDATE)
            {
                m_damage.Init(m_hObject);
                m_damage.m_fHitPoints = m_damage.m_fMaxHitPoints = 1000;
                m_damage.apply_damage_physics = false;
            }
            if (m == MID_TOUCHNOTIFY)
                ++touches;
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT s, DDWORD m, HMESSAGEREAD r) override
        {
            auto result = BaseClass::ObjectMessageFn(s, m, r);
            if (m == damage_message && remove_on_damage)
                GetServerDE()->RemoveObject(remove_on_damage);
            return result;
        }
    };
    Target* target(GameSession& g, DVector position)
    {
        static ClassInfo type{"FlareCharacterProbe",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                              {}};
        type.parent = g.objects.find_class("CBaseCharacter");
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = position;
        state.m_Flags = FLAG_SOLID | FLAG_TOUCH_NOTIFY;
        auto h = g.objects.create(&type, state);
        if (auto* r = g.objects.get(h))
            r->dimensions = {2, 2, 2};
        return dynamic_cast<Target*>(g.server.HandleToObject(h));
    }
    bool primary(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        auto* victim = target(g, {0, 50, 100});
        CHECK(owner && victim);
        auto* p = test_flare::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.objects.get(h)->state.m_Scale.x == 0);
        CHECK(g.objects.advance(.005) && p->sound && g.objects.get(h)->state.m_Scale.x == 0);
        auto oldsound = p->sound;
        CHECK(g.server.advance_projectiles(.05) && p->tracking && p->contacted && !p->m_bExplode &&
              victim->touches == 1);
        CHECK(!p->sound && !g.server.audio.get(oldsound) && p->m_fLifeTime == 5 && near(p->offset.z, -2));
        CHECK(!(g.objects.get(h)->state.m_Flags & FLAG_TOUCH_NOTIFY));
        CHECK(p->AdvanceFlight(.05) && victim->touches == 1 && near(g.objects.get(h)->state.m_Pos.z, 98));
        DVector moved{30, 50, 110};
        CHECK(g.server.TeleportObject(victim->m_hObject, &moved) == DE_OK);
        DRotation rot;
        CHECK(g.server.AlignRotation(rot, {1, 0, 0}) == DE_OK &&
              g.server.SetObjectRotation(victim->m_hObject, &rot) == DE_OK);
        CHECK(g.objects.advance(.01) && p->sound && p->sound != oldsound && near(g.objects.get(h)->state.m_Pos.x, 30) &&
              near(g.objects.get(h)->state.m_Pos.z, 108));
        CHECK(g.objects.advance(1 - g.objects.time()) && p->ticks == 0);
        CHECK(g.objects.advance(.002) && p->ticks == 1 && victim->m_damage.m_fHitPoints == 980);
        CHECK(victim->m_damage.last_damage.type == DamageType::Fire &&
              victim->m_damage.last_damage.attacker == owner->m_hObject &&
              vector_dot(victim->m_damage.last_damage.direction, victim->m_damage.last_damage.direction) == 0);
        auto tick = p->last_damage_time;
        CHECK(g.objects.advance(double(tick + 1) - g.objects.time()) && p->ticks == 1);
        CHECK(g.objects.advance(.002) && p->ticks == 2);
        victim->m_damage.m_bDead = true;
        CHECK(g.objects.advance(.002) && p->m_bExplode && g.objects.get(h)->life == Life::Alive);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().burn_ticks == 2 && g.server.projectile_reports.back().tracked);
        CHECK(g.server.scene_explosions.back().type == 13 && g.server.scene_explosions.back().voice);
        auto* brush = test_energy::actor(g, {300, 50, 100});
        CHECK(brush);
        p = test_flare::projectile(g, owner->m_hObject, {300, 50, 90});
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.server.advance_projectiles(.01) && p->m_bExplode && !p->tracking);
        CHECK(g.objects.advance(.002) && !g.objects.get(h));
        CHECK(brush->damage.last_damage.type == DamageType::Fire && brush->damage.last_damage.area_effect &&
              brush->damage.m_fHitPoints < 1000);
        p = test_flare::projectile(g, owner->m_hObject, {500, 50, 0});
        CHECK(p);
        h = p->m_hObject;
        SegmentHit sky;
        sky.polygon = 0;
        sky.texture_flags = 110;
        p->HandleContact(sky);
        CHECK(g.objects.get(h)->life == Life::Alive && p->m_bExplode); // Override has no base SKY discard.
        CHECK(g.objects.advance(.002) && !g.objects.get(h) && g.server.projectile_reports.back().outcome == "exploded");
        CHECK(g.unload_world() && g.server.audio.voices().empty() && CDestructable::m_dwNumDestructables == 0);
        std::cout << "Primary: contact/loop restart, world-offset following, strict periodic FIRE damage and "
                     "next-update death fizzle verified.\n";
        return true;
    }
    bool alternate(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, -100});
        CHECK(owner);
        auto* p = test_flare::projectile<CFlareAltProjectile>(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.objects.advance(.005) && !p->get_velocity && p->cached_velocity.z == 2000);
        CHECK(g.objects.advance(.095) && p->children == 0); // Source float > .1 boundary.
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().burst_children == 8 &&
              g.server.projectile_reports.back().delivered == 0);
        CHECK(g.server.scene_explosions.back().type == 15 && g.server.scene_explosions.back().voice);
        const float offsets[8][2]{{.135f, 0},    {-.135f, 0},   {0, -.135f},    {0, .135f},
                                  {.06f, -.06f}, {-.06f, .06f}, {-.06f, -.06f}, {.06f, .06f}};
        unsigned n = 0;
        for (auto id : g.objects.snapshot())
            if (auto* c = dynamic_cast<CFlareBurstProjectile*>(g.objects.get(id)->game.get()))
            {
                CHECK(n < 8 && c->m_fDamage == 10 && c->m_nRadius == 100 && c->m_fVelocity == 2000 && c->color_ready &&
                      c->m_damage.m_bGodMode);
                CHECK(near(c->m_vDir.x / c->m_vDir.z, offsets[n][0]) && near(c->m_vDir.y / c->m_vDir.z, offsets[n][1]));
                CHECK(c->m_hFiredFrom == owner->m_hObject);
                ++n;
            }
        CHECK(n == 8);
        CHECK(g.objects.remove(owner->m_hObject) && g.objects.finish_frame());
        CHECK(g.objects.advance(.002));
        for (auto id : g.objects.snapshot())
            if (auto* c = dynamic_cast<CFlareBurstProjectile*>(g.objects.get(id)->game.get()))
                CHECK(!c->m_hFiredFrom && g.objects.get(id)->state.m_Scale.x == .25f);
        for (auto id : g.objects.snapshot())
            if (dynamic_cast<CFlareBurstProjectile*>(g.objects.get(id)->game.get()))
                CHECK(g.objects.remove(g.objects.get(id)->handle));
        CHECK(g.objects.finish_frame());
        // Contact before burst explodes as FRAG. Contact on the burst update emits FRAG then BURST.
        owner = test_energy::actor(g, {0, 50, -100});
        CHECK(owner);
        p = test_flare::projectile<CFlareAltProjectile>(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        CHECK(g.objects.advance(.002));
        SegmentHit wall;
        wall.polygon = 0;
        wall.point = {0, 50, 0};
        p->HandleContact(wall);
        CHECK(g.objects.advance(.002) && g.server.projectile_reports.back().outcome == "exploded" &&
              g.server.projectile_reports.back().burst_children == 0);
        CHECK(g.server.scene_explosions.back().type == 15 && g.objects.finish_frame());
        p = test_flare::projectile<CFlareAltProjectile>(g, owner->m_hObject, {300, 50, 0});
        CHECK(p);
        CHECK(g.objects.advance(.002));
        p->HandleContact(wall);
        CHECK(g.objects.advance(.11) && g.server.projectile_reports.back().burst_children == 8 &&
              g.server.projectile_reports.back().outcome == "exploded");
        auto& reports = g.server.scene_explosions;
        CHECK(reports.size() >= 3 && reports[reports.size() - 2].type == 15 && reports.back().type == 14);
        CHECK(g.unload_world());
        std::cout << "Alternate: strict burst boundary, eight source directions/half damage, independent children and "
                     "FRAG-before-BURST ordering verified.\n";
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        auto* victim = target(g, {0, 50, 100});
        CHECK(owner && victim);
        auto* p = test_flare::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        auto h = p->m_hObject;
        CHECK(g.server.advance_projectiles(.01) && p->tracking);
        CHECK(g.objects.remove(owner->m_hObject) && g.objects.finish_frame() && !p->m_hFiredFrom);
        CHECK(g.objects.advance(1.01) && p->ticks == 1 && !victim->m_damage.last_damage.attacker);
        auto sound = p->sound;
        CHECK(g.objects.remove(victim->m_hObject) && g.objects.finish_frame() && !p->m_hHitObject);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h) && !g.server.audio.get(sound) &&
              g.server.scene_explosions.empty());
        owner = test_energy::actor(g, {0, 50, 0});
        victim = target(g, {0, 50, 100});
        CHECK(owner && victim);
        p = test_flare::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.server.advance_projectiles(.01));
        victim->remove_on_damage = h;
        CHECK(g.objects.advance(.01) && g.objects.finish_frame() && !g.objects.get(h) && g.server.message_count() == 0);
        p = test_flare::projectile(g, owner->m_hObject, {0, 50, 90});
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.server.advance_projectiles(.01));
        victim->remove_on_damage = nullptr;
        const auto expiry = p->track_start + 5;
        CHECK(g.objects.advance(double(expiry) - g.objects.time()) && g.objects.get(h)->life == Life::Alive);
        CHECK(g.objects.advance(.002) && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().outcome == "expired");
        // Alternate child Setup rejects lost shooters explicitly, with no fabricated attacker.
        auto* alt = test_flare::projectile<CFlareAltProjectile>(g, owner->m_hObject, {0, 50, 0});
        CHECK(alt);
        h = alt->m_hObject;
        CHECK(g.objects.advance(.002) && g.objects.remove(owner->m_hObject) && g.objects.finish_frame());
        CHECK(g.objects.advance(.11) && g.server.projectile_reports.back().outcome == "failed" &&
              g.server.projectile_reports.back().burst_children == 0 && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool weapon(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        auto* a = test_energy::actor(g, {0, 50, 0});
        CHECK(a);
        auto& inv = a->inventory;
        CHECK(inv.Init(a->m_hObject, nullptr) && inv.ObtainWeapon(3, 0) == InventoryResult::Ok);
        auto* w = inv.CurrentWeapon();
        CHECK(w && w->data.m_nAmmoUse == 1 && w->data.m_nAltAmmoUse == 8 && w->data.m_fRange == 5000 &&
              w->data.m_fProjVelocity == 2000);
        inv.SetAmmoCount(4, 9);
        WeaponFire f{{0, 50, 0}, {0, 0, 1}, 42, 3, false, false};
        CHECK(w->Fire(f) == FireResult::Fired && inv.GetAmmoCount(4) == 8 &&
              dynamic_cast<CFlareProjectile*>(g.server.HandleToObject(w->projectile)) && w->last_shot.damage >= 10 &&
              w->last_shot.damage <= 15);
        f.alternate = true;
        CHECK(w->Fire(f) == FireResult::Fired && inv.GetAmmoCount(4) == 0 &&
              dynamic_cast<CFlareAltProjectile*>(g.server.HandleToObject(w->projectile)) && w->last_shot.damage >= 20 &&
              w->last_shot.damage <= 30);
        CHECK(w->sounds_played == 2 && !w->sound_failures);
        inv.SetAmmoCount(4, 7);
        CHECK(w->Fire(f) == FireResult::Dry && inv.GetAmmoCount(4) == 7 && w->shots == 2);
        InventoryState state;
        std::string error;
        CHECK(inv.Save(state, error) && state.right[0] == 3);
        auto* b = test_energy::actor(g, {100, 50, 0});
        CHECK(b && b->inventory.Init(b->m_hObject, nullptr) &&
              b->inventory.Load(state, InventoryLoadType::KeepAlive, error));
        CHECK(b->inventory.CurrentWeapon()->data.m_nType == 3 && b->inventory.CurrentWeapon()->shots == 0);
        CHECK(g.enter_player(error) && test_flare::equip(g));
        auto* player = g.server_shell.Player(g.local_client);
        DVector eye;
        CHECK(g.server.GetObjectPos(player->m_hObject, &eye) == DE_OK);
        eye.y += player->eye_level;
        auto* wall = test_energy::actor(g, vector_add(eye, {0, 0, 30}));
        CHECK(wall);
        w = player->m_InventoryMgr.CurrentWeapon();
        f = {vector_add(eye, {0, 0, 40}), {0, 0, 1}, 42, 3, false, false};
        CHECK(w->Fire(f) == FireResult::Fired && near(w->last_shot.request.position.z, eye.z));
        CHECK(near(g.objects.get(w->projectile)->state.m_Pos.z, eye.z));
        CHECK(g.unload_world());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* owner = test_energy::actor(g, {0, 50, 0});
        CHECK(owner);
        CHECK(!test_flare::projectile(g, owner->m_hObject, {0, 50, 0})); // Actual missing SPR fails binding.
        CHECK(g.objects.finish_frame() && test_flare::mount(g, root));
        owner = test_energy::actor(g, {0, 50, 0});
        CHECK(owner);
        CHECK(!g.objects.create(g.objects.find_class("CFlareProjectile"), {}, Creation::World));
        auto* p = test_flare::projectile(g, owner->m_hObject, {0, 50, 0});
        CHECK(p);
        const auto h = p->m_hObject;
        DVector color;
        float alpha = 0;
        CHECK(g.server.GetObjectColor(h, color, alpha) && color.x == 1 && color.y == 1 && color.z == 1 && alpha == 1);
        CHECK(!g.server.SetObjectColor(h, {-1, 1, 1}, 1) && !g.server.SetObjectColor(h, {1, 1, 1}, 2));
        CHECK(g.server.GetObjectColor(h, color, alpha) && color.y == 1 && alpha == 1);
        g.server.collision_ready = false;
        CHECK(!p->AdvanceFlight(.01) && p->failed && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.unload_world());
        return true;
    }
    bool drop(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = {300, 100, 300};
        SAFE_STRCPY(state.m_Filename, "Models/Characters/Caleb.abc");
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* c = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        CHECK(c);
        auto& inv = c->m_InventoryMgr;
        CHECK(inv.Init(h, nullptr) && inv.ObtainWeapon(3, 1) == InventoryResult::Ok &&
              inv.ChangeWeaponSlot(1) == InventoryResult::Ok);
        CHECK(inv.CurrentWeapon()->ShowHandModel(true));
        const auto hand = inv.CurrentWeapon()->m_hHandModel;
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Ok && g.objects.get(hand)->attachment == 0);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && g.objects.get(hand));
        HOBJECT pickup = nullptr;
        for (unsigned i = 0; i < 400 && !pickup; ++i)
        {
            CHECK(g.step(.01));
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<FlareGunPU*>(g.objects.get(id)->game.get()))
                    pickup = g.objects.get(id)->handle;
        }
        CHECK(pickup && !g.objects.get(hand));
        CHECK(dynamic_cast<FlareGunPU*>(g.server.HandleToObject(pickup))->value.amount == 8 &&
              g.server.spawn_reports.back().request == "FlareGunPU");
        CHECK(g.unload_world());
        return true;
    }
    bool collect(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_flare::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        inv.SetAmmoCount(4, 0);
        for (unsigned i = 0; i < 3; ++i)
        {
            auto h = SpawnWeaponPickup(3, g.client_shell.player, {300, 100, 300}, {}, true);
            auto* p = dynamic_cast<FlareGunPU*>(g.server.HandleToObject(h));
            CHECK(p && p->value.amount == 8 && p->resource_id == 355);
            if (i == 2)
                inv.SetAmmoCount(4, inv.MaxAmmo(4));
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && p->picked_up == (i < 2));
            if (i < 2)
                CHECK(inv.GetAmmoCount(4) == 8 * (i + 1));
            CHECK(g.objects.finish_frame());
        }
        CHECK(inv.GetWeapon(2) && inv.GetWeapon(2, true));
        CHECK(g.unload_world());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_subwaya", error) && g.enter_player(error) &&
                  test_camera::complete_subway_intro(g) && test_flare::equip(g));
            auto* player = g.server_shell.Player(g.local_client);
            player->m_damage.m_bGodMode = true;
            auto& inv = player->m_InventoryMgr;
            for (bool left : {false, true})
                for (bool alt : {false, true})
                {
                    g.client_shell.weapon_input = {};
                    for (unsigned i = 0; i < 100; ++i)
                        CHECK(g.step(.01));
                    inv.SetAmmoCount(4, 200);
                    auto* w = inv.CurrentWeapon(left);
                    auto before = w->shots;
                    auto keys = g.client_shell.weapons[left ? 1 : 0].fire_keys;
                    g.client_shell.weapon_input = {!alt, alt, left};
                    for (unsigned i = 0; i < 190; ++i)
                        CHECK(g.step(.01));
                    const auto shots = w->shots - before;
                    std::cout << "Retail Flare left=" << left << " alt=" << alt << " shots=" << shots
                              << " keys=" << g.client_shell.weapons[left ? 1 : 0].fire_keys - keys
                              << " projectile error=" << w->projectile_error << '\n';
                    CHECK(shots > 0 && shots == g.client_shell.weapons[left ? 1 : 0].fire_keys - keys &&
                          inv.GetAmmoCount(4) == 200 - shots * (alt ? 8 : 1));
                    CHECK(w->sound_failures == 0 && w->projectile_error.empty() &&
                          w->last_shot.result == FireResult::Fired);
                }
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.attachments().empty());
        }
        CHECK(g.request_world("02_cityhub", error));
        unsigned created = 0, unknown = 0, failed = 0, flare = 0;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
        {
            auto& e = g.entries[i];
            created += e.state == EntryState::Created;
            unknown += e.state == EntryState::Unsupported;
            failed += e.state == EntryState::Failed;
            if (auto* a =
                    dynamic_cast<CultistAI*>(g.objects.get(e.object) ? g.objects.get(e.object)->game.get() : nullptr);
                a && a->m_InventoryMgr.CurrentWeapon() && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 3)
            {
                ++flare;
                CHECK(a->RuntimeReady());
            }
            if (e.state == EntryState::Failed)
                std::cout << "Failed: " << g.world.objects[i].class_name << '\n';
        }
        std::cout << "CityHub created=" << created << " unsupported=" << unknown << " failed=" << failed
                  << " flare NPCs=" << flare << '\n';
        CHECK(created == 452 && unknown == 1 && failed == 1 && flare == 1);
        CHECK(g.unload_world());
        return true;
    }
    bool ai(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic original flare NPC arena";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
        auto h =
            g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                             {{"Pos", DVector{0, 100, 0}}, {"AIWeapon1", std::string("FLAREGUN")}, {"Flares", 100.f}});
        auto* a = dynamic_cast<CultistAI*>(g.server.HandleToObject(h));
        CHECK(a && a->RuntimeReady());
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        for (unsigned i = 0; i < 600; ++i)
            CHECK(g.step(1.0 / 60));
        std::cout << "Flare NPC updates=" << a->runtime_updates << " shots=" << a->fire_shots
                  << " HP=" << player->m_damage.m_fHitPoints << " error=" << a->runtime_error << '\n';
        CHECK(a->RuntimeReady() && a->fire_shots > 0 && player->m_damage.m_fHitPoints < 100);
        auto* w = a->m_InventoryMgr.CurrentWeapon();
        CHECK(w && w->data.m_nType == 3 && w->projectile_error.empty() && w->m_hHandModel);
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"primary", primary},   {"alternate", alternate},  {"lifetime", lifetime},
        {"weapon", weapon},     {"collect", collect},      {"drop", drop},
        {"failures", failures}, {"private", private_data}, {"ai", ai}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
