#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/DestructableBrush.h"
#include "game/original/radius_damage.h"
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
    HOBJECT brush(GameSession& g, DVector center = {0, 10.001f, 0}, Properties props = {}, DVector dims = {10, 10, 10},
                  DVector offset = {})
    {
        const auto name = "body" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, vector_subtract(center, dims), vector_add(center, dims));
        model.translation = vector_subtract(center, offset);
        g.world.models.push_back(std::move(model));
        props["Name"] = name;
        props.try_emplace("Pushable", true);
        props.try_emplace("HitPoints", 1000.0f);
        return g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World, props);
    }
    CDestructable& damage(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<CDestructableBrush*>(g.server.HandleToObject(h))->m_damage;
    }
    bool impact(GameSession& g, HOBJECT h, float amount, DVector direction = {1, 0, 0},
                DamageType type = DamageType::Normal)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(m && g.server.WriteDamage(m, {direction, {}, amount, type, h}) == DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    HOBJECT body(GameSession& g, DVector pos, DVector dims = {2, 2, 2}, DDWORD flags = FLAG_SOLID)
    {
        ObjectCreateStruct state;
        state.m_Pos = pos;
        state.m_Flags = flags;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {0, 100, 0}, {{"ObjectMass", 750.0f}});
        CHECK(h);
        auto* r = g.objects.get(h);
        CHECK(r->world_body && r->body_properties.pushable && r->blocking_priority == 40);
        CHECK(r->body_properties.mass == 750 && r->body_properties.friction == 20);
        CHECK((r->state.m_Flags & (FLAG_GRAVITY | FLAG_BOXPHYSICS)) == (FLAG_GRAVITY | FLAG_BOXPHYSICS));
        CHECK((r->user_flags & (1 << 4)) && damage(g, h).apply_damage_physics);
        CHECK(!dynamic_cast<CDestructableBrush*>(r->game.get())->allow_marks);
        auto gravity = brush(g, {100, 100, 0}, {{"Pushable", false}, {"Gravity", true}});
        CHECK(gravity);
        CHECK(g.objects.get(gravity)->world_body && g.objects.get(gravity)->blocking_priority == 100);
        CHECK(!damage(g, gravity).apply_damage_physics && !(g.objects.get(gravity)->user_flags & (1 << 4)));
        const auto flags = r->state.m_Flags;
        g.server.SetObjectFlags(h, flags & ~FLAG_BOXPHYSICS);
        CHECK(r->state.m_Flags == flags);
        CHECK(!g.server.ConfigureWorldBody(h, {100, 7, true}));
        CHECK(g.server.RotateWorldModel(h, {}) == MoverResult::Invalid);
        CHECK(g.server.MoveWorldModel(h, {0, 101, 0}) == MoverResult::Unavailable);
        CHECK(!brush(g, {200, 100, 0}, {{"BoxPhysics", false}}));
        CHECK(!brush(g, {200, 100, 0}, {{"SpawnObject", std::string("Enemy")}}));
        CHECK(!brush(g, {200, 100, 0}, {{"ObjectMass", -1.0f}}));
        return true;
    }
    bool gravity(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {0, 100, 0}, {}, {10, 10, 10}, {30, 40, -20});
        CHECK(h);
        auto* r = g.objects.get(h);
        const auto initial = r->state.m_Pos;
        CHECK(g.step(.1));
        CHECK(near(r->state.m_Pos.y, initial.y - 10) && near(r->velocity.y, -200));
        for (int i = 0; i < 20; ++i)
            CHECK(g.step(.05));
        CHECK(near(object_bounds_center(*r).y, 10.001f) && near(r->velocity.y, 0));
        MovementState s;
        CHECK(g.server.GetMovementState(h, s) && s.standing && s.ground_normal.y > .99f);
        const auto pos = r->state.m_Pos;
        CHECK(g.step(.5) && near(pos.y, r->state.m_Pos.y) && r->body_steps == 192);
        r->activity = OBJSTATE_INACTIVE;
        auto n = r->body_steps;
        CHECK(g.step(.1) && r->body_steps == n);
        return true;
    }
    bool friction(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        CHECK(h);
        auto* r = g.objects.get(h);
        DVector velocity{70, 0, 0};
        CHECK(g.server.SetVelocity(h, &velocity) == DE_OK);
        CHECK(g.step(.1));
        CHECK(near(r->velocity.x, 70 * std::exp(-.7f)) && near(r->state.m_Pos.x, 10 * (1 - std::exp(-.7f))));
        const auto y = r->state.m_Pos.y;
        CHECK(near(y, 10.001f));
        DVector a{35, 0, 0};
        CHECK(g.server.SetAcceleration(h, &a) == DE_OK);
        const auto v = r->velocity.x, x = r->state.m_Pos.x;
        CHECK(g.step(.1));
        CHECK(near(r->velocity.x, 5 + (v - 5) * std::exp(-.7f)));
        CHECK(near(r->state.m_Pos.x, x + .5f + (v - 5) * (1 - std::exp(-.7f)) / 7));
        return true;
    }
    bool impulses(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {0, 100, 0});
        CHECK(h);
        auto& d = damage(g, h);
        CHECK(impact(g, h, 10, {3, 0, 0}) && near(g.objects.get(h)->velocity.x, 250) && d.damage_impulses == 1);
        CHECK(impact(g, h, 1, {0, 0, 0}) && d.damage_impulses == 1);
        CHECK(impact(g, h, 2, {0, .09f, 0}) && d.damage_impulses == 1);
        d.m_fArmorPoints = 100;
        CHECK(impact(g, h, 2, {0, 1, 0}) && near(g.objects.get(h)->velocity.y, 50));
        d.can_damage = false;
        CHECK(impact(g, h, 5) && d.damage_impulses == 2);
        auto zero = brush(g, {100, 100, 0}, {{"ObjectMass", 0.0f}});
        CHECK(zero);
        CHECK(impact(g, zero, 1) && damage(g, zero).m_fMass == 1 && near(g.objects.get(zero)->velocity.x, 2500));
        auto locked = brush(g, {200, 100, 0}, {{"TriggerDestroyOnly", true}});
        CHECK(locked);
        CHECK(impact(g, locked, 5) && damage(g, locked).damage_impulses == 0);
        CHECK(impact(g, locked, 0, {1, 0, 0}, DamageType::Death) && damage(g, locked).damage_impulses == 0);
        CHECK(g.step(.01) && !g.objects.get(locked));
        auto blast = brush(g, {400, 100, 0});
        auto receiver = brush(g, {450, 100, 0});
        CHECK(blast && receiver);
        const auto report =
            DamageObjectsInRadius(blast, g.server.HandleToObject(blast), {400, 100, 0}, 100, 2, DamageType::Explode);
        CHECK(!report.cancelled && report.delivered == 2 && near(g.objects.get(receiver)->velocity.x, 50));
        CHECK(damage(g, receiver).last_damage.area_effect && damage(g, receiver).damage_impulses == 1);
        return true;
    }
    bool pushing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        auto actor = body(g, {-15, 2.001f, 0});
        CHECK(h && actor);
        DVector end{-5, 2.001f, 0};
        CHECK(g.server.SetBlockingPriority(actor, 40));
        CHECK(g.server.MoveObject(actor, &end) == DE_OK && near(g.objects.get(h)->state.m_Pos.x, 0));
        CHECK(g.objects.get(actor)->state.m_Pos.x < -11.99f);
        CHECK(g.server.SetBlockingPriority(actor, 50));
        CHECK(g.server.MoveObject(actor, &end) == DE_OK && near(g.objects.get(actor)->state.m_Pos.x, end.x));
        CHECK(g.objects.get(h)->state.m_Pos.x > 7 && g.objects.get(h)->body_pushes == 1);
        auto wall = body(g, {20, 10, 0}, {1, 10, 30});
        CHECK(wall);
        auto original = g.objects.get(h)->state.m_Pos;
        end.x = 5;
        CHECK(g.server.MoveObject(actor, &end) == DE_OK);
        CHECK(g.objects.get(h)->state.m_Pos.x == original.x && g.objects.get(actor)->state.m_Pos.x < 0);
        CHECK(g.objects.remove(wall) && g.objects.finish_frame());
        auto second = brush(g, {30, 10.001f, 0});
        CHECK(second);
        end.x = 30;
        CHECK(g.server.MoveObject(actor, &end) == DE_OK);
        CHECK(g.objects.get(h)->state.m_Pos.x == original.x && g.objects.get(second)->state.m_Pos.x == 30);
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        CHECK(g.objects.get(g.client_shell.player)->blocking_priority == 50);
        auto& c = g.client_shell;
        c.rotation = {};
        for (int i = 0; i < 60; ++i)
            CHECK(g.step(1.0 / 60));
        const auto start = c.position;
        auto h = brush(g, {start.x, 40.001f, start.z + 60}, {}, {20, 40, 20});
        CHECK(h);
        c.movement_input.forward = true;
        for (int i = 0; i < 60; ++i)
            CHECK(g.step(1.0 / 60));
        CHECK(g.objects.get(h)->body_pushes > 20 && c.position.z > start.z + 180);
        CHECK(g.objects.get(c.player)->blocking_priority == 50 && g.objects.get(c.player)->body_steps == 0);
        c.movement_input.jump = true;
        CHECK(g.step(1.0 / 60));
        CHECK(g.objects.get(c.player)->blocking_priority == 40 && !c.move_mgr.m_bOnGround);
        return true;
    }
    bool riders(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        auto rider = body(g, {0, 22.002f, 0});
        auto detached = body(g, {0, 22.002f, 6}, {1, 2, 1}, FLAG_SOLID | FLAG_DONTFOLLOWSTANDING);
        CHECK(h && rider && detached);
        DVector end{5, 10.001f, 0};
        CHECK(g.server.MoveObject(h, &end) == DE_OK);
        CHECK(near(g.objects.get(rider)->state.m_Pos.x, 5) && g.objects.get(detached)->state.m_Pos.x == 0);
        auto wall = body(g, {9, 24, 0}, {1, 2, 3});
        CHECK(wall);
        end.x = 15;
        CHECK(g.server.MoveObject(h, &end) == DE_OK);
        CHECK(near(g.objects.get(h)->state.m_Pos.x, 5) && near(g.objects.get(rider)->state.m_Pos.x, 5));
        CHECK(g.objects.get(h)->body_result == MoverResult::Blocked);
        CHECK(g.objects.remove(wall) && g.objects.remove(detached) && g.objects.finish_frame());
        DVector jump{0, 50, 0};
        CHECK(g.server.SetVelocity(rider, &jump) == DE_OK);
        CHECK(g.server.MoveObject(h, &end) == DE_OK && near(g.objects.get(rider)->state.m_Pos.x, 5));
        CHECK(g.objects.remove(rider) && g.objects.finish_frame());
        auto stack = brush(g, {15, 30.002f, 0});
        CHECK(stack);
        g.server.SetObjectFlags(stack, g.server.GetObjectFlags(stack) & ~FLAG_DONTFOLLOWSTANDING);
        end.x = 25;
        CHECK(g.server.MoveObject(h, &end) == DE_OK && near(g.objects.get(h)->state.m_Pos.x, 15));
        for (auto id : g.objects.snapshot())
            CHECK(g.objects.remove(g.objects.get(id)->handle));
        CHECK(g.objects.finish_frame());
        h = brush(g, {0, 10.001f, 0}, {}, {200, 10, 10});
        CHECK(h);
        HOBJECT last = nullptr;
        for (int i = 0; i < 65; ++i)
        {
            last = body(g, {float(i * 5 - 160), 22.002f, 0}, {1, 2, 1});
            CHECK(last);
        }
        end = {.1f, 10.001f, 0};
        CHECK(g.server.MoveObject(h, &end) == DE_OK && near(g.objects.get(h)->state.m_Pos.x, 0));
        CHECK(g.objects.get(h)->body_result == MoverResult::Blocked);
        CHECK(g.objects.remove(last) && g.objects.finish_frame());
        CHECK(g.server.MoveObject(h, &end) == DE_OK && near(g.objects.get(h)->state.m_Pos.x, .1f));
        CHECK(g.objects.get(h)->mover_riders == 64);
        return true;
    }
    struct Sensor : BaseClass
    {
        unsigned touches = 0;
        std::function<void(HOBJECT)> callback;
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override
        {
            if (message == MID_TOUCHNOTIFY)
            {
                ++touches;
                if (callback)
                    callback(static_cast<HOBJECT>(data));
            }
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        ClassInfo type{"BodySensor",
                       g.objects.find_class("BaseClass"),
                       []() -> std::unique_ptr<BaseClass> { return std::make_unique<Sensor>(); },
                       {}};
        CHECK(g.objects.register_class(type));
        auto h = brush(g);
        CHECK(h);
        ObjectCreateStruct state;
        state.m_Pos = {-15, 2.001f, 0};
        state.m_Flags = FLAG_SOLID | FLAG_TOUCH_NOTIFY;
        auto actor = g.objects.create(&type, state);
        CHECK(actor);
        DVector dims{2, 2, 2};
        g.server.SetObjectDims(actor, &dims);
        auto* sensor = dynamic_cast<Sensor*>(g.server.HandleToObject(actor));
        CHECK(sensor);
        bool atomic = false, guarded = false;
        sensor->callback = [&](HOBJECT touched)
        {
            atomic =
                touched == h && g.objects.get(h)->state.m_Pos.x > 7 && near(g.objects.get(actor)->state.m_Pos.x, -5);
            DVector pos{};
            guarded = g.server.MoveObject(actor, &pos) == DE_ERROR;
            g.objects.remove(h);
        };
        CHECK(g.server.SetBlockingPriority(actor, 50));
        DVector end{-5, 2.001f, 0};
        CHECK(g.server.MoveObject(actor, &end) == DE_OK);
        CHECK(atomic && guarded && sensor->touches == 1 && !g.server.IsObjectAlive(h) && g.objects.get(h));
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        auto replacement = brush(g, {200, 100, 0});
        CHECK(replacement && !g.server.SetBlockingPriority(h, 50));
        CHECK(g.unload_world() && CDestructable::m_dwNumDestructables == 0);
        return true;
    }
    bool collision(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {0, 100, 0}, {}, {10, 10, 10}, {30, 40, 0});
        CHECK(h);
        auto wall = body(g, {50, 100, 0}, {1, 100, 200});
        CHECK(wall);
        DVector v{10000, 0, 1000};
        CHECK(g.server.SetVelocity(h, &v) == DE_OK && g.step(.02));
        auto* r = g.objects.get(h);
        CHECK(object_bounds_center(*r).x < 39.01f && object_bounds_center(*r).z > 19);
        CHECK(near(r->velocity.x, 0) && r->mover_blocks > 0);
        // A penetrating spawn is retained and visibly blocked, never teleported through the obstacle.
        auto stuck = brush(g, {50, 100, 0});
        CHECK(stuck);
        auto old = g.objects.get(stuck)->state.m_Pos;
        CHECK(g.step(.01));
        CHECK(g.objects.get(stuck)->state.m_Pos.y == old.y &&
              g.objects.get(stuck)->body_result == MoverResult::Blocked);
        return true;
    }
    bool containers(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        g.world.models.push_back(test_brush::box("water", {-100, 0, -100}, {100, 200, 100}));
        CHECK(g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                               {{"Name", std::string("water")}, {"Current", DVector{30, 0, 0}}}));
        auto h = brush(g, {0, 100, 0}, {}, {10, 10, 10}, {1000, 0, 0});
        CHECK(h);
        const auto old = g.objects.get(h)->state.m_Pos;
        CHECK(g.step(.1));
        CHECK(g.objects.get(h)->state.m_Pos.x > old.x && g.objects.get(h)->velocity.y > -100);
        g.world.models.push_back(test_brush::box("damage", {-100, 0, -100}, {100, 200, 100}));
        CHECK(g.objects.create(g.objects.find_class("Damage"), {}, Creation::World,
                               {{"Name", std::string("damage")}, {"Damage", 10.0f}}));
        const auto hp = damage(g, h).m_fHitPoints;
        CHECK(g.step(.1) && near(damage(g, h).m_fHitPoints, hp - 1));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"properties", properties}, {"gravity", gravity},      {"friction", friction}, {"impulses", impulses},
        {"pushing", pushing},       {"player", player},        {"riders", riders},     {"contacts", contacts},
        {"collision", collision},   {"containers", containers}};
    auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(path_from_utf8(argv[2])) ? 0 : 1;
}
