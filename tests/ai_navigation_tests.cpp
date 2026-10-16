#include "brush_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/SurfaceTypes.h"
#include "game/server_adapter.h"
#include <cmath>
#include <functional>
#include <iostream>
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
        return std::abs(a - b) < .002f;
    }
    bool same(DVector a, DVector b)
    {
        return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
    }
    bool setup(GameSession& g)
    {
        CHECK(g.objects.find_class("BaseClass") || g.initialize());
        auto floor = test_brush::box("floor", {-1000, -20, -1000}, {1000, -10, 1000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    struct Probe : AI_Mgr
    {
        DVector before{};
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            if (m == MID_UPDATE)
                before = m_MoveObj.GetPos();
            return AI_Mgr::EngineMessageFn(m, d, f);
        }
    };
    Probe* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AINavigationProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        if (!r)
            return nullptr;
        r->dimensions = {1, 2, 1};
        return dynamic_cast<Probe*>(r->game.get());
    }
    HOBJECT box(GameSession& g, DVector p, DVector d)
    {
        ObjectCreateStruct s;
        s.m_Pos = p;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
        if (h)
            g.server.SetObjectDims(h, &d);
        return h;
    }
    bool sample(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto& m = a->m_MoveObj;
        CHECK(!m.initialized && !m.sampled);
        CHECK(g.objects.advance(.01f));
        CHECK(m.samples == 0 && m.sample_failures == 0);
        CHECK(m.Init(a->m_hObject) && same(m.GetPos(), {0, 10, 0}) && same(m.GetLastPos(), {}));
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Pos = {10, 20, 30};
        CHECK(same(m.GetPos(), {0, 10, 0})); // Cached, not an implicit current-position query.
        g.server.SetNextUpdate(a->m_hObject, .001f);
        CHECK(g.objects.advance(.01f));
        CHECK(same(a->before, {0, 10, 0}) && same(m.GetPos(), {10, 20, 30}) && same(m.GetLastPos(), {0, 10, 0}));
        CHECK(m.samples == 2 && same(r->state.m_Pos, {10, 20, 30})); // Cache never integrates physics.
        r->activity = OBJSTATE_INACTIVE;
        r->state.m_Pos.x = 15;
        g.server.SetNextUpdate(a->m_hObject, .001f);
        CHECK(g.objects.advance(.01f));
        CHECK(m.samples == 2);
        const auto rot = rotation_from_euler({0, 1.57079632679f, 0});
        r->state.m_Rotation = rot;
        CHECK(m.Update() && same(m.GetForwardVector(), {1, 0, 0}) && same(m.GetRightVector(), {0, 0, -1}));
        CHECK(m.CalculatePath({20, 20, 30}) && m.Init(a->m_hObject) && m.path().empty());
        CHECK(g.objects.remove(a->m_hObject) && !m.Update() && !m.sampled && m.sample_failures == 1);
        CHECK(!m.CalculatePath({20, 20, 30}));
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool clearance(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->m_MoveObj.Init(a->m_hObject));
        auto& m = a->m_MoveObj;
        SegmentHit hit;
        const DVector start{0, 10, 0}, end{0, 10, 100}, dims{1, 2, 1};
        CHECK(m.ClearToPoint(start, end, dims, hit) && m.rays == 2 && m.outcome == CMovement::Outcome::Clear);
        auto center = box(g, {0, 10, 50}, {.1f, 5, 1});
        CHECK(center && m.ClearToPoint(start, end, dims, hit)); // Source has no center ray.
        auto right = box(g, {1.5f, 10, 60}, {.2f, 5, 1});
        auto left = box(g, {-1.5f, 10, 20}, {.2f, 5, 1});
        CHECK(right && left);
        CHECK(!m.ClearToPoint(start, end, dims, hit) && hit.object == right && m.rays == 1 && m.error.empty());
        CHECK(g.objects.remove(right));
        CHECK(!m.ClearToPoint(start, end, dims, hit) && hit.object == left && m.rays == 2);
        CHECK(g.objects.remove(left) && g.objects.remove(center) && g.objects.finish_frame());
        auto* character = actor(g, {0, 10, 50});
        CHECK(character);
        g.objects.get(character->m_hObject)->dimensions = {5, 5, 5};
        CHECK(m.ClearToPoint(start, end, dims, hit)); // Every CBaseCharacter descendant is filtered.
        CHECK(g.server.IntersectSegment(start, end, a->m_hObject, hit) == SegmentResult::Hit &&
              hit.object == character->m_hObject);
        // Unlike AI visibility, the character filter itself excludes neither glass nor volumes.
        for (const char* type : {"CDestructableBrush", "Water"})
        {
            const auto name = std::string("navigation-") + type;
            g.world.models.push_back(test_brush::box(name, {-5, 5, 45}, {5, 15, 55}));
            Properties props{{"Name", name}, {"Flags", std::int32_t(FLAG_SOLID)}};
            if (std::string_view(type) == "CDestructableBrush")
            {
                props["HitPoints"] = 10.f;
                props["SurfaceType"] = std::int32_t(SURFTYPE_GLASS);
            }
            auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
            CHECK(h);
            if (std::string_view(type) == "CDestructableBrush")
                CHECK(!m.ClearToPoint(start, end, dims, hit) && hit.object == h);
            else
            {
                CHECK(!(g.server.GetObjectFlags(h) & FLAG_SOLID) && m.ClearToPoint(start, end, dims, hit));
                CHECK(g.server.IntersectSegment(start, end, a->m_hObject, hit, false, SegmentObjects::SolidOrRayHit,
                                                SegmentFilter::IgnoreCharacters) == SegmentResult::Hit &&
                      hit.object == h);
            }
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
        }
        g.server.collision_ready = false;
        CHECK(!m.ClearToPoint(start, end, dims, hit) && m.outcome == CMovement::Outcome::Unavailable);
        g.server.collision_ready = true;
        CHECK(m.ClearToPoint(start, end, dims, hit) && m.error.empty() && m.rays == 2);
        CHECK(!m.ClearToPoint(start, start, dims, hit) && m.outcome == CMovement::Outcome::Invalid);
        CHECK(!m.ClearToPoint(start, {0, 100, 0}, dims, hit)); // Direction parallel to up has no defined side axis.
        CHECK(!m.ClearToPoint(start, end, {-1, 2, 1}, hit));
        return true;
    }
    bool rays(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        SegmentHit hit;
        auto b = box(g, {0, 10, 50}, {2, 2, 2});
        CHECK(b);
        CHECK(g.server.CastRay({0, 10, 0}, {0, 0, 4}, hit) == SegmentResult::Hit && hit.object == b &&
              near(hit.point.z, 48));
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, 0, 4}, hit) == SegmentResult::Miss);
        CHECK(g.server.CastRay({0, 10, 0}, {0, -1, 0}, hit) == SegmentResult::Hit && !hit.object &&
              near(hit.point.y, -10));
        CHECK(g.server.CastRay({1e7f, 0, 0}, {1, 0, 0}, hit) == SegmentResult::Miss);
        CHECK(g.server.CastRay({1e7f, 0, 0}, {-1, 0, 0}, hit) == SegmentResult::Miss);
        CHECK(g.server.CastRay({}, {}, hit) == SegmentResult::Invalid);
        CHECK(g.server.CastRay({}, {NAN, 0, 1}, hit) == SegmentResult::Invalid);
        g.server.collision_ready = false;
        CHECK(g.server.CastRay({0, 10, 0}, {0, 0, 1}, hit) == SegmentResult::Unavailable);
        CHECK(g.server.CastRay({}, {}, hit) == SegmentResult::Invalid);
        return true;
    }
    bool direct(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto& m = a->m_MoveObj;
        CHECK(!m.CalculatePath({0, 10, 100}) && m.path().empty());
        CHECK(m.Init(a->m_hObject));
        CHECK(m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Direct && m.rays == 2 &&
              m.path().size() == 1);
        DVector p{7, 7, 7};
        CHECK(m.GetNextPathPoint(p) && same(p, {0, 10, 100}));
        CHECK(!m.CalculatePath({}) && m.path().size() == 1 && m.rays == 0);
        CHECK(!m.CalculatePath({INFINITY, 0, 0}) && m.path().size() == 1);
        CHECK(!m.MoveToNextPathPoint() && m.path().empty() && m.added_points == 1);
        p = {7, 7, 7};
        CHECK(!m.GetNextPathPoint(p) && same(p, {7, 7, 7}) && !m.MoveToNextPathPoint());
        CHECK(m.CalculatePath({0, 10, 100}));
        g.server.collision_ready = false;
        CHECK(!m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Unavailable && m.path().empty());
        g.server.collision_ready = true;
        CHECK(m.CalculatePath({0, 10, 100}));
        m.Term();
        CHECK(m.path().empty() && m.added_points == 0);
        return true;
    }
    bool walls(GameSession& g)
    {
        CHECK(box(g, {-100, 10, 100}, {1, 10, 200}));
        CHECK(box(g, {100, 10, 100}, {1, 10, 200}));
        CHECK(box(g, {0, 10, 200}, {100, 10, 1}));
        return true;
    }
    bool detour(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->m_MoveObj.Init(a->m_hObject));
        auto& m = a->m_MoveObj;
        auto obstacle = box(g, {0, 10, 20.25f}, {4, 5, .25f});
        CHECK(obstacle && walls(g));
        CHECK(m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Detour && m.path().size() == 4);
        const auto width = std::sqrt(2.f) + .1f;
        CHECK(same(m.path()[0], {width, 10, 20 - std::sqrt(2.f)}));
        CHECK(same(m.path()[1], {4 * width, 10, 20 - std::sqrt(2.f)})); // Right chosen first, 1.5 * 2-width overshoot.
        CHECK(same(m.path()[2], {4 * width, 10, 20 - std::sqrt(2.f) + 2 * width})); // One extra forward step.
        CHECK(m.added_points == 4 && m.iterations == 3 && m.rays == 10);
        // Consolidation uses unfiltered solid rays: the owner's enclosing box blocks the first shortcut.
        CHECK(g.objects.remove(obstacle) && g.objects.finish_frame());
        CHECK(m.ConsolidatePath() && m.path().size() == 2 && m.added_points == 4);
        CHECK(same(m.path().front(), {width, 10, 20 - std::sqrt(2.f)}) && same(m.path().back(), {0, 10, 100}));
        CHECK(m.MoveToNextPathPoint() && !m.MoveToNextPathPoint() && m.path().empty());
        return true;
    }
    bool blocked(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->m_MoveObj.Init(a->m_hObject));
        auto& m = a->m_MoveObj;
        CHECK(box(g, {0, 10, 25}, {4, 5, 5}));
        CHECK(!m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Blocked && m.path().size() == 1);
        CHECK(m.rays == 3); // Missing lateral hits mean zero search range, not unlimited free space.
        CHECK(walls(g));
        CHECK(!m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Blocked && m.path().size() == 2);
        CHECK(m.error == "Source final detour clearance failed");
        return true;
    }
    bool limits(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->m_MoveObj.Init(a->m_hObject));
        auto& m = a->m_MoveObj;
        g.objects.get(a->m_hObject)->dimensions = {.1f, 0, .1f};
        CHECK(box(g, {0, 10, 25}, {10000, 5, 5}));
        CHECK(box(g, {20000, 10, 0}, {1, 5, 200}) && box(g, {-20000, 10, 0}, {1, 5, 200}));
        CHECK(!m.CalculatePath({0, 10, 100}) && m.outcome == CMovement::Outcome::Limit);
        CHECK(m.rays <= m.max_rays + 1 && m.iterations <= m.max_iterations + 1 && m.path().size() == 1 &&
              !m.error.empty());
        std::cout << "bounded search: " << m.rays << " ray attempts, " << m.iterations << " iterations\n";
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 4; ++cycle)
        {
            auto* a = actor(g);
            CHECK(a && a->m_MoveObj.Init(a->m_hObject));
            CHECK(!g.objects.get(old));
            old = a->m_hObject;
            for (int i = 0; i < 1000; ++i)
            {
                CHECK(a->m_MoveObj.CalculatePath({0, 10, 100}));
                CHECK(!a->m_MoveObj.MoveToNextPathPoint());
            }
            CHECK(a->m_MoveObj.CalculatePath({0, 10, 100}));
            CMovement external;
            CHECK(external.Init(old));
            CHECK(g.objects.remove(old) && !a->m_MoveObj.CalculatePath({0, 10, 100}));
            CHECK(g.unload_world() && !g.objects.get(old) && !external.Update() &&
                  !external.CalculatePath({0, 10, 100}));
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
            CHECK(setup(g));
        }
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            CHECK(!p->m_MoveObj.initialized); // Source CPlayerObj does not initialize this AI cache.
            const auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
            auto* a = actor(g, pos);
            CHECK(a && a->m_MoveObj.Init(a->m_hObject));
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/m_cultist1.abc", r->model, error));
            CHECK(a->m_MoveObj.CalculatePath(vector_add(pos, {0, 0, 1})) &&
                  a->m_MoveObj.outcome == CMovement::Outcome::Direct);
            CHECK(a->m_MoveObj.path().size() == 1 && a->m_MoveObj.rays == 2);
            std::cout << "cycle=" << cycle << " real SubwayA local clearance at " << pos.x << ' ' << pos.y << ' '
                      << pos.z << "\n";
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"sample", sample},   {"clearance", clearance}, {"rays", rays},         {"direct", direct}, {"detour", detour},
        {"blocked", blocked}, {"limits", limits},       {"lifetime", lifetime}, {"subway", subway}};
    auto i = tests.find(argv[1]);
    return i != tests.end() && i->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
