#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/KeyFramer.h"
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
    bool close(DVector a, DVector b)
    {
        auto delta = vector_subtract(a, b);
        return vector_dot(delta, delta) < .0001f;
    }
    HOBJECT brush(GameSession& g, DVector low = {-10, 40, -10}, DVector high = {10, 50, 10}, Properties props = {})
    {
        const auto name = "pathbrush" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, low, high);
        model.translation = {0, 40, 0}; // Bounds center deliberately differs from the pivot.
        g.world.models.push_back(model);
        props["Name"] = name;
        props.try_emplace("BoxPhysics", true);
        props.try_emplace("Gravity", false);
        return g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World, props);
    }
    HOBJECT body(GameSession& g, DVector pos, DVector dims = {1, 1, 1}, DDWORD flags = FLAG_SOLID)
    {
        auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                  {{"Pos", pos}, {"Flags", std::int32_t(flags)}});
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool command(GameSession& g, HOBJECT h, const char* text)
    {
        auto s = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        const bool ok =
            s && msg && g.server.WriteToMessageHString(msg, s) == DE_OK && g.server.EndMessage(msg) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    bool translation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        CHECK(h);
        auto* r = g.objects.get(h);
        const auto base = r->world_model_base, shape = r->world_model;
        const auto initial = r->state.m_Pos;
        CHECK(!(r->state.m_Flags & FLAG_GOTHRUWORLD));
        const DRotation unchanged = r->state.m_Rotation;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {100, 10, 20}), &unchanged) == MoverResult::Moved);
        CHECK(close(object_bounds_center(*r), {100, 55, 20}));
        CHECK(r->world_model_base == base && r->world_model == shape); // Immutable shell plus checked translation.
        CollisionHit hit;
        CHECK(sweep_object(*r, {100, 100, 20}, {100, 0, 20}, {}, hit) == CollisionQuery::Hit &&
              std::abs(hit.point.y - 60) < .01f);
        CHECK(sweep_object(*r, {0, 100, 0}, {0, 0, 0}, {}, hit) == CollisionQuery::Miss);
        CHECK(g.server.MoveKeyframedObject(h, initial, nullptr) == MoverResult::Moved);
        CHECK(close(object_bounds_center(*r), {0, 45, 0}) && r->mover_moves == 2);
        const DRotation opposite{vector_scale(unchanged.m_Vec, -1), -unchanged.m_Spin};
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {0, 10, 0}), &opposite) == MoverResult::Moved);
        CHECK(r->state.m_Rotation.m_Spin == unchanged.m_Spin);
        return true;
    }
    bool world_block(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        auto* r = g.objects.get(h);
        CHECK(r);
        auto passenger = body(g, {0, 51.001f, 0});
        CHECK(passenger);
        MovementState support;
        CHECK(g.server.GetMovementState(passenger, support) && support.ground_object == h);
        const auto initial = r->state.m_Pos, saved = g.objects.get(passenger)->state.m_Pos;
        const auto shape = r->world_model;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {0, -100, 0}), nullptr) == MoverResult::Blocked);
        CHECK(close(r->state.m_Pos, initial) && close(g.objects.get(passenger)->state.m_Pos, saved));
        CHECK(r->world_model == shape && r->mover_blocks == 1 && !r->mover_blocker && r->mover_moves == 0);
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {0, -20, 0}), nullptr) == MoverResult::Moved);
        CHECK(close(g.objects.get(passenger)->state.m_Pos, vector_add(saved, {0, -20, 0})));
        CHECK(g.server.GetMovementState(passenger, support) && support.ground_object == h);
        return true;
    }
    bool riders(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g, {-50, 40, -50}, {50, 50, 50});
        CHECK(h);
        auto riding = body(g, {-20, 51.001f, 0}),
             detached = body(g, {0, 51.001f, 0}, {1, 1, 1}, FLAG_SOLID | FLAG_DONTFOLLOWSTANDING),
             jumping = body(g, {20, 51.001f, 0});
        CHECK(riding && detached && jumping);
        DVector up{0, 100, 0};
        CHECK(g.server.SetVelocity(jumping, &up) == DE_OK);
        const auto original = g.objects.get(h)->state.m_Pos;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(original, {0, -5, 10}), nullptr) == MoverResult::Moved);
        CHECK(close(g.objects.get(riding)->state.m_Pos, {-20, 46.001f, 10}));
        CHECK(close(g.objects.get(detached)->state.m_Pos, {0, 51.001f, 0}) &&
              close(g.objects.get(jumping)->state.m_Pos, {20, 51.001f, 0}));
        CHECK(g.objects.get(h)->mover_riders == 1 && g.objects.get(h)->mover_pushes == 1);
        return true;
    }
    bool push_block(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        CHECK(h);
        auto pushed = body(g, {15, 45, 0});
        CHECK(pushed);
        const auto initial = g.objects.get(h)->state.m_Pos;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {10, 0, 0}), nullptr) == MoverResult::Moved);
        CHECK(std::abs(g.objects.get(pushed)->state.m_Pos.x - 21.002f) < .01f && g.objects.get(h)->mover_pushes == 1);
        const auto mover_before = g.objects.get(h)->state.m_Pos, body_before = g.objects.get(pushed)->state.m_Pos;
        auto wall = body(g, {30, 45, 0}, {1, 5, 5});
        CHECK(wall);
        CHECK(g.server.MoveKeyframedObject(h, vector_add(mover_before, {20, 0, 0}), nullptr) == MoverResult::Blocked);
        CHECK(close(g.objects.get(h)->state.m_Pos, mover_before) &&
              close(g.objects.get(pushed)->state.m_Pos, body_before));
        CHECK(g.objects.get(h)->mover_blocker == wall);
        CHECK(g.objects.remove(wall) && g.objects.finish_frame());
        auto other = brush(g, {50, 40, -10}, {60, 50, 10});
        CHECK(other);
        CHECK(g.server.MoveKeyframedObject(h, vector_add(mover_before, {100, 0, 0}), nullptr) == MoverResult::Blocked);
        CHECK(close(g.objects.get(h)->state.m_Pos, mover_before));
        return true;
    }
    class Probe : public BaseClass
    {
      public:
        HOBJECT mover = nullptr;
        bool remove = false;
        unsigned touches = 0;
        MoverResult recursive = MoverResult::Moved;

      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
        {
            if (message == MID_TOUCHNOTIFY)
            {
                ++touches;
                DVector pos;
                g_pServerDE->GetObjectPos(mover, &pos);
                recursive = g_pServerDE->MoveKeyframedObject(mover, vector_add(pos, {1, 0, 0}), nullptr);
                if (remove)
                {
                    g_pServerDE->RemoveObject(mover);
                    g_pServerDE->RemoveObject(m_hObject);
                }
            }
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        static ClassInfo info{
            "PathProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); }, {}};
        CHECK(g.objects.register_class(info));
        auto h = brush(g);
        CHECK(h);
        auto passenger = body(g, {0, 51.001f, 0});
        CHECK(passenger);
        ObjectCreateStruct desc;
        desc.m_Pos = {15, 51, 0};
        desc.m_Flags = FLAG_TOUCH_NOTIFY;
        auto sensor = g.objects.create(&info, desc);
        CHECK(sensor);
        DVector dims{1, 2, 2};
        g.server.SetObjectDims(sensor, &dims);
        CHECK(close(g.objects.get(sensor)->dimensions, dims));
        auto* probe = dynamic_cast<Probe*>(g.server.HandleToObject(sensor));
        probe->mover = h;
        auto initial = g.objects.get(h)->state.m_Pos;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {30, 0, 0}), nullptr) == MoverResult::Moved);
        CHECK(probe->touches == 1 && probe->recursive == MoverResult::Invalid);
        CHECK(close(g.objects.get(passenger)->state.m_Pos, {30, 51.001f, 0}));
        probe->remove = true;
        CHECK(g.server.MoveKeyframedObject(h, initial, nullptr) == MoverResult::Invalid);
        CHECK(probe->touches == 2 && g.objects.get(h)->life == Life::Pending &&
              g.objects.get(sensor)->life == Life::Pending);
        CHECK(close(g.objects.get(passenger)->state.m_Pos, {0, 51.001f, 0}));
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && !g.objects.get(sensor) && g.objects.get(passenger));
        CHECK(g.unload_world() && !g.server.string_count() && !g.server.list_count());
        return true;
    }
    bool validation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        auto* r = g.objects.get(h);
        CHECK(r);
        const auto initial = r->state.m_Pos;
        const auto shape = r->world_model;
        const auto rotated = rotation_from_euler({0, .01f, 0});
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {10, 0, 0}), &rotated) == MoverResult::Unavailable);
        CHECK(close(r->state.m_Pos, initial) && r->world_model == shape && r->mover_moves == 0);
        const DRotation zero{{}, 0};
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {10, 0, 0}), &zero) == MoverResult::Invalid);
        CHECK(g.server.MoveKeyframedObject(h, {NAN, 0, 0}, nullptr) == MoverResult::Invalid);
        g.server.collision_ready = false;
        CHECK(g.server.MoveKeyframedObject(h, vector_add(initial, {10, 0, 0}), nullptr) == MoverResult::Unavailable);
        g.server.collision_ready = true;
        auto polygon = brush(g, {50, 40, -10}, {60, 50, 10}, {{"BoxPhysics", false}});
        CHECK(polygon);
        auto p = g.objects.get(polygon)->state.m_Pos;
        CHECK(g.server.MoveKeyframedObject(polygon, vector_add(p, {10, 0, 0}), nullptr) == MoverResult::Unavailable);
        auto dynamic = brush(g, {100, 40, -10}, {110, 50, 10}, {{"Pushable", true}});
        CHECK(dynamic && g.objects.get(dynamic)->world_body);
        CHECK(g.server.MoveKeyframedObject(dynamic, {10, 50, 0}, nullptr) == MoverResult::Unavailable);
        CHECK(close(r->state.m_Pos, initial) && r->world_model == shape);
        CHECK(g.objects.remove(h) && g.server.MoveKeyframedObject(h, initial, nullptr) == MoverResult::Invalid);
        CHECK(g.objects.finish_frame() && g.server.MoveKeyframedObject(h, initial, nullptr) == MoverResult::Invalid);
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        auto h = brush(g, {-100, 40, -100}, {100, 50, 100});
        CHECK(h);
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        const auto half = g.objects.get(p->m_hObject)->dimensions.y;
        DVector position{0, 50 + half + .001f, 0};
        CHECK(g.server.TeleportObject(p->m_hObject, &position) == DE_OK);
        DVector zero{};
        CHECK(g.server.SetVelocity(p->m_hObject, &zero) == DE_OK);
        CHECK(g.step(1.0 / 60));
        auto initial = g.objects.get(p->m_hObject)->state.m_Pos;
        MovementState support;
        CHECK(g.server.GetMovementState(p->m_hObject, support) && support.ground_object == h);
        auto dest = vector_add(g.objects.get(h)->state.m_Pos, {10, 0, 0});
        CHECK(g.server.MoveKeyframedObject(h, dest, nullptr) == MoverResult::Moved);
        CHECK(g.step(1.0 / 60));
        auto actual = g.objects.get(p->m_hObject)->state.m_Pos;
        CHECK(std::abs(actual.x - initial.x - 10) < .01f && std::abs(actual.z - initial.z) < .01f);
        CHECK(close(g.client_shell.position, actual));
        CHECK(g.server.GetMovementState(p->m_hObject, support) && support.ground_object == h);
        CHECK(g.objects.get(h)->mover_riders == 1 && g.unload_world());
        return true;
    }
    bool timeline(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        auto h = brush(g);
        CHECK(h);
        const auto initial = g.objects.get(h)->state.m_Pos;
        for (unsigned i = 0; i < 2; ++i)
            CHECK(g.objects.create(g.objects.find_class("Key"), {}, Creation::World,
                                   {{"Name", "route" + std::to_string(i)},
                                    {"Pos", vector_add(initial, {float(i) * 100, 0, 0})},
                                    {"TimeStamp", float(i)}}));
        auto f = g.objects.create(g.objects.find_class("KeyFramer"), {}, Creation::World,
                                  {{"ObjectName", std::string(g.objects.get(h)->state.m_Name)},
                                   {"BaseKeyName", std::string("route")},
                                   {"StartActive", true}});
        CHECK(f);
        auto* path = dynamic_cast<KeyFramer*>(g.server.HandleToObject(f));
        CHECK(g.step(.01) && path->active && path->targets.size() == 1);
        CHECK(g.step(.25) && close(g.objects.get(h)->state.m_Pos, vector_add(initial, {25, 0, 0})));
        CHECK(g.step(.25) && close(g.objects.get(h)->state.m_Pos, vector_add(initial, {50, 0, 0})));
        CHECK(g.step(.5) && !path->active && close(g.objects.get(h)->state.m_Pos, vector_add(initial, {50, 0, 0})));
        CHECK(path->targets[0].moves == 2 && path->processed == 2); // Preserve original missing final snap.
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && path->targets.empty());
        CHECK(command(g, f, "ON") && g.step(.1) && g.unload_world());
        CHECK(!g.server.string_count() && !g.server.list_count() && !g.server.message_count());
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error) && g.step(1.0 / 60));
            auto* list = g.server.FindNamedObjects("htrain1");
            CHECK(list && list->m_pFirstLink);
            auto h = list->m_pFirstLink->m_hObject;
            g.server.RelinquishList(list);
            auto* f = dynamic_cast<KeyFramer*>(g.server.HandleToObject(h));
            CHECK(f && f->initialized && !f->active && f->keys.size() == 3 && f->targets.size() == 1);
            const auto target = f->targets[0].object;
            auto* r = g.objects.get(target);
            CHECK(r && r->world_model && r->world_model_base);
            const auto shape = r->world_model, base = r->world_model_base;
            const auto initial = r->state.m_Pos;
            CHECK(command(g, h, "ON"));
            unsigned moves = 0, blocked = 0;
            DVector last = initial;
            for (unsigned i = 0; i < 310 && f->active; ++i)
            {
                const auto attempts = f->targets[0].attempts;
                CHECK(g.step(1.0 / 60));
                if (f->targets[0].attempts == attempts)
                    continue;
                const auto a = f->time < 3 ? 0u : 1u;
                auto expected = vector_add(
                    f->targets[0].offset,
                    vector_add(f->keys[a].position,
                               vector_scale(vector_subtract(f->keys[a + 1].position, f->keys[a].position),
                                            (f->time - f->keys[a].time) / (f->keys[a + 1].time - f->keys[a].time))));
                if (f->targets[0].result == MoverResult::Moved)
                {
                    ++moves;
                    CHECK(close(r->state.m_Pos, expected));
                }
                else
                {
                    CHECK(f->targets[0].result == MoverResult::Blocked && !r->mover_blocker);
                    CHECK(close(r->state.m_Pos, last));
                    CollisionHit obstruction;
                    const auto center = object_bounds_center(*r);
                    CHECK(g.collision.sweep(center, vector_add(center, vector_subtract(expected, last)), r->dimensions,
                                            obstruction) == CollisionQuery::Hit);
                    CHECK(obstruction.start_solid || obstruction.fraction < 1 - 1e-6);
                    ++blocked;
                    std::cout << "blocked_time=" << f->time << " primary_polygon=" << obstruction.polygon
                              << " fraction=" << obstruction.fraction << " normal=" << obstruction.normal.x << ','
                              << obstruction.normal.y << ',' << obstruction.normal.z << '\n';
                }
                last = r->state.m_Pos;
                CHECK(r->world_model == shape && r->world_model_base == base);
                CollisionHit hit;
                auto center = object_bounds_center(*r);
                CHECK(sweep_object(*r, vector_add(center, {0, 300, 0}), vector_add(center, {0, -300, 0}), {}, hit) ==
                      CollisionQuery::Hit);
            }
            CHECK(moves == 297 && blocked == 2 && !f->active && f->processed == 3 && close(r->state.m_Pos, last));
            CHECK(!close(last, initial) && !close(last, vector_add(f->keys.back().position, f->targets[0].offset)));
            CHECK(r->mover_moves == moves && r->mover_blocks == blocked && g.objects.snapshot().size() == 232 &&
                  g.objects.attachments().size() == 11);
            std::cout << "cycle=" << cycle << " moves=" << moves << " blocks=" << blocked << " final=" << last.x << ','
                      << last.y << ',' << last.z << " consumed_keys=9 runtime=188\n";
            CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.server.string_count() &&
                  !g.server.list_count() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"translation", translation}, {"world_block", world_block}, {"riders", riders},
        {"push_block", push_block},   {"lifetime", lifetime},       {"validation", validation},
        {"player", player},           {"timeline", timeline},       {"subway", subway}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
