#pragma once
#include "player_fixture.h"
namespace test_strings
{
    inline test_model::Writer pe(const std::map<unsigned, std::u16string>& values, unsigned language = 1033,
                                 bool wide = false)
    {
        using test_model::Writer;
        std::map<unsigned, std::array<std::u16string, 16>> blocks;
        for (const auto& [id, text] : values)
            blocks[id / 16 + 1][id % 16] = text;
        Writer r;
        r.b.resize(40 + blocks.size() * 8);
        r.patch(12, 1u << 16);
        r.patch(16, 6);
        r.patch(20, 0x80000018);
        r.patch(36, static_cast<unsigned>(blocks.size()) << 16);
        std::size_t entry = 40;
        for (const auto& [id, strings] : blocks)
        {
            auto dir = static_cast<unsigned>(r.b.size());
            r.b.resize(dir + 40);
            r.patch(entry, id);
            r.patch(entry + 4, 0x80000000 | dir);
            entry += 8;
            r.patch(dir + 12, 1u << 16);
            r.patch(dir + 16, language);
            r.patch(dir + 20, dir + 24);
            auto data = static_cast<unsigned>(r.b.size());
            for (const auto& text : strings)
            {
                r.u16(static_cast<unsigned>(text.size()));
                for (char16_t c : text)
                    r.u16(c);
            }
            r.patch(dir + 24, 0x1000 + data);
            r.patch(dir + 28, static_cast<unsigned>(r.b.size()) - data);
        }
        Writer w;
        w.b.resize(0x200);
        w.b[0] = 'M';
        w.b[1] = 'Z';
        w.patch(60, 64);
        w.patch(64, 0x4550);
        w.patch(68, 0x10000 | 0x14c);
        w.patch(84, wide ? 240 : 224);
        w.patch(88, wide ? 0x20b : 0x10b);
        w.patch(88 + (wide ? 108 : 92), 16);
        w.patch(88 + (wide ? 128 : 112), 0x1000);
        w.patch(88 + (wide ? 132 : 116), static_cast<unsigned>(r.b.size()));
        const unsigned section = 88 + (wide ? 240 : 224);
        w.patch(section + 12, 0x1000);
        w.patch(section + 16, static_cast<unsigned>(r.b.size()));
        w.patch(section + 20, 0x200);
        w.b.insert(w.b.end(), r.b.begin(), r.b.end());
        return w;
    }
    // Minimal PE with one named resource (e.g. TEXT/CREDITS) and `languages` language entries.
    inline test_model::Writer text_pe(std::u16string_view type, std::u16string_view name, std::string_view payload,
                                      unsigned languages = 1)
    {
        using test_model::Writer;
        Writer r;
        r.b.resize(16 + 8 + 16 + 8 + 16 + 8 * languages);
        const unsigned names = 24, langs = 48;
        r.patch(12, 1); // One named entry.
        r.patch(20, 0x80000000 | names);
        r.patch(names + 12, 1);
        r.patch(names + 20, 0x80000000 | langs);
        r.patch(langs + 12, languages << 16);
        auto text = [&](std::u16string_view value)
        {
            const auto at = static_cast<unsigned>(r.b.size());
            r.u16(static_cast<unsigned>(value.size()));
            for (char16_t c : value)
                r.u16(c);
            return at;
        };
        r.patch(16, 0x80000000 | text(type));
        r.patch(names + 16, 0x80000000 | text(name));
        for (unsigned i = 0; i < languages; ++i)
        {
            const auto entry = static_cast<unsigned>(r.b.size());
            r.b.resize(entry + 16);
            r.patch(langs + 16 + 8 * i, 1033 + i);
            r.patch(langs + 20 + 8 * i, entry);
            r.patch(entry, 0x1000 + static_cast<unsigned>(r.b.size()));
            r.patch(entry + 4, static_cast<unsigned>(payload.size()));
            r.b.insert(r.b.end(), payload.begin(), payload.end());
        }
        Writer w;
        w.b.resize(0x200);
        w.b[0] = 'M';
        w.b[1] = 'Z';
        w.patch(60, 64);
        w.patch(64, 0x4550);
        w.patch(68, 0x10000 | 0x14c);
        w.patch(84, 224);
        w.patch(88, 0x10b);
        w.patch(88 + 92, 16);
        w.patch(88 + 112, 0x1000);
        w.patch(88 + 116, static_cast<unsigned>(r.b.size()));
        const unsigned section = 88 + 224;
        w.patch(section + 12, 0x1000);
        w.patch(section + 16, static_cast<unsigned>(r.b.size()));
        w.patch(section + 20, 0x200);
        w.b.insert(w.b.end(), r.b.begin(), r.b.end());
        return w;
    }
    inline bool mount(OpenBlood2::GameSession& g, const std::filesystem::path& root,
                      const std::vector<std::uint8_t>& bytes)
    {
        test_player::Directory dir;
        dir.files["cres.dll"] = bytes;
        test_player::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::ofstream file(root / "strings.rez", std::ios::binary);
        file.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
        file.close();
        std::string error;
        return file.good() && g.resources.mount(root / "strings.rez", error);
    }
} // namespace test_strings
