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
        return std::abs(a - b) < .0001f;
    }
    CPlayerObj* detached(GameSession& g)
    {
        return dynamic_cast<CPlayerObj*>(
            g.server.HandleToObject(g.objects.create(g.objects.find_class("CPlayerObj"), {})));
    }
    bool send(GameSession& g, HOBJECT from, HOBJECT to, float amount, DamageType type = DamageType::Normal)
    {
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(from), to, damage_message);
        Damage value{{0, 0, 1}, {1, 2, 3}, amount, type, from};
        CHECK(message && g.server.WriteDamage(message, value) == DE_OK && g.server.EndMessage(message) == DE_OK);
        return true;
    }
    DestructableState sample()
    {
        DestructableState s;
        s.mass = 350;
        s.max_health = 200;
        s.max_mega_health = 350;
        s.health = 73;
        s.death_health = -19;
        s.max_armor = 100;
        s.max_necro_armor = 200;
        s.armor = 29;
        s.resistance = .7f;
        s.last_type = DamageType::Melee;
        s.last_location = 231;
        s.last_direction = {1, 2, 3};
        s.last_amount = 17;
        s.last_percent = 1.75f;
        s.death_delay = -.25f;
        s.damage_target = "damage_sink";
        s.damage_message = "HIT";
        s.death_target = "death_sink";
        s.death_message = "DEATH";
        s.spawn_object = "HealthPU";
        s.spawn_velocity = {4, 5, 6};
        return s;
    }
    bool same(const DestructableState& a, const DestructableState& b)
    {
        CHECK(a.apply_damage_physics == b.apply_damage_physics && a.dead == b.dead &&
              a.nigh_invulnerable == b.nigh_invulnerable);
        CHECK(a.god_mode == b.god_mode && a.trigger_only == b.trigger_only);
        CHECK(a.mass == b.mass && a.max_health == b.max_health && a.max_mega_health == b.max_mega_health &&
              a.health == b.health);
        CHECK(a.death_health == b.death_health && a.max_armor == b.max_armor && a.max_necro_armor == b.max_necro_armor);
        CHECK(a.armor == b.armor && a.resistance == b.resistance && a.last_type == b.last_type &&
              a.last_location == b.last_location);
        CHECK(a.last_direction.x == b.last_direction.x && a.last_direction.y == b.last_direction.y &&
              a.last_direction.z == b.last_direction.z);
        CHECK(a.last_amount == b.last_amount && a.last_percent == b.last_percent && a.death_delay == b.death_delay);
        CHECK(a.damage_target == b.damage_target && a.damage_message == b.damage_message &&
              a.death_target == b.death_target && a.death_message == b.death_message);
        CHECK(a.spawn_object == b.spawn_object && a.spawn_velocity.x == b.spawn_velocity.x &&
              a.spawn_velocity.y == b.spawn_velocity.y && a.spawn_velocity.z == b.spawn_velocity.z);
        return true;
    }
    bool roundtrip(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* source = detached(g);
        auto* destination = detached(g);
        CHECK(source && destination);
        auto& d = destination->m_damage;
        std::string error;
        auto s = sample();
        s.apply_damage_physics = true;
        s.nigh_invulnerable = true;
        s.god_mode = true;
        s.trigger_only = true;
        CHECK(source->m_damage.Load(s, error));
        DestructableState saved;
        CHECK(source->m_damage.Save(saved, error) && same(saved, s));
        CHECK(send(g, source->m_hObject, destination->m_hObject, 1));
        CHECK(d.last_damage.attacker == source->m_hObject);
        const auto count = CDestructable::m_dwNumDestructables;
        const auto owner = d.m_hObject;
        auto* inventory = d.m_pInventoryMgr;
        for (int i = 0; i < 3; ++i)
        {
            CHECK(d.Load(saved, error));
            DestructableState result;
            CHECK(d.Save(result, error) && same(result, saved));
            CHECK(d.m_hObject == owner && d.m_pInventoryMgr == inventory && d.last_damage.attacker == nullptr);
            CHECK(d.GetLastDamager() == nullptr && CDestructable::m_dwNumDestructables == count);
        }
        source->m_damage.damage_target = "changed";
        CHECK(g.objects.remove(source->m_hObject) && g.objects.finish_frame());
        CHECK(saved.damage_target == "damage_sink" && d.damage_target == "damage_sink");
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.objects.snapshot().empty());
        CHECK(saved.health == 73 && saved.death_health == -19);
        auto* fresh = detached(g);
        CHECK(fresh && fresh->m_damage.Load(saved, error));
        CHECK(fresh->m_damage.m_hObject != owner && fresh->m_damage.m_fHitPoints == 73);
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.server.string_count() == 0);
        return true;
    }
    // Test-only owner for engine-restoration binding before list insertion.
    struct Probe : BaseClass
    {
        CDestructable damage;
        Probe()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override
        {
            if (message == MID_INITIALUPDATE)
                damage.m_hObject = m_hObject;
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    std::unique_ptr<BaseClass> make_probe()
    {
        return std::make_unique<Probe>();
    }
    Probe* probe(GameSession& g)
    {
        static ClassInfo type{"SavedDamageProbe", nullptr, make_probe, {}};
        if (!g.objects.find_class(type.name))
        {
            type.parent = g.objects.find_class("BaseClass");
            if (!g.objects.register_class(type))
                return nullptr;
        }
        return dynamic_cast<Probe*>(g.server.HandleToObject(g.objects.create(&type, {})));
    }
    bool behavior(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        auto* other = probe(g);
        CHECK(p && other);
        auto& d = p->m_damage;
        std::string error;
        DestructableState s;
        s.health = 100;
        s.max_health = 200;
        s.max_mega_health = 300;
        s.armor = 40;
        s.resistance = 1;
        s.nigh_invulnerable = true;
        CHECK(d.Load(s, error) && other->damage.Load(s, error));
        const auto count = CDestructable::m_dwNumDestructables;
        CHECK(other->damage.Load(s, error) && CDestructable::m_dwNumDestructables == count);
        CHECK(send(g, other->m_hObject, p->m_hObject, 50));
        CHECK(near(d.m_fHitPoints, 98.75f) && near(d.m_fArmorPoints, 38.75f) && near(d.m_fDeathHitPoints, -.25f));
        CHECK(send(g, p->m_hObject, other->m_hObject, 50));
        CHECK(other->damage.m_fHitPoints == 75 && other->damage.m_fArmorPoints == 15); // Modifier is character-only.
        CHECK(d.Heal(2) && near(d.m_fDeathHitPoints, 100.75f));
        CHECK(d.MegaHeal(500) && d.m_fDeathHitPoints == 300 && d.m_fHitPoints == 300);
        d.SetHitPoints(100);
        CHECK(d.m_fDeathHitPoints == 300); // Source setter does not reset the death counter.
        d.m_fDeathDelay = 1;
        CHECK(!d.IsDead() && g.step(.25) && d.m_fDeathDelay == 1);
        CHECK(send(g, other->m_hObject, p->m_hObject, 10, DamageType::Death));
        CHECK(d.m_bDead && !d.IsDead() && d.m_fHitPoints == 0 && d.m_fArmorPoints == 0 && d.m_fDeathHitPoints == 0);
        const auto accepted = d.accepted_damage;
        CHECK(send(g, other->m_hObject, p->m_hObject, 50) && d.accepted_damage == accepted);
        CHECK(g.step(.75) && near(d.m_fDeathDelay, .25f) && !d.IsDead());
        DestructableState delayed;
        CHECK(d.Save(delayed, error) && delayed.death_delay == .25f && delayed.dead);
        CHECK(g.step(.5) && near(d.m_fDeathDelay, -.25f) && d.IsDead());
        auto* restored = detached(g);
        CHECK(restored && restored->m_damage.Load(delayed, error));
        CHECK(!restored->m_damage.IsDead() && g.step(.3) &&
              restored->m_damage.IsDead()); // Remaining duration, no clock offset.
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool sentinel(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        auto& d = p->m_damage;
        std::string error;
        DestructableState s;
        d.can_damage = false;
        d.m_fHitPoints = 73;
        CHECK(d.Save(s, error) && s.health == 1e6f && d.m_fHitPoints == 73);
        CHECK(d.Load(s, error) && !d.can_damage && d.m_fHitPoints == 1e6f);
        CHECK(send(g, p->m_hObject, p->m_hObject, 20) && d.m_fHitPoints == 1e6f);
        s.health = 999999;
        CHECK(d.Load(s, error) && d.can_damage && d.m_fHitPoints == 999999);
        CHECK(send(g, p->m_hObject, p->m_hObject, 20) && d.m_fHitPoints == 999979);
        s.health = 1e6f;
        CHECK(d.Load(s, error) && !d.can_damage);
        s.health = 42;
        s.god_mode = true;
        CHECK(d.Load(s, error));
        CHECK(send(g, p->m_hObject, p->m_hObject, 20, DamageType::Death) && !d.m_bDead && d.m_fHitPoints == 42);
        s.god_mode = false;
        s.trigger_only = true;
        CHECK(d.Load(s, error));
        CHECK(send(g, p->m_hObject, p->m_hObject, 20) && d.m_fHitPoints == 42);
        CHECK(send(g, p->m_hObject, p->m_hObject, 1, DamageType::Death) && d.IsDead());
        return true;
    }
    struct Receiver : BaseClass
    {
        CDestructable* target = nullptr;
        DestructableState saved;
        unsigned calls = 0;
        bool rejected = false;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD id, HMESSAGEREAD) override
        {
            if (id == 1000 && target)
            {
                ++calls;
                std::string error;
                DestructableState out;
                out.health = 987;
                rejected = !target->Save(out, error) && out.health == 987 && !target->Load(saved, error);
            }
            return 1;
        }
    };
    std::unique_ptr<BaseClass> make_receiver()
    {
        return std::make_unique<Receiver>();
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto* p = detached(g);
        CHECK(p);
        auto& d = p->m_damage;
        std::string error;
        auto state = sample();
        CHECK(d.Load(state, error));
        auto ids = g.objects.snapshot();
        auto count = CDestructable::m_dwNumDestructables;
        std::vector<std::function<void(DestructableState&)>> corrupt{
            [](auto& s) { s.mass = -1; },
            [](auto& s) { s.health = -1; },
            [](auto& s) { s.max_health = -1; },
            [](auto& s) { s.max_necro_armor = std::numeric_limits<float>::infinity(); },
            [](auto& s) { s.death_health = std::numeric_limits<float>::quiet_NaN(); },
            [](auto& s) { s.death_delay = std::numeric_limits<float>::infinity(); },
            [](auto& s) { s.last_percent = -1; },
            [](auto& s) { s.resistance = -1; },
            [](auto& s) { s.last_direction.x = std::numeric_limits<float>::quiet_NaN(); },
            [](auto& s) { s.spawn_velocity.z = 1e9f; },
            [](auto& s) { s.last_type = static_cast<DamageType>(2); },
            [](auto& s) { s.damage_target = std::string(101, 'x'); },
            [](auto& s) { s.death_message = std::string("x\0y", 3); },
            [](auto& s) { s.spawn_object = "UnknownSavedClass"; }};
        for (auto& change : corrupt)
        {
            auto bad = state;
            change(bad);
            CHECK(!d.Load(bad, error) && !error.empty());
            DestructableState after;
            CHECK(d.Save(after, error) && same(after, state));
            CHECK(g.objects.snapshot() == ids && CDestructable::m_dwNumDestructables == count);
        }
        DestructableState unchanged;
        unchanged.health = 123;
        d.m_fMass = -1;
        CHECK(!d.Save(unchanged, error) && unchanged.health == 123);
        d.m_fMass = state.mass;
        static ClassInfo type{"SavedDamageReceiver", nullptr, make_receiver, {}};
        type.parent = g.objects.find_class("BaseClass");
        CHECK(g.objects.register_class(type));
        ObjectCreateStruct spec;
        SAFE_STRCPY(spec.m_Name, "damage_sink");
        auto* receiver = dynamic_cast<Receiver*>(g.server.HandleToObject(g.objects.create(&type, spec)));
        CHECK(receiver);
        receiver->target = &d;
        receiver->saved = state;
        CHECK(send(g, p->m_hObject, p->m_hObject, 1) && receiver->calls == 1 && receiver->rejected);
        CHECK(g.enter_player(error));
        auto* client = g.server_shell.Player(g.local_client);
        CHECK(client);
        CHECK(!client->m_damage.Load(state, error) && client->m_damage.m_fHitPoints == 100);
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(!d.Save(unchanged, error) && !d.Load(state, error));
        CHECK(g.objects.finish_frame() && g.unload_world() && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool capture(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(!g.server_shell.CapturePlayerDamage(nullptr));
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        p->m_damage.SetHitPoints(73);
        p->m_damage.SetArmorPoints(29);
        CHECK(g.server.SendWorldExit({"next", WorldExitType::World}, "arrival") == DE_OK &&
              g.pump_player_messages(error));
        auto& capture = g.server_shell.damage_capture;
        CHECK(capture.valid && capture.captures == 1 && capture.attempts == 2 && capture.state.health == 73 &&
              capture.state.armor == 29);
        CHECK(g.client_shell.world_exit.queued == 1 && p->m_damage.m_fHitPoints == 73);
        p->m_damage.m_fHitPoints = 67;
        CHECK(capture.state.health == 73);
        g.server.SetObjectState(p->m_hObject, OBJSTATE_INACTIVE);
        CHECK(!g.server_shell.CapturePlayerDamage(g.local_client) && !capture.valid && !capture.error.empty());
        CHECK(capture.state.health == 1 && capture.captures == 1);
        CHECK(g.unload_world() && capture.attempts == 0 && !capture.valid);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* source = g.server_shell.Player(g.local_client);
            CHECK(source);
            source->m_damage.SetHitPoints(73 + cycle);
            source->m_damage.SetArmorPoints(29);
            CHECK(send(g, source->m_hObject, source->m_hObject, 10));
            CHECK(g.server_shell.CapturePlayerDamage(g.local_client) &&
                  g.server_shell.CapturePlayerInventory(g.local_client));
            auto saved = g.server_shell.damage_capture.state;
            auto inventory = g.server_shell.inventory_capture.state;
            auto* restored = detached(g);
            CHECK(restored && restored->SetCharacter(0));
            CHECK(restored->m_damage.Load(saved, error) &&
                  restored->m_InventoryMgr.Load(inventory, InventoryLoadType::KeepAlive, error));
            DestructableState result;
            CHECK(restored->m_damage.Save(result, error) && same(result, saved));
            CHECK(restored->m_damage.m_hObject == restored->m_hObject &&
                  restored->m_damage.m_pInventoryMgr == &restored->m_InventoryMgr);
            CHECK(restored->m_damage.last_damage.attacker == nullptr && restored->m_InventoryMgr.UpdateHandModels());
            CHECK(g.objects.remove(source->m_hObject) && g.objects.finish_frame());
            CHECK(send(g, restored->m_hObject, restored->m_hObject, 10));
            CHECK(near(restored->m_damage.m_fHitPoints,
                       saved.health - 7)); // Saved armor=24 gives original 30% absorption on the next packet.
            CHECK(restored->m_damage.Heal(500) && restored->m_damage.m_fHitPoints == saved.max_health);
            CHECK(restored->m_damage.MegaHeal(500) && restored->m_damage.m_fDeathHitPoints == saved.max_mega_health);
            CHECK(restored->m_damage.AddNecroWard(500) && restored->m_damage.m_fArmorPoints == saved.max_necro_armor);
            std::cout << "cycle " << cycle << ": aggregate HP " << saved.health << ", armor " << saved.armor
                      << ", source-independent damage/heal; no world transfer\n";
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.objects.attachments().empty());
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 &&
                  CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    const auto root = path_from_utf8(argv[2]);
    bool ok = false;
    if (name == "roundtrip")
        ok = roundtrip(root);
    else if (name == "behavior")
        ok = behavior(root);
    else if (name == "sentinel")
        ok = sentinel(root);
    else if (name == "failures")
        ok = failures(root);
    else if (name == "capture")
        ok = capture(root);
    else if (name == "private")
        ok = private_data(root);
    return ok ? 0 : 1;
}
