#include "brush_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/DestructableBrush.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <functional>
#include <iostream>
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
    AI_Mgr* actor(GameSession& g, DVector pos = {0, 10, 0})
    {
        static const ClassInfo type{"AISightProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        SAFE_STRCPY(s.m_Name, "sight probe (no AI states)");
        const auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        if (!r)
            return nullptr;
        r->dimensions = {1, 10, 1};
        return dynamic_cast<AI_Mgr*>(r->game.get());
    }
    HOBJECT body(GameSession& g, DVector pos = {0, 10, 100}, DVector dims = {5, 10, 5})
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
        if (h)
            g.server.SetObjectDims(h, &dims);
        return h;
    }
    bool head(GameSession& g, HOBJECT h, float height = 8)
    {
        auto m = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *m, error));
        m->nodes[1].name = "head";
        for (auto& a : m->animations)
        {
            for (auto& p : a.nodes[0].frames)
                p.position = {};
            for (auto& p : a.nodes[1].frames)
                p.position = {0, height, 0};
        }
        g.objects.get(h)->model = m;
        g.objects.get(h)->state.m_ObjectType = OT_MODEL;
        return true;
    }
    bool direct(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* ai = actor(g);
        auto t = body(g);
        CHECK(ai && t && ai->CanSee(t));
        CHECK(ai->sight.outcome == AISightOutcome::Body && ai->sight.rays == 1 && !ai->sight.origin_head);
        CHECK(ai->sight.body_hit.object == t && !ai->sight.retried_head && !ai->m_hTarget);
        // No source distance/FOV/visibility-flag checks in CanSee itself.
        g.objects.get(t)->state.m_Pos = {0, 10, -900};
        g.objects.get(t)->activity = OBJSTATE_INACTIVE;
        CHECK(ai->CanSee(t));
        g.server.SetObjectFlags(t, FLAG_RAYHIT);
        CHECK(ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Body);
        g.server.SetObjectFlags(t, 0);
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Miss && ai->sight.rays == 1);
        CHECK(head(g, t));
        CHECK(!ai->CanSee(t) && !ai->sight.retried_head); // A miss never retries the head.
        return true;
    }
    bool retry(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* ai = actor(g);
        auto t = body(g);
        CHECK(ai && t && head(g, t));
        auto wall = body(g, {0, 5, 50}, {20, 7, 1});
        CHECK(wall);
        CHECK(ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Head && ai->sight.rays == 2);
        CHECK(ai->sight.body_hit.object == wall && ai->sight.head_hit.object == t && ai->sight.head.y == 18);
        // Current sampled animation, no retained node result or time advancement.
        auto* r = g.objects.get(t);
        auto m = std::make_shared<Model>(*r->model);
        r->model = m;
        for (auto& p : m->animations[0].nodes[1].frames)
            p.position = {0, 0, 0};
        const auto before = r->animation;
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Blocked && ai->sight.rays == 2);
        CHECK(r->animation.time == before.time && r->animation.revision == before.revision);
        m->nodes[1].name = "not_head";
        CHECK(!ai->CanSee(t) && ai->sight.rays == 1 && !ai->sight.retried_head);
        CHECK(head(g, ai->m_hObject, 20));
        CHECK(ai->CanSee(t) && ai->sight.origin_head && ai->sight.origin.y == 30 && ai->sight.rays == 1);
        return true;
    }
    HOBJECT brush(GameSession& g, const char* type, Properties props = {})
    {
        const auto name = "sight-brush" + std::to_string(g.world.models.size());
        auto m = test_brush::box(name, {-20, 0, 45}, {20, 40, 55});
        g.world.models.push_back(std::move(m));
        props["Name"] = name;
        if (std::string_view(type) == "CDestructableBrush")
            props.try_emplace("HitPoints", 10.f);
        props.try_emplace("Flags", std::int32_t(FLAG_SOLID));
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    bool filters(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* ai = actor(g);
        auto t = body(g);
        CHECK(ai && t);
        auto h = brush(g, "CDestructableBrush", {{"SurfaceType", std::int32_t(SURFTYPE_GLASS)}, {"CanDamage", false}});
        CHECK(h && ai->CanSee(t));
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({0, 10, 0}, {0, 10, 100}, ai->m_hObject, hit) == SegmentResult::Hit &&
              hit.object == h);
        auto* b = dynamic_cast<CDestructableBrush*>(g.objects.get(h)->game.get());
        CHECK(b);
        b->destructable = false;
        CHECK(!ai->CanSee(t) && ai->sight.body_hit.object == h);
        b->destructable = true;
        b->surface = SURFTYPE_WOOD;
        b->fire_through = true;
        CHECK(!ai->CanSee(t)); // FireThrough is a different rule.
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto volume = brush(g, "Ladder");
        CHECK(volume);
        g.server.SetObjectFlags(volume, g.server.GetObjectFlags(volume) | FLAG_RAYHIT);
        CHECK(ai->CanSee(t)); // IGNORE_LIQUID excludes non-liquid VolumeBrush descendants too.
        CHECK(g.objects.remove(volume) && g.objects.finish_frame());
        auto water = brush(g, "Water");
        CHECK(water && (g.server.GetObjectFlags(water) & FLAG_RAYHIT) && ai->CanSee(t));
        CHECK(g.objects.remove(water) && g.objects.finish_frame());
        auto door = brush(g, "Door", {{"FireThrough", true}});
        CHECK(door);
        CHECK(!ai->CanSee(t) && ai->sight.body_hit.object == door);
        return true;
    }
    bool world(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* ai = actor(g);
        auto t = body(g);
        CHECK(ai && t && head(g, t));
        auto m = test_brush::box("world", {-20, 0, 49}, {20, 40, 51});
        for (auto& s : m.surfaces)
        {
            s.flags = 1;
            s.texture_flags = SURFTYPE_GLASS;
        }
        std::string error;
        CHECK(g.collision.build(m, error));
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Blocked && ai->sight.rays == 2);
        CHECK(!ai->sight.body_hit.object && ai->sight.body_hit.polygon != UINT32_MAX);
        return true; // The original object filter never makes primary-world glass transparent.
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* ai = actor(g);
        auto t = body(g);
        CHECK(ai && t);
        CHECK(!ai->CanSee(nullptr) && ai->sight.outcome == AISightOutcome::Invalid && ai->sight.rays == 0);
        g.server.collision_ready = false;
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Unavailable && ai->sight.rays == 1);
        g.server.collision_ready = true;
        g.objects.get(t)->state.m_Pos = g.objects.get(ai->m_hObject)->state.m_Pos;
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Invalid && !ai->sight.retried_head);
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({}, {0, 0, 1}, nullptr, hit, false, SegmentObjects::SolidOnly,
                                        static_cast<SegmentFilter>(99)) == SegmentResult::Invalid);
        CHECK(g.objects.remove(t) && !ai->CanSee(t) && ai->sight.rays == 0);
        CHECK(g.objects.finish_frame() && !ai->CanSee(t));
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* ai = actor(g);
            auto t = body(g);
            CHECK(ai && t && ai->CanSee(t));
            CHECK(!old || !g.objects.get(old));
            const auto strings = g.server.string_count(), messages = g.server.message_count();
            for (int i = 0; i < 100; ++i)
                CHECK(ai->CanSee(t));
            CHECK(g.server.string_count() == strings && g.server.message_count() == messages && !ai->m_hTarget);
            auto saved = ai->sight;
            old = ai->m_hObject;
            CHECK(g.objects.remove(old) && !ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Invalid);
            CHECK(g.unload_world() && !g.objects.get(saved.target) && !g.objects.get(old));
            CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
            CHECK(setup(g));
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        auto* ai = actor(g, {0, 200, 0});
        auto t = body(g, {0, 200, 200}, {20, 150, 20});
        CHECK(ai && t);
        for (const char* name : {"m_civilian1", "f_civilian1", "labtech", "m_cultist1", "f_cultist1", "gideon"})
        {
            auto* r = g.objects.get(ai->m_hObject);
            CHECK(g.models.load("models/enemies/" + std::string(name) + ".abc", r->model, error));
            for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            {
                r->animation.animation = clip;
                r->animation.time = r->model->animations[clip].duration * .5;
                CHECK(ai->CanSee(t) && ai->sight.origin_head && ai->sight.outcome == AISightOutcome::Body);
            }
            std::cout << name << " clips=" << r->model->animations.size() << " visible body, animated head origins\n";
        }
        CHECK(ai->model_keys == 0); // Queries cannot advance or dispatch animation keys.
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
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            auto* ai = actor(g, g.objects.get(p->m_hObject)->state.m_Pos);
            CHECK(ai && !g.objects.get(old));
            old = ai->m_hObject;
            auto* r = g.objects.get(old);
            CHECK(g.models.load("models/enemies/m_cultist1.abc", r->model, error));
            CHECK(ai->CanSee(p->m_hObject) && ai->sight.origin_head && ai->sight.outcome == AISightOutcome::Body);
            std::cout << "cycle=" << cycle
                      << " body visible from cultist head at player position; fraction=" << ai->sight.body_hit.fraction
                      << '\n';
            CHECK(g.unload_world() && !g.objects.get(old) && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"body", direct},       {"head", retry},        {"filters", filters}, {"world", world},
        {"failures", failures}, {"lifetime", lifetime}, {"catalog", catalog}, {"subway", subway}};
    auto i = tests.find(argv[1]);
    return i != tests.end() && i->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
