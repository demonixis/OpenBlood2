#include "game/original/CultistAI.h"
#include "game/original/HandWeaponModel.h"
#include "game/original/WeaponPickups.h"
#include "player_fixture.h"
#include <functional>
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
        return std::abs(a - b) < .01f;
    }
    bool setup(GameSession& g, const std::filesystem::path& path)
    {
        CHECK(test_player::setup(g, path));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    CBaseCharacter* owner(GameSession& g)
    {
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {300, 100, 300};
        s.m_Rotation = rotation_from_euler({0, 1.5707963f, 0});
        SAFE_STRCPY(s.m_Filename, "Models/Characters/Caleb.abc");
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), s);
        auto* c = h ? dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h)) : nullptr;
        if (!c || !c->m_InventoryMgr.Init(h, nullptr) ||
            c->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA) != InventoryResult::Ok ||
            !c->m_InventoryMgr.CurrentWeapon()->ShowHandModel(true))
            return nullptr;
        return c;
    }
    WeaponPickup* pickup(GameSession& g, bool ammo = true)
    {
        return dynamic_cast<WeaponPickup*>(
            g.server.HandleToObject(SpawnWeaponPickup(WEAP_BERETTA, g.client_shell.player, {400, 100, 400}, {}, ammo)));
    }
    bool touch(GameSession& g, HOBJECT h)
    {
        return g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player);
    }
    bool collect(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        inv.SetAmmoCount(1, 0);
        auto* p = pickup(g);
        CHECK(p && p->value.kind == PickupKind::Weapon && p->value.amount == 12 && p->resource_id == 350);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(r->model && (r->user_flags & (1u << 7)) && near(r->state.m_Pos.y, 100));
        CHECK(touch(g, p->m_hObject) && p->picked_up && inv.CurrentWeapon(true) && inv.GetAmmoCount(1) == 12);
        CHECK(g.objects.finish_frame());
        p = pickup(g, false);
        CHECK(p && touch(g, p->m_hObject) && !p->picked_up); // Already dual, no ammo.
        CHECK(g.objects.remove(p->m_hObject) && g.objects.finish_frame());
        p = pickup(g);
        inv.SetAmmoCount(1, inv.MaxAmmo(1));
        CHECK(p && touch(g, p->m_hObject) && !p->picked_up);
        inv.SetAmmoCount(1, inv.MaxAmmo(1) - 1);
        CHECK(touch(g, p->m_hObject) && p->picked_up && inv.GetAmmoCount(1) == inv.MaxAmmo(1));
        CHECK(g.objects.finish_frame() && g.server.pickup_reports.size() == 2 && !g.server.message_count());
        CHECK(g.unload_world() && !g.server.string_count());
        return true;
    }
    bool protocol(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* p = pickup(g);
        CHECK(p);
        auto* player = g.server_shell.Player(g.local_client);
        const auto initial = player->m_InventoryMgr.GetAmmoCount(1);
        for (auto value : {PickupMessage{PickupKind::Weapon, 0, 12},
                           {PickupKind::Weapon, WEAP_MELEE, 12}, // No original pickup class.
                           {PickupKind::Weapon, 1, -1},
                           {PickupKind::Weapon, 1, std::numeric_limits<float>::infinity()}})
        {
            auto m = g.server.StartMessageToObject(p, player->m_hObject, add_weapon_message);
            CHECK(g.server.WritePickupMessage(m, value) == DE_INVALIDPARAMS);
            CHECK(g.server.EndMessage(m) != DE_OK);
        }
        auto m = g.server.StartMessageToObject(p, player->m_hObject, add_ammo_message);
        CHECK(g.server.WritePickupMessage(m, p->value) == DE_INVALIDPARAMS && g.server.EndMessage(m) != DE_OK);
        m = g.server.StartMessageToObject(player, p->m_hObject, picked_up_message);
        CHECK(g.server.WritePickupMessage(m, p->value) == DE_OK && g.server.EndMessage(m) == DE_OK && !p->picked_up);
        player->m_bDead = true;
        CHECK(touch(g, p->m_hObject) && !p->picked_up && !player->m_InventoryMgr.CurrentWeapon(true));
        CHECK(player->m_InventoryMgr.GetAmmoCount(1) == initial && !g.server.message_count());
        // String creation now follows the original bouncing SpawnItem (no update scheduled).
        auto bounced = g.objects.create_props(g.objects.find_class("BerettaPU"), {}, "Name bounce");
        auto* b = bounced ? dynamic_cast<WeaponPickup*>(g.objects.get(bounced)->game.get()) : nullptr;
        CHECK(b && b->bounce && b->bouncing);
        return true;
    }
    bool drop(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        auto& inv = c->m_InventoryMgr;
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok && inv.CurrentWeapon(true)->ShowHandModel(true));
        auto right = inv.CurrentWeapon()->m_hHandModel, left = inv.CurrentWeapon(true)->m_hHandModel;
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Ok && inv.CurrentWeapon() && !inv.CurrentWeapon(true) &&
              inv.last_dropped == left);
        auto* r = g.objects.get(left);
        auto* h = dynamic_cast<CHandWeaponModel*>(r->game.get());
        CHECK(h && h->dropped && !h->owner && !r->attachment && r->scheduled &&
              near(float(r->due - g.objects.time()), 1));
        CHECK(near(r->velocity.x, 150) && near(r->velocity.y, 150) && near(r->velocity.z, 0));
        CHECK(r->dimensions.x == r->dimensions.z && r->dimensions.x >= 10 && r->state.m_Flags & FLAG_NOSLIDING);
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Ok && !inv.CurrentWeapon() && inv.last_dropped == right &&
              inv.drops == 2);
        CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame());
        CHECK(g.server.IsObjectAlive(left) && g.server.IsObjectAlive(right));
        CHECK(g.unload_world() && !g.objects.get(left) && g.objects.attachments().empty());
        return true;
    }
    bool falling(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        auto h = c->m_InventoryMgr.CurrentWeapon()->m_hHandModel;
        CHECK(c->m_InventoryMgr.DropCurrentWeapon() == InventoryResult::Ok);
        CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame());
        // Real shared integration; no direct support injection or forced update.
        for (int i = 0; i < 99; ++i)
            CHECK(g.server.advance_physics(.01) && g.objects.advance(.01));
        CHECK(g.server.IsObjectAlive(h) && g.server.spawn_reports.empty());
        for (int i = 0; i < 20 && g.server.IsObjectAlive(h); ++i)
            CHECK(g.server.advance_physics(.01) && g.objects.advance(.01));
        CHECK(!g.server.IsObjectAlive(h) && g.server.spawn_reports.size() == 1);
        auto ph = g.server.spawn_reports.back().child;
        auto* p = dynamic_cast<WeaponPickup*>(g.server.HandleToObject(ph));
        CHECK(p && p->value.amount == 12 && g.server.IsObjectAlive(ph));
        for (int i = 0; i < 40; ++i)
            CHECK(g.server.advance_physics(.01) && g.objects.advance(.01));
        MovementState landed;
        CHECK(g.server.GetMovementState(ph, landed) && landed.standing);
        std::cout << "Drop flight -> BerettaPU at y=" << g.objects.get(ph)->state.m_Pos.y << " with 12 bullets\n";
        CHECK(touch(g, ph) && !g.server.IsObjectAlive(ph) && g.objects.finish_frame());
        CHECK(g.unload_world() && g.server.spawn_reports.empty() && !g.server.message_count());
        return true;
    }
    bool guards(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        auto& inv = c->m_InventoryMgr;
        g.server.set_game_settings(1, 1);
        CHECK(inv.DropCurrentWeapon() == InventoryResult::NotAvailable);
        CHECK(!SpawnWeaponPickup(WEAP_BERETTA, c->m_hObject, {300, 100, 300}, {}, true));
        CHECK(g.objects.finish_frame());
        g.server.set_game_settings(1, 0);
        auto* w = inv.CurrentWeapon();
        auto h = w->m_hHandModel;
        w->firing = true;
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Busy && g.objects.get(h)->attachment);
        w->firing = false;
        auto model = g.objects.get(h)->model;
        g.objects.get(h)->model.reset();
        CHECK(inv.DropCurrentWeapon() == InventoryResult::Invalid && inv.CurrentWeapon() == w &&
              g.objects.get(h)->attachment);
        g.objects.get(h)->model = model;
        CHECK(inv.ObtainWeapon(WEAP_MELEE) == InventoryResult::Ok &&
              inv.ChangeWeapon(WEAP_MELEE) == InventoryResult::Ok);
        CHECK(inv.CurrentWeapon()->ShowHandModel(true) && inv.DropCurrentWeapon() == InventoryResult::NotAvailable);
        auto& pi = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        CHECK(pi.UpdateHandModels() &&
              pi.DropCurrentWeapon() ==
                  InventoryResult::Ok); // Player input now synchronizes the views before the next draw.
        return true;
    }
    bool failures(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        auto h = c->m_InventoryMgr.CurrentWeapon()->m_hHandModel;
        CHECK(c->m_InventoryMgr.DropCurrentWeapon() == InventoryResult::Ok);
        auto* hand = dynamic_cast<CHandWeaponModel*>(g.server.HandleToObject(h));
        g.server.collision_ready = false;
        CHECK(g.objects.advance(1.01) && !hand->drop_error.empty() && g.server.IsObjectAlive(h));
        CHECK(!g.objects.get(h)->scheduled && !(g.objects.get(h)->state.m_Flags & FLAG_GRAVITY) &&
              g.server.spawn_reports.empty());
        CHECK(g.objects.advance(2) && g.server.spawn_reports.empty());
        return true;
    }
    bool missing_pickup(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        auto h = c->m_InventoryMgr.CurrentWeapon()->m_hHandModel;
        CHECK(c->m_InventoryMgr.DropCurrentWeapon() == InventoryResult::Ok);
        auto* r = g.objects.get(h);
        auto* hand = dynamic_cast<CHandWeaponModel*>(r->game.get());
        DVector pos{300, r->dimensions.y + .001f, 300}, zero{};
        CHECK(g.server.TeleportObject(h, &pos) == DE_OK && g.server.SetVelocity(h, &zero) == DE_OK);
        g.resources = Resources{};
        g.models.clear();
        CHECK(g.objects.advance(1.01) && g.server.IsObjectAlive(h) && !hand->drop_error.empty());
        CHECK(!hand->pickup && g.server.spawn_reports.size() == 1 && !g.server.spawn_reports.back().child);
        CHECK(!r->scheduled && g.objects.advance(2) && g.server.spawn_reports.size() == 1);
        CHECK(g.objects.finish_frame());
        for (auto id : g.objects.snapshot())
            CHECK(!dynamic_cast<WeaponPickup*>(g.objects.get(id)->game.get()));
        return true;
    }
    bool nosliding(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        ObjectCreateStruct state;
        state.m_Pos = {300, 100, 300};
        state.m_Flags = FLAG_GRAVITY | FLAG_NOSLIDING;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        CHECK(h);
        DVector dims{1, 1, 1}, end{400, -20, 400};
        CHECK(g.server.SetObjectDims(h, &dims) == DE_OK && g.server.MoveObject(h, &end) == DE_OK);
        auto pos = g.objects.get(h)->state.m_Pos;
        CHECK(pos.x > 300 && pos.x < 390 && near(pos.y, 1.001f) && near(pos.z, pos.x));
        CHECK(g.server.TeleportObject(h, &state.m_Pos) == DE_OK);
        g.server.SetObjectFlags(h, FLAG_GRAVITY);
        CHECK(g.server.MoveObject(h, &end) == DE_OK && near(g.objects.get(h)->state.m_Pos.x, 400));
        return true;
    }
    class DropOnHit : public BaseClass
    {
      public:
        static inline CInventoryMgr* inventory = nullptr;
        static inline InventoryResult result = InventoryResult::Invalid;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD message, HMESSAGEREAD) override
        {
            if (message == damage_message)
            {
                result = inventory->DropCurrentWeapon();
                GetServerDE()->RemoveObject(inventory->m_hOwner);
            }
            return 1;
        }
    };
    bool callback(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(setup(g, path));
        auto* c = owner(g);
        CHECK(c);
        static const ClassInfo type{"DropOnHit",
                                    g.objects.find_class("BaseClass"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<DropOnHit>(); },
                                    {}};
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct state;
        state.m_Pos = {300, 100, 500};
        state.m_Flags = FLAG_SOLID;
        auto target = g.objects.create(&type, state);
        CHECK(target);
        DVector dims{50, 50, 50};
        CHECK(g.server.SetObjectDims(target, &dims) == DE_OK);
        auto& inv = c->m_InventoryMgr;
        DropOnHit::inventory = &inv;
        inv.SetAmmoCount(1, 10);
        const auto h = c->m_hObject, hand = inv.CurrentWeapon()->m_hHandModel;
        WeaponFire fire;
        fire.weapon = WEAP_BERETTA;
        fire.position = {300, 100, 300};
        fire.direction = {0, 0, 1};
        CHECK(inv.FireCurrentWeapon(fire) == FireResult::Fired);
        CHECK(DropOnHit::result == InventoryResult::Busy && !g.server.IsObjectAlive(h) && inv.drops == 0);
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && !g.objects.get(hand) && !g.server.message_count());
        DropOnHit::inventory = nullptr;
        return true;
    }
    bool catalog(const std::filesystem::path& path)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        Config cfg;
        cfg.data_dir = path;
        CHECK(g.resources.mount_game(cfg, error));
        WorldModel floor;
        floor.points = {{-2000, 0, -2000}, {2000, 0, -2000}, {2000, 0, 2000}, {-2000, 0, 2000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        for (int load = 0; load < 2; ++load)
        {
            // Actual initialized source NPC; damage goes through the production message callback.
            auto h = g.objects.create(
                g.objects.find_class("CultistAI"), {}, Creation::World,
                {{"Pos", DVector{0, 150, 0}}, {"AIWeapon1", std::string("BERETTA")}, {"AIState", std::string("IDLE")}});
            auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
            CHECK(a && a->RuntimeReady());
            CHECK(a->m_InventoryMgr.UpdateHandModels());
            auto* weapon = a->m_InventoryMgr.CurrentWeapon();
            CHECK(weapon);
            auto hand = weapon->m_hHandModel;
            CHECK(hand && g.objects.get(hand)->model);
            // Death packets bypass region queries, preserving the source node from initialization.
            auto m = g.server.StartMessageToObject(a, h, damage_message);
            CHECK(g.server.WriteDamage(m, {{0, 0, 1}, {0, 150, 0}, 10000, DamageType::Death, h}) == DE_OK &&
                  g.server.EndMessage(m) == DE_OK);
            CHECK(a->m_damage.IsDead() && !a->runtime_failed && a->m_bRemoveMe && a->gib_events == 1 &&
                  a->m_InventoryMgr.drops == 1 && !a->m_InventoryMgr.CurrentWeapon());
            CHECK(g.objects.remove(h) && g.objects.finish_frame() && g.server.IsObjectAlive(hand));
            for (int i = 0; i < 130 && g.server.IsObjectAlive(hand); ++i)
                CHECK(g.server.advance_physics(.01) && g.objects.advance(.01));
            CHECK(!g.server.IsObjectAlive(hand) && g.server.spawn_reports.size() == 1);
            auto ph = g.server.spawn_reports.back().child;
            auto* p = dynamic_cast<WeaponPickup*>(g.server.HandleToObject(ph));
            CHECK(p && p->value.amount == 12 && g.objects.get(ph)->model &&
                  g.resources.find(g.objects.get(ph)->state.m_SkinName));
            for (int i = 0; i < 40; ++i)
                CHECK(g.server.advance_physics(.01) && g.objects.advance(.01));
            MovementState landed;
            CHECK(g.server.GetMovementState(ph, landed) && landed.standing);
            std::cout << "Retail Cultist lethal drop cycle " << load
                      << ": detached Beretta -> original pickup (original gib death accepted)\n";
            CHECK(g.objects.clear() && !g.objects.get(hand) && g.objects.attachments().empty() &&
                  !g.server.message_count());
            g.server.spawn_reports.clear();
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"collect", collect},     {"protocol", protocol}, {"drop", drop},
        {"falling", falling},     {"guards", guards},     {"failures", failures},
        {"nosliding", nosliding}, {"callback", callback}, {"missing_pickup", missing_pickup},
        {"catalog", catalog}};
    if (argc != 3)
        return 2;
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
