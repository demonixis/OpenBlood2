#include "app/movement_controls.h"
#include "remote_fixture.h"
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
    bool advance(GameSession& g, double t)
    {
        return g.objects.advance(t) && g.objects.finish_frame();
    }
    void place(GameSession& g, HOBJECT h, DVector p)
    {
        g.server.TeleportObject(h, &p);
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto* b = test_remote::bomb(g, {0, 300, 0});
        CHECK(b);
        auto h = b->m_hObject;
        b->HandleContact({{}, {0, 1, 0}, nullptr, 0});
        CHECK(b->armed && !b->m_bExplode && !(g.server.GetObjectFlags(h) & FLAG_GRAVITY));
        place(g, p->m_hObject, {0, 300, 10}); // Close living character does not trigger remote bombs.
        CHECK(advance(g, 12) && g.objects.get(h) && !b->m_bExplode && g.server.audio.voices().empty());
        place(g, p->m_hObject, {1000, 300, 0});
        p->camera_movement_blocked = true;
        CHECK(!p->OnCommandDetonate() && !b->m_bExplode);
        p->camera_movement_blocked = false;
        p->command_run = true;
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerDetonate{}));
        CHECK(b->m_bExplode && !b->exploded && !p->remote_bombs && p->remote_signals == 1);
        CHECK(advance(g, .002) && !g.objects.get(h));
        CHECK(g.server.projectile_reports.back().remote && g.server.projectile_reports.back().outcome == "exploded");
        const auto& e = g.server.scene_explosions.back();
        CHECK(e.type == 0 && e.damage == 100 && e.radius == 250 && e.voice && e.sound_error.empty());
        return true;
    }
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto dummy = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(dummy);
        auto* b = test_remote::bomb(g, {0, 300, 0});
        CHECK(b);
        b->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(!b->falling && !b->armed);
        b->HandleContact({{}, {0, 1, 0}, dummy});
        CHECK(!b->armed);
        auto* c = test_remote::bomb(g, {0, 400, 0}, {0, 0, 1}, 100, dummy);
        CHECK(c);
        g.server_shell.Player(g.local_client)->m_damage.m_bDead = true;
        c->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(c->falling && !c->armed && !c->m_bExplode && (g.server.GetObjectFlags(c->m_hObject) & FLAG_GRAVITY));
        DVector velocity;
        CHECK(g.server.GetVelocity(c->m_hObject, &velocity) == DE_OK && vector_dot(velocity, velocity) == 0);
        velocity = {0, -15, 0};
        g.server.SetVelocity(c->m_hObject, &velocity);
        c->HandleContact({{}, {0, 1, 0}, g.client_shell.player});
        CHECK(g.server.GetVelocity(c->m_hObject, &velocity) == DE_OK && velocity.y == -15);
        g.server.SetObjectFlags(dummy, FLAG_SOLID);
        b->HandleContact({{}, {1, 0, 0}, dummy});
        CHECK(b->armed && !b->falling);
        auto up = rotate_vector(g.objects.get(b->m_hObject)->state.m_Rotation, {0, 1, 0});
        CHECK(std::abs(up.x - 1) < .001f);
        c->HandleContact({{}, {0, 1, 0}, nullptr, 0});
        CHECK(c->armed && c->falling);
        CHECK(advance(g, 11) && !c->m_bExplode);
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto* b = test_remote::bomb(g, {0, 120, 0}, {0, -1, 0}, 100);
        CHECK(b);
        for (int i = 0; i < 100 && !b->armed; ++i)
            CHECK(g.server.advance_projectiles(.01));
        CHECK(b->armed && b->contacts == 1 && b->flight_steps > 0);
        auto before = g.objects.get(b->m_hObject)->state.m_Pos;
        CHECK(g.server.advance_projectiles(.4));
        auto after = g.objects.get(b->m_hObject)->state.m_Pos;
        CHECK(before.x == after.x && before.y == after.y && before.z == after.z);
        auto* bad = test_remote::bomb(g, {100, 300, 0});
        CHECK(bad);
        auto h = bad->m_hObject;
        CHECK(!bad->AdvanceFlight(.6) && bad->failed && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server_shell.Player(g.local_client)->remote_bombs->m_nInList == 1);
        return true;
    }
    bool ownership(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        const auto lists = g.server.list_count();
        auto* a = test_remote::bomb(g, {0, 300, 0});
        auto* b = test_remote::bomb(g, {100, 300, 0});
        CHECK(a && b);
        auto first = a->m_hObject, second = b->m_hObject;
        CHECK(p->remote_bombs->m_nInList == 2 && p->remote_bombs->m_pFirstLink->m_hObject == second);
        CHECK(p->AddRemoteBomb(first) && p->remote_bombs->m_nInList == 2);
        CHECK(g.objects.remove(first) && g.objects.finish_frame());
        CHECK(p->remote_bombs->m_nInList == 1);
        CHECK(g.leave_player() && !g.objects.get(second) && g.server.list_count() == lists);
        std::string error;
        CHECK(g.enter_player(error));
        p = g.server_shell.Player(g.local_client);
        auto* c = test_remote::bomb(g, {0, 300, 0});
        CHECK(c);
        auto third = c->m_hObject;
        CHECK(p->Respawn() && g.objects.finish_frame() && !g.objects.get(third) && !p->remote_bombs);
        auto* d = test_remote::bomb(g, {0, 300, 0});
        CHECK(d);
        auto fourth = d->m_hObject;
        CHECK(p->OnCommandDetonate() && !p->remote_bombs && g.leave_player());
        CHECK(g.objects.get(fourth) && d->m_hFiredFrom == nullptr);
        CHECK(advance(g, .002) && !g.objects.get(fourth));
        CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.list_count() == 0);
        return true;
    }
    bool damage(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        place(g, g.client_shell.player, {1000, 300, 0});
        auto* b = test_remote::bomb(g, {0, 300, 0});
        CHECK(b);
        auto h = b->m_hObject;
        b->m_damage.m_bDead = true;
        b->m_damage.m_fHitPoints = 0;
        CHECK(advance(g, .2) && g.objects.get(h));
        CHECK(advance(g, .06) && g.objects.get(h));
        CHECK(advance(g, .002) && !g.objects.get(h));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p->remote_bombs && p->remote_bombs->m_nInList == 0);
        CHECK(p->OnCommandDetonate() && !p->remote_bombs && p->remote_signals == 0);
        return true;
    }
    struct RemovalProbe : CBaseCharacter
    {
        HOBJECT owner = nullptr;
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
                g_pServerDE->RemoveObject(owner);
                g_pServerDE->RemoveObject(sender);
                g_pServerDE->RemoveObject(m_hObject);
            }
            return result;
        }
    };
    bool reentrant(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto* a = test_remote::bomb(g, {0, 300, 0});
        auto* b = test_remote::bomb(g, {1000, 300, 0});
        CHECK(a && b);
        auto ah = a->m_hObject, bh = b->m_hObject;
        static const ClassInfo type{"RemoteRemovalProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<RemovalProbe>(); },
                                    {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct s;
        s.m_Pos = {0, 300, 0};
        auto h = g.objects.create(&type, s);
        CHECK(h);
        dynamic_cast<RemovalProbe*>(g.server.HandleToObject(h))->owner = g.client_shell.player;
        CHECK(a->Detonate() && advance(g, .002));
        CHECK(!g.objects.get(ah) && !g.objects.get(bh) && !g.objects.get(h) && g.server.list_count() == 0);
        CHECK(g.server.projectile_reports.back().cancelled);
        CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool inventory(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto& inv = p->m_InventoryMgr;
        for (int i = 0; i < 11; ++i)
        {
            auto h = g.objects.create(g.objects.find_class("RemotesPU"), {}, Creation::World);
            CHECK(h);
            auto* pu = dynamic_cast<RemotesPU*>(g.server.HandleToObject(h));
            CHECK(pu && pu->resource_id == 307);
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p->m_hObject));
            CHECK(pu->picked_up == (i < 10));
            CHECK(g.objects.finish_frame());
        }
        CHECK(inv.GetWeapon(11) && inv.GetAmmoCount(11) == 10 && !inv.GetWeapon(10));
        p->command_run = true;
        g.client_shell.remote_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && inv.m_nCurWeaponSlot == 11);
        std::string e;
        CHECK(g.client_shell.SyncWeapons(g.models, g.resources, g.server_shell, e));
        auto* w = inv.CurrentWeapon();
        WeaponFire req{{0, 200, 0}, {0, 0, 1}, 2, 50, false, false, 0};
        auto hp = p->m_damage.m_fHitPoints;
        CHECK(w->Fire(req) == FireResult::Fired && w->projectile &&
              p->m_damage.m_fHitPoints == hp); // zero speed is not in-hand detonation
        req.projectile_speed = std::numeric_limits<float>::quiet_NaN();
        CHECK(w->Fire(req) == FireResult::Invalid && inv.GetAmmoCount(11) == 9);
        req.projectile_speed = 250;
        req.alternate = true;
        CHECK(w->Fire(req) == FireResult::Fired);
        auto* b = dynamic_cast<CRemoteBomb*>(g.server.HandleToObject(w->projectile));
        CHECK(b && b->m_vDir.y == -1 && b->m_fVelocity == 0);
        inv.SetAmmoCount(11, 1);
        req.alternate = false;
        CHECK(w->Fire(req) == FireResult::Fired && inv.GetAmmoCount(11) == 0 && inv.m_nCurWeaponSlot == 1);
        CHECK(p->remote_bombs->m_nInList == 3);
        g.client_shell.detonate_down = true;
        CHECK(g.client_shell.UpdateUse(g.server_shell) && !p->remote_bombs && p->remote_signals == 3);
        CHECK(g.client_shell.UpdateUse(g.server_shell) && p->remote_commands == 1);
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(!p->OnCommandDetonate() && inv.AddInventoryWeapon(8, 1) == InventoryResult::Invalid);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_remote::setup(g, root) && test_remote::equip(g));
        PlayerRestoreState state;
        std::string e;
        CHECK(g.capture_player(state, e) && state.inventory.right[11] == 50 && state.inventory.current_slot == 11 &&
              state.inventory.ammo[11] == 3);
        auto* b = test_remote::bomb(g, {0, 300, 0});
        CHECK(b);
        auto h = b->m_hObject;
        CHECK(g.leave_player() && !g.objects.get(h) && g.restore_player(state, e));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(!p->remote_bombs && p->m_InventoryMgr.CurrentWeapon()->data.m_nType == 50);
        auto bad = state;
        bad.inventory.right[12] = 50;
        CHECK(g.leave_player() && !g.restore_player(bad, e) &&
              g.restore_player(state, e, PlayerRestoreMode::KeepAlive));
        p = g.server_shell.Player(g.local_client);
        CHECK(p->m_InventoryMgr.GetAmmoCount(11) == 3 && !p->remote_bombs);
        MovementControls c;
        c.focused = true;
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = 1;
        event.key.scancode = SDL_SCANCODE_R;
        c.event(event, 1);
        CHECK(c.sample_remote(false) && !c.sample_remote(true) && !c.sample_remote(false));
        event.key.scancode = SDL_SCANCODE_B;
        c.event(event, 1);
        CHECK(c.sample_detonate(false) && !c.sample_detonate(true) && !c.sample_detonate(false));
        c.player_changed();
        CHECK(!c.sample_remote(false) && !c.sample_detonate(false));
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
                if (auto* pu = dynamic_cast<RemotesPU*>(g.objects.get(id)->game.get()))
                {
                    ++pickups;
                    authored.push_back(pu->m_hObject);
                    CHECK(pu->error.empty() && g.objects.get(id)->model);
                }
            CHECK(pickups == 2);
            std::cout << "CityHub cycle=" << cycle << " created=" << made << " unknown=" << unknown
                      << " failed=" << failed << " remote=" << pickups << '\n';
            g.server_shell.start_point_name = "01_egressa";
            CHECK(g.enter_player(e));
            auto* p = g.server_shell.Player(g.local_client);
            for (auto h : authored)
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p->m_hObject));
            CHECK(p->m_InventoryMgr.GetAmmoCount(11) == 2);
            g.client_shell.remote_down = true;
            CHECK(g.client_shell.UpdateUse(g.server_shell));
            CHECK(p->m_InventoryMgr.m_nCurWeaponSlot == 11 &&
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
        Config cfg;
        cfg.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(cfg, e) && g.request_world("01_subwaya", e) && g.enter_player(e) &&
              test_camera::complete_subway_intro(g));
        CHECK(test_remote::equip(g, 4));
        auto* p = g.server_shell.Player(g.local_client);
        p->m_damage.m_bGodMode = true;
        place(g, p->m_hObject, {0, 800, 0});
        auto* w = p->m_InventoryMgr.GetWeapon(11);
        auto step = [&](unsigned n)
        {
            for (unsigned i = 0; i < n; ++i)
                if (!g.step(.01))
                    return false;
            return true;
        };
        CHECK(step(120));
        g.client_shell.weapon_input.primary = true;
        CHECK(step(240));
        CHECK(w->shots == 0 &&
              g.client_shell.weapons[0].bomb_speed == 1750); // Remote source disables overhold explosion.
        g.client_shell.weapon_input.primary = false;
        CHECK(step(100));
        CHECK(w->shots == 1 && w->projectile && w->last_shot.request.projectile_speed == 1750 &&
              p->remote_bombs->m_nInList == 1);
        g.client_shell.weapon_input.alternate = true;
        CHECK(step(90));
        g.client_shell.weapon_input.alternate = false;
        CHECK(step(120));
        CHECK(w->shots == 2 && p->remote_bombs->m_nInList == 2 && w->sound_failures == 0);
        auto* last = dynamic_cast<CRemoteBomb*>(g.server.HandleToObject(w->projectile));
        CHECK(last && last->m_fVelocity == 0 && last->m_vDir.y == -1);
        CHECK(g.select_weapon(0, e));
        g.client_shell.detonate_down = true;
        CHECK(step(20));
        CHECK(!p->remote_bombs && p->remote_commands == 1 && p->remote_signals == 2);
        unsigned exploded = 0;
        for (const auto& r : g.server.projectile_reports)
            if (r.remote && r.outcome == "exploded")
                ++exploded;
        CHECK(exploded == 2);
        std::cout << "Actual remote sequence: overhold survived, two throws, two deferred detonations, ammo="
                  << p->m_InventoryMgr.GetAmmoCount(11) << '\n';
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
    return (mode == "rules"       ? rules(root)
            : mode == "contacts"  ? contacts(root)
            : mode == "motion"    ? motion(root)
            : mode == "ownership" ? ownership(root)
            : mode == "damage"    ? damage(root)
            : mode == "reentrant" ? reentrant(root)
            : mode == "inventory" ? inventory(root)
            : mode == "restore"   ? restore(root)
            : mode == "private"   ? private_world(argv[2])
                                  : mode == "private_fire" && private_fire(argv[2]))
               ? 0
               : 1;
}
