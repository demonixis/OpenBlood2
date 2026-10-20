#include "engine/resources.h"
#include "engine/texture.h"
#include "engine/world_view.h"
#include "player_fixture.h"
#include <iostream>
#include <limits>
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
    void u16(std::vector<std::uint8_t>& b, std::size_t i, unsigned v)
    {
        b[i] = v;
        b[i + 1] = v >> 8;
    }
    void u32(std::vector<std::uint8_t>& b, std::size_t i, unsigned v)
    {
        u16(b, i, v);
        u16(b, i + 2, v >> 16);
    }
    std::vector<std::uint8_t> fixture(bool alpha)
    {
        // Rectangular 4x2 -> 2x1 -> 1x1 chain: 11 indices, 6 mask bytes.
        std::vector<std::uint8_t> b(1068 + 11 + (alpha ? 6 : 0));
        u32(b, 4, 0xfffffffeu);
        u16(b, 8, 4);
        u16(b, 10, 2);
        u16(b, 12, 3);
        u32(b, 16, alpha ? 10 : 8);
        u32(b, 20, 123);
        b[26] = 236;
        u32(b, 36, 0x12345678);
        u32(b, 40, 0x98765432);
        const std::uint8_t palette[] = {17, 255, 0, 0, 34, 0, 255, 0, 51, 0, 0, 255, 68, 250, 100, 50};
        for (unsigned i = 0; i < sizeof(palette); ++i)
            b[44 + i] = palette[i];
        for (unsigned i = 0; i < 11; ++i)
            b[1068 + i] = i % 4;
        if (alpha)
        {
            b[1079] = 0xf0;
            b[1080] = 0x87;
            b[1081] = 0x12;
            b[1082] = 0x34;
            b[1083] = 0x56;
            b[1084] = 0x70;
        }
        return b;
    }
    bool decode()
    {
        Texture t;
        std::string e;
        CHECK(read_texture(fixture(false), t, e));
        CHECK(e.empty() && t.width == 4 && t.height == 2 && t.mipmaps == 3 && t.user_flags == 123 &&
              t.rgba.size() == 32);
        CHECK(t.rgba[0] == 255 && t.rgba[1] == 0 && t.rgba[2] == 0 && t.rgba[3] == 17);
        CHECK(t.rgba[4] == 0 && t.rgba[5] == 255 && t.rgba[6] == 0 && t.rgba[7] == 34);
        CHECK(t.rgba[8] == 0 && t.rgba[9] == 0 && t.rgba[10] == 255 && t.rgba[11] == 51);
        CHECK(read_texture(fixture(true), t, e));
        CHECK(t.rgba[3] == 255 && t.rgba[7] == 0 && t.rgba[11] == 136 && t.rgba[15] == 119 && t.rgba[31] == 68);
        return true;
    }
    bool malformed()
    {
        Texture t;
        std::string e;
        for (bool alpha : {false, true})
        {
            auto b = fixture(alpha);
            for (std::size_t n = 0; n < b.size(); ++n)
            {
                CHECK(!read_texture(std::span(b).first(n), t, e));
                CHECK(!e.empty() && t.rgba.empty() && t.width == 0);
            }
        }
        for (unsigned i = 0; i < 11; ++i)
        {
            auto b = fixture(false);
            if (i == 0)
                u32(b, 0, 1);
            if (i == 1)
                u32(b, 4, 0xfffffffbu);
            if (i == 2)
                u16(b, 8, 0);
            if (i == 3)
                u16(b, 8, 3);
            if (i == 4)
                u16(b, 8, 2048);
            if (i == 5)
                u16(b, 12, 0);
            if (i == 6)
                u16(b, 12, 4);
            if (i == 7)
                u16(b, 14, 1);
            if (i == 8)
                u32(b, 16, 0x100);
            if (i == 9)
                b.push_back(0);
            if (i == 10)
                u32(b, 16, 10);
            CHECK(!read_texture(b, t, e) && !e.empty() && t.rgba.empty());
        }
        return true;
    }
    bool mapping()
    {
        test_player::Directory directory;
        directory.files["a.dtx"] = fixture(false);
        directory.files["bad.dtx"] = {1, 2, 3};
        directory.files["b.dtx"] = fixture(true);
        auto spr = [](std::initializer_list<const char*> frames)
        {
            test_model::Writer w;
            w.u32(frames.size());
            w.u32(8);
            w.u32(0);
            w.u32(0);
            w.u32(0);
            for (auto frame : frames)
            {
                std::string_view path(frame);
                w.u16(path.size());
                for (auto c : path)
                    w.u8(c);
            }
            return w.b;
        };
        directory.files["animated.spr"] = spr({"A.DTX", "b.dtx", "a.dtx"});
        directory.files["invalid-frame.spr"] = spr({"a.dtx", "bad.dtx"});
        directory.files["absent-frame.spr"] = spr({"missing.dtx"});
        directory.files["truncated.spr"] = {1, 2, 3};
        auto small = fixture(false);
        small.resize(1069);
        u16(small, 8, 1);
        u16(small, 10, 1);
        u16(small, 12, 1);
        directory.files["small.dtx"] = small;
        directory.files["unequal.spr"] = spr({"a.dtx", "small.dtx"});
        test_model::Writer repeated;
        repeated.u32(4096);
        repeated.u32(8);
        repeated.u32(0);
        repeated.u32(0);
        repeated.u32(0);
        for (unsigned i = 0; i < 4096; ++i)
        {
            repeated.u16(5);
            for (char c : std::string_view("a.dtx"))
                repeated.u8(c);
        }
        for (unsigned i = 0; i < 17; ++i)
            directory.files["repeat" + std::to_string(i) + ".spr"] = repeated.b;
        directory.children["other"].files["missing.dtx"] = fixture(false);
        test_model::Writer writer;
        writer.b.resize(168);
        writer.b[0] = writer.b[62] = writer.b[124] = 13;
        writer.b[1] = writer.b[63] = writer.b[125] = 10;
        writer.b[126] = 26;
        writer.patch(127, 1);
        auto [offset, size] = test_player::write_dir(writer, directory);
        writer.patch(131, offset);
        writer.patch(135, size);
        const auto path = std::filesystem::path("texture-mapping.rez");
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(writer.b.data()), writer.b.size());
        file.close();
        Resources resources;
        std::string error;
        CHECK(resources.mount(path, error));
        World world;
        WorldModel model;
        model.name = "primary";
        model.translation = {500, 600, 700};
        model.points = {{8, 2, 4}, {12, 2, 4}, {12, 4, 4}};
        model.planes = {{{0, 0, 1}, 4}};
        model.surfaces = {{{4, 0, 4}, {2, 0, 0}, {0, -1, 0}, 0, 0, 0, 0, false, {}, {}}};
        model.polygons = {{0, {0, 1, 2}}};
        model.textures = {"A.DTX"};
        world.models = {model, model};
        WorldMesh mesh;
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.materials.size() == 2 && mesh.materials[1].error.empty() && mesh.texture_bytes == 36);
        CHECK(mesh.vertices[0].uv[0] == 2 && mesh.vertices[0].uv[1] == -1);
        CHECK(mesh.vertices[3].uv == mesh.vertices[0].uv && mesh.vertices[3].position.x == -492);
        CHECK(mesh.ranges[0].batches.size() == 1 && mesh.ranges[1].batches[0].material == 1);
        world.models[0].textures[0] = "Default";
        world.models[0].surfaces[0].flags = 1u << 4;
        CHECK(mesh.build(world, error, &resources) && mesh.sky == 1 && mesh.materials.size() == 2);
        CHECK(mesh.ranges[0].batches[0].sky_portal && mesh.ranges[0].batches[0].material == 0);
        for (const auto& material : mesh.materials)
            CHECK(material.error.empty());
        world.models[0].surfaces[0].flags = 0;
        CHECK(mesh.build(world, error, &resources) &&
              !mesh.materials[1].error.empty()); // Same path on an ordinary face still fails.
        world.models[0].textures[0] = "ANIMATED.SPR";
        CHECK(mesh.build(world, error, &resources));
        const auto animation = mesh.ranges[0].batches[0].material;
        CHECK(mesh.materials.size() == 4 && mesh.texture_bytes == 68);
        CHECK(mesh.materials[animation].fps == 8 && mesh.materials[animation].frames.size() == 3);
        CHECK(mesh.materials[animation].image.width == 4 && mesh.materials[animation].image.height == 2);
        CHECK(mesh.materials[animation].image.rgba.empty()); // No duplicate pixels on the manifest.
        CHECK(mesh.vertices[0].uv[0] == 2 && mesh.vertices[0].uv[1] == -1);
        std::uint32_t frame = 0;
        const auto a = mesh.ranges[1].batches[0].material;
        CHECK(world_material_frame(mesh, animation, 0, frame) && frame == a);
        CHECK(world_material_frame(mesh, animation, .124, frame) && frame == a);
        CHECK(world_material_frame(mesh, animation, .125, frame) && frame != a);
        CHECK(world_material_frame(mesh, animation, .25, frame) && frame == a);
        CHECK(world_material_frame(mesh, animation, .375, frame) && frame == a);
        CHECK(world_material_frame(mesh, animation, 1000000.125, frame));
        mesh.materials[animation].fps = 65537;
        CHECK(world_material_frame(mesh, animation, 0, frame) && frame == a);
        CHECK(world_material_frame(mesh, animation, 1000000.125, frame) && frame == a);
        CHECK(!world_material_frame(mesh, animation, -1, frame));
        CHECK(!world_material_frame(mesh, animation, std::numeric_limits<double>::infinity(), frame));
        mesh.materials[animation].frames[0] = animation;
        CHECK(!world_material_frame(mesh, animation, 0, frame));
        for (auto name : {"invalid-frame.spr", "absent-frame.spr", "truncated.spr"})
        {
            world.models[0].textures[0] = name;
            CHECK(mesh.build(world, error, &resources));
            CHECK(mesh.ranges[0].batches[0].material == 0);
            bool rejected = false;
            for (const auto& entry : mesh.materials)
                if (entry.path == name)
                    rejected = !entry.error.empty() && entry.frames.empty();
            CHECK(rejected);
        }
        world.models[0].textures[0] = "unequal.spr";
        CHECK(mesh.build(world, error, &resources));
        const auto unequal = mesh.ranges[0].batches[0].material;
        CHECK(unequal != 0 && mesh.materials[unequal].frames.size() == 2);
        CHECK(world_material_frame(mesh, unequal, 0, frame));
        CHECK(mesh.materials[frame].image.width == 4 && mesh.materials[frame].image.height == 2);
        CHECK(world_material_frame(mesh, unequal, .125, frame));
        CHECK(mesh.materials[frame].image.width == 1 && mesh.materials[frame].image.height == 1);
        CHECK(mesh.vertices[0].uv[0] == 2 && mesh.vertices[0].uv[1] == -1); // first-frame texture period
        world.models[0].textures[0] = "a.dtx";
        auto& animated = world.models[0];
        animated.surfaces.push_back(animated.surfaces[0]);
        animated.surfaces.back().has_effect = true;
        animated.surfaces.back().effect_name = "Pan";
        animated.surfaces.back().effect_parameters = "2 -4";
        animated.polygons.push_back({1, {0, 1, 2}});
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.effects.size() == 1 && mesh.effects[0].error.empty());
        CHECK(mesh.materials.size() == 2 && mesh.texture_bytes == 36);
        CHECK(mesh.ranges[0].batches.size() == 2 && mesh.ranges[1].batches.size() == 1);
        CHECK(mesh.ranges[0].batches[0].uv_rate == (std::array<double, 2>{}));
        CHECK(mesh.ranges[0].batches[1].uv_rate == (std::array<double, 2>{-2, 2}));
        CHECK(mesh.vertices[0].uv == mesh.vertices[3].uv); // Original mapping/geometry never advances.
        animated.surfaces[1].flags = 4;
        CHECK(mesh.build(world, error, &resources) && mesh.effects.size() == 1 && mesh.invisible == 1);
        CHECK(mesh.ranges[0].batches.size() == 1); // Hidden surface remains inspectable, never submitted.
        animated.surfaces[1].flags = 8;
        CHECK(mesh.build(world, error, &resources) && mesh.ranges[0].batches[1].translucent);
        CHECK(mesh.ranges[0].batches[1].uv_rate[1] == 2);
        animated.surfaces[1].effect_name = "Warble";
        CHECK(mesh.build(world, error, &resources) && !mesh.effects[0].error.empty());
        CHECK(mesh.ranges[0].batches[1].uv_rate == (std::array<double, 2>{}));
        animated.surfaces.pop_back();
        animated.polygons.pop_back();
        world.models[0].surfaces[0].flags = 8;
        // Shared material with opaque faces does not imply a shared blend mode.
        world.models[0].polygons.push_back(world.models[0].polygons[0]);
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.blended == 2 && mesh.ranges[0].batches.size() == 2);
        CHECK(mesh.ranges[0].batches[0].translucent && !mesh.ranges[1].batches[0].translucent);
        CHECK(mesh.ranges[0].batches[0].material == mesh.ranges[1].batches[0].material);
        CHECK(mesh.vertices[0].uv[0] == 2 && mesh.vertices[0].color.y == 1);
        world.models[0].surfaces[0].flags |= 16; // Sky remains an explicit opaque marker.
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.blended == 0 && mesh.ranges[0].batches[0].material == 0);
        world.models[0].surfaces[0].flags = 8;
        world.models[0].textures[0] = "missing.dtx";
        world.models[1].textures[0] = "bad.dtx";
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.materials.size() == 3 && !mesh.materials[1].error.empty() && !mesh.materials[2].error.empty());
        CHECK(mesh.blended == 0 && !mesh.ranges[0].batches[0].translucent);
        CHECK(mesh.ranges[0].batches[0].material == 0 && mesh.vertices[0].color.x == 1 &&
              mesh.vertices[0].color.y == 0);
        world.models[0].textures[0] = "../a.dtx";
        CHECK(!mesh.build(world, error, &resources) && mesh.vertices.empty() && mesh.materials.empty());
        world.models[0].textures[0] = "a.dtx";
        world.models[0].surfaces[0].texture = 1;
        CHECK(!mesh.build(world, error, &resources) && mesh.ranges.empty());
        world.models.assign(17, model);
        for (unsigned i = 0; i < 17; ++i)
            world.models[i].textures[0] = "repeat" + std::to_string(i) + ".spr";
        CHECK(!mesh.build(world, error, &resources) && mesh.materials.empty());
        CHECK(error == "World SPR frame-reference budget exceeded");
        world.models.pop_back();
        CHECK(mesh.build(world, error, &resources) && mesh.materials.size() == 18 && mesh.texture_bytes == 36);
        std::error_code ec;
        CHECK(std::filesystem::remove(path, ec) && !ec);
        return true;
    }
    bool corpus(const char* root)
    {
        Resources resources;
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(resources.mount_game(c, e));
        std::size_t decoded = 0, rejected = 0, alpha = 0;
        for (const auto& [path, resource] : resources.index())
        {
            if (!path.ends_with(".dtx"))
                continue;
            std::vector<std::uint8_t> b;
            Texture t;
            CHECK(resources.read(path, b, e));
            if (read_texture(b, t, e))
            {
                ++decoded;
                alpha += (t.flags & 2) != 0;
                CHECK(t.rgba.size() == t.width * t.height * 4);
            }
            else
            {
                ++rejected;
                CHECK(b.size() >= 8 && b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0);
                CHECK(t.rgba.empty() && !e.empty());
                std::cout << path << ": " << e << '\n';
            }
        }
        std::cout << "DTX corpus: decoded=" << decoded << " rejected=" << rejected << " alpha=" << alpha << '\n';
        CHECK(decoded > 3000 && rejected == 5 && alpha > 100);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view c = argv[1];
    return (c == "mapping"               ? mapping()
            : c == "decode"              ? decode()
            : c == "malformed"           ? malformed()
            : c == "corpus" && argc == 3 ? corpus(argv[2])
                                         : false)
               ? 0
               : 1;
}
