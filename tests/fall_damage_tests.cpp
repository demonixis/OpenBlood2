#include "audio_fixture.h"
#include "brush_fixture.h"
#include "game/original/Corpse.h"
#include "game/original/RotatingDoor.h"
#include <cmath>
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
    bool floor(GameSession& g)
    {
        WorldModel m;
        m.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        m.surfaces.resize(1);
        m.surfaces[0].flags = 1;
        m.surfaces[0].texture_flags = SURFTYPE_STONE;
        m.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(m, error));
        g.server.collision_ready = true;
        g.world_path = "diagnostic-fall.dat";
        g.state = SessionState::Inspection;
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(g.enter_player(error));
        for (int i = 0; i < 90; ++i)
            CHECK(g.step(1. / 60));
        return true;
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root) && floor(g));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        auto* r = g.objects.get(p->m_hObject);
        CHECK(r->character_body && r->body_properties.mass == 250 && r->body_properties.friction == 0);
        p->m_damage.m_fResistance = 1;
        DVector v{0, -1201, 0};
        CHECK(g.server.SetVelocity(p->m_hObject, &v) == DE_OK);
        // Explicit callbacks isolate original strict thresholds from the replacement solver.
        p->m_bFalling = p->m_bServerFalling = false;
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, 3100);
        CHECK(p->fall_damage_requests == 0);
        p->m_bFalling = true;
        v.y = -1100;
        CHECK(g.server.SetVelocity(p->m_hObject, &v) == DE_OK);
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, 3100);
        CHECK(p->fall_damage_requests == 0);
        v.y = -1100.01f;
        CHECK(g.server.SetVelocity(p->m_hObject, &v) == DE_OK);
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, 3000);
        CHECK(p->fall_damage_requests == 0);
        const auto hp = p->m_damage.m_fHitPoints;
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, 3025);
        CHECK(p->fall_damage_deliveries == 1 && near(p->m_damage.m_fHitPoints, hp - 1));
        CHECK(p->m_damage.last_damage.attacker == p->m_hObject && p->m_damage.last_damage.type == DamageType::Normal &&
              p->m_damage.last_damage.direction.y == -1);
        p->m_bFalling = false;
        p->m_bServerFalling = true;
        p->m_damage.m_bGodMode = true;
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, 3100);
        CHECK(p->fall_damage_deliveries == 2 && near(p->m_damage.m_fHitPoints, hp - 1));
        p->m_damage.m_bGodMode = false;
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, -3100);
        CHECK(p->fall_damage_requests == 2);
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, nullptr, std::numeric_limits<float>::infinity());
        CHECK(!p->fall_damage_error.empty() && p->fall_damage_requests == 2);
        // Door branch takes priority only when it newly queues the original push.
        g.world.models.push_back(test_brush::box("door", {500, 0, 500}, {510, 100, 600}));
        auto door = g.objects.create(g.objects.find_class("RotatingDoor"), {}, Creation::World,
                                     {{"Name", std::string("door")}});
        auto* d = dynamic_cast<RotatingDoor*>(g.server.HandleToObject(door));
        CHECK(d);
        d->state = Door::Opening;
        d->push_player_back = true;
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, door, 3100);
        CHECK(p->door_push_pending && p->fall_damage_requests == 2);
        g.objects.engine_message(p->m_hObject, MID_TOUCHNOTIFY, door, 3100);
        CHECK(p->fall_damage_deliveries == 3 && near(p->m_damage.m_fHitPoints, hp - 5));
        CHECK(g.unload_world() && g.objects.record_count() == 0);
        return true;
    }
    std::function<void(BaseClass&, void*, float)> contact;
    struct Probe : BaseClass
    {
        DDWORD EngineMessageFn(DDWORD m, void* d, float f) override
        {
            if (m == MID_TOUCHNOTIFY && contact)
                contact(*this, d, f);
            return BaseClass::EngineMessageFn(m, d, f);
        }
    };
    ClassInfo type{
        "FallContact", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); }, {}};
    HOBJECT box(GameSession& g, DVector p, DDWORD flags)
    {
        ObjectCreateStruct s;
        s.m_Pos = p;
        s.m_Flags = flags;
        auto h = g.objects.create(&type, s);
        DVector dims{1, 1, 1};
        if (g.server.SetObjectDims(h, &dims) != DE_OK)
            return nullptr;
        return h;
    }
    bool physics(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(g.objects.register_class(type));
        auto a = box(g, {0, 200, 0}, FLAG_SOLID | FLAG_TOUCH_NOTIFY);
        CHECK(a);
        DVector v{0, -4000, 0}, end{0, -100, 0};
        CHECK(g.server.SetVelocity(a, &v) == DE_OK);
        bool world = false, guarded = false;
        contact = [&](BaseClass& self, void* other, float force)
        {
            DVector incoming;
            g.server.GetVelocity(self.m_hObject, &incoming);
            world = self.m_hObject == a && !other && near(force, 40) && incoming.y == -4000;
            guarded = g.server.MoveObject(a, &end) != DE_OK && !g.unload_world();
            incoming.x = 7;
            g.server.SetVelocity(a, &incoming);
        };
        CHECK(g.server.MoveObject(a, &end) == DE_OK);
        contact = {};
        CHECK(world && guarded && near(g.objects.get(a)->velocity.y, 0) && g.objects.get(a)->velocity.x == 7);
        CHECK(g.objects.get(a)->impact_notifications == 1 && near(g.objects.get(a)->last_impact_force, 40));
        // Pure sensor overlaps never receive invented stopping force.
        auto sensor = box(g, {10, 200, 0}, FLAG_TOUCH_NOTIFY);
        CHECK(sensor);
        DVector start{10, 205, 0};
        end = {10, 195, 0};
        CHECK(g.server.TeleportObject(a, &start) == DE_OK && g.server.SetVelocity(a, &v) == DE_OK);
        unsigned overlaps = 0;
        bool zero = true;
        contact = [&](BaseClass&, void*, float f)
        {
            ++overlaps;
            zero &= f == 0;
        };
        CHECK(g.server.MoveObject(a, &end) == DE_OK);
        contact = {};
        CHECK(overlaps == 2 && zero && g.objects.get(a)->velocity.y == -4000);
        CHECK(g.objects.remove(sensor) && g.objects.finish_frame());
        // Relative box force has opposite reciprocal sign, with each live body's velocity.
        auto b = box(g, {10, 200, 0}, FLAG_SOLID | FLAG_TOUCH_NOTIFY);
        CHECK(b);
        CHECK(g.server.TeleportObject(a, &start) == DE_OK && g.server.SetVelocity(a, &v) == DE_OK);
        bool first = false, second = false;
        contact = [&](BaseClass& self, void*, float f)
        {
            if (self.m_hObject == a)
                first = near(f, 80);
            if (self.m_hObject == b)
                second = near(f, -80);
        };
        CHECK(g.server.MoveObject(a, &end) == DE_OK);
        contact = {};
        CHECK(first && second && near(g.objects.get(a)->velocity.y, 0));
        // Pending removal suppresses the reciprocal callback and is never destroyed in dispatch.
        CHECK(g.server.TeleportObject(a, &start) == DE_OK && g.server.SetVelocity(a, &v) == DE_OK);
        unsigned callbacks = 0;
        contact = [&](BaseClass& self, void*, float)
        {
            ++callbacks;
            g.objects.remove(self.m_hObject);
        };
        CHECK(g.server.MoveObject(a, &end) == DE_OK);
        contact = {};
        CHECK(callbacks == 1 && g.objects.get(a) && !g.server.IsObjectAlive(a));
        CHECK(g.objects.finish_frame() && !g.objects.get(a));
        CHECK(g.unload_world() && g.objects.record_count() == 0);
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root) && floor(g));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        p->m_damage.m_fResistance = 1;
        auto h = p->m_hObject;
        DVector start{0, 200, 0}, v{0, -1600, 0}, end{0, 0, 0};
        CHECK(g.server.TeleportObject(h, &start) == DE_OK && g.server.SetVelocity(h, &v) == DE_OK);
        CHECK(p->UpdateLandingSound() && p->m_bFalling && p->m_bServerFalling);
        CHECK(g.server.MoveObject(h, &end) == DE_OK);
        CHECK(p->fall_damage_deliveries == 1 && near(p->fall_damage_force, 4000) && near(p->fall_damage_amount, 40) &&
              near(p->m_damage.m_fHitPoints, 60) && g.objects.get(h)->velocity.y == 0);
        for (int i = 0; i < 60; ++i)
            CHECK(g.step(1. / 60));
        CHECK(p->fall_damage_deliveries == 1 && !p->m_bServerFalling && !p->m_bFalling);
        PlayerRestoreState snapshot;
        std::string error;
        CHECK(g.capture_player(snapshot, error) && g.leave_player());
        CHECK(g.restore_player(snapshot, error));
        p = g.server_shell.Player(g.local_client);
        CHECK(p);
        h = p->m_hObject;
        CHECK(g.objects.get(h)->character_body && g.objects.get(h)->body_properties.mass == 250);
        // A real bound world-model top gets the same world stopping force as primary BSP.
        g.world.models.push_back(test_brush::box("platform", {-50, 50, -50}, {50, 60, 50}));
        auto platform =
            g.objects.create(g.objects.find_class("Door"), {}, Creation::World, {{"Name", std::string("platform")}});
        CHECK(platform && g.objects.get(platform)->world_model);
        CHECK(g.server.TeleportObject(h, &start) == DE_OK && g.server.SetVelocity(h, &v) == DE_OK &&
              p->UpdateLandingSound());
        CHECK(g.server.MoveObject(h, &end) == DE_OK);
        CHECK(p->fall_damage_deliveries == 1 && near(p->fall_damage_force, 4000) && near(p->m_damage.m_fHitPoints, 20));
        CHECK(g.objects.get(h)->contact_object == platform && near(g.objects.get(h)->velocity.y, 0));
        CHECK(g.unload_world() && g.objects.record_count() == 0 && g.server.audio.voices().empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        for (int hz : {30, 60, 120})
        {
            GameSession g;
            std::string error;
            Config c;
            c.data_dir = root;
            CHECK(g.initialize() && g.resources.mount_game(c, error) && floor(g));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            const auto h = p->m_hObject;
            DVector start{0, 1800, 0}, zero{};
            CHECK(g.server.TeleportObject(h, &start) == DE_OK && g.server.SetVelocity(h, &zero) == DE_OK);
            for (int i = 0; i < hz * 4; ++i)
                CHECK(g.step(1. / hz));
            CHECK(p->fall_damage_deliveries == 1 && p->fall_damage_force > 5500 && p->m_damage.IsDead() && p->m_bDead);
            CHECK(p->deaths == 1 && g.client_shell.death_camera.active);
            CHECK(p->m_damage.last_damage.attacker == h && p->m_damage.last_damage.type == DamageType::Normal);
            unsigned corpses = 0;
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<CCorpse*>(g.objects.get(id)->game.get()))
                    ++corpses;
            CHECK(corpses == 1 && g.objects.get(h)->velocity.y == 0);
            std::cout << hz << " Hz: force=" << p->fall_damage_force << " incomingY=" << p->fall_damage_speed
                      << " rawDamage=" << p->fall_damage_amount << " corpse=" << corpses << '\n';
            CHECK(g.leave_player() && g.unload_world() && g.objects.record_count() == 0 &&
                  g.server.audio.voices().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "rules"     ? rules(argv[2])
            : mode == "physics" ? physics(argv[2])
            : mode == "player"  ? player(argv[2])
                                : mode == "private" && private_data(argv[2]))
               ? 0
               : 1;
}
