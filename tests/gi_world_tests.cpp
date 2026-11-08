// GI lot G1 prerequisites: lightmap texel frames, root-BSP point contents, world-model roles
// and the ambient-fill audit (gi_contracts.md). The private cases read every mounted world.
#include "engine/collision.h"
#include "engine/config.h"
#include "engine/gi_world.h"
#include "engine/resources.h"
#include "engine/sha256.h"
#include "engine/world.h"
#include "engine/world_view.h"
#include <cmath>
#include <cstring>
#include <map>
#include <filesystem>
#include <iostream>
#include <string_view>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    void hash_floats(Sha256& hash, std::initializer_list<float> values)
    {
        for (const float value : values)
        {
            std::uint8_t bytes[4];
            std::uint32_t bits;
            std::memcpy(&bits, &value, 4);
            for (int i = 0; i < 4; ++i)
                bytes[i] = static_cast<std::uint8_t>(bits >> (8 * i));
            hash.update(bytes);
        }
    }
    // Digest of everything WorldMesh::build derives from the lightmaps: every vertex (light
    // coordinates and colors included) and every lightmap page.
    Sha256Digest mesh_digest(const WorldMesh& mesh)
    {
        Sha256 hash;
        for (const auto& v : mesh.vertices)
            hash_floats(hash, {v.position.x, v.position.y, v.position.z, v.color.x, v.color.y, v.color.z, v.uv[0],
                               v.uv[1], v.lightmap[0], v.lightmap[1], v.normal.x, v.normal.y, v.normal.z});
        for (const auto& material : mesh.materials)
            if (material.path.starts_with("lightmap page "))
                hash.update(material.image.rgba);
        return hash.finish();
    }
    bool mount(const std::filesystem::path& root, Resources& resources)
    {
        Config config;
        config.data_dir = root;
        std::string error;
        if (!resources.mount_game(config, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        return true;
    }
    template <typename F> bool each_world(const Resources& resources, F&& visit)
    {
        for (const auto& [path, resource] : resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            std::string error;
            World world;
            if (!resources.read(path, bytes, error) || !read_world(bytes, world, error))
            {
                std::cerr << path << ": " << error << '\n';
                return false;
            }
            if (!visit(path, world))
                return false;
        }
        return true;
    }
    // Page and vertex digests of every world with and without lightmaps: the G1 refactor
    // must leave them byte-identical.
    bool digests(const std::filesystem::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        return each_world(resources,
                          [&](const std::string& path, const World& world)
                          {
                              for (const bool lightmaps : {true, false})
                              {
                                  WorldMesh mesh;
                                  std::string error;
                                  if (!mesh.build(world, error, &resources, lightmaps))
                                  {
                                      std::cerr << path << ": " << error << '\n';
                                      return false;
                                  }
                                  std::cout << "DIGEST " << path << (lightmaps ? " lightmaps " : " vertex ")
                                            << digest_text(mesh_digest(mesh)) << " faces=" << mesh.lightmapped
                                            << " unfitted=" << mesh.lightmap_unfitted << '\n';
                              }
                              return true;
                          });
    }

    bool near(double a, double b, double tolerance = 1e-4)
    {
        return std::abs(a - b) <= tolerance;
    }
    WorldModel lightmapped_quad(DVector normal, float distance, DVector p, DVector q, std::uint8_t w, std::uint8_t h)
    {
        WorldModel model;
        model.lightmaps = true;
        model.planes.push_back({normal, distance});
        Surface surface;
        surface.lightmap_p = p;
        surface.lightmap_q = q;
        surface.flags = 1u << 7;
        model.surfaces.push_back(surface);
        model.lightmap_texels.assign(std::size_t(w) * h, 0xffff);
        Polygon polygon;
        polygon.lightmap_width = w;
        polygon.lightmap_height = h;
        model.polygons.push_back(polygon);
        return model;
    }
    // Texel centers round-trip through position()/coordinate() on unit and non-unit axes,
    // and the origin follows the mesh rule.
    bool frame()
    {
        {
            auto model = lightmapped_quad({0, 0, 1}, 0, {1, 0, 0}, {0, 1, 0}, 3, 2);
            model.points = {{0, 0, 0}, {40, 0, 0}, {40, 20, 0}, {0, 20, 0}};
            model.polygons[0].points = {0, 1, 2, 3};
            LightmapFrame f;
            CHECK(lightmap_frame(model, model.polygons[0], 4, f));
            CHECK(f.origin[0] == 0 && f.origin[1] == 0 && f.unfitted == 0 && f.width == 3 && f.height == 2);
            DVector x;
            CHECK(f.position(2, 1, x) && near(x.x, 40) && near(x.y, 20) && near(x.z, 0));
            const auto st = f.coordinate({40, 20, 0});
            CHECK(near(st[0], 2.5) && near(st[1], 1.5));
            // Refusals: no stored lightmap, no lightmap flag, out-of-range texel block.
            auto bad = model;
            bad.polygons[0].lightmap_width = 0;
            CHECK(!lightmap_frame(bad, bad.polygons[0], 4, f));
            bad = model;
            bad.surfaces[0].flags = 0;
            CHECK(!lightmap_frame(bad, bad.polygons[0], 4, f));
            bad = model;
            bad.polygons[0].lightmap_offset = 1;
            CHECK(!lightmap_frame(bad, bad.polygons[0], 4, f));
        }
        {
            // Tilted plane, axes of length 1.5 and 1.25.
            const DVector n{0, .6f, .8f}, p{1.5f, 0, 0}, q{0, 1.f, -.75f};
            auto model = lightmapped_quad(n, 10, p, q, 4, 4);
            model.points = {{0, 10 * .6f, 10 * .8f}, {40, 10 * .6f, 10 * .8f}, {40, 10 * .6f + 32, 10 * .8f - 24},
                            {0, 10 * .6f + 32, 10 * .8f - 24}};
            model.polygons[0].points = {0, 1, 2, 3};
            LightmapFrame f;
            CHECK(lightmap_frame(model, model.polygons[0], 4, f));
            for (double sx : {0., 1.5, 3.})
                for (double sy : {0., 2., 3.})
                {
                    DVector x;
                    CHECK(f.position(sx, sy, x));
                    const auto st = f.coordinate(x);
                    CHECK(near(st[0], sx + .5, 1e-3) && near(st[1], sy + .5, 1e-3));
                    CHECK(near(vector_dot(n, x), 10, 1e-3));
                }
            auto flat = f;
            flat.q = flat.p; // Degenerate axes.
            DVector x;
            CHECK(!flat.position(0, 0, x));
        }
        return true;
    }
    bool contents()
    {
        WorldModel model;
        CHECK(point_contents(model, {}) == PointContents::Unknown);
        model.planes.push_back({{0, 0, 1}, 0});
        BspNode node;
        node.sides = {BspNode::outside, BspNode::inside};
        model.bsp_nodes.push_back(node);
        model.root_node = 0;
        CHECK(point_contents(model, {0, 0, 10}) == PointContents::Empty);
        CHECK(point_contents(model, {0, 0, -10}) == PointContents::Solid);
        CHECK(point_contents(model, {0, 0, -.00005f}) == PointContents::Empty); // Original -0.0001 bias.
        model.bsp_nodes[0].sides = {0, 0};                                          // A cycle never terminates.
        CHECK(point_contents(model, {0, 0, 1}) == PointContents::Unknown);
        model.bsp_nodes[0].sides = {5, BspNode::inside};
        CHECK(point_contents(model, {0, 0, -1}) == PointContents::Unknown); // Invalid child.
        // Clearance against a floor polygon at z = 0.
        WorldModel floor;
        floor.points = {{-100, -100, 0}, {100, -100, 0}, {100, 100, 0}, {-100, 100, 0}};
        floor.planes.push_back({{0, 0, 1}, 0});
        Surface solid;
        solid.flags = 1; // SURF_SOLID: only solid faces are collision geometry.
        floor.surfaces.push_back(solid);
        Polygon quad;
        quad.points = {0, 1, 2, 3};
        floor.polygons.push_back(quad);
        CollisionWorld collision;
        std::string error;
        CHECK(collision.build(floor, error));
        bool clear = false;
        CHECK(point_clear(collision, {0, 0, 20}, 16, clear) && clear);
        CHECK(point_clear(collision, {0, 0, 10}, 16, clear) && !clear);
        return true;
    }
    WorldObject object(std::string class_name, PropertyValue name)
    {
        WorldObject o;
        o.class_name = std::move(class_name);
        o.properties.push_back({"Name", 0, 0, std::move(name)});
        return o;
    }
    bool roles()
    {
        World world;
        for (const char* name : {"PhysicsBSP", "Door1", "door1", "Sky", "Water1", "Glass", "Machine", "Loose"})
        {
            WorldModel model;
            model.name = name;
            world.models.push_back(model);
        }
        world.objects = {object("Door", std::string("Door1")),
                         object("DemoSkyWorldModel", std::string("Sky")),
                         object("Water", std::string("WATER1")),
                         object("CDestructableBrush", std::string("Glass")),
                         object("Tchernotronic", std::string("Machine")),
                         object("Door", std::string("Door1")),
                         object("Trigger", std::string("Missing")),
                         object("RotatingDoor", 3.f)};
        const auto r = world_model_roles(world);
        CHECK(r.size() == 8);
        CHECK(r[0].role == WorldModelRole::Primary);
        CHECK(r[1].role == WorldModelRole::Mover && r[1].objects == 2 && r[1].object_class == "Door");
        CHECK(r[2].role == WorldModelRole::Unbound && r[2].objects == 0); // Shadowed duplicate name.
        CHECK(r[3].role == WorldModelRole::Sky);
        CHECK(r[4].role == WorldModelRole::Container); // Case-insensitive name.
        CHECK(r[5].role == WorldModelRole::Destructible);
        CHECK(r[6].role == WorldModelRole::Unbound && r[6].objects == 0); // Tchernotronic binds no model.
        CHECK(r[7].role == WorldModelRole::Unbound);
        CHECK(std::string_view(world_model_role_name(WorldModelRole::Destructible)) == "destructible");
        // Bounds containment skips the primary and sky models and grows by the margin.
        world.models[1].points = {{0, 0, 0}, {10, 20, 30}};
        world.models[3].points = {{-100, -100, -100}, {100, 100, 100}};
        world.models[5].points = {{50, 0, 0}, {60, 10, 10}};
        const auto bounds = world_model_bounds(world);
        CHECK(bounds[1].max.z == 30 && bounds[2].min.x > bounds[2].max.x);
        CHECK(world_model_containing(bounds, r, {5, 5, 5}, 0) == 1);
        CHECK(world_model_containing(bounds, r, {-50, 0, 0}, 0) == -1); // Only the sky model covers it.
        CHECK(world_model_containing(bounds, r, {45, 5, 5}, 0) == -1);
        CHECK(world_model_containing(bounds, r, {45, 5, 5}, 6) == 5);
        return true;
    }
    bool ambient()
    {
        DVector a{};
        CHECK(world_info_ambient_any_case("AmbientLight 15 10 5", a) && a.x == 15 && a.y == 10 && a.z == 5);
        CHECK(world_info_ambient_any_case("SkyPanTexture x; ambientlight 16 8 8;", a) && a.x == 16 && a.z == 8);
        CHECK(!world_info_ambient_any_case("AmbientLightX 1 2 3", a));
        CHECK(!world_info_ambient_any_case("AmbientLight 1 2 300", a));
        World world;
        world.info = "ambientlight 16 8 8";
        auto model = lightmapped_quad({0, 0, 1}, 0, {1, 0, 0}, {0, 1, 0}, 2, 1);
        model.points = {{0, 0, 0}, {20, 0, 0}, {20, 1, 0}};
        model.polygons[0].points = {0, 1, 2};
        model.polygons[0].colors = {{{20, 9, 8}}, {{40, 40, 40}}, {{16, 4, 30}}};
        // RGB565: (2,2,1) expands to 16 8 8; (0,1,1) to 0 4 8 (red clearly below 16).
        model.lightmap_texels = {std::uint16_t(2 << 11 | 2 << 5 | 1), std::uint16_t(0 << 11 | 1 << 5 | 1)};
        Surface bright = model.surfaces[0];
        bright.flags |= 1u << 5;
        model.surfaces.push_back(bright);
        Polygon ignored = model.polygons[0];
        ignored.surface = 1;
        ignored.colors = {{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}};
        model.polygons.push_back(ignored);
        world.models.push_back(model);
        const auto audit = audit_ambient(world);
        CHECK(audit.has_ambient && audit.case_mismatch);
        CHECK(audit.vertices == 3 && audit.vertices_below == 1); // 16 4 30: green below 8.
        CHECK(audit.darkest_vertex == (std::array<int, 3>{16, 4, 8}));
        CHECK(audit.texels == 2 && audit.texels_below == 1 && audit.darkest_texel == (std::array<int, 3>{0, 4, 8}));
        world.info = "AmbientLight 16 8 8";
        CHECK(!audit_ambient(world).case_mismatch);
        return true;
    }
    // Every mounted world: lightmap frames solve on every stored lightmap, start points lie in
    // empty space, and the role/ambient/contents tables the G1 report records.
    bool retail(const std::filesystem::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        std::size_t total_cells = 0, total_empty = 0, single_worlds = 0;
        std::map<std::string, std::size_t> role_totals;
        const bool ok = each_world(
            resources,
            [&](const std::string& path, const World& world)
            {
                const bool single = !path.starts_with("worlds/multi/") && path.find('/', 7) == std::string::npos;
                // Frames: every SURF_LIGHTMAP polygon with stored texels.
                std::size_t frames = 0, unfitted = 0, worst_texels = 0;
                double worst = 0;
                for (const auto& model : world.models)
                    for (const auto& polygon : model.polygons)
                    {
                        if (!model.lightmaps || polygon.surface >= model.surfaces.size() ||
                            !(model.surfaces[polygon.surface].flags & (1u << 7)) || !polygon.lightmap_width)
                            continue;
                        const auto count =
                            polygon.base_points == UINT16_MAX ? polygon.points.size() : polygon.base_points;
                        LightmapFrame f;
                        if (!lightmap_frame(model, polygon, count, f))
                        {
                            std::cerr << path << ": lightmap frame refused\n";
                            return false;
                        }
                        ++frames;
                        unfitted += f.unfitted;
                        for (unsigned sy : {0u, f.height - 1})
                            for (unsigned sx : {0u, f.width - 1})
                            {
                                DVector x;
                                if (!f.position(sx, sy, x))
                                {
                                    std::cerr << path << ": singular lightmap frame\n";
                                    return false;
                                }
                                const auto st = f.coordinate(x);
                                worst = std::max({worst, std::abs(st[0] - sx - .5), std::abs(st[1] - sy - .5)});
                                worst_texels = std::max<std::size_t>(worst_texels, std::size_t(f.width) * f.height);
                            }
                    }
                // Start points.
                std::size_t starts = 0, starts_empty = 0, starts_clear = 0, starts_in_model = 0;
                const auto& primary = world.models.front();
                CollisionWorld collision;
                std::string collision_error;
                if (!collision.build(primary, collision_error))
                {
                    std::cerr << path << ": " << collision_error << '\n';
                    return false;
                }
                const auto model_roles = world_model_roles(world);
                const auto bounds = world_model_bounds(world);
                for (const auto& o : world.objects)
                    if (o.class_name == "GameStartPoint")
                        if (const auto* pos = o.find("Pos"))
                            if (const auto* v = std::get_if<DVector>(pos))
                            {
                                ++starts;
                                const bool is_empty = point_contents(primary, *v) == PointContents::Empty;
                                starts_empty += is_empty;
                                bool clear = false;
                                if (!point_clear(collision, *v, 8, clear))
                                {
                                    std::cerr << path << ": invalid clearance query\n";
                                    return false;
                                }
                                starts_clear += clear;
                                starts_in_model += world_model_containing(bounds, model_roles, *v, 0) >= 0;
                                if (!is_empty)
                                    std::cout << "START-NOT-EMPTY " << path << ' ' << v->x << ' ' << v->y << ' ' << v->z
                                              << '\n';
                            }
                // 128-unit lattice of cell centers over the root point bounds (the DAT footer
                // extents also cover sky models in the void).
                std::size_t cells = 0, empty = 0;
                DVector lo = primary.points.empty() ? DVector{} : primary.points.front(), hi = lo;
                for (const auto& point : primary.points)
                {
                    lo = {std::min(lo.x, point.x), std::min(lo.y, point.y), std::min(lo.z, point.z)};
                    hi = {std::max(hi.x, point.x), std::max(hi.y, point.y), std::max(hi.z, point.z)};
                }
                // Whole cells only: floor(extent / 128) per axis, centers at lo + 64 + 128 k.
                const auto cells_along = [](float a, float b) { return int(std::floor((b - a) / 128)); };
                const int nx = cells_along(lo.x, hi.x), ny = cells_along(lo.y, hi.y), nz = cells_along(lo.z, hi.z);
                for (int k = 0; k < nz; ++k)
                    for (int j = 0; j < ny; ++j)
                        for (int i = 0; i < nx; ++i)
                        {
                            ++cells;
                            empty += point_contents(primary, {float(lo.x + 64 + 128. * i), float(lo.y + 64 + 128. * j),
                                                              float(lo.z + 64 + 128. * k)}) == PointContents::Empty;
                        }
                if (single)
                {
                    ++single_worlds;
                    total_cells += cells;
                    total_empty += empty;
                }
                std::map<std::string, std::size_t> roles;
                for (const auto& info : model_roles)
                {
                    ++roles[world_model_role_name(info.role)];
                }
                std::cout << "WORLD " << path << " frames=" << frames << " unfitted_axes=" << unfitted
                          << " roundtrip=" << worst << " starts=" << starts_empty << '/' << starts
                          << " clear=" << starts_clear
                          << " in_model=" << starts_in_model
                          << " lattice_empty=" << empty << '/' << cells << " roles";
                for (const auto& [name, n] : roles)
                {
                    std::cout << ' ' << name << '=' << n;
                    role_totals[name] += n;
                }
                const auto a = audit_ambient(world);
                std::cout << " | ambient=" << (a.has_ambient ? "" : "none") << a.ambient.x << ' ' << a.ambient.y << ' '
                          << a.ambient.z << (a.case_mismatch ? " (lowercase key)" : "") << " darkest_texel="
                          << a.darkest_texel[0] << ' ' << a.darkest_texel[1] << ' ' << a.darkest_texel[2]
                          << " darkest_vertex=" << a.darkest_vertex[0] << ' ' << a.darkest_vertex[1] << ' '
                          << a.darkest_vertex[2] << " texels=" << a.texels << " below=" << a.texels_below
                          << " vertices=" << a.vertices << " below=" << a.vertices_below << '\n';
                // A majority of start points in empty space confirms the sentinel convention.
                if (worst > 1e-2 || (starts && starts_empty * 2 < starts))
                {
                    std::cerr << path << ": frame round trip or start-point contents out of bounds\n";
                    return false;
                }
                return true;
            });
        std::cout << "TOTAL single_worlds=" << single_worlds << " lattice_empty=" << total_empty << '/' << total_cells
                  << " roles";
        for (const auto& [name, n] : role_totals)
            std::cout << ' ' << name << '=' << n;
        std::cout << '\n';
        return ok;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view c = argv[1];
    if (c == "digests" && argc == 3)
        return digests(argv[2]) ? 0 : 1;
    if (c == "retail" && argc == 3)
        return retail(argv[2]) ? 0 : 1;
    if (argc != 2)
        return 2;
    const bool ok = c == "frame"      ? frame()
                    : c == "contents" ? contents()
                    : c == "roles"    ? roles()
                    : c == "ambient"  ? ambient()
                                      : false;
    return ok ? 0 : 1;
    return 2;
}
