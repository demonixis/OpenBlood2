#include "app/movement_controls.h"
#include "brush_fixture.h"
#include "game/original/CultistAI.h"
#include "proximity_fixture.h"
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool advance(GameSession& g, double t)
    {
        return g.objects.advance(t) && g.objects.finish_frame();
    }
    void place(GameSession& g, HOBJECT h, DVector p)
    {
        g.server.TeleportObject(h, &p);
    }
    struct OwnerProbe : AI_Mgr
    {
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_INITIALUPDATE)
                m_damage.Init(m_hObject);
            return BaseClass::EngineMessageFn(m, d, f);
        }
    };
    bool ai_delay(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        static const ClassInfo type{"BombAIProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<OwnerProbe>(); },
                                    {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct state;
        state.m_Pos = {1000, 300, 0};
        auto owner = g.objects.create(&type, state);
        CHECK(owner);
        place(g, g.client_shell.player, {0, 300, 225});
        auto* b = test_proximity::bomb(g, {0, 300, 0}, {0, 0, 1}, 0, owner);
        CHECK(b);
        b->HandleContact({{}, {0, 1, 0}, nullptr, 0});
        CHECK(advance(g, 1.99) && b->get_away == 2 && !b->m_bExplode);
        CHECK(advance(g, .02) && b->m_bExplode);
        auto child = b->m_hObject;
        CHECK(g.objects.remove(owner) && g.objects.remove(g.client_shell.player) && g.objects.finish_frame());
        CHECK(g.objects.get(child) && b->m_hFiredFrom == nullptr);
        CHECK(advance(g, .51) && !g.objects.get(child) && g.server.projectile_reports.back().outcome == "exploded");
        return true;
    }
    struct RemovingCharacter : CBaseCharacter
    {
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_INITIALUPDATE)
            {
                m_damage.Init(m_hObject);
                m_damage.SetHitPoints(1000);
            }
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD read) override
        {
            auto result = BaseClass::ObjectMessageFn(sender, m, read);
            if (m == damage_message)
            {
                g_pServerDE->RemoveObject(sender);
                g_pServerDE->RemoveObject(m_hObject);
            }
            return result;
        }
    };
    bool reentrant(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* b = test_proximity::bomb(g, {0, 300, 0});
        CHECK(b);
        const auto h = b->m_hObject;
        static const ClassInfo type{"BombRemovalProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass>
                                    { return std::make_unique<RemovingCharacter>(); },
                                    {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct state;
        state.m_Pos = {0, 300, 0};
        const auto target = g.objects.create(&type, state);
        CHECK(target);
        b->m_bExplode = b->hit_character = true;
        CHECK(advance(g, .002) && !g.objects.get(h) && !g.objects.get(target));
        CHECK(!g.server.projectile_reports.empty() && g.server.projectile_reports.back().cancelled);
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* b = test_proximity::bomb(g, {0, 300, 0});
        CHECK(b);
        auto h = b->m_hObject;
        place(g, g.client_shell.player, {1000, 300, 0});
        CHECK(advance(g, 11) && g.objects.get(h)); // override has no base ten-second fuse
        b->HandleContact({{0, 300, 0}, {0, 1, 0}, nullptr, 0});
        CHECK(b->armed && !(g.server.GetObjectFlags(h) & FLAG_GRAVITY) && b->sounds == 1);
        CHECK(advance(g, .749) && b->scans == 0);
        CHECK(advance(g, .002) && b->scans == 1 && b->sounds == 2 && !b->m_bExplode);
        place(g, g.client_shell.player, {225.01f, 300, 0});
        CHECK(advance(g, .01) && !b->m_bExplode);
        place(g, g.client_shell.player, {225, 300, 0});
        CHECK(advance(g, .01) && b->m_bExplode && b->detected == g.client_shell.player);
        CHECK(advance(g, .49) && g.objects.get(h) && b->sounds == 3);
        CHECK(advance(g, .02) && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().proximity && g.server.projectile_reports.back().outcome == "exploded");
        CHECK(g.server.scene_explosions.back().damage == 100 && g.server.scene_explosions.back().radius == 250);
        return true;
    }
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* b = test_proximity::bomb(g, {0, 300, 0});
        CHECK(b);
        b->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(!b->armed && !b->m_bExplode);
        auto dummy = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(dummy);
        b->HandleContact({{}, {0, 1, 0}, dummy});
        CHECK(!b->armed);
        g.server.SetObjectFlags(dummy, FLAG_SOLID);
        b->HandleContact({{}, {1, 0, 0}, dummy});
        CHECK(b->armed && !b->hit_character);
        auto rot = g.objects.get(b->m_hObject)->state.m_Rotation;
        const auto up = rotate_vector(rot, {0, 1, 0});
        CHECK(std::abs(up.x - 1) < .001f);
        auto* c = test_proximity::bomb(g, {0, 400, 0}, {0, 0, 1}, 0, dummy);
        CHECK(c);
        c->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(c->m_bExplode && c->hit_character);
        auto h = c->m_hObject;
        CHECK(advance(g, .002) && !g.objects.get(h));
        // Dead characters do not stick/detonate.
        auto* d = test_proximity::bomb(g, {0, 500, 0}, {0, 0, 1}, 0, dummy);
        CHECK(d);
        g.server_shell.Player(g.local_client)->m_damage.m_bDead = true;
        d->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(!d->armed && !d->m_bExplode);
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* b = test_proximity::bomb(g, {0, 120, 0}, {0, -1, 0}, 100);
        CHECK(b);
        auto h = b->m_hObject;
        for (int i = 0; i < 100 && !b->armed; ++i)
            CHECK(g.server.advance_projectiles(.01));
        CHECK(b->armed && b->contacts == 1 && b->flight_steps > 0);
        auto p = g.objects.get(h)->state.m_Pos;
        CHECK(g.server.advance_projectiles(.4));
        auto q = g.objects.get(h)->state.m_Pos;
        CHECK(p.x == q.x && p.y == q.y && p.z == q.z);
        auto* bad = test_proximity::bomb(g, {100, 300, 0});
        CHECK(bad && !bad->AdvanceFlight(.6) && bad->failed);
        return true;
    }
    bool damage_lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* b = test_proximity::bomb(g, {0, 300, 0});
        CHECK(b);
        auto h = b->m_hObject;
        b->m_damage.m_bDead = true;
        b->m_damage.m_fHitPoints = 0;
        CHECK(advance(g, .2) && g.objects.get(h));
        CHECK(advance(g, .06) && g.objects.get(h));
        CHECK(advance(g, .002) && !g.objects.get(h)); // source subclass before aggregate decrement
        auto* c = test_proximity::bomb(g, {0, 300, 0});
        CHECK(c);
        auto child = c->m_hObject;
        CHECK(g.leave_player());
        CHECK(g.objects.get(child) && c->m_hFiredFrom == nullptr);
        CHECK(advance(g, 12) && g.objects.get(child));
        CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.list_count() == 0);
        return true;
    }
    bool inventory(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto& inv = p->m_InventoryMgr;
        for (int i = 0; i < 11; ++i)
        {
            auto h = g.objects.create(g.objects.find_class("ProximitiesPU"), {}, Creation::World,
                                      {{"Name", std::string("BombPickup")}});
            CHECK(h);
            auto* pu = dynamic_cast<ProximitiesPU*>(g.server.HandleToObject(h));
            CHECK(pu && pu->pickup_sound == "sounds/powerups/ammo2.wav");
            CHECK(g.server.GetObjectUserFlags(h) & (1u << 7));
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p->m_hObject));
            CHECK(pu->picked_up == (i < 10));
            CHECK(g.objects.finish_frame());
        }
        CHECK(inv.GetWeapon(10) && inv.GetAmmoCount(10) == 10 && inv.CurrentWeapon()->data.m_nType != 49);
        CWeapon* best = nullptr;
        CHECK(inv.FindBestWeapon(500, best) && (!best || best->data.m_nType != WEAP_PROXIMITYBOMB));
        g.client_shell.proximity_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell));
        CHECK(inv.m_nCurWeaponSlot == 10);
        std::string e;
        CHECK(g.client_shell.SyncWeapons(g.models, g.resources, g.server_shell, e));
        auto* w = inv.CurrentWeapon();
        auto hp = p->m_damage.m_fHitPoints;
        WeaponFire request{{0, 200, 0}, {0, 0, 1}, 2, 49, false, false, 0};
        CHECK(w->Fire(request) == FireResult::Fired && !w->projectile && inv.GetAmmoCount(10) == 9 &&
              p->m_damage.m_fHitPoints < hp);
        p->m_damage.m_bDead = false;
        p->m_damage.SetHitPoints(1000);
        p->m_bDead = false;
        request.projectile_speed = std::numeric_limits<float>::quiet_NaN();
        CHECK(w->Fire(request) == FireResult::Invalid && inv.GetAmmoCount(10) == 9);
        request.projectile_speed = 250;
        request.alternate = true;
        CHECK(w->Fire(request) == FireResult::Fired);
        auto* b = dynamic_cast<CProximityBomb*>(g.server.HandleToObject(w->projectile));
        CHECK(b && b->m_vDir.y == -1 && b->m_fVelocity == 0);
        inv.SetAmmoCount(10, 1);
        request.alternate = false;
        CHECK(w->Fire(request) == FireResult::Fired && inv.GetAmmoCount(10) == 0 && inv.m_nCurWeaponSlot == 1);
        CHECK(inv.SelectInventoryWeapon(49) == InventoryResult::NotAvailable);
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(inv.AddInventoryWeapon(7, 1) == InventoryResult::Invalid &&
              inv.SelectInventoryWeapon(49) == InventoryResult::Invalid &&
              inv.SelectNextWeapon() == InventoryResult::Invalid && inv.GetAmmoCount(10) == 0);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_proximity::setup(g, root) && test_proximity::equip(g));
        PlayerRestoreState snapshot;
        std::string error;
        CHECK(g.capture_player(snapshot, error));
        CHECK(snapshot.inventory.right[10] == 49 && snapshot.inventory.current_slot == 10 &&
              snapshot.inventory.ammo[10] == 3);
        CHECK(g.leave_player() && g.restore_player(snapshot, error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p && p->m_InventoryMgr.CurrentWeapon()->data.m_nType == 49);
        CHECK(g.capture_player(snapshot, error));
        auto bad = snapshot;
        bad.inventory.right[11] = 49;
        CHECK(g.leave_player() && !g.restore_player(bad, error) && g.restore_player(snapshot, error));
        CHECK(g.capture_player(snapshot, error));
        CHECK(g.leave_player() && g.restore_player(snapshot, error, PlayerRestoreMode::KeepAlive));
        CHECK(g.server_shell.Player(g.local_client)->m_InventoryMgr.GetWeapon(10)->data.m_nType == 49 &&
              g.server_shell.Player(g.local_client)->m_InventoryMgr.GetAmmoCount(10) == 3);
        MovementControls c;
        c.focused = true;
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = 1;
        e.key.scancode = SDL_SCANCODE_P;
        c.event(e, 1);
        CHECK(c.sample_proximity(false) && !c.sample_proximity(true) && !c.sample_proximity(false));
        c.player_changed();
        CHECK(!c.sample_proximity(false));
        return true;
    }
    bool private_world(const char* path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", e));
            unsigned made = 0, unknown = 0, failed = 0, pickups = 0;
            unsigned omitted = 0;
            std::vector<HOBJECT> authored;
            for (const auto& row : g.entries)
            {
                made += row.state == EntryState::Created;
                unknown += row.state == EntryState::Unsupported;
                failed += row.state == EntryState::Failed;
                omitted += row.state == EntryState::MissingWorldModel;
            }
            CHECK(made == 453 && unknown == 0 && failed == 0 && omitted == 1);
            for (auto id : g.objects.snapshot())
                if (auto* pu = dynamic_cast<ProximitiesPU*>(g.objects.get(id)->game.get()))
                {
                    ++pickups;
                    authored.push_back(pu->m_hObject);
                    CHECK(pu->error.empty() && g.objects.get(id)->model);
                }
            CHECK(pickups == 2);
            std::cout << "CityHub cycle=" << cycle << " created=" << made << " unknown=" << unknown
                      << " failed=" << failed << " proximity=" << pickups << '\n';
            g.server_shell.start_point_name = "01_egressa";
            CHECK(g.enter_player(e));
            auto* p = g.server_shell.Player(g.local_client);
            for (auto h : authored)
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p->m_hObject));
            CHECK(p->m_InventoryMgr.GetAmmoCount(10) == 2);
            g.client_shell.proximity_down = true;
            CHECK(g.client_shell.UpdateUse(g.server_shell));
            CHECK(p->m_InventoryMgr.m_nCurWeaponSlot == 10 &&
                  g.client_shell.SyncWeapons(g.models, g.resources, g.server_shell, e));
            CHECK(g.objects.finish_frame());
            for (auto h : authored)
                CHECK(!g.objects.get(h));
            CHECK(g.unload_world());
        }
        return true;
    }
    bool private_fire(const char* path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        CHECK(g.request_world("01_subwaya", e) && g.enter_player(e) && test_camera::complete_subway_intro(g));
        CHECK(test_proximity::equip(g, 5));
        auto* p = g.server_shell.Player(g.local_client);
        p->m_damage.m_bGodMode = true;
        place(g, p->m_hObject, {0, 800, 0});
        auto* w = p->m_InventoryMgr.GetWeapon(10);
        auto step = [&](unsigned n)
        {
            for (unsigned i = 0; i < n; ++i)
                if (!g.step(.01))
                {
                    std::cerr << e << '\n';
                    return false;
                }
            return true;
        };
        CHECK(step(120));
        g.client_shell.weapon_input.primary = true;
        CHECK(step(40));
        g.client_shell.weapon_input.primary = false;
        CHECK(step(90));
        CHECK(w->shots == 1 && p->m_InventoryMgr.GetAmmoCount(10) == 4 && w->projectile);
        auto* b = dynamic_cast<CProximityBomb*>(g.server.HandleToObject(w->projectile));
        CHECK(b && b->m_fVelocity >= 250 && b->m_fVelocity <= 1750);
        std::cout << "Actual bomb throw speed=" << b->m_fVelocity << " keys=" << g.client_shell.weapons[0].fire_keys
                  << '\n';
        g.client_shell.weapon_input.alternate = true;
        CHECK(step(100));
        g.client_shell.weapon_input.alternate = false;
        CHECK(step(100));
        CHECK(w->shots >= 2);
        CHECK(w->sound_failures == 0);
        g.client_shell.weapon_input.primary = true;
        const auto old = w->shots;
        CHECK(step(220));
        g.client_shell.weapon_input.primary = false;
        CHECK(step(40));
        CHECK(w->shots == old + 1 && w->last_shot.request.projectile_speed == 0 && !w->projectile);
        std::cout << "Original bomb sequence shots=" << w->shots << " remaining=" << p->m_InventoryMgr.GetAmmoCount(10)
                  << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    std::filesystem::path root = argv[2];
    if (mode != "private" && mode != "private_fire")
        std::filesystem::create_directories(root);
    return (mode == "reentrant"   ? reentrant(root)
            : mode == "ai_delay"  ? ai_delay(root)
            : mode == "rules"     ? rules(root)
            : mode == "contacts"  ? contacts(root)
            : mode == "motion"    ? motion(root)
            : mode == "lifetime"  ? damage_lifetime(root)
            : mode == "inventory" ? inventory(root)
            : mode == "restore"   ? restore(root)
            : mode == "private"   ? private_world(argv[2])
                                  : mode == "private_fire" && private_fire(argv[2]))
               ? 0
               : 1;
}
