#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/WorldModelDebris.h"
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
        return std::abs(a - b) < .02f;
    }
    HOBJECT piece(GameSession& g, std::string name, DVector p = {0, 100, 0}, Properties props = {},
                  Creation reason = Creation::World)
    {
        auto m = test_brush::box(name, vector_subtract(p, {4, 2, 8}), vector_add(p, {4, 2, 8}));
        m.translation = p;
        g.world.models.push_back(std::move(m));
        props["Name"] = name;
        props.try_emplace("HitPoints", 20.f);
        props.try_emplace("BoxPhysics", false);
        return g.objects.create(g.objects.find_class("WorldModelDebris"), {}, reason, props);
    }
    WorldModelDebris* debris(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<WorldModelDebris*>(g.server.HandleToObject(h));
    }
    bool kill(GameSession& g, HOBJECT h)
    {
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(message && g.server.WriteDamage(message, {{}, {}, 100, DamageType::Death, h}) == DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK);
        return true;
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = piece(g, "off", {0, 100, 0},
                       {{"StartOn", true},
                        {"InitialVelocity", DVector{12, 34, 56}},
                        {"GravityMultiplier", .5f},
                        {"YAxisRevTime", 2.f}});
        auto* r = g.objects.get(h);
        auto* d = debris(g, h);
        CHECK(r && d && d->ready && !d->on && d->rotating.state == Rotating::Off);
        CHECK(!(r->state.m_Flags & (FLAG_SOLID | FLAG_VISIBLE | FLAG_GRAVITY)) && r->world_body &&
              r->body_properties.rotating_debris);
        CHECK(d->initial_velocity.z == 56 && d->gravity_multiplier == .5f && d->rotating.normal_velocity.y > 3);
        CHECK(g.step(.1) && near(r->state.m_Pos.y, 100) && r->body_steps == 0);
        CHECK(d->Start());
        CHECK(r->state.m_Flags == (FLAG_VISIBLE | FLAG_GRAVITY | FLAG_RAYHIT | FLAG_SOLID));
        CHECK(d->on && d->starts == 1 && r->velocity.y == 34 && r->blocking_priority == 100);
        CHECK(d->Start() && d->starts == 2 && d->rotating.state == Rotating::SpinUp);
        CHECK(!piece(g, "bad", {200, 100, 0}, {{"GravityMultiplier", std::numeric_limits<float>::infinity()}}));
        CHECK(!piece(g, "restore", {200, 100, 0}, {}, Creation::Restore));
        auto s = piece(g, "string", {200, 100, 0}, {{"YAxisRevTime", 1.f}, {"InitialVelocity", DVector{1, 2, 3}}},
                       Creation::String);
        CHECK(s && debris(g, s)->rotating.normal_velocity.y == 0 && debris(g, s)->initial_velocity.y == 2);
        auto grav = piece(g, "gravity", {300, 100, 0}, {{"Gravity", true}});
        CHECK(grav && debris(g, grav)->ready && g.objects.get(grav)->world_body);
        return true;
    }
    bool naming(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto model = test_brush::box("parent", {-5, 100, -5}, {5, 110, 5});
        model.translation = {0, 105, 0};
        g.world.models.push_back(model);
        auto parent = g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World,
                                       {{"Name", std::string("parent")}, {"HitPoints", 100.f}});
        CHECK(parent);
        auto a = piece(g, "parentdebris0", {40, 100, 0});
        CHECK(a);
        // A duplicate name shares existing geometry, with distinct original instances.
        auto b = g.objects.create(g.objects.find_class("WorldModelDebris"), {}, Creation::World,
                                  {{"Name", std::string("parentdebris0")}});
        CHECK(b);
        auto wrong = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                      {{"Name", std::string("parentdebris1")}});
        CHECK(wrong);
        auto c = piece(g, "parentdebris2", {100, 100, 0});
        auto gap = piece(g, "parentdebris4", {200, 100, 0});
        CHECK(c && gap);
        CHECK(kill(g, parent));
        CHECK(!debris(g, a)->on);
        CHECK(g.objects.advance(.002));
        CHECK(!g.objects.get(parent));
        CHECK(debris(g, a)->starts == 1 && debris(g, b)->starts == 1 && debris(g, c)->starts == 1 &&
              !debris(g, gap)->on);
        CHECK(g.server.brush_destructions.back().world_debris_started == 3 &&
              !g.server.brush_destructions.back().world_debris_failed);
        CHECK(g.server.list_count() == 0 && g.server.message_count() == 0 && g.server.string_count() == 0);
        CHECK(g.unload_world());
        CHECK(CDestructable::m_dwNumDestructables == 0 && g.server.brush_destructions.empty());
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = piece(g, "flying", {0, 100, 0},
                       {{"GravityMultiplier", .5f},
                        {"InitialVelocity", DVector{20, 0, 0}},
                        {"YAxisRevTime", 2.f},
                        {"YAxisSpinDownTime", .1f}});
        CHECK(h && debris(g, h)->Start());
        auto* d = debris(g, h);
        auto* r = g.objects.get(h);
        // Explicit initial callback sets custom acceleration, before the only integrator.
        CHECK(g.objects.advance(.01));
        CHECK(r->acceleration.y == 1000);
        CHECK(g.server.advance_physics(.1));
        CHECK(near(r->state.m_Pos.y, 95) && near(r->state.m_Pos.x, 2) && near(r->velocity.y, -100));
        for (int i = 0; i < 80; ++i)
            CHECK(g.step(.01));
        CHECK(d->landings == 1 && !d->on && d->rotating.state == Rotating::Off);
        CHECK(near(object_bounds_center(*r).y, 2.001f) && near(r->velocity.y, 0));
        CHECK(r->acceleration.y == 1000 && (r->state.m_Flags & FLAG_GRAVITY)); // Source never clears these at rest.
        CHECK(d->rotating.pose_angles.y > 0 && r->mover_moves > 0);
        CHECK(d->Start());
        CHECK(d->on && d->starts == 2 && r->velocity.x == 20);
        r->activity = OBJSTATE_INACTIVE;
        const auto steps = r->body_steps;
        CHECK(g.step(.1) && r->body_steps == steps);
        r->activity = OBJSTATE_ACTIVE;
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.step(.01) && r->body_steps > steps);
        return true;
    }
    bool rotation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = piece(g, "rotate", {0, 100, 0});
        CHECK(h && debris(g, h)->Start());
        auto* r = g.objects.get(h);
        CHECK(g.server.RotateWorldModel(h, {{}, {0, .4f, 0}, r->state.m_Pos, r->state.m_Pos}) == MoverResult::Moved);
        CHECK(r->world_model->origin.y == 100);
        const auto before = r->world_model;
        CHECK(g.server.RotateWorldModel(h, {{0, .4f, 0}, {0, 1000, 0}, r->state.m_Pos, r->state.m_Pos}) ==
              MoverResult::Unavailable);
        CHECK(r->world_model == before);
        auto low = piece(g, "blocked", {50, 2.001f, 0});
        CHECK(low && debris(g, low)->Start());
        auto* l = g.objects.get(low);
        auto original = l->world_model;
        CHECK(g.server.RotateWorldModel(low, {{}, {.5f, 0, 0}, l->state.m_Pos, l->state.m_Pos}) ==
              MoverResult::Blocked);
        CHECK(l->world_model == original && l->state.m_Rotation.m_Spin == 1 && l->mover_blocks == 1);
        CHECK(g.server.MoveWorldModel(h, {1, 2, 3}) == MoverResult::Unavailable);
        return true;
    }
    struct RemoveContact : BaseClass
    {
        DDWORD EngineMessageFn(DDWORD m, void* data, DFLOAT) override
        {
            if (m == MID_TOUCHNOTIFY)
                GetServerDE()->RemoveObject(static_cast<HOBJECT>(data));
            return 1;
        }
    };
    const ClassInfo contact_class{
        "RemoveContact", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<RemoveContact>(); }, {}};
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        CHECK(g.objects.register_class(contact_class));
        ObjectCreateStruct state;
        state.m_Pos = {5, 100, 0};
        state.m_Flags = FLAG_TOUCH_NOTIFY;
        auto sensor = g.objects.create(&contact_class, state);
        DVector dims{1, 20, 20};
        CHECK(sensor && g.server.SetObjectDims(sensor, &dims) == DE_OK);
        auto h =
            piece(g, "contact", {0, 100, 0}, {{"InitialVelocity", DVector{100, 0, 0}}, {"GravityMultiplier", 0.f}});
        CHECK(h && debris(g, h)->Start());
        const auto id = g.objects.get(h)->id;
        CHECK(g.objects.advance(.01));
        CHECK(g.server.advance_physics(.1));
        CHECK(!g.objects.get(id) && g.objects.get(sensor));
        CHECK(CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            GameSession g;
            CHECK(test_player::setup(g, root, false));
            auto h = piece(g, "gone");
            CHECK(h && debris(g, h)->Start());
            const auto id = g.objects.get(h)->id;
            CHECK(kill(g, h));
            CHECK(g.objects.advance(.002) && !g.objects.get(id));
            CHECK(g.server.brush_destructions.size() == 1 && g.server.list_count() == 0);
            auto broken = piece(g, "broken");
            CHECK(broken && debris(g, broken)->Start());
            g.server.collision_ready = false;
            CHECK(g.objects.advance(.02));
            CHECK(debris(g, broken)->failed && !debris(g, broken)->error.empty());
            auto pose = g.objects.get(broken)->state.m_Rotation;
            CHECK(g.objects.advance(.1));
            CHECK(g.objects.get(broken)->state.m_Rotation.m_Spin == pose.m_Spin);
            CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0 && g.server.string_count() == 0);
        }
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
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            std::vector<HOBJECT> pieces;
            for (auto id : g.objects.snapshot())
                if (auto* r = g.objects.get(id); dynamic_cast<WorldModelDebris*>(r->game.get()))
                {
                    pieces.push_back(r->handle);
                    CHECK(r->world_model && debris(g, r->handle)->ready && !debris(g, r->handle)->on);
                }
            std::cout << "CityHub pieces=" << pieces.size() << '\n';
            CHECK(pieces.size() == 10);
            unsigned created = 0, unknown = 0, failed = 0;
            for (auto& e : g.entries)
            {
                created += e.state == EntryState::Created;
                unknown += e.state == EntryState::Unsupported;
                failed += e.state == EntryState::Failed;
            }
            std::cout << "Created=" << created << " unsupported=" << unknown << " failed=" << failed << '\n';
            CHECK(created == 453 && unknown == 0 && failed == 1);
            // Exercise original destruction/name activation, not direct fragment creation.
            auto* list = g.server.FindNamedObjects("billboard");
            CHECK(list && list->m_pFirstLink);
            auto parent = list->m_pFirstLink->m_hObject;
            g.server.RelinquishList(list);
            CHECK(kill(g, parent));
            CHECK(g.step(.01));
            CHECK(!g.objects.get(parent));
            unsigned started = 0;
            for (auto h : pieces)
                if (debris(g, h)->starts)
                    ++started;
            std::cout << "Billboard starts=" << started << '\n';
            CHECK(started == 6);
            for (int n = 0; n < 100; ++n)
                CHECK(g.step(.01));
            unsigned landed = 0;
            for (auto h : pieces)
                if (auto* d = debris(g, h))
                {
                    CHECK(!d->failed);
                    landed += d->landings > 0;
                }
            std::cout << "Landed=" << landed << " list tokens live=" << g.server.list_count() << '\n';
            CHECK(g.unload_world() && g.server.list_count() == 0 && g.server.message_count() == 0 &&
                  g.server.string_count() == 0 && CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"properties", properties}, {"naming", naming},     {"motion", motion},       {"rotation", rotation},
        {"lifetime", lifetime},     {"contacts", contacts}, {"private", private_data}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
