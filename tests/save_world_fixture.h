#pragma once
#include "player_fixture.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// Synthetic DAT worlds with the assets a saved local player needs (save_load_contracts lot 5):
// one floor triangle, the authored rows given, Caleb, Ophelia and the default weapons' models. The DAT
// layout is the one world_tests and save_session_tests use.
namespace save_world
{
    using namespace OpenBlood2;
    // Little-endian DAT writer, the layout world_tests uses for synthetic worlds.
    struct Writer
    {
        std::vector<std::uint8_t> data;
        void byte(unsigned v)
        {
            data.push_back(static_cast<std::uint8_t>(v));
        }
        void u16(unsigned v)
        {
            byte(v);
            byte(v >> 8);
        }
        void u32(std::uint32_t v)
        {
            u16(v);
            u16(v >> 16);
        }
        void real(float f)
        {
            u32(std::bit_cast<std::uint32_t>(f));
        }
        void vec(float x = 0, float y = 0, float z = 0)
        {
            real(x);
            real(y);
            real(z);
        }
        void str(std::string_view s)
        {
            u16(static_cast<unsigned>(s.size()));
            data.insert(data.end(), s.begin(), s.end());
        }
        void cstr(std::string_view s)
        {
            data.insert(data.end(), s.begin(), s.end());
            byte(0);
        }
        void zero(std::size_t n)
        {
            data.resize(data.size() + n);
        }
        void append(const Writer& w)
        {
            data.insert(data.end(), w.data.begin(), w.data.end());
        }
        void patch(std::size_t p, std::uint32_t n)
        {
            for (unsigned i = 0; i < 4; ++i)
                data[p + i] = static_cast<std::uint8_t>(n >> (i * 8));
        }
    };
    inline Writer prop(std::string_view name, unsigned type, const Writer& value)
    {
        Writer p;
        p.str(name);
        p.byte(type);
        p.u32(0);
        p.u16(static_cast<unsigned>(value.data.size()));
        p.append(value);
        return p;
    }
    inline Writer text(std::string_view name, std::string_view value)
    {
        Writer v;
        v.str(value);
        return prop(name, 0, v);
    }
    inline Writer vector(std::string_view name, DVector value)
    {
        Writer v;
        v.vec(value.x, value.y, value.z);
        return prop(name, 1, v);
    }
    inline Writer object(std::string_view cls, std::initializer_list<Writer> properties)
    {
        Writer v;
        v.str(cls);
        v.u32(static_cast<unsigned>(properties.size()));
        for (const auto& p : properties)
            v.append(p);
        Writer out;
        out.u16(static_cast<unsigned>(v.data.size()));
        out.append(v);
        return out;
    }
    // One floor triangle textured with `texture`, after the given object rows.
    inline std::vector<std::uint8_t> dat(const std::vector<Writer>& objects, std::string_view texture = "t.dtx")
    {
        Writer w;
        w.u32(56);
        w.u32(48);
        w.u32(0);
        w.u32(0);
        w.zero(32);
        w.u32(static_cast<unsigned>(objects.size()));
        for (const auto& o : objects)
            w.append(o);
        w.patch(8, static_cast<unsigned>(w.data.size()));
        const auto start = w.data.size();
        w.u32(0);
        w.zero(32);
        w.u32(0);
        w.str("SyntheticTriangle");
        w.u32(0);
        for (unsigned n : {3u, 1u, 1u, 0u, 1u, 0u, 3u, 0u, 0u, 0u})
            w.u32(n);
        w.u32(0);
        w.vec();
        w.vec(10, 10, 10);
        w.vec();
        w.u32(static_cast<unsigned>(texture.size() + 1));
        w.u32(1);
        w.cstr(texture);
        w.byte(3);
        w.byte(0); // Polygon vertex counts.
        w.vec(0, 1, 0);
        w.real(0); // Plane.
        w.vec();
        w.vec(1, 0, 0);
        w.vec(0, 0, 1);
        w.zero(24);
        w.u32(0xffffffff);
        w.zero(8);
        w.u16(0);
        w.u32(0);
        w.u32(1);
        w.u32(0);
        w.byte(0);
        w.u16(20);
        w.zero(12);
        w.u32(0);
        for (unsigned i = 0; i < 3; ++i)
        {
            w.u16(i);
            w.zero(3);
        }
        w.vec(-1000, 0, -1000);
        w.vec(1000, 0, -1000);
        w.vec(0, 0, 1000);
        w.patch(start, static_cast<unsigned>(w.data.size()));
        w.u32(0); // No additional models.
        w.vec(-2000, -2000, -2000);
        w.vec(2000, 2000, 2000);
        return w.data;
    }
    inline std::vector<std::uint8_t> dtx() // Smallest version -2 palette image.
    {
        test_model::Writer image;
        image.u32(0);
        image.u32(0xfffffffeu);
        image.u16(2);
        image.u16(2);
        image.u16(1);
        image.u16(0);
        image.u32(0);
        image.u32(0);
        image.b.resize(1068);
        image.b[44] = image.b[45] = image.b[46] = image.b[47] = 255;
        image.b.insert(image.b.end(), {0, 0, 0, 0});
        return image.b;
    }
    // Writes `file` holding the given worlds (worlds/<name>.dat) and the player assets.
    inline bool write_archive(const std::filesystem::path& file,
                              const std::map<std::string, std::vector<std::uint8_t>>& worlds)
    {
        test_player::Directory dir;
        for (const auto& [name, bytes] : worlds)
            dir.children["worlds"].files[name + ".dat"] = bytes;
        dir.files["t.dtx"] = dtx();
        dir.children["models"].children["characters"].files["caleb.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].children["characters"].files["caleb1.dtx"] = dtx();
        dir.children["models"].children["characters"].files["ophelia.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].children["characters"].files["ophelia.dtx"] = dtx();
        for (auto path : {"beretta_pv.abc", "lh_beretta_pv.abc", "c_knife_pv.abc"})
            dir.children["models"].children["weapons"].files[path] = test_model::abc(true).b;
        for (auto path : {"c_beretta_pv_t.dtx", "c_knife_pv_t.dtx", "o_beretta_pv_t.dtx", "o_knife_pv_t.dtx"})
            dir.children["skins"].children["weapons"].files[path] = dtx();
        for (auto name : {"beretta_pu", "knifec_pu"})
        {
            dir.children["models"].children["powerups"].files[std::string(name) + ".abc"] = test_model::abc(true).b;
            dir.children["skins"].children["powerups"].files[std::string(name) + ".dtx"] = dtx();
        }
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(archive.b.data()), static_cast<std::streamsize>(archive.b.size()));
        return !ec && out.good();
    }
} // namespace save_world
