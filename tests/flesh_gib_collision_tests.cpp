#include "blood_spurt_fixture.h"
#include <cmath>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root, false));
        CHECK(test_blood_spurt::mount(g.resources, root, error));
        return true;
    }
    bool room(GameSession& g, DVector origin)
    {
        WorldModel world;
        // Finite floor, ceiling and wall, with no ordinary objects involved.
        world.points = {{-4000, 0, -4000},   {4000, 0, -4000},   {4000, 0, 4000},   {-4000, 0, 4000},
                        {-4000, 200, -4000}, {4000, 200, -4000}, {4000, 200, 4000}, {-4000, 200, 4000},
                        {200, 0, -4000},    {200, 200, -4000},  {200, 200, 4000},  {200, 0, 4000}};
        for (auto& point : world.points)
            point = vector_add(point, origin);
        world.surfaces.resize(1);
        world.surfaces[0].flags = 1;
        world.polygons = {{0, {0, 1, 2, 3}}, {0, {4, 5, 6, 7}}, {0, {8, 9, 10, 11}}};
        std::string error;
        g.server.collision_ready = g.collision.build(world, error);
        CHECK(g.server.collision_ready);
        return true;
    }
    bool fragment(GameSession& g, DVector position, DVector velocity)
    {
        FleshGib f;
        f.position = position;
        f.velocity = velocity;
        f.sound_path = "sounds/gibs/flesh/gib_impact1.wav";
        std::string error;
        CHECK(g.models.load("models/characters/caleb.abc", f.model, error));
        g.server.flesh_gibs.active.push_back(std::move(f));
        return true;
    }
    bool query(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root) && room(g, {}));
        SegmentHit hit;
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, 9, 0}, hit) == SegmentResult::Miss);
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, 0, 0}, hit) == SegmentResult::Hit &&
              near(hit.point.y, 0) && near(float(hit.fraction), 1));
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, -10, 0}, hit) == SegmentResult::Hit &&
              near(hit.point.y, 0) && hit.normal.y > .9f && near(float(hit.fraction), .5f));
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, 11, 0}, hit) == SegmentResult::Miss);
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, 10, 0}, hit) == SegmentResult::Invalid);
        // A direction ray still reaches a distant surface; changing its API would break sprite placement.
        CHECK(g.server.CastWorldRay({0, 10, 0}, {0, 1, 0}, hit) == SegmentResult::Hit && near(hit.point.y, 200));
        ObjectCreateStruct state;
        state.m_Pos = {0, 5, 0};
        state.m_Flags = FLAG_SOLID;
        auto object = g.objects.create(g.objects.find_class("BaseClass"), state);
        CHECK(object);
        g.objects.get(object)->dimensions = {2, 2, 2};
        CHECK(g.server.IntersectSegment({0, 10, 0}, {0, 1, 0}, nullptr, hit) == SegmentResult::Hit &&
              hit.object == object);
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, 1, 0}, hit) == SegmentResult::Miss && !hit.object);
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, -1, 0}, hit) == SegmentResult::Hit && !hit.object &&
              near(hit.point.y, 0));
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {1e8f, 0, 0}, hit) == SegmentResult::Invalid);
        CHECK(g.server.IntersectWorldSegment({std::numeric_limits<float>::quiet_NaN(), 0, 0}, {}, hit) ==
              SegmentResult::Invalid);
        g.server.collision_ready = false;
        CHECK(g.server.IntersectWorldSegment({0, 10, 0}, {0, -1, 0}, hit) == SegmentResult::Unavailable);
        return true;
    }
    bool fragments(const std::filesystem::path& root)
    {
        for (auto origin : {DVector{}, DVector{850, 1200, -600}, DVector{-850, -1200, 600}})
        {
            GameSession g;
            CHECK(setup(g, root) && room(g, origin));
            CHECK(fragment(g, vector_add(origin, {0, 100, 0}), {20, -10, 0}));
            CHECK(g.server.flesh_gibs.advance(.01, g.server));
            const auto& moving = g.server.flesh_gibs.active.front();
            CHECK(moving.impacts == 0 && moving.splat_event == 0 && g.server.audio.voices().empty() &&
                  g.server.blood_splats.active.empty());
            CHECK(near(moving.position.x, origin.x + .2f) && near(moving.position.y, origin.y + 99.9f));
            g.server.flesh_gibs.clear();
            CHECK(fragment(g, vector_add(origin, {199, 100, 0}), {200, 0, 0}));
            CHECK(g.server.flesh_gibs.advance(.01, g.server));
            const auto& wall = g.server.flesh_gibs.active.front();
            CHECK(wall.impacts == 1 && !wall.resting && near(wall.position.x, origin.x + 199.7f) &&
                  wall.velocity.x < 0 && wall.error.empty() && wall.sound_error.empty() && wall.splat_event);
            CHECK(g.server.audio.voices().size() == 1 && g.server.blood_splats.active.size() == 1);
            g.server.flesh_gibs.clear();
            g.server.audio.clear();
            g.server.blood_splats.clear();
            CHECK(fragment(g, vector_add(origin, {0, 1, 0}), {0, -200, 0}));
            CHECK(g.server.flesh_gibs.advance(.01, g.server));
            const auto& floor = g.server.flesh_gibs.active.front();
            CHECK(floor.impacts == 1 && floor.resting && near(floor.position.y, origin.y + .3f) &&
                  floor.error.empty() && floor.sound_error.empty() && floor.splat_event);
            CHECK(g.server.audio.voices().size() == 1);
            const auto sound = g.server.audio.voices().begin()->first;
            CHECK(!g.server.audio.get(sound)->request.loop && g.server.audio.get(sound)->auto_remove);
            for (unsigned frame = 2; frame <= 201; ++frame)
            {
                CHECK(g.server.flesh_gibs.advance(frame * .01, g.server));
                CHECK(g.server.advance_audio(.01, origin, {}));
            }
            CHECK(floor.impacts == 1 && g.server.audio.voices().empty() &&
                  g.server.blood_splats.active.size() == 1);
        }
        return true;
    }
    bool ceiling(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root) && room(g, {}));
        CHECK(fragment(g, {0, 100, 0}, {0, -10, 0}));
        // The faulty endpoint-as-direction ray hits the distant ceiling every frame,
        // even though this fragment is falling and never comes close to it.
        for (unsigned frame = 1; frame <= 12; ++frame)
        {
            CHECK(g.server.flesh_gibs.advance(frame * .01, g.server));
            CHECK(g.server.advance_audio(.01, {}, {}));
        }
        const auto& f = g.server.flesh_gibs.active.front();
        std::cout << "Falling fragment: impacts=" << f.impacts << ", voices=" << g.server.audio.voices().size()
                  << ", splats=" << g.server.blood_splats.active.size() << '\n';
        CHECK(f.impacts == 0 && f.position.y < 100 && f.position.y > 90 && g.server.audio.voices().empty() &&
              g.server.blood_splats.active.empty());
        return true;
    }
    bool particles(const std::filesystem::path& root)
    {
        for (auto origin : {DVector{}, DVector{850, 1200, -600}, DVector{-850, -1200, 600}})
        {
            GameSession g;
            CHECK(setup(g, root) && room(g, origin));
            std::minstd_rand random(2);
            std::string error;
            BloodSpurtSettings settings;
            settings.position = vector_add(origin, {0, 100, 0});
            settings.min_velocity = settings.max_velocity = {0, -10, 0};
            CParticleExplosionFX distant;
            CHECK(distant.CreateObject(settings, g.resources, g.server, 0, random, error));
            CHECK(distant.Update(.01, .01, g.server, random, 16384));
            CHECK(distant.impacts == 0 && distant.emitting && distant.splat_event == 0 &&
                  g.server.audio.voices().empty() && g.server.blood_splats.active.empty());
            CHECK(near(distant.position.y, origin.y + 99.9f));
            settings.position = vector_add(origin, {0, 1, 0});
            settings.min_velocity = settings.max_velocity = {0, -200, 0};
            CParticleExplosionFX floor;
            CHECK(floor.CreateObject(settings, g.resources, g.server, 0, random, error));
            CHECK(floor.Update(.01, .01, g.server, random, 16384));
            CHECK(floor.impacts == 1 && !floor.emitting && floor.splat_event && floor.sound_error.empty() &&
                  near(floor.position.y, origin.y + .3f) && g.server.audio.voices().size() == 1);
            for (unsigned frame = 2; frame <= 101; ++frame)
            {
                CHECK(floor.Update(frame * .01, .01, g.server, random, 16384));
                CHECK(g.server.advance_audio(.01, origin, {}));
            }
            CHECK(floor.impacts == 1 && g.server.audio.voices().empty() &&
                  g.server.blood_splats.active.size() == 1);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "query"       ? query(argv[2])
            : mode == "fragments" ? fragments(argv[2])
            : mode == "ceiling"   ? ceiling(argv[2])
                                   : mode == "particles" && particles(argv[2]))
               ? 0
               : 1;
}
