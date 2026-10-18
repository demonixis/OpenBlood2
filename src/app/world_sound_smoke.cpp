#include "world_sound_smoke.h"
#include "engine/audio_stream.h"
#include "engine/ima_variation.h"
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
        std::size_t pattern_notes = 0, band_count = 0, section_count = 0;
        for (const auto& asset : music.assets)
        {
            if (asset.kind != MusicAssetKind::Instruments)
            {
                if (!asset.bands || !asset.bands_error.empty() || asset.bands->bands.size() != 1)
                    return false;
                std::array<ImaMidiSetup, 80> setup;
                std::string error;
                if (!ima_band_setup(asset.bands->bands.front(), setup, error) || setup[25].status != 0xb9)
                    return false;
                ++band_count;
            }
            if (asset.kind == MusicAssetKind::Section)
            {
                if (!asset.timeline || !asset.timeline_error.empty() || asset.timeline->mutes.empty() ||
                    asset.timeline->chords.size() != 1 || !asset.timeline->discarded_chords.empty() || !asset.section ||
                    !asset.section_error.empty() || asset.section->commands.size() != 1 ||
                    asset.section->chords.size() != 1 || asset.section->mutes.empty())
                    return false;
                ImaSectionTiming timing;
                std::string error;
                if (!ima_section_timing(*asset.section, timing, error))
                    return false;
                ++section_count;
            }
            if (asset.kind == MusicAssetKind::Style)
            {
                if (!asset.style || !asset.style_error.empty())
                    return false;
                pattern_notes += asset.style->notes;
                for (const auto& pattern : asset.style->patterns)
                {
                    const auto effective = prepare_ima_pattern_channels(pattern);
                    ImaVariationChoice choice;
                    std::string error;
                    std::array<std::uint16_t, 16> current{}, draws{};
                    // Explicit diagnostic decision, independent of UI and game RNG.
                    if (!choose_ima_variations(effective, 0, current, 0xffff, 0, draws, choice, error) ||
                        choice.draws_used != 16 || choice.fallback_channels != 0)
                        return false;
                    for (std::size_t ch = 0; ch < 16; ++ch)
                        if (!effective.play_modes[ch] || choice.selected[ch] != 1 || choice.candidates[ch] != 0xffff)
                            return false;
                }
            }
        }
        if (pattern_notes != 3049 || band_count != 44 || !section_count)
            return false;
        const auto& bank = *music.assets.front().bank;
        if (bank.instruments.size() != 14 || bank.waves.size() != 35 || bank.decoded_bytes != 913403 * sizeof(float) ||
            !bank.waves[8].settings || bank.waves[8].settings->loop_in_bounds)
            return false;
        // An explicit diagnostic note, not a MUSIC/playlist success. It shares the real SDL stream.
        auto& sampler = g.server.audio.instruments;
        std::string note_error;
        // This authored region used to refuse its paired legacy/standard pan records.
        const auto& region = bank.instruments[6].regions[4];
        const auto note =
            sampler.play(music.assets.front().bank, 6, (region.key_low + region.key_high) / 2, 100, 1, .2, note_error);
        if (!note || !note_error.empty() || !sampler.voices().back().legacy_pan || sampler.voices().back().pan != 6 ||
            sampler.voices().back().pan_destination != 4)
            return false;
        const auto before_note = g.server.audio.submitted_frames;
        if (!g.server.advance_audio(.02, {}, {}) || sampler.voices().size() != 1 || sampler.voices()[0].frames != 960 ||
            g.server.audio.submitted_frames <= before_note || !sampler.release(note))
            return false;
        if (!g.server.advance_audio(4, {}, {}) || !sampler.voices().empty() || sampler.completed == 0)
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
