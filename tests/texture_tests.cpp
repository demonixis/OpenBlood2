#include "engine/resources.h"
#include "engine/texture.h"
#include "engine/world_view.h"
#include "player_fixture.h"
#include <iostream>
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
        model.surfaces = {{{4, 0, 4}, {2, 0, 0}, {0, -1, 0}, 0, 0, 0, 0}};
        model.polygons = {{0, {0, 1, 2}}};
        model.textures = {"A.DTX"};
        world.models = {model, model};
        WorldMesh mesh;
        CHECK(mesh.build(world, error, &resources));
        CHECK(mesh.materials.size() == 2 && mesh.materials[1].error.empty() && mesh.texture_bytes == 36);
        CHECK(mesh.vertices[0].uv[0] == 2 && mesh.vertices[0].uv[1] == -1);
        CHECK(mesh.vertices[3].uv == mesh.vertices[0].uv && mesh.vertices[3].position.x == -492);
        CHECK(mesh.ranges[0].batches.size() == 1 && mesh.ranges[1].batches[0].material == 1);
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
