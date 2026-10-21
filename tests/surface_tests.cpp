#include "engine/light_grid.h"
#include "engine/normal_maps.h"
#include "engine/resources.h"
#include "engine/surface_materials.h"
#include "engine/surface_report.h"
#include <cmath>
#include <iostream>
#include <map>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool glob()
    {
        CHECK(surface_glob("*", ""));
        CHECK(surface_glob("textures/*brick*", "Textures\\Greys\\RedBrick01.dtx"));
        CHECK(surface_glob("*brik*", "textures/greys/grybrik3.dtx"));
        // Several stars (the donor matcher only honored the first).
        CHECK(surface_glob("skins/*skin*", "skins/characters/calebskin.dtx"));
        CHECK(surface_glob("a?c*", "abcdef") && !surface_glob("a?c", "ac"));
        CHECK(!surface_glob("*glass", "glass1.dtx") && surface_glob("*glass*", "glass1.dtx"));
        CHECK(!surface_glob("textures/*", "skins/textures/x.dtx"));
        // Bounded work on adversarial input.
        CHECK(!surface_glob(std::string(200, '*') + "b", std::string(400, 'a')));
        return true;
    }
    bool manifest()
    {
        SurfaceManifest m;
        std::string e;
        CHECK(m.parse(R"({"version": 1, "classes": {"stone": {"roughness": 0.7, "normal": 2},
                         "default": {"roughness": 0.5}, "metal": {"metallic": 1, "roughness": 0.3}},
                         "rules": [{"match": "*brik*", "class": "stone"}, {"match": "*", "class": "metal"}],
                         "textures": {"Textures\\Odd.DTX": {"class": "stone", "metallic": 0.5}}})",
                      e));
        CHECK(e.empty() && m.classes() == 3 && m.rules() == 2 && m.overrides() == 1);
        CHECK(m.resolve("textures/grybrik.dtx").name == "stone" && m.resolve("textures/grybrik.dtx").normal == 2);
        CHECK(m.resolve("x.dtx").name == "metal" && m.resolve("x.dtx").roughness == .3f);
        const auto& odd = m.resolve("textures/odd.dtx");
        CHECK(odd.name == "stone" && odd.metallic == .5f && odd.roughness == .7f);
        SurfaceManifest empty;
        CHECK(empty.parse(R"({"version": 1, "classes": {"default": {"roughness": 0.5}}})", e));
        CHECK(empty.resolve("anything").roughness == .5f && empty.resolve("anything").name == "default");
        // Every failure is explicit and leaves the previous manifest intact.
        for (const char* bad :
             {"", "[]", R"({"classes": {}})", R"({"version": 2})", R"({"version": 1, "unknown": 1})",
              R"({"version": 1, "classes": {"a": {"roughness": 2}}})",
              R"({"version": 1, "classes": {"a": {"glow": 1}}})",
              R"({"version": 1, "rules": [{"match": "*", "class": "missing"}]})",
              R"({"version": 1, "rules": [{"match": "", "class": "default"}]})",
              R"({"version": 1, "rules": [{"match": "*", "class": "default", "x": 1}]})",
              R"({"version": 1, "version": 1})", R"({"version": 1,})", R"({"version": 1} x)",
              R"({"version": 1, "classes": {"a": {"roughness": 0.01}}})",
              R"({"version": 1, "classes": {"a": {"roughness": "0.5"}}})",
              "{\"version\": 1, \"comment\": \"\\u0041\"}", "[[[[[[[[[[[[1]]]]]]]]]]]]"})
        {
            CHECK(!m.parse(bad, e) && !e.empty());
            CHECK(m.resolve("textures/grybrik.dtx").name == "stone");
        }
        CHECK(!m.parse(std::string(SurfaceManifest::byte_limit + 1, ' '), e));
        // Emission: strength [0, 8] and threshold [0, 0.95], inherited by overrides.
        CHECK(m.parse(R"({"version": 1, "classes": {"lamp": {"emissive": 2, "emissive_threshold": 0.4}},
                         "rules": [{"match": "*lamp*", "class": "lamp"}],
                         "textures": {"a/lampx.dtx": {"class": "lamp", "emissive": 0.5}}})",
                      e));
        CHECK(m.resolve("b/lamp.dtx").emissive == 2 && m.resolve("b/lamp.dtx").emissive_threshold == .4f);
        CHECK(m.resolve("a/lampx.dtx").emissive == .5f && m.resolve("a/lampx.dtx").emissive_threshold == .4f);
        CHECK(m.resolve("other.dtx").emissive == 0 && m.resolve("other.dtx").emissive_threshold == .5f);
        for (const char* bad : {R"({"version": 1, "classes": {"a": {"emissive": 9}}})",
                                R"({"version": 1, "classes": {"a": {"emissive": -1}}})",
                                R"({"version": 1, "classes": {"a": {"emissive_threshold": 1}}})"})
            CHECK(!m.parse(bad, e) && !e.empty() && m.resolve("b/lamp.dtx").emissive == 2);
        // lookup() names the deciding binding.
        auto found = m.lookup("A\\LampX.DTX");
        CHECK(found.source == SurfaceSource::Texture && m.override_path(found.index) == "a/lampx.dtx");
        found = m.lookup("b/lamp.dtx");
        CHECK(found.source == SurfaceSource::Rule && m.rule_pattern(found.index) == "*lamp*");
        found = m.lookup("other.dtx");
        CHECK(found.source == SurfaceSource::Default && found.material->name == "default");
        return true;
    }
    // Material inspection (roadmap 5.5) over a synthetic mesh.
    bool report()
    {
        SurfaceManifest m;
        std::string e;
        CHECK(m.parse(R"({"version": 1, "classes": {"lamp": {"emissive": 2, "normal": 0}, "stone": {"normal": 1.5}},
                         "rules": [{"match": "*lamp*", "class": "lamp"}, {"match": "*brik*", "class": "stone"}],
                         "textures": {"textures/gone.dtx": {"class": "stone"}, "textures/odd.dtx": {"roughness": 0.2}}})",
                      e));
        WorldMesh mesh;
        mesh.materials.push_back({});                         // The white slot.
        mesh.materials.push_back({"textures/lamp1.dtx", {}}); // Rule, emissive, class without maps.
        mesh.materials.push_back({"textures/brik2.dtx", {}}); // Rule, generated map.
        mesh.materials.push_back({"textures/odd.dtx", {}});   // Override.
        mesh.materials.push_back({"textures/plain.dtx", {}}); // Default.
        mesh.materials.push_back({"textures/broken.dtx", "missing"});
        mesh.materials.push_back({"sprites/fire.spr", {}});
        mesh.materials.back().frames = {1, 2};
        mesh.materials.push_back({"lightmap page 0", {}});
        mesh.fullbright_faces = {0, 3, 0, 0, 2, 0, 0, 0};
        const auto exists = [](std::string_view path) { return path != "textures/gone.dtx"; };
        auto r = surface_report(m, mesh, true, exists);
        CHECK(r.materials.size() == 6);
        const auto& lamp = r.materials[0];
        CHECK(lamp.material == 1 && lamp.source == SurfaceSource::Rule && lamp.binding == "*lamp*" && lamp.emissive() &&
              lamp.normal_map == NormalMapState::ClassOff && lamp.fullbright_faces == 3);
        CHECK(r.materials[1].normal_map == NormalMapState::Generated && r.materials[1].values.name == "stone");
        CHECK(r.materials[2].source == SurfaceSource::Texture && r.materials[2].binding == "textures/odd.dtx" &&
              r.materials[2].values.roughness == .2f);
        CHECK(r.materials[3].source == SurfaceSource::Default && r.materials[3].fullbright_faces == 2);
        CHECK(r.materials[4].normal_map == NormalMapState::Failed);
        CHECK(r.materials[5].normal_map == NormalMapState::Animated && r.materials[5].values.name == "default");
        CHECK(r.defaults == 3 && r.failed == 1 && r.emissive == 1 && r.generated == 3 && r.fullbright_materials == 2 &&
              r.fullbright_faces == 5 && r.emissive_fullbright == 1 && r.classes["default"] == 4);
        CHECK(r.unbound_overrides == std::vector<std::string>{"textures/gone.dtx"});
        // Without normal maps (the option), no map is generated; without `exists` nothing is checked.
        r = surface_report(m, mesh, false);
        CHECK(r.generated == 0 && r.materials[1].normal_map == NormalMapState::Disabled && r.unbound_overrides.empty());
        return true;
    }
    bool builtin()
    {
        SurfaceManifest m;
        std::string e;
        CHECK(m.parse_builtin(e) && m.classes() > 10 && m.rules() > 50);
        CHECK(m.resolve("textures/greys/grybrik3.dtx").name == "stone");
        CHECK(m.resolve("textures/greys/drkmetal2.dtx").name == "metal_rough");
        CHECK(m.resolve("textures/reds/cathglassa.dtx").name == "glass");
        CHECK(m.resolve("textures/blueyellow/waterbrown03.dtx").name == "water");
        CHECK(m.resolve("textures/greys/cratemetal1.dtx").name == "metal_rough");
        CHECK(m.resolve("textures/greys/cratewood1.dtx").name == "wood");
        CHECK(m.resolve("skins/characters/caleb.dtx").name == "character");
        CHECK(m.resolve("textures/greys/unmatched.dtx").name == "default");
        // Emissive classes (roadmap 5.2): lights, neon signs, screens and the donor's hazard words.
        CHECK(m.resolve("textures/greys/flolite1.dtx").name == "light" &&
              m.resolve("textures/greys/flolite1.dtx").emissive > 0);
        CHECK(m.resolve("textures/greys/amberlight.dtx").name == "light");
        CHECK(m.resolve("textures/reds/fnopen.dtx").name == "neon" &&
              m.resolve("textures/blueyellow/fneon3.dtx").name == "neon");
        CHECK(m.resolve("textures/greys/monitors.dtx").name == "screen" &&
              m.resolve("textures/reds/tvbig.dtx").name == "screen");
        CHECK(m.resolve("textures/blueyellow/exitsign1.dtx").name == "light");
        CHECK(m.resolve("textures/x/lava1.dtx").name == "hazard" && m.resolve("textures/x/lava1.dtx").emissive > 0);
        CHECK(m.resolve("textures/sky/lightning1.dtx").name == "matte"); // Sky first.
        CHECK(m.resolve("skins/powerups/flashlight.dtx").emissive == 0);
        CHECK(m.resolve("textures/greys/grybrik3.dtx").emissive == 0);
        return true;
    }
    Texture image(std::uint32_t w, std::uint32_t h, std::uint32_t mipmaps, auto shade)
    {
        Texture t{w, h, mipmaps, 0, 0, {}, {}};
        t.rgba.resize(std::size_t(w) * h * 4);
        for (std::uint32_t y = 0; y < h; ++y)
            for (std::uint32_t x = 0; x < w; ++x)
            {
                const auto v = static_cast<std::uint8_t>(shade(x, y));
                auto* p = &t.rgba[(std::size_t(y) * w + x) * 4];
                p[0] = p[1] = p[2] = v;
                p[3] = 255;
            }
        t.mip_rgba.resize(texture_mip_bytes(t), 128);
        return t;
    }
    bool normal_maps()
    {
        std::string e;
        Texture out;
        // Flat albedo: every level is the straight-up normal (128, 128, 255).
        const auto flat = image(8, 8, 4, [](auto, auto) { return 90; });
        CHECK(generate_normal_map(flat, 1, out, e) && out.width == 8 && out.mipmaps == 4);
        CHECK(out.rgba.size() == flat.rgba.size() && out.mip_rgba.size() == flat.mip_rgba.size());
        for (std::size_t i = 0; i < out.rgba.size(); i += 4)
            CHECK(out.rgba[i] == 128 && out.rgba[i + 1] == 128 && out.rgba[i + 2] == 255 && out.rgba[i + 3] == 90);
        for (std::size_t i = 0; i < out.mip_rgba.size(); i += 4)
            CHECK(out.mip_rgba[i] == 128 && out.mip_rgba[i + 2] == 255);
        // Brightness rising along +u tilts the normal toward -u; along +v toward -v.
        const auto ramp_u = image(16, 16, 1, [](auto x, auto) { return x < 8 ? 20 : 200; });
        CHECK(generate_normal_map(ramp_u, 1, out, e));
        const auto at = [&](std::uint32_t x, std::uint32_t y, int k) { return int(out.rgba[(y * 16 + x) * 4 + k]); };
        CHECK(at(7, 5, 0) < 128 && at(7, 5, 1) == 128 && at(7, 5, 2) < 255 && at(3, 5, 0) == 128);
        const auto ramp_v = image(16, 16, 1, [](auto, auto y) { return y < 8 ? 20 : 200; });
        CHECK(generate_normal_map(ramp_v, 2, out, e) && at(3, 7, 1) < 128 && at(3, 7, 0) == 128);
        // Strength 0 is flat; wrapping keeps the edge consistent with the opposite side.
        CHECK(generate_normal_map(ramp_v, 0, out, e) && at(3, 7, 1) == 128);
        CHECK(generate_normal_map(ramp_v, 1, out, e) && at(3, 0, 1) > 128 && at(3, 15, 1) > 128);
        // Invalid inputs fail explicitly.
        auto broken = flat;
        broken.rgba.pop_back();
        CHECK(!generate_normal_map(broken, 1, out, e) && !e.empty() && out.rgba.empty());
        CHECK(!generate_normal_map(flat, -1, out, e) && !generate_normal_map(flat, NAN, out, e));
        // The cache is content-addressed and bounded.
        NormalMapCache cache;
        const auto a = cache.get(ramp_v, 1, e), b = cache.get(ramp_v, 1, e), c = cache.get(ramp_v, 2, e);
        CHECK(a && a == b && c && c != a && cache.stats.generated == 2 && cache.stats.hits == 1);
        cache.byte_limit = a->rgba.size() + a->mip_rgba.size();
        CHECK(cache.get(ramp_u, 1, e) && cache.stats.evicted == 2 && cache.stats.entries == 1);
        CHECK(!cache.get(broken, 1, e) && !e.empty());
        return true;
    }
    bool light_grid()
    {
        std::string e;
        LightGrid grid;
        CHECK(build_light_grid({}, grid, e) && grid.cell_count() == 1 && grid.cells == std::vector<std::uint32_t>{0});
        std::vector<StaticLight> lights;
        lights.push_back({{0, 0, 0}, {1, 1, 1}, 100});
        lights.push_back({{1000, 0, 0}, {.5f, 0, 0}, 300, {0, -1, 0}, .5f});
        CHECK(build_light_grid(lights, grid, e) && grid.lights.size() == 6 && grid.cell_size >= LightGrid::minimum_cell);
        CHECK(grid.lights[4][3] == .5f && grid.lights[5][1] == -1);
        auto cell_of = [&](float x, float y, float z)
        {
            const float p[3]{x, y, z};
            std::size_t c[3];
            for (int k = 0; k < 3; ++k)
                c[k] = std::size_t(std::floor((p[k] - grid.origin[k]) / grid.cell_size));
            const auto packed = grid.cells[(c[2] * grid.dimensions[1] + c[1]) * grid.dimensions[0] + c[0]];
            std::vector<std::uint32_t> out;
            for (std::uint32_t i = 0; i < (packed & 15); ++i)
                out.push_back(grid.cells[grid.cell_count() + (packed >> 4) + i]);
            return out;
        };
        CHECK(cell_of(0, 0, 0) == std::vector<std::uint32_t>{0});
        CHECK(cell_of(1000, 0, 0) == std::vector<std::uint32_t>{1});
        CHECK(cell_of(500, 0, 0).empty());
        // A crowded cell keeps the strongest lights, strongest first.
        lights.clear();
        for (int i = 0; i < 12; ++i)
            lights.push_back({{0, 0, 0}, {float(i + 1) / 12, float(i + 1) / 12, float(i + 1) / 12}, 50});
        CHECK(build_light_grid(lights, grid, e) && grid.dropped == 4 * grid.cell_count());
        const auto kept = cell_of(0, 0, 0);
        CHECK(kept.size() == LightGrid::cell_light_limit && kept.front() == 11 && kept.back() == 4);
        // Huge extents grow the cells instead of the cell count.
        lights = {{{-50000, 0, 0}, {1, 1, 1}, 100}, {{50000, 0, 50000}, {1, 1, 1}, 100}};
        CHECK(build_light_grid(lights, grid, e) && grid.cell_count() <= LightGrid::cell_limit);
        lights[0].radius = NAN;
        CHECK(!build_light_grid(lights, grid, e) && !e.empty());
        return true;
    }
    // Measures the built-in manifest against the retail catalogue (roadmap 5.0) and times the
    // normal-map generator on every world texture.
    bool retail(const char* root)
    {
        Resources resources;
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(resources.mount_game(c, e));
        SurfaceManifest m;
        CHECK(m.parse_builtin(e));
        std::map<std::string, std::size_t> classes;
        std::size_t textures = 0, world = 0, world_default = 0, world_emissive = 0;
        NormalMapCache cache;
        for (const auto& [path, resource] : resources.index())
        {
            if (!path.ends_with(".dtx"))
                continue;
            ++textures;
            const auto& material = m.resolve(path);
            ++classes[material.name];
            if (!path.starts_with("textures/") || path.starts_with("textures/sky"))
                continue;
            ++world;
            world_default += material.name == "default";
            world_emissive += material.emissive > 0;
            Texture t;
            if (!read_texture_resource(resources, path, t, e))
                continue;
            CHECK(cache.get(t, material.normal > 0 ? material.normal : 1, e));
        }
        std::cout << "DTX " << textures << ", world " << world << " (" << world_default << " default, "
                  << world_emissive << " emissive)\n";
        for (const auto& [name, count] : classes)
            std::cout << "  " << name << ": " << count << '\n';
        std::cout << "Normal maps: " << cache.stats.generated << " in " << cache.stats.generation_ns / 1000000
                  << " ms, " << cache.stats.bytes / 1024 << " KiB\n";
        CHECK(textures > 3000 && world > 1500 && world_default * 2 < world && world_emissive >= 50);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view c = argv[1];
    return (c == "glob"                  ? glob()
            : c == "manifest"            ? manifest()
            : c == "builtin"             ? builtin()
            : c == "normal_maps"         ? normal_maps()
            : c == "report"              ? report()
            : c == "light_grid"          ? light_grid()
            : c == "retail" && argc == 3 ? retail(argv[2])
                                         : false)
               ? 0
               : 1;
}
