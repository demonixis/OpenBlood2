#pragma once
#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>
namespace test_model
{
    struct Writer
    {
        std::vector<std::uint8_t> b;
        void u8(unsigned v)
        {
            b.push_back(static_cast<std::uint8_t>(v));
        }
        void u16(unsigned v)
        {
            u8(v);
            u8(v >> 8);
        }
        void u32(unsigned v)
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
            b.insert(b.end(), s.begin(), s.end());
        }
        void patch(std::size_t p, std::uint32_t n)
        {
            for (unsigned i = 0; i < 4; ++i)
                b[p + i] = static_cast<std::uint8_t>(n >> (8 * i));
        }
        void section(const char* name, const Writer& body, bool last = false)
        {
            str(name);
            auto at = b.size();
            u32(0);
            b.insert(b.end(), body.b.begin(), body.b.end());
            patch(at, last ? UINT32_MAX : static_cast<std::uint32_t>(b.size()));
        }
    };
    inline Writer abc(bool weapon = false, bool character = false)
    {
        Writer file, header, g, n, a, d;
        header.str("MonolithExport Model File v6");
        header.str("test");
        file.section("Header", header);
        g.vec(-10, -10, -10);
        g.vec(10, 10, 10);
        g.u32(0);
        g.u16(0);
        g.u32(1);
        for (int i = 0; i < 3; ++i)
        {
            g.real(i == 1 ? 1 : 0);
            g.real(i == 2 ? 1 : 0);
        }
        for (int i = 0; i < 3; ++i)
            g.u16(i);
        g.u8(0);
        g.u8(127);
        g.u8(0);
        g.u32(3);
        g.u32(3);
        for (int i = 0; i < 3; ++i)
        {
            g.vec(i == 1 ? 1 : 0, 0, i == 2 ? 1 : 0);
            g.u8(0);
            g.u8(127);
            g.u8(0);
            g.u8(i ? 1 : 0);
            g.u16(0);
            g.u16(0);
        }
        file.section("Geometry", g);
        for (int i = 0; i < 2; ++i)
        {
            n.vec(-10, -10, -10);
            n.vec(10, 10, 10);
            n.str(i ? "hand" : "root");
            n.u16(i);
            n.u8(i ? 6 : 2);
            n.u32(i ? 2 : 0);
            if (i)
            {
                n.u16(2);
                n.u16(2);
            }
            n.u32(i ? 0 : 1);
        }
        file.section("Nodes", n);
        const char* names[] = {"static_model", "idle",     "draw",           "holster",  "start_fire",
                               "fire",         "end_fire", "start_alt_fire", "alt_fire", "end_alt_fire"};
        const char* character_names[] = {"idle1",
                                         "idle2",
                                         "walk_pistol",
                                         "walk_knife1",
                                         "run_pistol",
                                         "run_knife1",
                                         "jmp_pistol",
                                         "jmp_knife1",
                                         "crouch_1pistol",
                                         "crouch_knife1",
                                         "crawl_1pistol",
                                         "crawl_knife1",
                                         "strafe_right_1pistol",
                                         "fire_stand_1pistol",
                                         "fire_stand_knife1",
                                         "fire_walk_1pistol",
                                         "fire_walk_knife1",
                                         "fire_run_1pistol",
                                         "fire_run_knife1",
                                         "fire_jump_1pistol",
                                         "fire_jump_knife1",
                                         "fire_crouch_1pistol",
                                         "fire_crouch_knife1",
                                         "fire_crawl_1pistol",
                                         "fire_crawl_knife1"};
        const int count = character ? 25 : weapon ? 10 : 2;
        a.u32(count);
        for (int clip = 0; clip < count; ++clip)
        {
            const bool single = !weapon && !character && clip;
            a.str(character ? character_names[clip] : weapon ? names[clip] : clip ? "static" : "walk");
            a.u32(single ? 0 : 100);
            a.vec(-10, -10, -10);
            a.vec(10, 10, 10);
            a.u32(single ? 1 : 3);
            for (int f = 0; f < (single ? 1 : 3); ++f)
            {
                a.u32(f * 50);
                a.vec(-10, -10, -10);
                a.vec(10, 10, 10);
                a.str(weapon   ? ((clip == 5 || clip == 8) && f == 1 ? "fire_key" : "")
                      : f == 0 ? "START"
                      : f == 1 ? "SOUND \"left step\"; FOOT LEFT"
                               : "END");
            }
            for (int node = 0; node < 2; ++node)
            {
                for (int f = 0; f < (single ? 1 : 3); ++f)
                {
                    a.vec(node ? 0 : static_cast<float>(f * 10), node ? 2 : 0, 0);
                    a.vec();
                    a.real(2);
                }
                if (node)
                {
                    for (int f = 0; f < (single ? 1 : 3); ++f)
                    {
                        for (int duplicate = 0; duplicate < 2; ++duplicate)
                        {
                            a.u8(f * 4 + duplicate * 2);
                            a.u8(2);
                            a.u8(3);
                        }
                    }
                    a.vec(.5f, 1, 2);
                    a.vec(1, 0, 0);
                }
                else
                {
                    a.vec(1, 1, 1);
                    a.vec();
                }
            }
            if (character)
            {
                const std::string_view name = character_names[clip];
                d.vec(12, name.find("crouch") != name.npos || name.find("crawl") != name.npos ? 26 : 41, 12);
            }
            else
                d.vec(1, 2, 1);
        }
        file.section("Animation", a);
        file.section("AnimDims", d, true);
        return file;
    }
} // namespace test_model
