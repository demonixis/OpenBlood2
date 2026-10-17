#include "assault_fixture.h"
#include "brush_fixture.h"
#include "game/original/CultistAI.h"
#include <algorithm>
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
        return std::abs(a - b) < .003f;
    }
    struct Victim : CBaseCharacter
    {
        HOBJECT remove_on_damage = nullptr, remove_on_touch = nullptr;
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
            {
                ++touches;
                if (remove_on_touch)
                    g_pServerDE->RemoveObject(remove_on_touch);
            }
            return BaseClass::EngineMessageFn(m, d, f);
        }
        DDWORD ObjectMessageFn(HOBJECT s, DDWORD m, HMESSAGEREAD r) override
        {
            auto value = BaseClass::ObjectMessageFn(s, m, r);
            if (m == damage_message && remove_on_damage)
                g_pServerDE->RemoveObject(remove_on_damage);
            return value;
        }
    };
    Victim* victim(GameSession& g, DVector position)
    {
        static ClassInfo type{
            "GrenadeVictim", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Victim>(); }, {}};
        type.parent = g.objects.find_class("CBaseCharacter");
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = position;
        state.m_Flags = FLAG_SOLID | FLAG_TOUCH_NOTIFY;
        auto h = g.objects.create(&type, state);
        if (auto* r = g.objects.get(h))
            r->dimensions = {10, 10, 10};
        return dynamic_cast<Victim*>(g.server.HandleToObject(h));
    }
    bool key_bounds(const std::filesystem::path&)
    {
        auto bytes = test_model::abc().b;
        const std::string marker("\x05\0START", 7);
        auto found = std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end());
        CHECK(found != bytes.end());
        const auto text = std::size_t(found - bytes.begin());
        CHECK(text > 28);
        auto write = [&](std::size_t at, std::uint32_t v)
        {
            for (unsigned i = 0; i < 4; ++i)
                bytes[at + i] = std::uint8_t(v >> (8 * i));
        };
        Model original, model;
        std::string error;
        CHECK(read_model(bytes, original, error));
        write(text - 24, std::bit_cast<std::uint32_t>(20.f));
        CHECK(read_model(bytes, model, error) && model.invalid_key_bounds == 1 &&
              !model.animations[0].keys[0].bounds_valid && model.animations[0].keys[0].min.x == 20);
        std::vector<DVector> a, b;
        CHECK(sample_vertices(original, 0, 25, a) && sample_vertices(model, 0, 25, b) && a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i)
            CHECK(a[i].x == b[i].x && a[i].y == b[i].y && a[i].z == b[i].z);
        write(text - 24, 0x7f800000);
        CHECK(!read_model(bytes, model, error) && model.animations.empty());
        write(text - 24, std::bit_cast<std::uint32_t>(20.f));
        write(text - 28, 101);
        CHECK(!read_model(bytes, model, error));
        return true;
    }
    bool modes(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root) && test_assault::equip(g));
        auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
        auto* w = inv.CurrentWeapon();
        CHECK(w->GetAmmoType() == 1 && w->GetAmmoType(true) == 5 && w->data.m_nFireType == 1 && w->data.m_bLoopAnim &&
              !w->data.m_bAltLoopAnim);
        CHECK(inv.ObtainWeapon(9, 2) == InventoryResult::AlreadyHave && !inv.GetWeapon(2, true));
        CHECK(test_assault::fire_modes(g));
        inv.SetAmmoCount(1, 0);
        inv.SetAmmoCount(5, 4);
        WeaponFire shot{{500, 100, 500}, {0, 0, 1}, 42, 9, true};
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Dry && inv.GetAmmoCount(5) == 4);
        inv.SetAmmoCount(5, 5);
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Fired && inv.GetAmmoCount(5) == 0 &&
              inv.GetAmmoCount(1) == 0);
        auto* grenade = dynamic_cast<CGrenade*>(g.server.HandleToObject(w->projectile));
        CHECK(grenade && grenade->m_fDamage >= 100 && grenade->m_fDamage <= 140 && grenade->m_nRadius == 200 &&
              grenade->m_fVelocity == 1500);
        shot.alternate = false;
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Dry);
        CHECK(g.unload_world() && !g.server.message_count() && g.server.audio.voices().empty());
        return true;
    }
    bool damage(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root) && test_assault::equip(g));
        auto* p = g.server_shell.Player(g.local_client);
        DVector eye = g.objects.get(p->m_hObject)->state.m_Pos;
        eye.y += p->eye_level;
        auto* c = victim(g, vector_add(eye, {0, 0, 100}));
        CHECK(c);
        auto& inv = p->m_InventoryMgr;
        auto* w = inv.CurrentWeapon();
        WeaponFire shot{eye, {0, 0, 1}, 42, 9, false};
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Fired && w->last_shot.damage >= 10 &&
              w->last_shot.damage <= 16 && w->last_shot.damage_delivered && c->m_damage.m_fHitPoints < 1000 &&
              inv.GetAmmoCount(1) == 199 && inv.GetAmmoCount(5) == 100);
        c->remove_on_damage = p->m_hObject;
        CHECK(g.server_shell.OnMessage(g.local_client, shot) == FireResult::Fired);
        CHECK(g.objects.finish_frame() && !g.objects.get(c->remove_on_damage));
        CHECK(g.unload_world() && !g.server.message_count());
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        auto* p = test_assault::grenade(g, {300, 300, 300});
        CHECK(p);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(r->model && r->dimensions.x == 1 && (r->state.m_Flags & FLAG_GRAVITY) &&
              !(r->state.m_Flags & FLAG_POINTCOLLIDE));
        CHECK(g.step(.1));
        CHECK(near(r->state.m_Pos.z, 310) && near(r->state.m_Pos.y, 300 + .5f * g.server.gravity.y * .1f * .1f));
        CHECK(p->flight_steps == 12 && r->body_steps == 0 && p->sound && !p->trail_error.empty());
        CHECK(near(r->velocity.y, g.server.gravity.y * .1f));
        const auto loop = p->sound;
        CHECK(g.objects.remove(p->m_hObject) && g.objects.finish_frame() && !g.server.audio.get(loop));
        CHECK(g.unload_world());
        return true;
    }
    bool bounce(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        auto* p = test_assault::grenade(g, {300, 100, 300});
        CHECK(p);
        auto* r = g.objects.get(p->m_hObject);
        SegmentHit hit;
        hit.polygon = 3;
        hit.normal = {0, 1, 0};
        DVector v{100, -20, 40};
        CHECK(g.server.SetVelocity(p->m_hObject, &v) == DE_OK);
        p->HandleContact(hit);
        CHECK(p->bounces_left == 4 && near(r->velocity.x, 50) && near(r->velocity.y, -7) && near(r->velocity.z, 20) &&
              p->bounce_sounds == 1);
        CHECK(std::abs(p->pitch_velocity) <= 6.284f && std::abs(p->yaw_velocity) <= 6.284f);
        const auto prior = p->pitch;
        CHECK(g.step(.01) && p->pitch != prior);
        auto* c = victim(g, {500, 100, 500});
        CHECK(c);
        hit.object = c->m_hObject;
        hit.polygon = UINT32_MAX;
        p->HandleContact(hit);
        CHECK(p->bounces_left == 3 && !p->m_bExplode && vector_dot(r->velocity, r->velocity) == 0);
        for (int i = 0; i < 6; ++i)
            p->HandleContact(hit);
        CHECK(p->bounces_left == 0 && p->bounce_sounds == 5 && !p->m_bExplode);
        CHECK(g.unload_world());
        return true;
    }
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        auto* c = victim(g, {300, 100, 330});
        CHECK(c);
        auto* p = test_assault::grenade(g, {300, 100, 300}, {0, 0, 1}, 1000);
        CHECK(p);
        const auto h = p->m_hObject;
        CHECK(p->AdvanceFlight(.05) && p->m_bExplode && p->bounces_left == 5 && c->touches == 1 &&
              c->m_damage.m_fHitPoints == 1000);
        CHECK(g.step(.01) && !g.objects.get(h) && c->m_damage.m_fHitPoints < 1000);
        CHECK(g.server.projectile_reports.back().outcome == "exploded" &&
              g.server.projectile_reports.back().contacts == 1 && g.server.projectile_reports.back().explosions == 2);
        CHECK(g.server.scene_explosions.back().type == 3 && g.server.scene_explosions.back().normal.y == 1 &&
              g.server.scene_explosions.back().voice);
        CHECK(g.unload_world());
        return true;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        auto* c = victim(g, {300, 100, 300});
        CHECK(c);
        auto* p = test_assault::grenade(g, {300, 100, 250});
        CHECK(p);
        SegmentHit hit;
        hit.object = c->m_hObject;
        c->m_damage.m_bDead = true;
        p->HandleContact(hit);
        CHECK(!p->m_bExplode && p->bounces_left == 5);
        c->m_damage.m_bDead = false;
        g.server.SetObjectFlags(c->m_hObject, 0);
        p->HandleContact(hit);
        CHECK(!p->m_bExplode && p->bounces_left == 5);
        g.server.SetObjectFlags(c->m_hObject, FLAG_SOLID);
        c->remove_on_damage = g.client_shell.player;
        p->HandleContact(hit);
        CHECK(p->m_bExplode);
        CHECK(g.step(.01) && g.server.projectile_reports.back().outcome == "exploded" && !g.local_client);
        CHECK(g.server.projectile_reports.back().explosions == 2 && !g.server.message_count());
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        auto* c = victim(g, {300, 30, 300});
        CHECK(c);
        auto* p = test_assault::grenade(g, {300, 100, 300}, {0, 0, 1}, 0);
        CHECK(p);
        auto h = p->m_hObject;
        // No flight: exercise exact original float deadline independently of scheduler quantization.
        p->m_fStartTime = -3;
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && g.objects.get(h) && !p->exploded);
        p->m_fStartTime = -3.01f;
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && p->exploded && !p->sound &&
              c->m_damage.m_fHitPoints < 1000);
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.projectile_reports.size() == 1 && g.server.projectile_reports.back().delivered > 0 &&
              g.server.projectile_reports.back().explosions == 1);
        // Owner unlink does not cancel an independent fuse; it loses attribution.
        p = test_assault::grenade(g, {600, 100, 600}, {0, 0, 1}, 0);
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.leave_player() && p->m_hFiredFrom == nullptr);
        for (int i = 0; i < 320 && g.objects.get(h); ++i)
            CHECK(g.step(.01));
        CHECK(!g.objects.get(h) && g.server.projectile_reports.back().outcome == "exploded");
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool failure(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_assault::setup(g, root));
        CHECK(!g.objects.create(g.objects.find_class("CGrenade"), {}, Creation::World));
        auto* p = test_assault::grenade(g, {300, 100, 300});
        CHECK(p);
        auto h = p->m_hObject;
        g.server.collision_ready = false;
        CHECK(!p->AdvanceFlight(.01) && p->failed && g.server.projectile_reports.size() == 1);
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        g.server.collision_ready = true;
        p = test_assault::grenade(g, {300, 100, 300});
        CHECK(p && !p->AdvanceFlight(.6));
        CHECK(g.objects.finish_frame());
        // Remove the projectile in the hit actor callback: no later substep or duplicate callback.
        auto* c = victim(g, {300, 100, 330});
        CHECK(c);
        p = test_assault::grenade(g, {300, 100, 300}, {0, 0, 1}, 1000);
        CHECK(p);
        h = p->m_hObject;
        c->remove_on_touch = h;
        CHECK(p->AdvanceFlight(.2) && c->touches == 1 && g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
} // namespace
bool drop(const std::filesystem::path& root)
{
    GameSession g;
    CHECK(test_assault::setup(g, root));
    ObjectCreateStruct state;
    state.m_ObjectType = OT_MODEL;
    state.m_Pos = {300, 100, 300};
    SAFE_STRCPY(state.m_Filename, "Models/Characters/Caleb.abc");
    auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
    auto* c = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
    CHECK(c);
    auto& inv = c->m_InventoryMgr;
    CHECK(inv.Init(h, nullptr) && inv.ObtainWeapon(9, 1) == InventoryResult::Ok &&
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
            if (dynamic_cast<AssaultRiflePU*>(g.server.HandleToObject(g.objects.get(id)->handle)))
                pickup = g.objects.get(id)->handle;
    }
    CHECK(pickup && !g.objects.get(hand));
    auto* p = dynamic_cast<AssaultRiflePU*>(g.server.HandleToObject(pickup));
    CHECK(p->value.amount == 50 && g.server.spawn_reports.back().request == "AssaultRiflePU");
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
    g.world_path = "synthetic M16 NPC arena with original resources";
    g.state = SessionState::Inspection;
    CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                           {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
    auto h = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                              {{"Pos", DVector{0, 100, 0}}, {"AIWeapon1", std::string("ASSAULTRIFLE")}});
    auto* a = dynamic_cast<CultistAI*>(g.server.HandleToObject(h));
    CHECK(a && a->RuntimeReady());
    g.server_shell.PreStartWorld();
    CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
    auto* player = g.server_shell.Player(g.local_client);
    // The installed Cultist has no original rifle/autorifle clip. Exercise the
    // weapon service independently, then verify autonomous use fails explicitly.
    CHECK(a->SetTarget(g.server.GetObjectName(player->m_hObject)));
    auto* w = a->m_InventoryMgr.CurrentWeapon();
    CHECK(w && w->data.m_nType == 9);
    for (unsigned i = 0; i < 10; ++i)
        CHECK(a->Fire(false));
    CHECK(a->fire_shots > 0 && player->m_damage.m_fHitPoints < 100 && w->sounds_played == w->shots &&
          !w->sound_failures);
    const auto before = a->fire_shots;
    for (unsigned tick = 0; tick < 120; ++tick)
        CHECK(g.step(1.0 / 60));
    std::cout << "M16 direct AI fire=" << before << " autonomous additional=" << a->fire_shots - before
              << " error=" << a->runtime_error << '\n';
    CHECK(a->runtime_failed && a->runtime_error.find("animation model/clip unavailable") != std::string::npos &&
          a->fire_shots == before);
    CHECK(g.server.GetAnimIndex(h, "fire_stand_rifle") == UINT32_MAX &&
          g.server.GetAnimIndex(h, "fire_stand_autorifle") == UINT32_MAX);
    CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && g.objects.attachments().empty());
    return true;
}

