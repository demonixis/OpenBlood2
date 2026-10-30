#pragma once
#include "../model_fixture.h"
#include "engine/resources.h"
#include <fstream>
namespace test_network_audio
{
    inline bool setup(OpenBlood2::Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        using test_model::Writer;
        Writer wave;
        const auto text = [](Writer& w, std::string_view s)
        {
            for (char c : s)
                w.u8(c);
        };
        text(wave, "RIFF");
        wave.u32(36 + 16000);
        text(wave, "WAVEfmt ");
        wave.u32(16);
        wave.u16(1);
        wave.u16(1);
        wave.u32(8000);
        wave.u32(16000);
        wave.u16(2);
        wave.u16(16);
        text(wave, "data");
        wave.u32(16000);
        for (unsigned i = 0; i < 8000; ++i)
            wave.u16(16384);
        Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        rez.b.insert(rez.b.end(), wave.b.begin(), wave.b.end());
        auto directory = unsigned(rez.b.size());
        rez.u32(0);
        rez.u32(168);
        rez.u32(unsigned(wave.b.size()));
        rez.u32(0);
        rez.u32(1);
        rez.u32(('w' << 16) | ('a' << 8) | 'v');
        rez.u32(0);
        text(rez, "tone");
        rez.u8(0);
        rez.u8(0);
        rez.patch(131, directory);
        rez.patch(135, unsigned(rez.b.size()) - directory);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec)
            return false;
        {
            std::ofstream f(root / "sound.rez", std::ios::binary);
            f.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
            if (!f)
                return false;
        }
        return resources.mount(root / "sound.rez", error);
    }
} // namespace test_network_audio
