#include "world_sound_smoke.h"
#include "engine/audio_stream.h"
#include "engine/ima_curve_event.h"
#include "engine/ima_note_event.h"
#include "engine/ima_note_pitch.h"
#include "engine/ima_selection.h"
#include "engine/ima_variation.h"
#include "game/original/SoundFX.h"
namespace OpenBlood2
{
    bool check_world_sound_smoke(GameSession& g)
    {
        // The world-owned music data exists independently of inspector visibility/reload.
        auto& music = g.music_catalog;
        if (!music.error.empty() || music.assets.empty() || music.personalities.size() != 1 ||
            !music.personalities.front().metadata || !music.personalities.front().error.empty() ||
            !music.assets.front().bank || !music.assets.front().bank_error.empty())
            return false;
        std::size_t pattern_notes = 0, band_count = 0, section_count = 0, curves = 0, curve_points = 0,
                    curve_events = 0;
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
                if (!asset.selected_band || !asset.band_choice || asset.band_choice->scope != ImaBandScope::Section ||
                    asset.band_choice->index != 0 || asset.selected_band.get() != &asset.bands->bands.front() ||
                    !asset.selected_band_error.empty() || asset.selected_band_file != asset.filename ||
                    !asset.band_setup_ready || !asset.band_setup_error.empty() || asset.band_setup[25].status != 0xb9 ||
                    !asset.harmony || !asset.harmony_error.empty() || asset.harmony->chords.size() != 1 ||
                    asset.harmony->base.scale_mask != 0xab5ab5 || !asset.harmony->base_flag ||
                    asset.harmony->chords.front().harmony.enabled || !asset.timeline || !asset.timeline_error.empty() ||
                    asset.timeline->mutes.empty() || asset.timeline->chords.size() != 1 ||
                    !asset.timeline->discarded_chords.empty() || !asset.section || !asset.section_error.empty() ||
                    asset.section->commands.size() != 1 || asset.section->chords.size() != 1 ||
                    asset.section->mutes.empty())
                    return false;
                ImaSectionTiming timing;
                std::string error;
                if (!asset.section_style || !ima_section_timing(*asset.section, timing, error))
                    return false;
                for (auto tick : {0, int(asset.timeline->ticks_per_measure), asset.timeline->duration - 1})
                {
                    ImaSectionSelection selected;
                    ImaPatternSelection pattern;
                    if (!select_ima_section(*asset.section, *asset.timeline, tick, selected, error) ||
                        !select_ima_pattern(*asset.section_style, selected.request, pattern, error) ||
                        !pattern.pattern || selected.chord != 0 || !selected.mute)
                        return false;
                    const auto channels = prepare_ima_pattern_channels(asset.section_style->patterns[*pattern.pattern]);
                    ImaVariationChoice variation;
                    std::array<std::uint16_t, 16> current{}, draws{};
                    if (!choose_ima_variations(channels, asset.harmony->chords[selected.chord].variation_flags, current,
                                               0xffff, 0, draws, variation, error) ||
                        variation.draws_used != 16)
                        return false;
                }
                // Explicitly activate this retained chord for a diagnostic note conversion only.
                // This runs with the UI hidden and in inspector-free builds, without choosing a live song chord.
                auto context = asset.harmony->chords.front().harmony;
                context.enabled = true;
                ImaNote note;
                note.type = 9;
                note.music_value = 0x4000;
                note.play_mode = 0x83;
                ImaNotePitch pitch;
                if (!resolve_ima_note_pitch(note, {}, context, pitch, error) || !pitch.converted)
                    return false;
                ImaCursorSeed seed;
                seed.random_channels = 0xffff;
                if (!music.cursor.start(asset.section, asset.section_style, asset.timeline, asset.harmony, seed, error))
                    return false;
                music.cursor_file = asset.filename;
                for (unsigned step = 0; step < 8 && music.cursor.active(); ++step)
                {
                    const auto active_before = music.cursor.active_notes().size();
                    const std::array<std::uint16_t, 4096> cursor_draws{};
                    if (!music.step_cursor_notes(cursor_draws) || !music.cursor.last_notes() ||
                        !music.cursor.last_step() ||
                        music.cursor.last_notes()->rows.size() != music.cursor.last_step()->admitted ||
                        music.cursor.last_step()->interval <= 0 || music.cursor.state().steps != step + 1)
                        return false;
                    const auto& notes = *music.cursor.last_notes();
                    if (!notes.curves_prepared || notes.opaque_curve_chunks ||
                        notes.dispatch.size() !=
                            notes.rows.size() - notes.suppressed + notes.curve_messages + notes.note_offs.size() ||
                        music.cursor.active_notes().size() + notes.note_offs.size() !=
                            active_before + notes.rows.size() - notes.suppressed ||
                        notes.shortened > active_before)
                        return false;
                }
                music.cursor_file = asset.filename;
                // Retained by the catalogue until actual world unload/reload, even with UI hidden.
                ++section_count;
            }
            if (asset.kind == MusicAssetKind::Style)
            {
                if (!asset.style || !asset.style_error.empty() || !asset.curves || !asset.curves_error.empty() ||
                    asset.curves->chunks != asset.style->curve_chunks)
                    return false;
                curves += asset.curves->curves.size();
                curve_points += asset.curves->points;
                for (const auto& curve : asset.curves->curves)
                {
                    std::vector<ImaTimedMidiMessage> messages;
                    std::string error;
                    if (!prepare_ima_curve_events(curve, 10000, messages, error))
                        return false;
                    curve_events += messages.size();
                }
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
                    for (const auto& grid : pattern.grids)
                        for (const auto& note : grid.notes)
                        {
                            // Explicit synthetic runtime harmony; not the authored song's resolved chord.
                            ImaNotePitch pitch;
                            if (!resolve_ima_note_pitch(note, pattern, {true, 0x91, 0xab5, 0, 0, 0}, pitch, error))
                                return false;
                            const std::size_t count = pitch.pitch ? unsigned(note.time_range != 0) +
                                                                        unsigned(note.duration_range != 0) +
                                                                        unsigned(note.velocity_range != 0)
                                                                  : 0;
                            ImaNoteEvents events;
                            if (!prepare_ima_note_events(note, {10000, pitch.pitch, 0, {}},
                                                         std::span(draws).first(count), events, error) ||
                                events.emitted != (pitch.pitch != 0) || events.draws_used != count)
                                return false;
                            if (events.emitted && (events.messages[1].time <= events.messages[0].time ||
                                                   events.messages[0].key < 1 || events.messages[0].key > 127 ||
                                                   events.messages[0].velocity != events.messages[1].velocity))
                                return false;
                        }
                    for (std::size_t ch = 0; ch < 16; ++ch)
                        if (!effective.play_modes[ch] || choice.selected[ch] != 1 || choice.candidates[ch] != 0xffff)
                            return false;
                }
            }
        }
        if (pattern_notes != 3049 || band_count != 44 || !section_count || curves != 2 || curve_points != 2 ||
            curve_events != 206)
            return false;
        const auto& bank = *music.assets.front().bank;
        if (bank.instruments.size() != 14 || bank.waves.size() != 35 || bank.decoded_bytes != 913403 * sizeof(float) ||
            !bank.waves[8].settings || bank.waves[8].settings->loop_in_bounds)
            return false;
        // An explicit diagnostic note, not a MUSIC/playlist success. It shares the real SDL stream.
        auto& sampler = g.server.audio.instruments;
        if (sampler.queued_messages || !sampler.pending_midi().empty() || sampler.dispatch_failures)
            return false; // The preceding world's future note must have been discarded on reload.
        std::string note_error;
        // This authored region used to refuse its paired legacy/standard pan records.
        const auto& region = bank.instruments[6].regions[4];
        const auto note =
            sampler.play(music.assets.front().bank, 6, (region.key_low + region.key_high) / 2, 100, 1, .2, note_error);
        if (!note || !note_error.empty() || !sampler.voices().back().legacy_pan || sampler.voices().back().pan != 6 ||
            sampler.voices().back().pan_destination != 4)
            return false;
        const MusicAssetRecord* routed = nullptr;
        for (const auto& asset : music.assets)
            if (asset.kind == MusicAssetKind::Section && asset.band_bank)
            {
                routed = &asset;
                break;
            }
        if (!routed || routed->band_bank != music.assets.front().bank || !routed->band_bank_error.empty())
            return false;
        const auto& channel = routed->band_channels[0];
        std::optional<std::size_t> instrument;
        if (!find_dls_instrument(*routed->band_bank, channel, 128, instrument, note_error))
            return false;
        const auto& routed_region = routed->band_bank->instruments[*instrument].regions.front();
        const auto routed_note = sampler.play_channel(
            routed->band_bank, channel, (routed_region.key_low + routed_region.key_high) / 2, 100, 1, .2, note_error);
        if (!routed_note || sampler.voices().back().controllers != channel.controllers ||
            !sampler.voices().back().channel)
            return false;
        const auto before_note = g.server.audio.submitted_frames;
        if (!g.server.advance_audio(.02, {}, {}) || sampler.voices().size() != 2 || sampler.voices()[0].frames != 960 ||
            sampler.voices()[1].frames != 960 || g.server.audio.submitted_frames <= before_note)
            return false;
        auto controls = channel.controllers;
        if (!apply_dls_controller(controls, 0xb4, 7, 0, note_error) ||
            !apply_dls_controller(controls, 0xb4, 10, 127, note_error) ||
            !apply_dls_controller(controls, 0xe4, 127, 127, note_error) ||
            !sampler.set_controllers(routed_note, controls, note_error) || !g.server.advance_audio(.01, {}, {}) ||
            sampler.voices().size() != 2 || sampler.voices()[1].frames != 1440 || sampler.voices()[1].gain != 0 ||
            sampler.voices()[1].controllers != controls ||
            sampler.voices()[1].channel->controllers != channel.controllers ||
            !sampler.set_controllers(routed_note, channel.controllers, note_error) || sampler.controls_updated != 2 ||
            sampler.controls_rejected || !sampler.release(note) || !sampler.release(routed_note))
            return false;
        if (!g.server.advance_audio(4, {}, {}) || !sampler.voices().empty() || sampler.completed == 0)
            return false;
        // Immediate MIDI diagnostic: actual selected band setup, held notes, channel controls and note-off.
        for (const auto& m : routed->band_setup)
            if (!sampler.receive({}, m.status, m.data1, m.data2, .2, note_error))
                return false;
        const auto midi_key = std::uint8_t((routed_region.key_low + routed_region.key_high) / 2);
        // This actual Cabal region explicitly allows same-key overlap; verify its authored rule.
        if (routed_region.options != 1 || !sampler.receive(routed->band_bank, 0x94, midi_key, 100, .2, note_error) ||
            !g.server.advance_audio(.02, {}, {}) || sampler.voices().size() != 1 ||
            sampler.voices()[0].midi_channel != 4 || !sampler.receive({}, 0xb4, 7, 0, .2, note_error) ||
            sampler.voices()[0].gain != 0 ||
            !sampler.receive({}, 0xb4, 7, std::uint8_t(channel.controllers.volume), .2, note_error))
            return false;
        const auto older_note = sampler.voices()[0].id;
        if (!sampler.receive(routed->band_bank, 0x94, midi_key, 100, .2, note_error) || sampler.midi_choked != 0 ||
            !g.server.advance_audio(.02, {}, {}) || sampler.voices().size() != 2 ||
            !sampler.receive({}, 0x84, midi_key, 100, .2, note_error) || !g.server.advance_audio(.001, {}, {}) ||
            sampler.voices().size() != 1 || sampler.voices()[0].id != older_note ||
            !sampler.receive({}, 0x84, midi_key, 100, .2, note_error) || !g.server.advance_audio(4, {}, {}) ||
            !sampler.voices().empty() || sampler.midi_rejected || sampler.midi_received != 86 ||
            sampler.midi_released != 2 || sampler.midi_unmatched_stops)
            return false;
        // Original tempo conversion reaches actual audio output, also hidden and after reload.
        // Source band/tempo are real; this short phrase is synthetic, not a MUSIC playlist.
        const auto origin = sampler.sample_clock();
        if (!routed->section)
            return false;
        const ImaTempo tempo{0, routed->section->tempo, routed->section->tempo_fraction};
        ImaClock clock;
        std::array<std::uint64_t, 4> boundaries;
        if (!clock.prepare(std::span(&tempo, 1), origin, note_error))
            return false;
        for (unsigned i = 0; i < 4; ++i)
            if (!clock.sample(2 * (i + 1), boundaries[i], note_error))
                return false;
        std::vector<ImaTimedMidiMessage> scheduled;
        for (const auto& m : routed->band_setup)
            scheduled.push_back({0, m.status, m.data1, m.data2});
        scheduled.push_back({8, 0x84, midi_key, 100}); // Deliberately submitted before its note-on.
        scheduled.push_back({2, 0x94, midi_key, 100});
        scheduled.push_back({4, 0xb4, 7, 0});
        scheduled.push_back({6, 0xb4, 7, std::uint8_t(channel.controllers.volume)});
        auto advance_to = [&](std::uint64_t frame)
        { return g.server.advance_audio(double(frame - sampler.sample_clock()) / DlsSampler::rate, {}, {}); };
        if (!sampler.enqueue_ima(routed->band_bank, clock, scheduled, .2, note_error) || !sampler.voices().empty() ||
            !advance_to(boundaries[0] - 1) || !sampler.voices().empty() || !advance_to(boundaries[0] + 1) ||
            sampler.voices().size() != 1 || sampler.voices()[0].frames != 1 || !advance_to(boundaries[1]) ||
            sampler.voices()[0].gain != 0 || !advance_to(boundaries[2]) || sampler.voices()[0].gain <= 0 ||
            !advance_to(boundaries[3] + 48) || !sampler.voices().empty() || !sampler.pending_midi().empty() ||
            sampler.sample_clock() != boundaries[3] + 48 || sampler.queued_messages != 84 ||
            sampler.dispatched_messages != 52 || sampler.dispatch_failures || sampler.queue_rejected ||
            sampler.coalesced_messages)
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
        const DlsTimedMidi future[] = {{sampler.sample_clock() + DlsSampler::rate * 60, 0x94, midi_key, 100}};
        return !g.objects.get(h) && g.server.audio.output_error.empty() &&
               sampler.enqueue(routed->band_bank, future, .2, note_error) && sampler.pending_midi().size() == 1;
    }
} // namespace OpenBlood2
