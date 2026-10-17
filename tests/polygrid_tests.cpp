#include "app/polygrid_scene.h"
#include "blood_splat_fixture.h"
#include "brush_fixture.h"
#include "engine/container.h"
#include "game/original/PolyGrid.h"
#include "game/original/VolumeBrush.h"
#include "game/original/trigger_helpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(float a, float b)
    {
        return std::abs(a - b) < .0001f;
    }
    bool formulas(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_blood_splat::mount(resources, root, error));
        SpriteStore store(resources);
        PolyGridSettings s;
        s.dims = {10, 2, 20};
        s.sprite = "sprites/blood1.spr";
        s.scale_min = {1, 1};
        s.scale_max = {2, 2};
        s.duration = {1, 0};
        s.color1 = {255, 128, 1};
        s.color2 = {0, 255, 255};
        for (auto [n, size] : {std::pair{4u, 2u}, {8u, 2u}, {18u, 4u}, {72u, 8u}, {288u, 16u}, {50000u, 16u}})
        {
            s.polies = n;
            CPolyGridFX f;
            CHECK(f.Init(s, store, 0, error) && f.width == size && f.settings.polies == std::min(n, 32767u));
        }
        s.polies = 160;
        CPolyGridFX f;
        CHECK(f.Init(s, store, 0, error) && f.width == 8 && near(f.palette[0].x, 248.f / 255) &&
              near(f.palette[0].z, 0));
        CHECK(near(f.palette[255].z, 248.f / 256) && f.distance[0] == 255 && f.distance[4 * 8 + 4] == 0);
        std::array<float, 4> phases{0, 1, 4, 3};
        CHECK(f.Update(1, true, phases));
        CHECK(f.scale[0] == 2 && f.scaling_up[0] && f.scale[1] == 1 && phases[0] == 50);
        // Independently evaluated original four-ring rows at t=1, rates 50/10/30/20.
        const int first[] = {10, 5, -52, 36, -28, 51, -43, 77};
        const int middle[] = {2, -2, -13, -48, -19, -33, -4, 71};
        for (unsigned x = 0; x < 8; ++x)
            CHECK(f.heights[x] == first[x] && f.heights[32 + x] == middle[x]);
        auto heights = f.heights;
        CHECK(f.Update(2, false, phases) && f.heights == heights && phases[0] == 50 && f.scale[0] == 2);
        CHECK(f.Update(2.25, true, phases) && !f.scaling_up[0] && f.scale[0] == 2 && near(phases[0], 62.5f));
        CHECK(f.Update(2.5, true, phases) && near(f.scale[0], 1.75f));
        CPolyGridFX other;
        CHECK(other.Init(s, store, 2.5, error) && other.Update(2.75, true, phases) && near(phases[0], 87.5f));
        CHECK(!f.Update(1, true, phases) && !f.error.empty());
        s.plasma = 0;
        s.polies = 32767;
        CPolyGridFX normal;
        CHECK(normal.Init(s, store, 0, error) && normal.width == 128 && normal.Update(.1, true, phases) &&
              near(normal.count, 5));
        for (int bad = 0; bad < 9; ++bad)
        {
            auto invalid = s;
            switch (bad)
            {
                case 0:
                    invalid.dims.x = 0;
                    break;
                case 1:
                    invalid.alpha = 2;
                    break;
                case 2:
                    invalid.polies = 3;
                    break;
                case 3:
                    invalid.plasma = 2;
                    break;
                case 4:
                    invalid.rates[0] = 256;
                    break;
                case 5:
                    invalid.duration[0] = NAN;
                    break;
                case 6:
                    invalid.color1.x = -1;
                    break;
                case 7:
                    invalid.scale_min[0] = 0;
                    break;
                case 8:
                    invalid.sprite = "missing.spr";
                    break;
            }
            CPolyGridFX rejected;
            CHECK(!rejected.Init(invalid, store, 0, error) && !error.empty());
        }
        return true;
    }
    bool send_trigger(GameSession& g, HOBJECT receiver, const char* command)
    {
        auto m = g.server.StartMessageToObject(nullptr, receiver, 1000);
        auto text = g.server.CreateString(command);
        if (!m || !text)
            return false;
        const bool ok = g.server.WriteToMessageHString(m, text) == DE_OK;
        g.server.FreeString(text);
        if (ok)
            g.server.EndMessage(m);
        else
            g.server.EndHMessageWrite(m);
        return ok;
    }
    bool lifetime(const std::filesystem::path&)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize());
        g.world.models.push_back(test_brush::box("pool", {-10, -5, -20}, {10, 5, 20}));
        auto h = g.objects.create(
            g.objects.find_class("Water"), {}, Creation::World,
            {{"Name", std::string("pool")}, {"SpriteSurfaceName", std::string{}}, {"Current", DVector{20, 30, 400}}});
        auto* volume = dynamic_cast<VolumeBrush*>(g.server.HandleToObject(h));
        CHECK(volume && volume->surface_error.empty() && g.server.polygrids.active.size() == 1);
        const auto child = volume->surface_object;
        CHECK(g.server.IsObjectAlive(child));
        auto& fx = g.server.polygrids.active[0].effect;
        CHECK(fx.pose.position.y == 4 && fx.settings.dims.y == 2 && near(fx.settings.pan[0], 1.2f) &&
              near(fx.settings.pan[1], 1.3f));
        auto step = [&] { return g.objects.advance(.01) && g.server.polygrids.advance(g.objects.time(), g.server); };
        CHECK(step());
        const auto fixed = fx.pose.position;
        CHECK(g.server.advance_physics(.1) && g.server.damage_containers());
        CHECK(g.objects.get(child)->state.m_Pos.x == fixed.x && g.objects.get(child)->state.m_Pos.y == fixed.y);
        const auto before = fx.updates;
        CHECK(send_trigger(g, h, "OFF") && step() && !fx.visible && fx.updates == before);
        CHECK(send_trigger(g, h, "ON") && step() && fx.visible && fx.updates == before + 1);
        const auto id = fx.id;
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        CHECK(g.server.IsObjectAlive(child) && step() && g.server.polygrids.active.size() == 1);
        CHECK(g.objects.remove(child));
        PolyGridView scene;
        CHECK(update_polygrid_scene(g, scene, error) && scene.entries.empty());
        CHECK(g.objects.finish_frame() && g.server.polygrids.advance(g.objects.time(), g.server) &&
              g.server.polygrids.active.empty() && g.server.polygrids.expired == 1);
        auto raw = g.objects.create(g.objects.find_class("PolyGrid"), {});
        auto* grid = dynamic_cast<PolyGrid*>(g.server.HandleToObject(raw));
        CHECK(grid);
        PolyGridSettings s;
        s.dims = {1, 0, 1};
        CHECK(grid->Setup(s) && grid->effect > id && !grid->Setup(s));
        CHECK(!g.server.PublishPolyGrid(raw, s, error) && g.server.polygrids.failed == 1 &&
              !g.server.unavailable_services.empty());
        CHECK(g.unload_world() && g.server.polygrids.active.empty() && g.server.polygrids.phases[0] == 0 &&
              g.server.polygrids.phases[1] == 1);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        PolyGridView scene;
        std::uint64_t last = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_CityHub", error));
            for (auto& r : g.server.polygrids.reports)
                if (!r.error.empty())
                    std::cerr << r.error << "\n";
            CHECK(!g.server.polygrids.active.empty() && g.server.polygrids.failed == 0);
            unsigned surfaces = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* record = g.objects.get(id);
                auto* v = dynamic_cast<VolumeBrush*>(record->game.get());
                if (!v || !v->show_surface)
                    continue;
                CHECK(v->surface_error.empty() && v->surface_object);
                ++surfaces;
                auto* surface = g.objects.get(v->surface_object);
                CHECK(surface);
                const auto& shape = *record->container();
                CHECK(near(surface->state.m_Pos.y, shape.max.y - v->surface_height / 2));
                CHECK(near(surface->state.m_Pos.x, (shape.min.x + shape.max.x) / 2) &&
                      near(surface->state.m_Pos.z, (shape.min.z + shape.max.z) / 2));
                std::cout << "CityHub " << record->state.m_Name << " origin " << record->state.m_Pos.x << ","
                          << record->state.m_Pos.y << "," << record->state.m_Pos.z << " bounds " << shape.min.x << ","
                          << shape.min.y << "," << shape.min.z << " to " << shape.max.x << "," << shape.max.y << ","
                          << shape.max.z << " surface " << surface->state.m_Pos.x << "," << surface->state.m_Pos.y
                          << "," << surface->state.m_Pos.z << "\n";
            }
            CHECK(surfaces == g.server.polygrids.active.size());
            CHECK(g.enter_player(error));
            for (int i = 0; i < 5; ++i)
                CHECK(g.step(1.0 / 60));
            CHECK(update_polygrid_scene(g, scene, error) && scene.entries.size() == surfaces);
            for (auto& e : scene.entries)
            {
                CHECK(e.error.empty() && e.range != UINT32_MAX);
                std::cout << "Surface " << e.id << " grid " << e.width << " texture " << e.texture << " triangles "
                          << scene.mesh.ranges[e.range].count / 3 << "\n";
            }
            for (auto& i : g.server.polygrids.active)
            {
                CHECK(i.effect.id > last && i.effect.updates >= 5);
                last = i.effect.id;
            }
            auto& checked = g.server.polygrids.active[0].effect;
            CHECK(checked.sprite);
            auto frame = checked.track.frame;
            checked.track.frame = UINT32_MAX;
            CHECK(!update_polygrid_scene(g, scene, error) && !error.empty());
            checked.track.frame = frame;
            const auto updates = g.server.polygrids.active[0].effect.updates;
            CHECK(update_polygrid_scene(g, scene, error) && updates == g.server.polygrids.active[0].effect.updates);
            CHECK(g.unload_world());
            scene.clear();
            CHECK(update_polygrid_scene(g, scene, error) && scene.entries.empty() && g.server.polygrids.active.empty());
        }
        return true;
    }
    bool mesh(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_blood_splat::mount(resources, root, error));
        SpriteStore store(resources);
        PolyGridSettings s;
        s.dims = {10, 2, 20};
        CPolyGridFX fx;
        CHECK(fx.Init(s, store, 0, error));
        PolyGridView scene;
        fx.heights[0] = -127;
        fx.heights[1] = 127;
        CHECK(scene.add(1, true, {{0, 5, 0}, {}}, s.dims, fx.width, fx.heights, fx.palette, {2, 3}, {.25f, .5f}, .7f,
                        "alpha.dtx", resources, error));
        CHECK(scene.entries[0].error.empty() && scene.mesh.vertices.size() == 294 &&
              scene.mesh.vertices[0].position.y == -2 && scene.mesh.vertices[1].position.y == 2);
        CHECK(scene.mesh.vertices[0].uv[0] == .25f && scene.mesh.vertices[0].uv[1] == .5f &&
              scene.mesh.vertices.back().position.z == 20);
        const auto revision = scene.revision;
        scene.begin_frame();
        CHECK(scene.add(1, false, {}, s.dims, fx.width, fx.heights, fx.palette, {2, 3}, {}, .7f, "ALPHA.DTX", resources,
                        error) &&
              scene.draws.empty() && scene.revision == revision);
        CHECK(scene.add(2, true, {}, s.dims, fx.width, fx.heights, fx.palette, {2, 3}, {}, .7f, "bad.dtx", resources,
                        error) &&
              !scene.entries.back().error.empty() && scene.draws.empty());
        CHECK(!scene.add(2, true, {}, s.dims, 129, fx.heights, fx.palette, {2, 3}, {}, .7f, "", resources, error));
        scene.clear();
        CHECK(scene.mesh.materials.size() == 1 && scene.entries.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string mode = argv[1];
    bool ok = mode == "formulas"   ? formulas(argv[2])
              : mode == "lifetime" ? lifetime(argv[2])
              : mode == "mesh"     ? mesh(argv[2])
              : mode == "private"  ? private_data(argv[2])
                                   : false;
    return ok ? 0 : 1;
}
