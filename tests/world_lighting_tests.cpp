// Baked world lighting: DAT per-vertex RGB, lit mesh vertices, the world shade sampler,
// the AmbientLight info record and model lighting on actual retail sessions.
#include "app/model_lighting.h"
#include "app/model_scene.h"
#include "engine/collision.h"
#include "sprite_view_fixture.h"
#include <algorithm>
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
    bool origin()
    {
        std::string error;
        double o = 0;
        CHECK(lightmap_origin(1925, 1984, 5, o) && o == 1920);       // Ordinary floor snap.
        CHECK(lightmap_origin(1920, 1984, 6, o) && o == 1900);       // On a line: one cell lower...
        CHECK(lightmap_origin(1920, 1984, 5, o) && o == 1920);       // ...or on it, as stored.
        CHECK(lightmap_origin(-40, -24, 2, o) && o == -40);          // Negative line.
        CHECK(lightmap_origin(-239.99998, -144, 7, o) && o == -260); // Noise above a line.
        CHECK(!lightmap_origin(1925, 1984, 50, o) && o == 1920);     // Nothing fits: snapped minimum.
        CHECK(!lightmap_origin(0, 10, 0, o));
        return true;
    }
    // The floor quad with a stored 3x3 lightmap: x/z in [-10, 10] snap to origin -20.
    World lightmapped_world()
    {
        auto w = floor_world();
        auto& m = w.models[0];
        m.surfaces[0].flags = 1u << 7;
        m.surfaces[0].lightmap_p = {1, 0, 0};
        m.surfaces[0].lightmap_q = {0, 0, 1};
        auto& p = m.polygons[0];
        p.lightmap_extent = {40, 40};
        p.lightmap_width = p.lightmap_height = 3;
        p.lightmap_offset = 0;
        // RGB565: texel i has red = 2i, green = 4i, blue = 31 - i.
        for (unsigned i = 0; i < 9; ++i)
            m.lightmap_texels.push_back(static_cast<std::uint16_t>((2 * i) << 11 | (4 * i) << 5 | (31 - i)));
        m.lightmaps = true;
        return w;
    }
    bool lightmap(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_sprite_view::mount(resources, root, error));
        auto w = lightmapped_world();
        WorldMesh mesh;
        CHECK(mesh.build(w, error, &resources));
        CHECK(mesh.lightmapped == 1 && mesh.lit == 0 && mesh.lightmap_pages == 1 && mesh.lightmap_texels == 9 &&
              mesh.lightmap_unfitted == 0);
        const auto page = static_cast<std::uint32_t>(mesh.materials.size() - 1);
        const auto& image = mesh.materials[page].image;
        CHECK(mesh.materials[page].path == "lightmap page 0" && image.width == WorldMesh::lightmap_page_size &&
              mesh.ranges[0].batches.size() == 1 && mesh.ranges[0].batches[0].lightmap == page);
        // Texel (i, j) sits at page (1 + i, 1 + j); the one-texel border replicates edges.
        auto texel = [&](unsigned x, unsigned y) { return &image.rgba[(std::size_t(y) * image.width + x) * 4]; };
        auto expand = [](unsigned v, unsigned bits) { return bits == 5 ? (v << 3 | v >> 2) : (v << 2 | v >> 4); };
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned i = 0; i < 3; ++i)
            {
                const unsigned k = j * 3 + i;
                const auto* t = texel(1 + i, 1 + j);
                CHECK(t[0] == expand(2 * k, 5) && t[1] == expand(4 * k, 6) && t[2] == expand(31 - k, 5) && t[3] == 255);
            }
        CHECK(std::equal(texel(0, 0), texel(0, 0) + 4, texel(1, 1)) &&
              std::equal(texel(4, 4), texel(4, 4) + 4, texel(3, 3)));
        // Corner (-10, -10) is half a cell from the origin: texel coordinate .5 + .5 = 1, plus the border.
        const auto& v = mesh.vertices[0];
        CHECK(v.position.x == -10 && v.position.z == -10);
        CHECK(std::abs(v.lightmap[0] - 2.f / image.width) < 1e-7f &&
              std::abs(v.lightmap[1] - 2.f / image.height) < 1e-7f);
        // Model sampling reads the scaled lightmap at the hit: the quad center is texel (1, 1).
        WorldShade shade;
        CHECK(shade.build(mesh, error));
        DVector light{};
        float fraction = 0;
        CHECK(shade.trace({0, 10, 0}, {0, -10, 0}, std::vector<WorldDraw>{{0, {}, {{0, 0, 0}, 1}}}, light, fraction));
        const auto* center = texel(2, 2);
        CHECK(near(light, {world_saturate_scale * center[0] / 255.f, world_saturate_scale * center[1] / 255.f,
                           world_saturate_scale * center[2] / 255.f}));
        // Fully bright, sky, empty entries and models without a located block use no lightmap.
        for (int variant = 0; variant < 4; ++variant)
        {
            auto other = lightmapped_world();
            auto& m = other.models[0];
            if (variant == 0)
                m.surfaces[0].flags |= 1u << 5;
            if (variant == 1)
                m.surfaces[0].flags |= 1u << 4;
            if (variant == 2)
                m.polygons[0].lightmap_width = m.polygons[0].lightmap_height = 0;
            if (variant == 3)
                m.lightmaps = false;
            CHECK(mesh.build(other, error, &resources) && mesh.lightmapped == 0 && mesh.lightmap_pages == 0);
            CHECK(mesh.vertices.empty() || mesh.vertices[0].lightmap[0] < 0);
        }
        // A reference beyond the stored texels fails the build.
        w.models[0].polygons[0].lightmap_offset = 1;
        CHECK(!mesh.build(w, error, &resources) && error == "Invalid world lightmap reference");
        return true;
    }
    WorldObject light_object(std::string type, std::vector<WorldProperty> properties)
    {
        return {std::move(type), std::move(properties)};
    }
    bool lights()
    {
        std::string error;
        World w;
        w.objects.push_back(light_object("Light", {{"Pos", 2, 0, DVector{0, 100, 0}},
                                                   {"LightRadius", 3, 0, 200.f},
                                                   {"LightColor", 2, 0, DVector{255, 128, 0}},
                                                   {"BrightScale", 3, 0, .5f},
                                                   {"LightObjects", 5, 0, true}}));
        // A cone pointing down (pitch 90 degrees) with a 90-degree FOV.
        w.objects.push_back(light_object("DirLight", {{"Pos", 2, 0, DVector{0, 100, 0}},
                                                      {"LightRadius", 3, 0, 100.f},
                                                      {"InnerColor", 2, 0, DVector{255, 255, 255}},
                                                      {"FOV", 3, 0, 90.f},
                                                      {"Rotation", 7, 0, EulerAngles{{1.57079632679f, 0, 0}}},
                                                      {"ClipLight", 5, 0, false}}));
        w.objects.push_back(light_object("Light", {{"Pos", 2, 0, DVector{}},
                                                   {"LightRadius", 3, 0, 50.f},
                                                   {"LightColor", 2, 0, DVector{1, 1, 1}},
                                                   {"LightObjects", 5, 0, false}}));
        w.objects.push_back(light_object("ObjectLight", {{"Pos", 2, 0, DVector{}}, {"LightRadius", 3, 0, -1.f}}));
        w.objects.push_back(light_object("GlobalDirLight", {}));
        std::vector<StaticLight> out;
        StaticLightStats stats;
        CHECK(world_static_lights(w, out, stats, error) && out.size() == 2 && stats.without_objects == 1 &&
              stats.invalid == 1);
        CHECK(near(out[0].color, {.5f, 64 / 255.f, 0}) && out[0].cos_half < -1 && out[0].clip);
        CHECK(near(out[1].direction, {0, -1, 0}) && std::abs(out[1].cos_half - std::cos(.785398163f)) < 1e-5f &&
              !out[1].clip);
        // Linear falloff, no N.L: half the radius gives half the color.
        CHECK(near(static_light_at(out[0], {0, 0, 0}), vector_scale(out[0].color, .5f)));
        CHECK(near(static_light_at(out[0], {0, 300, 0}), {0, 0, 0}));
        // Cone: on axis (1 - 50/100) * 1; at 45 degrees off axis the linear cone factor reaches zero.
        CHECK(near(static_light_at(out[1], {0, 50, 0}), {.5f, .5f, .5f}));
        CHECK(near(static_light_at(out[1], {30, 70, 0}), {0, 0, 0}));
        // 18.4 degrees off axis at distance sqrt(1000): linear falloff times the linear cone factor.
        const float cone = (3 / std::sqrt(10.f) - std::cos(.785398163f)) / (1 - std::cos(.785398163f));
        CHECK(
            near(static_light_at(out[1], {10, 70, 0}), vector_scale({1, 1, 1}, (1 - std::sqrt(1000.f) / 100) * cone)));
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
            // The original light term with these light objects reproduces the preprocessor's
            // vertex colors: unshadowed sums, no N.L (SubwayA every vertex, CityHub most).
            std::vector<StaticLight> statics;
            StaticLightStats light_stats;
            CHECK(world_static_lights(game.world, statics, light_stats, error) && light_stats.invalid == 0);
            std::size_t vertices = 0, matched = 0;
            const auto& root = game.world.models.front();
            for (std::size_t index = 0; index < root.polygons.size(); index += 7)
            {
                const auto& polygon = root.polygons[index];
                if (root.surfaces[polygon.surface].flags & ((1u << 2) | (1u << 4)))
                    continue;
                for (std::size_t k = 0; k < polygon.base_points; ++k)
                {
                    DVector predicted = world_ambient;
                    for (const auto& light : statics)
                        predicted = vector_add(
                            predicted, vector_scale(static_light_at(light, root.points[polygon.points[k]]), 255));
                    const auto& rgb = polygon.colors[k];
                    ++vertices;
                    matched += std::abs(std::min(predicted.x, 255.f) - rgb[0]) <= 3 &&
                               std::abs(std::min(predicted.y, 255.f) - rgb[1]) <= 3 &&
                               std::abs(std::min(predicted.z, 255.f) - rgb[2]) <= 3;
                }
            }
            CHECK(vertices > 1000 && matched * 100 >= vertices * (std::string(name) == "01_SubwayA" ? 99 : 90));
            std::cout << name << ": light term matches " << matched << "/" << vertices << " baked vertices\n";
            CHECK(darkest[0] == int(world_ambient.x) && darkest[1] == int(world_ambient.y) &&
                  darkest[2] == int(world_ambient.z));
            std::size_t located = 0, entries = 0;
            for (const auto& model : game.world.models)
            {
                located += model.lightmaps;
                for (const auto& polygon : model.polygons)
                    entries += polygon.lightmap_width != 0;
            }
            CHECK(located == game.world.models.size() && entries > 1000);
            WorldMesh mesh;
            CHECK(mesh.build(game.world, error, &game.resources) && mesh.lightmapped > 1000 &&
                  mesh.lightmap_unfitted == 0);
            ModelView scene;
            scene.gpu_skinning = true;
            ModelLighting lighting;
            CHECK(lighting.build(mesh, game.world, error));
            std::vector<WorldDraw> draws;
            WorldViewStats stats;
            CHECK(world_draws(mesh, game.objects, draws, stats, error));
            CHECK(update_model_scene(game, scene, error) && lighting.apply(game, mesh, draws, scene, error));
            CHECK(lighting.stats.models > 0 && lighting.stats.sampled == lighting.stats.models);
            // Static lights reach models: Gouraud-shaded characters get directional sources and
            // every light term stays within 0-1 after the ambient clamp.
            std::size_t directional = 0;
            for (const auto& instance : scene.instances)
            {
                const auto& light = instance.light;
                CHECK(light.count <= ModelLight::source_limit && light.ambient.x >= 0 && light.ambient.x <= 1 &&
                      light.ambient.z >= 0 && light.ambient.z <= 1);
                directional += light.count;
            }
            CHECK(lighting.stats.lights.lights > 100 && lighting.stats.lights.invalid == 0 && directional > 0 &&
                  directional == lighting.stats.sources);
            // Sources are the strongest lights at the center, strongest first (view models use the camera).
            for (const auto& entry : scene.entries)
            {
                if (entry.close || entry.range >= scene.instances.size())
                    continue;
                const auto& instance = scene.instances[entry.range];
                for (std::uint32_t k = 1; k < instance.light.count; ++k)
                {
                    auto strength = [&](const ModelLight::Source& source)
                    {
                        const StaticLight light{source.position, source.color, source.radius, source.direction,
                                                source.cos_half};
                        const auto value = static_light_at(light, instance.center);
                        return std::max({value.x, value.y, value.z});
                    };
                    CHECK(strength(instance.light.sources[k - 1]) >= strength(instance.light.sources[k]));
                }
            }
            // Unchanged models and ranges reuse their samples on the next frame.
            CHECK(update_model_scene(game, scene, error) && lighting.apply(game, mesh, draws, scene, error));
            CHECK(lighting.stats.reused + lighting.stats.sampled == lighting.stats.models && lighting.stats.reused > 0);
            std::cout << name << ": " << polygons << " polygons, darkest lightmapped light " << darkest[0] << " "
                      << darkest[1] << " " << darkest[2] << ", " << mesh.lightmapped << " lightmapped ("
                      << mesh.lightmap_pages << " pages, " << mesh.lightmap_texels << " texels) / " << mesh.lit
                      << " vertex-lit / " << mesh.fullbright << " fully bright faces, " << lighting.stats.models
                      << " lit models (" << lighting.stats.sources << " directional sources, " << lighting.stats.flat
                      << " flat, " << lighting.stats.occluded << " occluded), " << lighting.stats.lights.lights
                      << " static lights, ModelAdd " << lighting.stats.model_add.x << "\n";
            CHECK(game.objects.finish_frame() && game.unload_world());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::string c = argc > 1 ? argv[1] : "";
    const std::filesystem::path root = argc > 2 ? argv[2] : "";
    const bool ok = c == "mesh"       ? mesh(root)
                    : c == "shade"    ? shade(root)
                    : c == "origin"   ? origin()
                    : c == "lights"   ? lights()
                    : c == "lightmap" ? lightmap(root)
                    : c == "ambient"  ? ambient()
                    : c == "retail"   ? retail(root)
                                      : false;
    if (ok)
        std::cout << "world_lighting." << c << " passed\n";
    return ok ? 0 : 1;
}
