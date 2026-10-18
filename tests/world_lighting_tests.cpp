// Baked world lighting: DAT per-vertex RGB, lit mesh vertices, the world shade sampler,
// the AmbientLight info record and model lighting on actual retail sessions.
#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "engine/collision.h"
#include "sprite_view_fixture.h"
#include <cmath>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f && std::abs(a.z - b.z) < 1e-4f;
    }
    // One textured floor quad at y = 0, corners lit 0/51/102/255 (fan triangles 0-1-2, 0-2-3).
    World floor_world()
    {
        World w;
        WorldModel m;
        m.name = "primary";
        m.points = {{-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10}};
        m.planes = {{{0, 1, 0}, 0}};
        m.surfaces.resize(1);
        m.textures = {"alpha.dtx"};
        Polygon p{0, {0, 1, 2, 3}, 4};
        p.colors = {{{0, 0, 0}}, {{51, 51, 51}}, {{102, 102, 102}}, {{255, 0, 0}}};
        m.polygons = {p};
        w.models.push_back(m);
        w.min = {-10, -1, -10};
        w.max = {10, 1, 10};
        return w;
    }
    bool mesh(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_sprite_view::mount(resources, root, error));
        auto w = floor_world();
        WorldMesh mesh;
        CHECK(mesh.build(w, error, &resources));
        CHECK(mesh.vertices.size() == 6 && mesh.lit == 1 && mesh.fullbright == 0);
        // Fan corners keep their own baked light, in the 0-1 range.
        const DVector expected[] = {{0, 0, 0}, {.2f, .2f, .2f}, {.4f, .4f, .4f}, {0, 0, 0}, {.4f, .4f, .4f}, {1, 0, 0}};
        for (int i = 0; i < 6; ++i)
            CHECK(near(mesh.vertices[i].color, expected[i]));
        // SURF_BRIGHT and geometry without baked colors draw fully bright.
        w.models[0].surfaces[0].flags = 1u << 5;
        CHECK(mesh.build(w, error, &resources) && mesh.lit == 0 && mesh.fullbright == 1);
        for (const auto& v : mesh.vertices)
            CHECK(near(v.color, {1, 1, 1}));
        w = floor_world();
        w.models[0].polygons[0].colors.clear();
        CHECK(mesh.build(w, error, &resources) && mesh.fullbright == 1 && near(mesh.vertices[2].color, {1, 1, 1}));
        // Unresolved materials keep the explicit marker; no resources keep orientation colors.
        w = floor_world();
        w.models[0].textures = {"missing.dtx"};
        CHECK(mesh.build(w, error, &resources) && mesh.lit == 0 && near(mesh.vertices[0].color, {1, 0, 1}));
        CHECK(mesh.build(floor_world(), error) && mesh.lit == 0 && !near(mesh.vertices[0].color, {0, 0, 0}));
        return true;
    }
    bool shade(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_sprite_view::mount(resources, root, error));
        WorldMesh mesh;
        CHECK(mesh.build(floor_world(), error, &resources));
        WorldShade shade;
        CHECK(shade.build(mesh, error) && shade.stats.triangles == 2);
        const std::vector<WorldDraw> draws{{0, {}, {{0, 0, 0}, 1}}};
        DVector light{};
        float fraction = 0;
        // Barycentric light of the second triangle (corners 0, 2, 3) at x = -5, z = 5.
        CHECK(shade.trace({-5, 10, 5}, {-5, -10, 5}, draws, light, fraction));
        CHECK(std::abs(fraction - .5f) < 1e-5f && near(light, {.25f * 0 + .25f * .4f + .5f * 1, .1f, .1f}));
        // Either side, nearest along the segment; misses and absent draws return false.
        CHECK(shade.trace({-5, -10, 5}, {-5, 10, 5}, draws, light, fraction) && std::abs(fraction - .5f) < 1e-5f);
        CHECK(!shade.trace({50, 10, 0}, {50, -10, 0}, draws, light, fraction));
        CHECK(!shade.trace({0, 10, 0}, {0, 5, 0}, draws, light, fraction));
        CHECK(!shade.trace({0, 10, 0}, {0, -10, 0}, {}, light, fraction));
        // Current range transforms: translated by +100 x and rotated 180 degrees about Y.
        const std::vector<WorldDraw> moved{{0, {100, 0, 0}, {{0, 1, 0}, 0}}};
        CHECK(shade.trace({105, 10, -5}, {105, -10, -5}, moved, light, fraction));
        CHECK(near(light, {.6f, .1f, .1f}));
        CHECK(!shade.trace({-5, 10, 5}, {-5, -10, 5}, moved, light, fraction));
        // Translucent faces are not surfaces a model stands in the light of.
        auto glass = floor_world();
        glass.models[0].surfaces[0].flags = 1u << 3;
        CHECK(mesh.build(glass, error, &resources) && shade.build(mesh, error) && shade.stats.triangles == 0);
        CHECK(!shade.trace({0, 10, 0}, {0, -10, 0}, draws, light, fraction));
        return true;
    }
    bool ambient()
    {
        std::string error;
        DVector a{};
        CHECK(world_info_ambient("AmbientLight 17 23 23; WorldName \"Pickman St. Station\";", a) &&
              near(a, {17, 23, 23}));
        CHECK(world_info_ambient("MasterPalette x.dtx;  AmbientLight 0 0 0", a) && near(a, {0, 0, 0}));
        CHECK(!world_info_ambient("WorldName \"AmbientLight 1 2 3\"", a));
        CHECK(!world_info_ambient("AmbientLights 1 2 3", a) && !world_info_ambient("AmbientLight 1 2", a));
        CHECK(!world_info_ambient("AmbientLight 1 2 300", a) && !world_info_ambient("", a));
        return true;
    }
    // Retail data: every polygon carries baked colors; CityHub's darkest baked light is its
    // AmbientLight; GPU-skinned session models read the baked world light.
    bool retail(const std::filesystem::path& root)
    {
        std::string error;
        GameSession game;
        Config config;
        config.data_dir = root;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        for (const char* name : {"01_SubwayA", "02_CityHub"})
        {
            CHECK(game.request_world(name, error) && game.enter_player(error) && game.step(.02));
            std::size_t polygons = 0;
            std::array<int, 3> darkest{255, 255, 255};
            for (const auto& model : game.world.models)
                for (const auto& polygon : model.polygons)
                {
                    CHECK(polygon.colors.size() == polygon.points.size());
                    ++polygons;
                    const auto flags = model.surfaces[polygon.surface].flags;
                    if ((flags & (1u << 7)) && !(flags & (1u << 2)))
                        for (const auto& c : polygon.colors)
                            for (int k = 0; k < 3; ++k)
                                darkest[k] = std::min<int>(darkest[k], c[k]);
                }
            DVector world_ambient{};
            CHECK(world_info_ambient(game.world.info, world_ambient));
            CHECK(darkest[0] == int(world_ambient.x) && darkest[1] == int(world_ambient.y) &&
                  darkest[2] == int(world_ambient.z));
            WorldMesh mesh;
            CHECK(mesh.build(game.world, error, &game.resources) && mesh.lit > 1000);
            ModelView scene;
            scene.gpu_skinning = true;
            ModelLighting lighting;
            CHECK(lighting.build(mesh, game.world, error));
            std::vector<WorldDraw> draws;
            WorldViewStats stats;
            CHECK(world_draws(mesh, game.objects, draws, stats, error));
            CHECK(update_model_scene(game, scene, error) && lighting.apply(game, mesh, draws, scene, error));
            CHECK(lighting.stats.models > 0 && lighting.stats.sampled == lighting.stats.models);
            std::size_t lit = 0;
            for (const auto& instance : scene.instances)
            {
                CHECK(instance.color.x >= 0 && instance.color.x <= 1 && instance.color.z >= 0 && instance.color.z <= 1);
                lit += instance.material && !near(instance.color, {1, 1, 1});
            }
            CHECK(lit > 0);
            // Unchanged models and ranges reuse their samples on the next frame.
            CHECK(update_model_scene(game, scene, error) && lighting.apply(game, mesh, draws, scene, error));
            CHECK(lighting.stats.reused + lighting.stats.sampled == lighting.stats.models && lighting.stats.reused > 0);
            std::cout << name << ": " << polygons << " polygons, darkest lightmapped light " << darkest[0] << " "
                      << darkest[1] << " " << darkest[2] << ", " << mesh.lit << " baked / " << mesh.fullbright
                      << " fully bright faces, " << lighting.stats.models << " lit models, " << lighting.stats.ambient
                      << " ambient fallbacks, ModelAdd " << lighting.stats.model_add.x << "\n";
            CHECK(game.objects.finish_frame() && game.unload_world());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::string c = argc > 1 ? argv[1] : "";
    const std::filesystem::path root = argc > 2 ? argv[2] : "";
    const bool ok = c == "mesh"      ? mesh(root)
                    : c == "shade"   ? shade(root)
                    : c == "ambient" ? ambient()
                    : c == "retail"  ? retail(root)
                                     : false;
    if (ok)
        std::cout << "world_lighting." << c << " passed\n";
    return ok ? 0 : 1;
}
