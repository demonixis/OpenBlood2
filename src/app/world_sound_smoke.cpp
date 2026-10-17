#include "world_sound_smoke.h"
#include "engine/audio_stream.h"
#include "game/original/SoundFX.h"
namespace OpenBlood2
{
    bool check_world_sound_smoke(GameSession& g)
    {
        // The world-owned music data exists independently of inspector visibility/reload.
        const auto& music = g.music_catalog;
        if (!music.error.empty() || music.assets.empty() || !music.assets.front().bank ||
            !music.assets.front().bank_error.empty())
            return false;
        const auto& bank = *music.assets.front().bank;
        if (bank.instruments.size() != 14 || bank.waves.size() != 35 || bank.decoded_bytes != 913403 * sizeof(float) ||
            !bank.waves[8].settings || bank.waves[8].settings->loop_in_bounds)
            return false;
        unsigned count = 0;
        for (const auto& e : g.entries)
            if (e.state == EntryState::WorldSound)
            {
                const auto* v = g.server.audio.get(e.sound);
                if (!v || !v->world_owned || !v->request.loop || v->done || v->cursor <= 0 ||
                    v->clip->frames() != 198623 || v->clip->rate != 22050 || g.objects.get(v->owner) ||
                    v->request.space != SoundSpace::Ambient)
                    return false;
                ++count;
            }
        if (count != 4 || !g.server.audio.has_output() || !g.server.audio.submitted_frames ||
            !g.server.audio.output_error.empty())
            return false;
        // Labelled SoundFX probe with an actual decoded SubwayA asset; runs while UI is hidden and after reload.
        std::string resource;
        for (const auto& entry : g.entries)
            if (entry.state == EntryState::WorldSound)
            {
                resource = g.server.audio.get(entry.sound)->request.resource;
                break;
            }
        auto h = g.objects.create(g.objects.find_class("SoundFX"), {}, Creation::World,
                                  {{"Name", std::string("diagnostic_soundfx")},
                                   {"StartOn", false},
                                   {"Sound", resource},
                                   {"FileStream", true}});
        auto* sound = dynamic_cast<SoundFX*>(g.server.HandleToObject(h));
        if (!sound)
            return false;
        auto command = [&](const char* text)
        {
            auto str = g.server.CreateString(text);
            auto message = g.server.StartMessageToObject(nullptr, h, 1000);
            const bool ok = message && g.server.WriteToMessageHString(message, str) == DE_OK &&
                            g.server.EndMessage(message) == DE_OK;
            g.server.FreeString(str);
            return ok;
        };
        if (!command("ON") || !sound->sound || sound->state != SoundFX::On)
            return false;
        const auto voice = sound->sound;
        const auto submitted = g.server.audio.submitted_frames;
        if (!g.server.advance_audio(.02, {}, {}) || g.server.audio.submitted_frames <= submitted ||
            !g.server.audio.get(voice) || g.server.audio.get(voice)->cursor <= 0 ||
            !g.server.audio.get(voice)->request.loop || !g.server.audio.get(voice)->clip->stream ||
            !g.server.audio.get(voice)->clip->stream->refills || !g.server.audio.get(voice)->clip->samples.empty() ||
            !command("OFF") || sound->state != SoundFX::Off || g.server.audio.get(voice) || !g.objects.remove(h))
            return false;
        g.objects.finish_frame();
        return !g.objects.get(h) && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
