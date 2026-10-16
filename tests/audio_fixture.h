#pragma once
#include "player_fixture.h"
namespace test_audio
{
    using namespace OpenBlood2;
    inline std::vector<std::uint8_t> wave(unsigned channels = 1, unsigned bits = 16, unsigned rate = 8000,
                                          unsigned frames = 800)
    {
        test_model::Writer w;
        for (char c : std::string("RIFF"))
            w.u8(c);
        w.u32(36 + frames * channels * (bits / 8));
        for (char c : std::string("WAVEfmt "))
            w.u8(c);
        w.u32(16);
        w.u16(1);
        w.u16(channels);
        w.u32(rate);
        w.u32(rate * channels * (bits / 8));
        w.u16(channels * (bits / 8));
        w.u16(bits);
        for (char c : std::string("data"))
            w.u8(c);
        w.u32(frames * channels * (bits / 8));
        for (unsigned i = 0; i < frames * channels; ++i)
        {
            if (bits == 8)
                w.u8(192);
            else
                w.u16(16384);
        }
        return w.b;
    }
    inline bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_player::setup(g, root, false))
            return false;
        test_player::Directory d;
        d.files["cue.wav"] = wave();
        d.files["long.wav"] = wave(1, 16, 8000, 8000);
        d.files["stereo.wav"] = wave(2);
        auto w = test_model::Writer{};
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "sound.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        if (!g.resources.mount(root / "sound.rez", error))
            return false;
        return true;
    }
} // namespace test_audio