bool collect(const std::filesystem::path& root)
{
    GameSession g;
    CHECK(test_assault::setup(g, root));
    auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
    inv.SetAmmoCount(1, 0);
    inv.SetAmmoCount(5, 0);
    auto h = SpawnWeaponPickup(9, g.client_shell.player, {300, 100, 300}, {}, true);
    auto* p = dynamic_cast<AssaultRiflePU*>(g.server.HandleToObject(h));
    CHECK(p && p->value.amount == 50 && p->resource_id == 353 && g.objects.get(h)->model);
    CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && p->picked_up);
    CHECK(inv.GetAmmoCount(1) == 50 && inv.GetAmmoCount(5) == 0 && g.objects.finish_frame() && !g.objects.get(h));
    CHECK(inv.GetWeapon(2) && !inv.GetWeapon(2, true));
    inv.SetAmmoCount(1, inv.MaxAmmo(1));
    h = SpawnWeaponPickup(9, g.client_shell.player, {300, 100, 300}, {}, true);
    p = dynamic_cast<AssaultRiflePU*>(g.server.HandleToObject(h));
    CHECK(p && g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && !p->picked_up);
    inv.SetAmmoCount(1, inv.MaxAmmo(1) - 1);
    CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && p->picked_up &&
          inv.GetAmmoCount(1) == inv.MaxAmmo(1));
    CHECK(g.objects.finish_frame());
    h = SpawnWeaponPickup(9, g.client_shell.player, {300, 100, 300}, {}, false);
    p = dynamic_cast<AssaultRiflePU*>(g.server.HandleToObject(h));
    CHECK(p && p->value.amount == 0 && g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) &&
          !p->picked_up);
    auto message = g.server.StartMessageToObject(g.server.HandleToObject(h), g.client_shell.player, add_weapon_message);
    CHECK(message && g.server.WritePickupMessage(message, {PickupKind::Weapon, 4, 12}) == DE_INVALIDPARAMS);
    g.server.EndMessage(message);
    CHECK(g.server.pickup_reports.size() == 2 && !g.server.message_count());
    CHECK(g.unload_world() && !g.server.string_count());
    return true;
}
bool restore(const std::filesystem::path& root)
{
    GameSession g;
    CHECK(test_assault::setup(g, root) && test_assault::equip(g));
    auto& inv = g.server_shell.Player(g.local_client)->m_InventoryMgr;
    inv.SetAmmoCount(1, 73);
    inv.SetAmmoCount(5, 17);
    InventoryState snapshot;
    std::string error;
    CHECK(inv.Save(snapshot, error) && snapshot.right[2] == 9 && !snapshot.left[2]);
    auto h = g.objects.create(g.objects.find_class("CPlayerObj"));
    auto* p = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
    CHECK(p);
    CHECK(p->m_InventoryMgr.Load(snapshot, InventoryLoadType::KeepAlive, error));
    auto& restored = p->m_InventoryMgr;
    CHECK(restored.CurrentWeapon()->data.m_nType == 9 && restored.CurrentWeapon()->m_pInventoryMgr == &restored &&
          restored.GetAmmoCount(1) == 73 && restored.GetAmmoCount(5) == 17 && !restored.CurrentWeapon(true));
    snapshot.left[2] = 9;
    CHECK(!restored.Load(snapshot, InventoryLoadType::KeepAlive, error) && restored.CurrentWeapon()->data.m_nType == 9);
    CHECK(g.unload_world());
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
        CHECK(g.request_world("02_cityhub", error));
        unsigned created = 0, unknown = 0, failed = 0, assault = 0;
        unsigned omitted = 0;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
        {
            const auto& e = g.entries[i];
            created += e.state == EntryState::Created;
            unknown += e.state == EntryState::Unsupported;
            failed += e.state == EntryState::Failed;
            omitted += e.state == EntryState::MissingWorldModel;
            if (g.world.objects[i].class_name == "AssaultRiflePU")
            {
                ++assault;
                CHECK(e.state == EntryState::Created);
            }
        }
        std::cout << "CityHub M16=" << assault << " created=" << created << " unknown=" << unknown
                  << " failed=" << failed << '\n';
        CHECK(assault == 1 && created == 453 && unknown == 0 && failed == 0 && omitted == 1);
        CHECK(g.unload_world());
        // Labelled arena uses actual private models/audio, with controlled geometry.
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& surf : floor.surfaces)
            surf.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic M16 private-asset arena";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
        CHECK(test_assault::equip(g) && test_assault::fire_modes(g));
        auto* p = test_assault::grenade(g, {300, 100, 300});
        CHECK(p && g.objects.get(p->m_hObject)->model);
        const auto h = p->m_hObject;
        for (unsigned i = 0; i < 320 && g.objects.get(h); ++i)
            CHECK(g.step(.01));
        CHECK(!g.objects.get(h) && g.server.projectile_reports.back().outcome == "exploded");
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.attachments().empty());
    }
    return true;
}
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{{"key_bounds", key_bounds},
                                                                                         {"modes", modes},
                                                                                         {"damage", damage},
                                                                                         {"motion", motion},
                                                                                         {"bounce", bounce},
                                                                                         {"contacts", contacts},
                                                                                         {"rules", rules},
                                                                                         {"lifetime", lifetime},
                                                                                         {"failure", failure},
                                                                                         {"drop", drop},
                                                                                         {"collect", collect},
                                                                                         {"restore", restore},
                                                                                         {"ai", ai},
                                                                                         {"private", private_game}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
