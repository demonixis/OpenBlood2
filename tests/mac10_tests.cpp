#include "brush_fixture.h"
#include "game/original/CultistAI.h"
#include "mac10_fixture.h"
#include <cmath>
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
    bool collect(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_mac10::setup(g, root));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        inv.SetAmmoCount(1, 0);
        for (unsigned n = 0; n < 3; ++n)
        {
            auto h = SpawnWeaponPickup(WEAP_SUBMACHINEGUN, g.client_shell.player, {300, 100, 300}, {}, true);
            auto* p = dynamic_cast<SubMachineGunPU*>(g.server.HandleToObject(h));
            CHECK(p && p->value.amount == 20 && p->resource_id == 354 && g.objects.get(h)->model);
            if (n == 2)
                inv.SetAmmoCount(1, inv.MaxAmmo(1));
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player));
            CHECK(p->picked_up == (n < 2));
            if (n < 2)
                CHECK(inv.GetAmmoCount(1) == 20 * (n + 1));
            else
            {
                inv.SetAmmoCount(1, inv.MaxAmmo(1) - 1);
                CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && p->picked_up &&
                      inv.GetAmmoCount(1) == inv.MaxAmmo(1));
            }
            CHECK(g.objects.finish_frame() && !g.objects.get(h));
        }
        CHECK(inv.GetWeapon(2) && inv.GetWeapon(2, true));
        auto h = SpawnWeaponPickup(2, g.client_shell.player, {300, 100, 300}, {}, false);
        auto* p = dynamic_cast<WeaponPickup*>(g.server.HandleToObject(h));
        CHECK(p && p->value.amount == 0 && g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) &&
              !p->picked_up);
        CHECK(g.server.pickup_reports.size() == 3 && g.server.message_count() == 0);
        CHECK(g.unload_world() && g.server.string_count() == 0);
        return true;
    }
    bool automatic(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_mac10::setup(g, root) && test_mac10::equip(g) && test_mac10::fire_modes(g));
        auto* w = g.server_shell.Player(g.local_client)->m_InventoryMgr.CurrentWeapon();
        CHECK(w->data.m_bLoopAnim && w->data.m_bAltLoopAnim && !w->data.m_bSemiAuto);
        CHECK(g.leave_player() && g.server.audio.voices().empty());
        return true;
    }
    struct Victim : BaseClass
    {
        CDestructable damage;
        bool remove = false;
        Victim()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD m, void* p, float v) override
        {
            if (m == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject, nullptr);
                damage.SetHitPoints(1000);
                damage.m_fMaxHitPoints = 1000;
            }
            return BaseClass::EngineMessageFn(m, p, v);
        }
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD m, HMESSAGEREAD r) override
        {
            auto result = BaseClass::ObjectMessageFn(sender, m, r);
            if (remove && m == damage_message)
            {
                g_pServerDE->RemoveObject(sender);
                g_pServerDE->RemoveObject(m_hObject);
            }
            return result;
        }
    };
    bool damage(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_mac10::setup(g, root) && test_mac10::equip(g));
        ClassInfo type{"Mac10Victim",
                       g.objects.find_class("BaseClass"),
                       []() -> std::unique_ptr<BaseClass> { return std::make_unique<Victim>(); },
                       {}};
        CHECK(g.objects.register_class(type));
        DVector eye;
        g.server.GetObjectPos(g.client_shell.player, &eye);
        eye.y += g.server_shell.Player(g.local_client)->eye_level;
        ObjectCreateStruct state;
        state.m_Pos = vector_add(eye, {0, 0, 200});
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, state);
        CHECK(h);
        DVector dims{100, 100, 20};
        CHECK(g.server.SetObjectDims(h, &dims) == DE_OK);
        auto* target = dynamic_cast<Victim*>(g.server.HandleToObject(h));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        WeaponFire shot{eye, {0, 0, 1}, 42, WEAP_SUBMACHINEGUN, false, false};
        for (bool alt : {false, true})
        {
            shot.alternate = alt;
            const auto hp = target->damage.m_fHitPoints, ammo = inv.GetAmmoCount(1);
            CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Fired);
            auto* w = inv.CurrentWeapon();
            CHECK(w->last_shot.damage >= 8 && w->last_shot.damage <= 12 && w->last_shot.hit.object == h &&
                  w->last_shot.damage_delivered);
            CHECK(target->damage.m_fHitPoints < hp && inv.GetAmmoCount(1) == ammo - 1 && w->sounds_played == w->shots);
        }
        inv.SetAmmoCount(1, 0);
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Dry);
        inv.SetAmmoCount(1, 2);
        target->remove = true;
        const auto owner = g.client_shell.player;
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Fired);
        CHECK(g.objects.finish_frame() && !g.objects.get(owner) && !g.objects.get(h) && !g.server.message_count());
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool restore(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_mac10::setup(g, root) && test_mac10::equip(g));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        inv.SetAmmoCount(1, 73);
        InventoryState snapshot;
        std::string error;
        CHECK(inv.Save(snapshot, error) && snapshot.right[2] == 2 && snapshot.left[2] == 2);
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"));
        auto* detached = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(detached);
        CHECK(detached->m_InventoryMgr.Load(snapshot, InventoryLoadType::KeepAlive, error));
        auto& loaded = detached->m_InventoryMgr;
        CHECK(loaded.CurrentWeapon()->data.m_nType == 2 && loaded.CurrentWeapon(true) && loaded.GetAmmoCount(1) == 73);
        CHECK(loaded.CurrentWeapon()->m_pInventoryMgr == &loaded && loaded.CurrentWeapon()->shots == 0);
        CHECK(g.unload_world());
        return true;
    }
    bool drop(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_mac10::setup(g, root));
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = {300, 100, 300};
        SAFE_STRCPY(state.m_Filename, "Models/Characters/Caleb.abc");
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* c = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        CHECK(c);
        auto& inv = c->m_InventoryMgr;
        CHECK(inv.Init(h, nullptr) && inv.ObtainWeapon(2, 1) == InventoryResult::Ok &&
              inv.ChangeWeaponSlot(1) == InventoryResult::Ok);
        CHECK(inv.CurrentWeapon()->ShowHandModel(true));
        const auto hand = inv.CurrentWeapon()->m_hHandModel;
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Ok && g.objects.get(hand) &&
              g.objects.get(hand)->attachment == 0);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && g.objects.get(hand));
        HOBJECT pickup = nullptr;
        for (unsigned i = 0; i < 400 && !pickup; ++i)
        {
            CHECK(g.step(.01));
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<SubMachineGunPU*>(g.server.HandleToObject(g.objects.get(id)->handle)))
                    pickup = g.objects.get(id)->handle;
        }
        CHECK(pickup && !g.objects.get(hand));
        auto* p = dynamic_cast<SubMachineGunPU*>(g.server.HandleToObject(pickup));
        CHECK(p->value.amount == 20 && g.server.spawn_reports.back().request == "SubMachineGunPU");
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
        for (auto& surface : floor.surfaces)
            surface.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic Mac10 NPC arena with original resources";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
        auto h = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                  {{"Pos", DVector{0, 100, 0}}, {"AIWeapon1", std::string("SUBMACHINEGUN")}});
        auto* a = dynamic_cast<CultistAI*>(g.server.HandleToObject(h));
        CHECK(a && a->RuntimeReady());
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        for (unsigned tick = 0; tick < 600; ++tick)
            CHECK(g.step(1.0 / 60));
        auto* w = a->m_InventoryMgr.CurrentWeapon();
        std::cout << "Mac10 NPC updates=" << a->runtime_updates << " shots=" << a->fire_shots
                  << " HP=" << player->m_damage.m_fHitPoints << " error=" << a->runtime_error << '\n';
        CHECK(a->RuntimeReady() && a->attack_updates > 0 && a->fire_shots > 0 && w &&
              w->data.m_nType == WEAP_SUBMACHINEGUN && w->sounds_played == w->shots && !w->sound_failures &&
              player->m_damage.m_fHitPoints < 100 && w->m_hHandModel);
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && g.objects.attachments().empty());
        return true;
    }
    bool private_game(const std::filesystem::path& root)
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
                  test_camera::complete_subway_intro(g));
            CHECK(test_mac10::equip(g));
            for (auto& view : g.client_shell.weapons)
            {
                std::cout << "View left=" << view.left << '\n';
                if (view.model)
                    for (auto& a : view.model->animations)
                    {
                        std::cout << a.name << " duration=" << a.duration;
                        for (auto& k : a.keys)
                            if (!k.text.empty())
                                std::cout << " key@" << k.time << ':' << k.text;
                        std::cout << '\n';
                    }
            }
            CHECK(test_mac10::fire_modes(g));
            auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
            CHECK(inv.CurrentWeapon()->m_hHandModel && inv.CurrentWeapon(true)->m_hHandModel);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.attachments().empty());
        }
        CHECK(g.request_world("02_cityhub", error));
        unsigned created = 0, unknown = 0, failed = 0, mac = 0;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
        {
            const auto& e = g.entries[i];
            created += e.state == EntryState::Created;
            unknown += e.state == EntryState::Unsupported;
            failed += e.state == EntryState::Failed;
            if (g.world.objects[i].class_name == "SubMachineGunPU")
            {
                ++mac;
                CHECK(e.state == EntryState::Created);
            }
        }
        std::cout << "CityHub created=" << created << " unknown=" << unknown << " failed=" << failed << " Mac10=" << mac
                  << '\n';
        CHECK(mac == 1 && created == 432 && unknown == 21 && failed == 1);
        for (auto id : g.objects.snapshot())
            if (auto* a = dynamic_cast<CultistAI*>(g.server.HandleToObject(g.objects.get(id)->handle));
                a && a->runtime_failed)
                std::cout << "NPC failure " << a->runtime_error << '\n';
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"collect", collect}, {"automatic", automatic},  {"damage", damage}, {"restore", restore},
        {"drop", drop},       {"private", private_game}, {"ai", ai}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
