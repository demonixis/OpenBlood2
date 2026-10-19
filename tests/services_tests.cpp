#include "game/original/SmellHint.h"
#include "game/original/WorldProperties.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>

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
    using test_model::abc;
    using test_model::Writer;
    bool model_reader()
    {
        Model m;
        std::string error;
        CHECK(read_model(abc().b, m, error));
        CHECK(m.nodes.size() == 2 && m.nodes[1].parent == 0);
        CHECK(m.vertices.size() == 3 && m.faces[0].vertices[2] == 2);
        CHECK(m.faces[0].uv[1][0] == 1);
        CHECK(animation_index(m, "walk") == 0 && animation_index(m, "missing") == no_animation);
        CHECK(animation_index(m, "WaLk") == 0 && animation_index(m, "WALK") == 0);
        CHECK(animation_index(m, "wal") == no_animation && animation_index(m, "walk ") == no_animation);
        auto duplicate = m;
        duplicate.animations.push_back(m.animations[0]);
        duplicate.animations.back().name = "WALK";
        CHECK(animation_index(duplicate, "WALK") == 0); // Same first-match rule as the reference engine.
        std::vector<NodePose> pose;
        CHECK(sample_pose(m, 0, 25, pose));
        CHECK(std::abs(pose[1].position.x - 5) < .0001 && pose[1].position.y == 2);
        std::vector<DVector> vertices;
        CHECK(sample_vertices(m, 0, 25, vertices));
        CHECK(vertices[2].x == 8 && vertices[2].y == 4 && vertices[2].z == 6);
        CHECK(sample_pose(m, 0, 1000, pose));
        CHECK(pose[1].position.x == 20);
        CHECK(!sample_pose(m, no_animation, 0, pose) && pose.empty());
        return true;
    }
    bool model_rejection()
    {
        auto bytes = abc().b;
        Model m;
        std::string error;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            CHECK(!read_model(std::span(bytes).first(i), m, error));
            CHECK(m.animations.empty());
        }
        auto bad = bytes;
        bad[8] = 0;
        CHECK(!read_model(bad, m, error)); // Header section link into its own header.
        std::mt19937 random(79);
        for (unsigned i = 0; i < 1500; ++i)
        {
            bad = bytes;
            bad[random() % bad.size()] ^= static_cast<std::uint8_t>(1u << (random() % 8));
            read_model(bad, m, error);
        }
        return true;
    }
    bool animation_events()
    {
        Model m;
        std::string error;
        CHECK(read_model(abc().b, m, error));
        AnimationTrack track;
        std::vector<std::uint32_t> events;
        CHECK(advance_animation(m, track, .25, events, error));
        CHECK((events == std::vector<std::uint32_t>{0, 1, 2, 0, 1, 2, 0, 1}));
        CHECK(track.time == 50);
        track.reset();
        track.looping = false;
        CHECK(advance_animation(m, track, .5, events, error));
        CHECK((events == std::vector<std::uint32_t>{0, 1, 2}));
        CHECK(track.done);
        CHECK(advance_animation(m, track, 1, events, error) && events.empty());
        track.animation = 1;
        track.looping = true;
        track.reset();
        CHECK(advance_animation(m, track, 1, events, error));
        CHECK(events.size() == 1 && track.done);
        CHECK(advance_animation(m, track, 0, events, error) && events.empty());
        track.animation = 0;
        track.reset();
        CHECK(advance_animation(m, track, .1, events, error));
        CHECK((events == std::vector<std::uint32_t>{0, 1, 2, 0}));
        CHECK(advance_animation(m, track, 0, events, error) && events.empty());
        auto before = track;
        events = {42};
        CHECK(!advance_animation(m, track, -1, events, error));
        CHECK(track.time == before.time && events[0] == 42);
        m.animations[0].duration = 1;
        for (auto& k : m.animations[0].keys)
            k.time = 0;
        CHECK(!advance_animation(m, track, 60, events, error));
        CHECK(track.time == before.time && events[0] == 42);
        return true;
    }
    std::function<void(BaseClass&, DDWORD, void*)> callback;
    struct Probe : BaseClass
    {
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override
        {
            if (callback)
                callback(*this, message, data);
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    std::unique_ptr<BaseClass> make_probe()
    {
        return std::make_unique<Probe>();
    }
    ClassInfo probe_class{"ServiceProbe", nullptr, make_probe, {}};
    HOBJECT probe(GameSession& game)
    {
        return game.objects.create(&probe_class);
    }
    bool setup(GameSession& game)
    {
        callback = {};
        return game.initialize() && game.objects.register_class(probe_class);
    }
    bool model_callbacks()
    {
        GameSession game;
        CHECK(setup(game));
        Model m;
        std::string error;
        CHECK(read_model(abc().b, m, error));
        auto h = probe(game);
        auto* r = game.objects.get(h);
        r->model = std::make_shared<Model>(std::move(m));
        r->state.m_Flags = FLAG_MODELKEYS;
        std::vector<std::string> seen;
        bool reentrant_rejected = false;
        callback = [&](BaseClass&, DDWORD message, void* data)
        {
            if (message == MID_MODELSTRINGKEY)
            {
                auto& args = *static_cast<ArgList*>(data);
                seen.emplace_back(args.argv[0]);
                if (seen.back() == "SOUND")
                    seen.emplace_back(args.argv[1]);
                reentrant_rejected = !game.unload_world() && !game.step(.01) && !game.server.advance_models(.01) &&
                                     !game.server.advance_physics(.01) && game.objects.get(h) == r;
            }
        };
        CHECK(game.step(.05));
        CHECK(reentrant_rejected);
        CHECK((seen == std::vector<std::string>{"START", "SOUND", "left step", "FOOT"}));
        auto revision = r->animation.revision;
        game.server.SetModelAnimation(h, 0);
        CHECK(revision == r->animation.revision);
        game.server.SetModelLooping(h, DFALSE);
        CHECK(game.step(.05));
        CHECK(game.server.GetModelPlaybackState(h) == MS_PLAYDONE);
        CHECK(game.server.GetAnimIndex(h, "walk") == 0);
        DVector dims;
        CHECK(game.server.GetModelAnimUserDims(h, &dims, 0) == DE_OK && dims.y == 2);
        CHECK(game.server.GetModelAnimUserDims(h, &dims, 9) != DE_OK && dims.y == 0);
        CHECK(game.server.ResetModelAnimation(h) == DE_OK);
        seen.clear();
        callback = [&](BaseClass&, DDWORD message, void* data)
        {
            if (message == MID_MODELSTRINGKEY)
            {
                seen.emplace_back(static_cast<ArgList*>(data)->argv[0]);
                game.server.SetModelAnimation(h, 1);
            }
        };
        CHECK(game.step(.1));
        CHECK(seen.size() == 1 && seen[0] == "START");
        game.server.SetModelAnimation(h, 0);
        seen.clear();
        callback = [&](BaseClass&, DDWORD message, void*)
        {
            if (message == MID_MODELSTRINGKEY)
            {
                seen.emplace_back("removed");
                game.objects.remove(h);
            }
        };
        CHECK(game.step(.1));
        CHECK(!game.objects.get(h) && seen.size() == 1);
        callback = {};
        return true;
    }
    WorldModel floor_model()
    {
        WorldModel w;
        w.points = {{-100, 0, -100}, {100, 0, -100}, {100, 0, 100}, {-100, 0, 100}};
        w.surfaces.resize(1);
        w.surfaces[0].flags = 1;
        w.polygons = {{0, {0, 1, 2, 3}}};
        return w;
    }
    bool collision_queries()
    {
        CollisionWorld world;
        std::string error;
        CHECK(world.build(floor_model(), error));
        CHECK(world.triangles() == 2);
        CollisionHit hit;
        CHECK(world.sweep({0, 10, 0}, {0, -10, 0}, {1, 2, 1}, hit) == CollisionQuery::Hit);
        CHECK(std::abs(hit.fraction - .4) < 1e-5 && hit.normal.y == 1);
        CHECK(world.sweep({0, 10, 0}, {0, -100000, 0}, {}, hit) ==
              CollisionQuery::Hit); // Exact shared diagonal; no crack.
        CHECK(world.sweep({0, 2, 0}, {20, 2, 0}, {1, 2, 1}, hit) == CollisionQuery::Miss);
        CHECK(world.sweep({0, 2, 0}, {0, 5, 0}, {1, 2, 1}, hit) == CollisionQuery::Miss);
        CHECK(world.sweep({0, 1, 0}, {0, -1, 0}, {1, 2, 1}, hit) == CollisionQuery::Hit && hit.start_solid);
        CHECK(world.sweep({200, 10, 0}, {200, -10, 0}, {1, 2, 1}, hit) == CollisionQuery::Miss);
        CHECK(world.sweep({0, 10, 0}, {0, -10, 0}, {-1, 2, 1}, hit) == CollisionQuery::Invalid);
        CHECK(sweep_box({-20, 0, 0}, {20, 0, 0}, {1, 1, 1}, {0, 0, 0}, {1, 1, 1}, hit) == CollisionQuery::Hit);
        CHECK(std::abs(hit.fraction - .45) < 1e-5 && hit.normal.x == -1);
        CHECK(sweep_box({0, 0, 0}, {1, 0, 0}, {1, 1, 1}, {0, 0, 0}, {1, 1, 1}, hit) == CollisionQuery::Hit &&
              hit.start_solid);
        auto w = floor_model();
        w.surfaces[0].flags = 0;
        CHECK(world.build(w, error));
        CHECK(world.triangles() == 0);
        w.surfaces[0].flags = 1;
        w.polygons[0].points[0] = 100;
        CHECK(!world.build(w, error) && world.triangles() == 0);
        return true;
    }
    WorldModel cabin_model(bool wall = false)
    {
        WorldModel w;
        w.points = {{-100, 0, -100}, {100, 0, -100},  {100, 0, 100}, {-100, 0, 100}, {-100, 10, -100}, {100, 10, -100},
                    {100, 10, 100},  {-100, 10, 100}, {0, 0, -100},  {0, 0, 100},    {0, 10, 100},     {0, 10, -100}};
        w.surfaces.resize(1);
        w.surfaces[0].flags = 1;
        // DAT winding agrees with the outward plane normal: the floor faces up, the ceiling down.
        w.polygons = {{0, {0, 3, 2, 1}}, {0, {4, 5, 6, 7}}};
        if (wall)
            w.polygons.push_back({0, {8, 9, 10, 11}});
        return w;
    }
    bool start_departure()
    {
        CollisionWorld world;
        std::string error;
        CHECK(world.build(cabin_model(), error));
        CollisionHit hit;
        const DVector start{0, 9, 0}, half{1, 2, 1};
        // A box crossing the ceiling by 2 units blocks every move by default.
        CHECK(world.sweep(start, {0, 4, 0}, half, hit) == CollisionQuery::Hit && hit.start_solid);
        CHECK(hit.departed == 0);
        // Leaving the overlapped ceiling along its outward normal; both triangles are counted.
        for (auto mode : {StartOverlap::Leave, StartOverlap::LeaveOrSlide})
        {
            CHECK(world.sweep(start, {0, 4, 0}, half, hit, 0, mode) == CollisionQuery::Miss && hit.departed == 2);
            // The floor still stops the same move.
            CHECK(world.sweep(start, {0, -5, 0}, half, hit, 0, mode) == CollisionQuery::Hit);
            CHECK(!hit.start_solid && hit.normal.y == 1 && std::abs(hit.fraction - .5) < 1e-5 && hit.departed == 2);
            // Moving deeper or not at all still fails.
            for (auto end : {DVector{0, 12, 0}, DVector{3, 10, 0}, start})
                CHECK(world.sweep(start, end, half, hit, 0, mode) == CollisionQuery::Hit && hit.start_solid);
        }
        // Sliding parallel to a deep overlap is only free for LeaveOrSlide.
        CHECK(world.sweep(start, {5, 9, 0}, half, hit, 0, StartOverlap::Leave) == CollisionQuery::Hit &&
              hit.start_solid);
        CHECK(world.sweep(start, {5, 9, 0}, half, hit, 0, StartOverlap::LeaveOrSlide) == CollisionQuery::Miss);

        GameSession g;
        CHECK(setup(g));
        CHECK(g.collision.build(cabin_model(), error));
        g.server.collision_ready = true;
        auto h = probe(g);
        auto* r = g.objects.get(h);
        r->dimensions = half;
        r->state.m_Pos = start;
        r->state.m_Flags = FLAG_SOLID;
        // MoveObject keeps the explicit failure for deeper or sliding solid moves.
        auto up = DVector{0, 12, 0}, side = DVector{5, 9, 0};
        CHECK(g.server.MoveObject(h, &up) == DE_ERROR && r->state.m_Pos.y == 9 && r->start_departures == 0);
        CHECK(g.server.MoveObject(h, &side) == DE_ERROR && r->state.m_Pos.x == 0);
        // Solid characters also retain sub-position gravity velocity without turning
        // an unrepresentable displacement into a failed placement check.
        r->state.m_Flags |= FLAG_GRAVITY;
        r->character_body = true;
        auto solid_still = r->state.m_Pos;
        CHECK(g.server.advance_physics(1e-7) && r->velocity.y < 0);
        CHECK(r->state.m_Pos.y == solid_still.y && r->start_departures == 0);
        CHECK(g.server.MoveObject(h, &solid_still) == DE_ERROR);
        CHECK(g.server.advance_physics(1. / 60) && r->state.m_Pos.y < solid_still.y && r->start_departures > 0);
        r->state.m_Flags = FLAG_SOLID;
        r->character_body = false;
        r->state.m_Pos = start;
        r->velocity = {};
        r->start_departures = 0;
        // The original placement helper lowers a solid body out of the ceiling it starts through.
        CHECK(g.server.MoveObjectToGround(h) == DE_OK);
        CHECK(std::abs(r->state.m_Pos.y - 2) < .01f && r->start_departures == 2 && r->grounded);
        // A non-solid passive object resting through a wall can settle and slide along it.
        CHECK(g.collision.build(cabin_model(true), error));
        r->state.m_Flags = FLAG_GRAVITY;
        r->state.m_Pos = {0.5f, 5, 0};
        r->start_departures = 0;
        // A first interactive frame may be shorter than float position precision.
        // Passive integration retains gravity velocity without a zero-length
        // placement sweep; explicit sweeps must still report the authored overlap.
        auto still = r->state.m_Pos;
        CHECK(g.server.MoveObject(h, &still) == DE_ERROR);
        CHECK(g.server.advance_physics(1e-7));
        CHECK(r->state.m_Pos.x == still.x && r->state.m_Pos.y == still.y && r->state.m_Pos.z == still.z &&
              r->velocity.y < 0 && r->start_departures == 0);
        CHECK(g.server.advance_physics(1. / 60));
        CHECK(r->state.m_Pos.y < still.y && r->start_departures > 0);
        auto down = DVector{0.5f, 1, 0};
        CHECK(g.server.MoveObject(h, &down) == DE_OK && std::abs(r->state.m_Pos.y - 2) < .01f);
        auto along = DVector{0.5f, 2.001f, 30};
        CHECK(g.server.MoveObject(h, &along) == DE_OK && r->state.m_Pos.z > 29.9f && r->start_departures >= 2);
        // Moving further into the wall's solid (+x) side is still refused.
        auto into = DVector{3, 2.001f, 30};
        CHECK(g.server.MoveObject(h, &into) == DE_ERROR && r->state.m_Pos.x == .5f);
        // An authored solid body 1.5 units through the wall's front face (03_Museum's Cultist
        // "pud"): the placement helper nudges it out along the face normal, then lowers it.
        r->state.m_Flags = FLAG_SOLID;
        r->dimensions = {2, 2, 2};
        r->state.m_Pos = {.5f, 5, 0};
        CHECK(g.server.MoveObjectToGround(h) == DE_OK && r->placement_nudges == 1);
        CHECK(std::abs(r->placement_nudge.x - 1.501f) < .001f && std::abs(r->state.m_Pos.x - 2.001f) < .001f &&
              std::abs(r->state.m_Pos.y - 2) < .01f);
        // Deeper than the smaller horizontal half extent: nothing moves, the overlap stays explicit.
        // The polygon separates towards its nearer side; a narrow body (half z 0.25) needing
        // 1 unit along x exceeds its smaller half extent.
        r->dimensions = {2, 2, .25f};
        r->state.m_Pos = {-1, 5, 0};
        CHECK(g.server.MoveObjectToGround(h) == DE_ERROR && r->placement_nudges == 1 && r->state.m_Pos.x == -1 &&
              r->state.m_Pos.y == 5);
        CHECK(g.server.ResolveInitialWorldOverlap(h) == DE_ERROR && r->placement_nudges == 1 &&
              r->state.m_Pos.x == -1 && r->state.m_Pos.y == 5);
        // Spawn placement uses the same bounded separation without the world-only ground drop.
        r->dimensions = {2, 2, 2};
        r->state.m_Pos = {.5f, 5, 0};
        r->velocity = {0, 500, 0};
        CHECK(g.server.ResolveInitialWorldOverlap(h) == DE_OK && r->placement_nudges == 2);
        CHECK(std::abs(r->state.m_Pos.x - 2.001f) < .001f && r->state.m_Pos.y == 5 && r->velocity.y == 500);
        CHECK(g.server.ResolveInitialWorldOverlap(h) == DE_OK && r->placement_nudges == 2);
        // A center exactly on the wall has two equally near sides. The +x side
        // is obstructed by a second wall: only the complete opposite solve may commit.
        auto corridor = cabin_model(true);
        auto add_wall = [&](float x)
        {
            const auto begin = static_cast<std::uint32_t>(corridor.points.size());
            for (auto p : {DVector{x, 0, -100}, DVector{x, 0, 100}, DVector{x, 10, 100}, DVector{x, 10, -100}})
                corridor.points.push_back(p);
            corridor.polygons.push_back({0, {std::uint16_t(begin), std::uint16_t(begin + 1),
                                              std::uint16_t(begin + 2), std::uint16_t(begin + 3)}});
        };
        add_wall(3);
        CHECK(g.collision.build(corridor, error));
        r->state.m_Pos = {0, 5, 0};
        CHECK(g.server.ResolveInitialWorldOverlap(h) == DE_OK && r->placement_nudges == 3);
        CHECK(std::abs(r->state.m_Pos.x + 2.001f) < .001f && r->state.m_Pos.y == 5 && r->velocity.y == 500);
        CHECK(g.collision.sweep(r->state.m_Pos, r->state.m_Pos, r->dimensions, hit) == CollisionQuery::Miss);
        // Both directions blocked: retain the authored pose and counters atomically.
        add_wall(-3);
        CHECK(g.collision.build(corridor, error));
        r->state.m_Pos = {0, 5, 0};
        CHECK(g.server.ResolveInitialWorldOverlap(h) == DE_ERROR && r->state.m_Pos.x == 0 && r->state.m_Pos.y == 5 &&
              r->placement_nudges == 3 && r->velocity.y == 500);
        return true;
    }
    bool collision_traversal()
    {
        WorldModel model;
        model.surfaces.resize(1);
        model.surfaces[0].flags = 1;
        for (unsigned x = 0; x < 32; ++x)
            for (unsigned z = 0; z < 32; ++z)
            {
                auto first = static_cast<std::uint32_t>(model.points.size());
                const float px = x * 10, pz = z * 10;
                model.points.insert(
                    model.points.end(),
                    {{px - 2, 0, pz - 2}, {px + 2, 0, pz - 2}, {px + 2, 0, pz + 2}, {px - 2, 0, pz + 2}});
                model.polygons.push_back(
                    {0,
                     {static_cast<std::uint16_t>(first), static_cast<std::uint16_t>(first + 1),
                      static_cast<std::uint16_t>(first + 2), static_cast<std::uint16_t>(first + 3)}});
            }
        CollisionWorld world;
        std::string error;
        CHECK(world.build(model, error));
        CollisionHit hit;
        // The segment's enclosing box overlaps the entire grid. Only its actual
        // path near y=0 requires expensive triangle tests, even without a hit.
        CHECK(world.sweep({0, 50, 0}, {310, -50, 310}, {}, hit) == CollisionQuery::Miss);
        CHECK(world.work().triangle_tests < 128 && world.work().node_rejections > 0);
        CHECK(world.sweep({0, 50, 0}, {300, -50, 300}, {}, hit) == CollisionQuery::Hit);
        CHECK(hit.fraction == .5 && hit.polygon == 15 * 32 + 15 && hit.normal.y == 1);
        // Nearest-hit pruning must still examine equal-time and penetrating
        // contacts. Source polygon zero wins independently of BVH order.
        CHECK(world.sweep({155, 0, 155}, {155, 0, 155}, {500, 1, 500}, hit) == CollisionQuery::Hit);
        CHECK(hit.start_solid && hit.fraction == 0 && hit.polygon == 0);
        CHECK(world.transform({}, {}, {0, 20, 0}));
        CHECK(world.sweep({150, 50, 150}, {150, -50, 150}, {}, hit) == CollisionQuery::Hit);
        CHECK(hit.fraction == .3 && std::abs(hit.point.y - 20) < 1e-5);
        CHECK(world.work().triangle_bounds_tests == world.work().triangle_tests + world.work().triangle_rejections);
        world.clear();
        CHECK(world.work().node_tests == 0 && world.work().triangle_tests == 0);
        CHECK(world.sweep({}, {}, {}, hit) == CollisionQuery::Miss && world.work().node_tests == 0);
        CHECK(world.build(model, error) && world.work().triangle_bounds_tests == 0);
        CHECK(world.sweep({}, {}, {-1, 0, 0}, hit) == CollisionQuery::Invalid && world.work().node_tests == 0);
        model.polygons[0].points[0] = UINT16_MAX;
        CHECK(!world.build(model, error) && world.nodes() == 0 && world.work().node_tests == 0);
        return true;
    }
    bool movement_contacts()
    {
        GameSession game;
        CHECK(setup(game));
        std::string error;
        CHECK(game.collision.build(floor_model(), error));
        game.server.collision_ready = true;
        auto h = probe(game);
        auto* r = game.objects.get(h);
        r->dimensions = {1, 2, 1};
        r->state.m_Pos = {0, 10, 0};
        r->state.m_Flags = FLAG_SOLID | FLAG_GRAVITY;
        CHECK(game.step(.1));
        CHECK(r->grounded && std::abs(r->state.m_Pos.y - 2.001f) < .002f && r->velocity.y == 0);
        auto destination = DVector{20, -2, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_OK);
        CHECK(r->state.m_Pos.x > 19.99 && r->state.m_Pos.y > 2);
        auto sensor = probe(game);
        auto* s = game.objects.get(sensor);
        s->state.m_Pos = {30, 2, 0};
        s->dimensions = {1, 4, 4};
        s->state.m_Flags = FLAG_TOUCH_NOTIFY;
        int touches = 0;
        callback = [&](BaseClass& o, DDWORD msg, void* data)
        {
            if (msg == MID_TOUCHNOTIFY)
            {
                ++touches;
                if (o.m_hObject == sensor && data == h)
                    game.objects.remove(sensor);
            }
        };
        destination = {40, 2.001f, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_OK);
        CHECK(touches == 1 && game.objects.get(sensor)->life == Life::Pending);
        CHECK(game.objects.finish_frame() && !game.objects.get(sensor));
        auto box = probe(game);
        auto* b = game.objects.get(box);
        b->state.m_Pos = {50, 2, 0};
        b->dimensions = {1, 4, 4};
        b->state.m_Flags = FLAG_SOLID;
        destination = {100, 2.001f, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_OK);
        CHECK(r->state.m_Pos.x < 48.001f && r->contact_object == box);
        callback = {};
        game.unload_world();
        CHECK(!game.server.collision_ready);
        return true;
    }
    // Float-rounded contacts can leave a body a few ulps inside an adjacent
    // face. MoveObject tolerates such overlaps without ever moving deeper;
    // default sweeps and deeper overlaps keep their start-solid semantics.
    bool contact_overlap()
    {
        CollisionWorld world;
        std::string error;
        CHECK(world.build(floor_model(), error));
        CollisionHit hit;
        const DVector half{1, 2, 1}, inside{0, 2 - 1e-4f, 0};
        CHECK(world.sweep(inside, {20, inside.y, 0}, half, hit) == CollisionQuery::Hit && hit.start_solid);
        CHECK(world.sweep(inside, {20, inside.y, 0}, half, hit, .01) == CollisionQuery::Miss);
        CHECK(world.sweep(inside, {0, 5, 0}, half, hit, .01) == CollisionQuery::Miss);
        CHECK(world.sweep(inside, inside, half, hit, .01) == CollisionQuery::Miss);
        CHECK(world.sweep(inside, {5, 0, 0}, half, hit, .01) == CollisionQuery::Hit);
        CHECK(hit.allowed_overlap && !hit.start_solid && hit.fraction == 0 && hit.normal.y == 1);
        CHECK(std::abs(hit.penetration - 1e-4) < 1e-5 && hit.polygon == 0);
        CHECK(world.sweep({0, 1.95f, 0}, {20, 1.95f, 0}, half, hit, .01) == CollisionQuery::Hit && hit.start_solid);
        CHECK(sweep_box({1.9999f, 0, 0}, {5, 0, 0}, half, {}, half, hit, .01) == CollisionQuery::Miss);
        CHECK(sweep_box({1.9999f, 0, 0}, {-5, 0, 0}, half, {}, half, hit, .01) == CollisionQuery::Hit);
        CHECK(hit.allowed_overlap && hit.fraction == 0 && hit.normal.x == 1);
        CHECK(sweep_box({1.9999f, 0, 0}, {-5, 0, 0}, half, {}, half, hit) == CollisionQuery::Hit && hit.start_solid);

        GameSession game;
        CHECK(setup(game));
        // CityHub-like converging corner: wall A faces -Z and wall B faces
        // (-1, 0, 2)/sqrt(5); the box fits exactly at x=1108, z=2484.
        WorldModel w;
        w.surfaces.resize(1);
        w.surfaces[0].flags = 1;
        w.points = {{900, 0, 2300},  {1400, 0, 2300}, {1400, 0, 2600},   {900, 0, 2600},
                    {1000, 0, 2496}, {1300, 0, 2496}, {1300, 200, 2496}, {1000, 200, 2496},
                    {1000, 0, 2412}, {1168, 0, 2496}, {1168, 200, 2496}, {1000, 200, 2412}};
        w.polygons = {{0, {0, 1, 2, 3}}, {0, {4, 5, 6, 7}}, {0, {8, 9, 10, 11}}};
        CHECK(game.collision.build(w, error));
        game.server.collision_ready = true;
        auto h = probe(game);
        auto* r = game.objects.get(h);
        r->dimensions = {12, 42, 12};
        r->state.m_Pos = {1050, 42.001f, 2470};
        r->state.m_Flags = FLAG_SOLID;
        std::mt19937 random(7);
        std::uniform_real_distribution<float> unit(-1, 1);
        for (int i = 0; i < 2000; ++i)
        {
            auto destination = vector_add(r->state.m_Pos, {2.8f + unit(random), -.07f, unit(random) * 2});
            CHECK(game.server.MoveObject(h, &destination) == DE_OK);
            // Never deeper than the tolerance.
            CHECK(game.collision.sweep(r->state.m_Pos, r->state.m_Pos, r->dimensions, hit) == CollisionQuery::Miss ||
                  hit.penetration <= .01);
        }
        CHECK(r->state.m_Pos.x > 1100 && r->state.m_Pos.x < 1108.01f);
        // A body slightly inside the floor slides freely and is lifted by a
        // downward move; the contact is inspectable.
        game.collision.clear();
        CHECK(game.collision.build(floor_model(), error));
        r->dimensions = half;
        r->state.m_Pos = inside;
        const auto contacts = r->overlap_contacts;
        auto destination = DVector{10, inside.y, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_OK);
        CHECK(r->state.m_Pos.x == 10 && r->state.m_Pos.y == inside.y && r->overlap_contacts == contacts);
        // The whole tolerated overlap is lifted, not only the usual 0.001.
        r->state.m_Pos.y = 2 - .005f;
        destination = {12, 1, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_OK);
        CHECK(r->state.m_Pos.y > 2 && r->state.m_Pos.y < 2.002f && r->overlap_contacts == contacts + 1);
        CHECK(std::abs(r->last_overlap_depth - .005f) < 1e-5f && r->contact_normal.y == 1);
        // Real penetration still fails visibly and names its contact.
        r->state.m_Pos = {0, 1.5f, 0};
        destination = {5, 1.5f, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_ERROR);
        CHECK(!game.objects.diagnostics().empty() &&
              game.objects.diagnostics().back().find("world polygon 0, depth 0.5") != std::string::npos);
        // A tolerated overlap (floor, polygon 0) loses the equal-time tie and
        // cannot mask a deep wall overlap (polygon 1).
        auto walled = floor_model();
        walled.points.insert(walled.points.end(), {{.5f, 0, -100}, {.5f, 100, -100}, {.5f, 100, 100}, {.5f, 0, 100}});
        walled.polygons.push_back({0, {4, 5, 6, 7}});
        game.collision.clear();
        CHECK(game.collision.build(walled, error));
        CHECK(game.collision.sweep(inside, {0, 1, 0}, half, hit, .01) == CollisionQuery::Hit);
        CHECK(hit.start_solid && hit.polygon == 1);
        r->state.m_Pos = inside;
        destination = {0, 1, 0};
        CHECK(game.server.MoveObject(h, &destination) == DE_ERROR);
        CHECK(game.objects.diagnostics().back().find("world polygon 1, depth 0.5") != std::string::npos);
        game.unload_world();
        return true;
    }
    WorldModel placement_corner(bool cage = false)
    {
        auto w = floor_model();
        w.points.insert(w.points.end(), {{0, 0, -100}, {0, 100, -100}, {0, 100, 100}, {0, 0, 100}});
        w.polygons.push_back({0, {4, 5, 6, 7}});
        if (cage)
        {
            w.points.insert(w.points.end(), {{4, 0, -100}, {4, 100, -100}, {4, 100, 100}, {4, 0, 100}});
            w.polygons.push_back({0, {8, 9, 10, 11}});
        }
        return w;
    }
    bool placement_queries()
    {
        CollisionWorld w;
        std::string error;
        CHECK(w.build(placement_corner(), error));
        auto p = w.place_body({4, 4, 0}, {1, 1, 1}, {4, 2, 0}, {3, 2, 1});
        CHECK(p.result == BodyPlacementResult::Placed && !p.corrections && p.candidate.y == 2);
        p = w.place_body({4, 4, 0}, {1, 1, 1}, {2, 1, 0}, {3, 2, 1});
        CHECK(p.result == BodyPlacementResult::Adjusted && p.corrections == 2 && p.distance < 2.01);
        CHECK(std::abs(p.candidate.x - 3.001) < .0001 && std::abs(p.candidate.y - 2.001) < .0001);
        CollisionHit hit;
        CHECK(w.sweep(p.candidate, p.candidate, {3, 2, 1}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep({4, 4, 0}, p.candidate, {}, hit) == CollisionQuery::Miss);
        // The nearest escape is below the floor. Retain the clear source side instead.
        p = w.place_body({4, 4, 0}, {1, 1, 1}, {2, -.25f, 0}, {3, 2, 4});
        CHECK(p.result == BodyPlacementResult::Adjusted && p.candidate.y > 2 && p.candidate.x > 3);
        CHECK(w.sweep(p.candidate, p.candidate, {3, 2, 4}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep({4, 4, 0}, p.candidate, {}, hit) == CollisionQuery::Miss);
        CHECK(w.place_body({4, 4, 0}, {1, 1, 1}, {-4, 2, 0}, {1, 1, 1}).result == BodyPlacementResult::Blocked);
        CHECK(w.place_body({.5, 4, 0}, {1, 1, 1}, {4, 2, 0}, {1, 1, 1}).result == BodyPlacementResult::Blocked);
        CHECK(w.place_body({NAN, 4, 0}, {1, 1, 1}, {}, {1, 1, 1}).result == BodyPlacementResult::Invalid);
        CHECK(w.place_body({4, 4, 0}, {0, 1, 1}, {}, {1, 1, 1}).result == BodyPlacementResult::Invalid);
        CHECK(w.place_body({4, 4, 0}, {1, 1, 1}, {}, {-1, 1, 1}).result == BodyPlacementResult::Invalid);
        CHECK(w.build(placement_corner(true), error));
        // An exact-width corridor permits touching faces, just like sweep().
        // The old extra .001 skin oscillated between its opposite walls.
        p = w.place_body({2.5f, 4, 0}, {1, 1, 1}, {2.5f, 4, 0}, {2, 1, 1});
        CHECK(p.result == BodyPlacementResult::Adjusted && p.touching_fit && p.corrections == 1 && p.distance == .5 &&
              p.candidate.x == 2);
        CHECK(w.sweep(p.candidate, p.candidate, {2, 1, 1}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep(p.anchor, p.candidate, {}, hit) == CollisionQuery::Miss);
        // Multiple constraints: solve the tight corridor before the lower floor
        // polygon. An exact fit must still resolve every other overlapping face.
        auto tight = placement_corner(true);
        std::rotate(tight.polygons.begin(), tight.polygons.begin() + 1, tight.polygons.end());
        CHECK(w.build(tight, error));
        p = w.place_body({2.5f, 4, 0}, {1, 1, 1}, {2.5f, .5f, 0}, {2, 1, 1});
        CHECK(p.result == BodyPlacementResult::Adjusted && p.touching_fit && p.corrections == 2 && p.candidate.x == 2 &&
              p.candidate.y == 1);
        CHECK(w.sweep(p.candidate, p.candidate, {2, 1, 1}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep(p.anchor, p.candidate, {}, hit) == CollisionQuery::Miss);
        // Even slight real overlap must still fail; never shrink the original body.
        p = w.place_body({2, 4, 0}, {1, 1, 1}, {2, 4, 0}, {2.0001f, 1, 1});
        CHECK(p.result == BodyPlacementResult::Blocked && !p.touching_fit);
        p = w.place_body({2, 4, 0}, {1, 1, 1}, {2, 4, 0}, {3, 1, 1});
        CHECK(p.result == BodyPlacementResult::Blocked && p.corrections <= 16 && p.distance <= 3);
        CHECK(p.candidates <= 145);
        // Short seat backs admit a wider corpse above them. The shortest X
        // corrections oscillate; the source-side Y separation resolves both.
        auto seats = placement_corner(true);
        for (auto& point : seats.points)
            if (point.y == 100)
                point.y = 2;
        CHECK(w.build(seats, error));
        p = w.place_body({2, 4, 0}, {1, 1, 1}, {2, 1, 0}, {3, 1, 1});
        CHECK(p.result == BodyPlacementResult::Adjusted && p.touching_fit && p.candidate.y >= 3 &&
              p.corrections <= 16 && p.distance <= 3 && p.candidates <= 145);
        CHECK(w.sweep(p.candidate, p.candidate, {3, 1, 1}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep(p.anchor, p.candidate, {}, hit) == CollisionQuery::Miss);
        // A living body straddles the end of a narrow doorway. The wider corpse
        // fits past the jambs, but neither full-source X separation can find it.
        // This fixture is generated geometry, not an extracted retail world.
        auto doorway = floor_model();
        doorway.points.insert(
            doorway.points.end(),
            {{0, 0, -4}, {0, 16, -4}, {0, 16, -2}, {0, 0, -2}, {-10, 0, -2}, {-8, 0, -2}, {-8, 16, -2}, {-10, 16, -2}});
        doorway.polygons.push_back({0, {4, 5, 6, 7}});
        doorway.polygons.push_back({0, {8, 9, 10, 11}});
        CHECK(w.build(doorway, error));
        const DVector source_half{3.01f, 4, 3.01f}, death_half{5, 2, 5};
        p = w.place_body({-4, 5, 1}, source_half, {-4, 2, 1}, death_half);
        CHECK(p.result == BodyPlacementResult::Adjusted && p.source_sweep && p.candidate.z >= 3 && p.distance <= 5 &&
              p.candidates <= 145);
        CHECK(w.sweep(p.candidate, p.candidate, death_half, hit) == CollisionQuery::Miss);
        CHECK(w.sweep(p.anchor, p.candidate, {}, hit) == CollisionQuery::Miss);
        CHECK(w.sweep(p.anchor, vector_add(p.anchor, vector_subtract(p.candidate, p.requested)), source_half, hit) ==
              CollisionQuery::Miss);
        // The short corpse can fit beneath a lintel that the living source box
        // cannot sweep through. Center-only separation must still refuse it.
        doorway.points.insert(doorway.points.end(), {{-20, 8, 4.1f}, {20, 8, 4.1f}, {20, 8, 12}, {-20, 8, 12}});
        doorway.polygons.push_back({0, {12, 13, 14, 15}});
        CHECK(w.build(doorway, error));
        CHECK(w.sweep({-4, 2, 3}, {-4, 2, 3}, death_half, hit) == CollisionQuery::Miss);
        CHECK(w.sweep({-4, 5, 1}, {-4, 5, 3}, source_half, hit) == CollisionQuery::Hit);
        p = w.place_body({-4, 5, 1}, source_half, {-4, 2, 1}, death_half);
        CHECK(p.result == BodyPlacementResult::Blocked && p.candidates <= 145);
        return true;
    }
    bool placement_service()
    {
        GameSession g;
        CHECK(setup(g));
        std::string error;
        CHECK(g.collision.build(placement_corner(), error));
        g.server.collision_ready = true;
        auto h = probe(g);
        auto* r = g.objects.get(h);
        CHECK(r);
        r->state.m_ObjectType = OT_MODEL;
        r->state.m_Flags = FLAG_GRAVITY;
        r->state.m_Pos = {4, 4, 0};
        r->dimensions = {3, 2, 1};
        CHECK(g.server.ConfigureCharacterBody(h, 200, 9));
        BodyPlacement p;
        CHECK(g.server.PlaceNonSolidModel(h, {2, 1, 0}, {4, 4, 0}, {1, 1, 1}, p));
        CHECK(p.result == BodyPlacementResult::Adjusted && r->state.m_Pos.x == p.candidate.x && r->dimensions.x == 3 &&
              r->dimensions.y == 2 && g.objects.time() == 0);
        CHECK(g.server.advance_physics(.01)); // Corrected body survives ordinary single gravity integration.
        const auto before = r->state.m_Pos;
        CHECK(!g.server.PlaceNonSolidModel(h, {-4, 2, 0}, {4, 4, 0}, {1, 1, 1}, p));
        CHECK(p.result == BodyPlacementResult::Blocked && r->state.m_Pos.x == before.x && r->state.m_Pos.y == before.y);
        g.server.collision_ready = false;
        CHECK(!g.server.PlaceNonSolidModel(h, {}, {}, {1, 1, 1}, p) && p.result == BodyPlacementResult::Unavailable);
        g.server.collision_ready = true;
        r->state.m_Flags |= FLAG_SOLID;
        CHECK(!g.server.PlaceNonSolidModel(h, {}, {}, {1, 1, 1}, p) && p.result == BodyPlacementResult::Invalid);
        r->state.m_Flags &= ~FLAG_SOLID;
        CHECK(g.objects.remove(h) && !g.server.PlaceNonSolidModel(h, {}, {}, {1, 1, 1}, p));
        CHECK(g.objects.finish_frame() && !g.server.PlaceNonSolidModel(h, {}, {}, {1, 1, 1}, p));
        return true;
    }
    bool original_services()
    {
        GameSession game;
        CHECK(setup(game));
        std::string long_music(250, 'a');
        auto h = game.objects.create(
            game.objects.find_class("WorldProperties"), {}, Creation::World,
            {{"WorldName", std::string("Test map")}, {"Gravity", DVector{0, -500, 0}}, {"AmbientList", long_music}});
        CHECK(h && dynamic_cast<WorldProperties*>(game.objects.get(h)->game.get()));
        CHECK(game.objects.get(h)->activity == OBJSTATE_INACTIVE);
        CHECK(std::string(game.server.GetVarValueString(game.server.GetGameConVar("WorldName"))) == "Test map");
        CHECK(std::string(game.server.GetVarValueString(game.server.GetGameConVar("AmbientList"))) == long_music);
        CHECK(game.server.gravity.y == -500);
        auto variable = game.server.GetGameConVar("FogFarZ");
        CHECK(game.server.GetVarValueFloat(variable) == 5000);
        game.server.SetGameConVar("FogFarZ", "4200");
        CHECK(game.server.GetGameConVar("FogFarZ") == variable && game.server.GetVarValueFloat(variable) == 4200);
        auto smell = game.objects.create(game.objects.find_class("SmellHint"));
        CHECK(smell && SmellHint::m_dwNumSmells == 1 && SmellHint::HandleToLink(smell));
        CHECK(game.step(19) && game.objects.get(smell));
        CHECK(game.step(1) && !game.objects.get(smell) && SmellHint::m_dwNumSmells == 0);
        game.unload_world();
        CHECK(std::string(game.server.GetVarValueString(variable)).empty());
        CHECK(game.server.gravity.y == -2000);
        smell = game.objects.create(game.objects.find_class("SmellHint"));
        CHECK(smell && SmellHint::m_dwNumSmells == 1);
        game.unload_world();
        CHECK(SmellHint::m_dwNumSmells == 0);
        auto rejected =
            game.objects.create(game.objects.find_class("SmellHint"), {}, Creation::World, {{"DifficultyMed", false}});
        CHECK(rejected);
        CHECK(game.objects.finish_frame() && SmellHint::m_dwNumSmells == 0);
        return true;
    }
    // GameStartPoint 05_egress lies 112 units above the endelevator floor; the
    // standing box crosses the car ceiling until MoveObjectToGround lowers it.
    bool elevator_entry(const char* root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = path_from_utf8(root);
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        CHECK(game.request_world("05_cdmb", error));
        if (!game.enter_player(error))
        {
            std::cerr << error << '\n';
            return false;
        }
        const auto* player = game.objects.get(game.client_shell.player);
        CHECK(player && player->life == Life::Alive && player->state.m_Pos.x == 0 && player->state.m_Pos.z == -1520);
        const float feet = player->state.m_Pos.y - player->dimensions.y;
        std::cout << "05_CDMB entry at " << player->state.m_Pos.y << ", feet " << feet << ", departed faces "
                  << player->start_departures << '\n';
        CHECK(player->start_departures > 0 && std::abs(feet + 5568) < 5);
        CHECK(game.leave_player() && game.unload_world());
        return true;
    }
    bool private_services(const char* root)
    {
        GameSession game;
        CHECK(setup(game));
        Config config;
        config.data_dir = path_from_utf8(root);
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        CHECK(game.request_world("01_SubwayA", error));
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        legacy_copy(state.m_Filename, "Models\\Characters\\Caleb.abc");
        state.m_Flags = FLAG_MODELKEYS;
        bool initialized = false;
        callback = [&](BaseClass& o, DDWORD message, void*)
        {
            if (message == MID_INITIALUPDATE)
                initialized = game.server.GetAnimIndex(o.m_hObject, "idle1") == 0;
        };
        auto h = game.objects.create(&probe_class, state);
        CHECK(h && initialized);
        auto* r = game.objects.get(h);
        CHECK(r->model && r->model_error.empty());
        CHECK(r->model->nodes.size() == 20 && r->model->animations.size() == 86 && r->model->vertices.size() == 784);
        std::shared_ptr<const Model> same;
        CHECK(game.models.load("models/characters/caleb.abc", same, error) && same == r->model);
        auto crawl = game.server.GetAnimIndex(h, "crawl_pistol");
        DVector dims;
        CHECK(game.server.GetModelAnimUserDims(h, &dims, crawl) == DE_OK && dims.y == 28);
        CHECK(same->nodes.back().deformed_vertices.size() == 189);
        int keys = 0;
        callback = [&](BaseClass&, DDWORD message, void*)
        {
            if (message == MID_MODELSTRINGKEY)
                ++keys;
        };
        std::vector<NodePose> pose;
        for (std::uint32_t i = 0; i < same->animations.size(); ++i)
        {
            game.server.SetModelAnimation(h, i);
            game.server.ResetModelAnimation(h);
            game.server.SetModelLooping(h, DFALSE);
            // The loaded world now includes authored emitters overlapping conveyors.
            // Preserve the entire clip/session duration while respecting the existing
            // 0.5-second physics catch-up bound; do not raise the engine limit.
            double remaining = same->animations[i].duration / 1000.0 + .001;
            while (remaining > 0)
            {
                const double step = std::min(remaining, .5);
                CHECK(game.step(step));
                remaining -= step;
            }
            CHECK(game.server.GetModelPlaybackState(h) == MS_PLAYDONE);
            CHECK(sample_pose(*same, i, same->animations[i].duration / 2.0, pose) && pose.size() == 20);
            std::vector<DVector> vertices;
            CHECK(sample_vertices(*same, i, same->animations[i].duration / 2.0, vertices));
            for (const auto& face : same->faces)
                for (auto index : face.vertices)
                {
                    auto p = vertices[index];
                    CHECK(physics_vector(p));
                    CHECK(p.x > -200 && p.x < 200 && p.y > -200 && p.y < 200 && p.z > -200 && p.z < 200);
                }
        }
        CHECK(keys > 0);
        CHECK(game.collision.triangles() > 1000);
        DVector spawn{};
        for (const auto& entry : game.entries)
            if (auto* object = game.objects.get(entry.object); object && object->type->name == "GameStartPoint")
                spawn = object->state.m_Pos;
        std::cout << "Original start point: " << spawn.x << "," << spawn.y << "," << spawn.z << "\n";
        CollisionHit hit;
        auto q = game.collision.sweep(spawn, vector_add(spawn, {0, -1000, 0}), {12, 42, 12}, hit);
        CHECK(q == CollisionQuery::Hit);
        CHECK(!hit.start_solid);
        std::cout << "Caleb: " << same->vertices.size() << " vertices, " << same->nodes.size() << " nodes, "
                  << same->animations.size() << " animations; dispatched commands: " << keys << "\n";
        std::cout << "Primary world: " << game.collision.triangles() << " solid triangles, " << game.collision.nodes()
                  << " BVH nodes; probe contact y=" << hit.point.y << " polygon=" << hit.polygon << "\n";
        callback = {};
        game.unload_world();
        CHECK(!game.objects.get(h) && !game.server.collision_ready);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string name = argv[1];
    bool ok = false;
    if (name == "model_reader")
        ok = model_reader();
    else if (name == "model_rejection")
        ok = model_rejection();
    else if (name == "animation_events")
        ok = animation_events();
    else if (name == "model_callbacks")
        ok = model_callbacks();
    else if (name == "collision_queries")
        ok = collision_queries();
    else if (name == "start_departure")
        ok = start_departure();
    else if (name == "collision_traversal")
        ok = collision_traversal();
    else if (name == "movement_contacts")
        ok = movement_contacts();
    else if (name == "contact_overlap")
        ok = contact_overlap();
    else if (name == "placement_queries")
        ok = placement_queries();
    else if (name == "placement_service")
        ok = placement_service();
    else if (name == "original_services")
        ok = original_services();
    else if (name == "elevator_entry" && argc == 3)
        ok = elevator_entry(argv[2]);
    else if (name == "private" && argc == 3)
        ok = private_services(argv[2]);
    callback = {};
    return ok ? 0 : 1;
}
