#include "engine/ima_cursor.h"
#include "game/music_catalog.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Fixture
    {
        std::shared_ptr<ImaSection> section = std::make_shared<ImaSection>();
        std::shared_ptr<ImaStyle> style = std::make_shared<ImaStyle>();
        std::shared_ptr<ImaTimeline> timeline = std::make_shared<ImaTimeline>();
        std::shared_ptr<ImaHarmony> harmony = std::make_shared<ImaHarmony>();
        ImaCursorSeed seed;
        ImaBand band;
        std::vector<std::uint8_t> percussion_map;
        bool prepare(std::string& error)
        {
            ImaPersonality personality;
            personality.scale_mask = 0xab5ab5;
            return prepare_ima_timeline(*section, *style, *timeline, error) &&
                   prepare_ima_harmony(*section, *timeline, personality, *harmony, error);
        }
        bool start(ImaSectionCursor& c, std::string& error) const
        {
            return c.start(section, style, timeline, harmony, seed, error);
        }
    };
    void command(Fixture& f, int tick, std::uint32_t flags)
    {
        ImaSectionCommand c;
        c.time = tick;
        for (unsigned i = 0; i < 4; ++i)
            c.value[i] = std::uint8_t(flags >> (i * 8));
        f.section->commands.push_back(c);
    }
    Fixture fixture(unsigned index = 0, bool notes = false)
    {
        Fixture f;
        f.section->measures = 4;
        f.section->ticks_per_measure = 768;
        f.section->ticks_per_beat = 192;
        f.section->chords.resize(1);
        f.section->chords[0].chord_pattern = 0x91;
        f.style->beats_per_measure = 4;
        f.style->beat_unit = 4;
        for (unsigned p = 0; p < 3; ++p)
        {
            ImaPattern pattern;
            pattern.flags = p == 2 ? 0x224 : 0x204;
            pattern.timing = {192, 4, 192, std::int16_t(2 - (p == 1))};
            pattern.length = 768 * pattern.timing[3];
            pattern.grids.resize(p == 2 ? 3 : 8);
            for (unsigned g = 0; g < pattern.grids.size(); ++g)
            {
                pattern.grids[g].index = std::int16_t(100 - g); // Authored index must not drive lookup.
                for (unsigned ch = 0; ch < 16; ++ch)
                {
                    ImaNote n;
                    n.channel = std::uint8_t(ch);
                    n.variations = std::uint16_t((index + 1) * (g + 1) * (ch + 1));
                    pattern.grids[g].notes.push_back(n);
                }
            }
            for (unsigned ch = 0; ch < 16; ++ch)
                for (unsigned v = 0; v < 16; ++v)
                    pattern.variation_flags[ch][v] = std::uint16_t(index * 977 + ch * 13 + v * 3881);
            f.style->patterns.push_back(std::move(pattern));
        }
        if (notes)
        {
            for (unsigned ch = 0; ch < 16; ++ch)
                f.band.channels[ch].transpose = std::int8_t(int((index + ch) % 25) - 12);
            if (index % 2)
            {
                f.percussion_map.resize(128);
                for (unsigned i = 0; i < 128; ++i)
                    f.percussion_map[i] = std::uint8_t((i + index) % 128);
            }
            for (auto& p : f.style->patterns)
            {
                p.pitch_range_channels = std::uint16_t(index * 997);
                for (unsigned ch = 0; ch < 16; ++ch)
                {
                    p.upper_pitch[ch] = 84;
                    p.lower_pitch[ch] = 36;
                    p.play_modes[ch] = std::uint8_t(ch % 7);
                }
                for (auto& g : p.grids)
                    for (auto& n : g.notes)
                    {
                        n.type = index % 2 ? 8 : 9;
                        n.music_value = std::uint16_t(0x4000 | (n.channel % 3) * 0x100);
                        n.midi_note = std::uint8_t(n.channel % 7 ? 48 + n.channel : 0);
                        n.play_mode = std::uint8_t((index + n.channel) % 7 | (index % 3 ? 0x80 : 0));
                        n.pitch_modifier = std::uint8_t(n.channel % 4 * 16);
                        n.velocity = std::uint8_t(80 + index % 64);
                        n.duration = std::int16_t(200 - index * 2);
                        n.time_offset = std::int16_t(int(index % 31) - 15);
                        n.time_range = std::uint8_t(index % 2 ? 255 : 0);
                        n.duration_range = std::uint8_t(index % 3 ? 7 : 0);
                        n.velocity_range = std::uint8_t(index % 5 ? 128 : 0);
                    }
            }
        }
        f.seed.random_channels = 0x5555;
        f.seed.cycle_channels = 0xaaaa;
        f.seed.variations.fill(1);
        return f;
    }
    std::array<std::uint16_t, 4096> draws(std::uint64_t step)
    {
        std::array<std::uint16_t, 4096> out;
        for (unsigned i = 0; i < out.size(); ++i)
            out[i] = std::uint16_t((step * 313 + i * 997 + 32760) & 32767);
        return out;
    }
    bool behavior()
    {
        std::string error;
        auto f = fixture();
        command(f, 768, 0x220);
        command(f, 1536, 0x200);
        CHECK(f.prepare(error));
        // Keep a two-measure first pattern despite the next command's one-measure request.
        f.style->patterns[1].flags = 0;
        ImaSectionCursor c;
        CHECK(f.start(c, error));
        for (int i = 0; i < 4; ++i)
            CHECK(c.step(draws(i), error));
        CHECK(c.state().grid == 4);
        CHECK(c.step(draws(4), error));
        CHECK(c.last_step()->command_restart && c.last_step()->pattern_selected && c.state().pattern == 2 &&
              c.last_step()->grid_ordinal == 4 && !c.last_step()->grid && c.state().grid == 1);
        CHECK(c.step(draws(5), error) && c.last_step()->grid == 1 && c.last_step()->draws_used == 0);
        while (c.active())
            CHECK(c.step(draws(c.state().steps), error));
        CHECK(c.state().ended && c.state().tick == 0 && c.state().grid == 0 && c.state().serial == 0);
        auto before = c.state();
        CHECK(!c.step({}, error) && c.state() == before);
        // Chord flags/scale alone do not cause reevaluation, but mask/root changes do.
        f = fixture();
        auto chord = f.section->chords[0];
        chord.beat = 1;
        f.section->chords.push_back(chord);
        CHECK(f.prepare(error));
        f.harmony->chords[1].variation_flags ^= 0xffff;
        f.harmony->chords[1].harmony.scale_mask ^= 0x555;
        CHECK(f.start(c, error) && c.step(draws(0), error) && c.step({}, error));
        CHECK(c.last_step()->section.chord == 1 && !c.last_step()->chord_changed && !c.last_step()->variations_changed);
        f = fixture();
        chord = f.section->chords[0];
        chord.beat = 1;
        chord.chord_pattern = 0x89;
        f.section->chords.push_back(chord);
        CHECK(f.prepare(error) && f.start(c, error) && c.step(draws(0), error));
        before = c.state();
        const auto last_tick = c.last_step()->tick;
        CHECK(!c.step({}, error) && c.state() == before && c.last_step()->tick == last_tick);
        auto bad_draws = draws(1);
        bad_draws[0] = 32768;
        CHECK(!c.step(bad_draws, error) && c.state() == before);
        CHECK(c.step(draws(1), error) && c.last_step()->chord_changed && c.last_step()->draws_used == 8);
        return true;
    }
    bool bounds_lifetime()
    {
        std::string error;
        auto f = fixture();
        CHECK(f.prepare(error));
        MusicCatalog catalog;
        CHECK(f.start(catalog.cursor, error));
        std::weak_ptr<const ImaStyle> retained = f.style;
        auto before = catalog.cursor.state();
        CHECK(!catalog.cursor.start({}, f.style, f.timeline, f.harmony, {}, error) && catalog.cursor.state() == before);
        auto bad = fixture();
        CHECK(bad.prepare(error));
        bad.style->patterns[0].timing[0] = 0;
        CHECK(!bad.start(catalog.cursor, error) && catalog.cursor.state() == before);
        bad = fixture();
        CHECK(bad.prepare(error));
        bad.style->patterns[0].length = 192 * 32768;
        CHECK(!bad.start(catalog.cursor, error));
        bad = fixture();
        CHECK(bad.prepare(error));
        bad.style->patterns[0].grids[0].notes[0].channel = 16;
        CHECK(!bad.start(catalog.cursor, error));
        bad = fixture();
        CHECK(bad.prepare(error));
        bad.harmony->chords[0].source++;
        CHECK(!bad.start(catalog.cursor, error));
        bad = fixture();
        CHECK(bad.prepare(error));
        bad.style->patterns[0].grids.resize(65537);
        CHECK(!bad.start(catalog.cursor, error));
        bad = fixture();
        CHECK(bad.prepare(error));
        bad.style->patterns[0].grids[0].notes.resize(262145);
        CHECK(!bad.start(catalog.cursor, error));
        f = {};
        CHECK(!retained.expired() && catalog.cursor.step(draws(0), error));
        catalog.inspect({}, {}); // The world catalogue resets even when the new request fails.
        CHECK(retained.expired() && !catalog.cursor.active() && !catalog.cursor.last_step());
        auto next = fixture();
        CHECK(next.prepare(error) && next.start(catalog.cursor, error));
        retained = next.style;
        next = {};
        catalog.cursor.stop();
        CHECK(retained.expired());
        return true;
    }
    bool notes_behavior()
    {
        std::string error;
        auto f = fixture(0, true);
        f.style->patterns.resize(1);
        auto& p = f.style->patterns.front();
        ImaNote n;
        n.type = 9;
        n.midi_note = 60;
        n.velocity = 80;
        n.duration = 12;
        n.variations = 0xffff;
        n.time_offset = -3;
        n.time_range = 3;
        n.duration_range = 5;
        n.velocity_range = 7;
        auto muted = n;
        muted.channel = 1;
        muted.type = 255;
        auto excluded = n;
        excluded.channel = 2;
        excluded.type = 255;
        excluded.variations = 0;
        auto zero = n;
        zero.midi_note = 0;
        auto drum = n;
        drum.channel = 5;
        drum.type = 8;
        drum.midi_note = 40;
        drum.time_offset = 0;
        drum.duration = 10;
        drum.time_range = drum.duration_range = drum.velocity_range = 0;
        p.grids[0].notes = {n, muted, excluded, zero, drum};
        p.grids[1].notes = p.grids[0].notes;
        p.grids[1].notes.back().type = 14; // Unsupported after earlier rows prepared.
        f.section->mutes = {{0, 2, 0}};
        f.seed.random_channels = 1;
        f.seed.cycle_channels = 0;
        f.seed.variations.fill(0xffff);
        f.band.channels[0].transpose = 2;
        f.band.channels[5].transpose = -12;
        f.percussion_map.resize(128);
        for (unsigned i = 0; i < 128; ++i)
            f.percussion_map[i] = std::uint8_t(i);
        f.percussion_map[40] = 45;
        CHECK(f.prepare(error));
        ImaSectionCursor c;
        CHECK(f.start(c, error));
        const std::array<std::uint16_t, 5> pool{5, 2, 4, 6, 32768};
        const auto initial = c.state();
        CHECK(!c.step_notes(std::span(pool).first(3), -5, f.band, f.percussion_map, error));
        CHECK(c.state() == initial && !c.last_notes() && !c.last_step());
        auto invalid = pool;
        invalid[2] = 32768;
        CHECK(!c.step_notes(invalid, -5, f.band, f.percussion_map, error) && c.state() == initial);
        CHECK(!c.step_notes(pool, -5, f.band, std::span(f.percussion_map).first(127), error));
        CHECK(!c.step_notes(pool, 2147483647, f.band, f.percussion_map, error) && c.state() == initial);
        CHECK(c.step_notes(pool, -5, f.band, f.percussion_map, error));
        const auto& report = *c.last_step();
        const auto& notes = *c.last_notes();
        CHECK(report.admitted == 3 && report.muted == 1 && report.excluded_variation == 1 && report.draws_used == 1);
        CHECK(notes.grid_time == -5 && notes.draws_used == 3 && notes.suppressed == 1 && notes.rows.size() == 3);
        CHECK(notes.rows[0].row == 0 && notes.rows[1].row == 3 && notes.rows[2].row == 4);
        CHECK((notes.rows[0].events.messages[0] == ImaTimedMidiMessage{-7, 0x94, 62, 83}));
        CHECK((notes.rows[0].events.messages[1] == ImaTimedMidiMessage{7, 0x84, 62, 83}));
        CHECK(!notes.rows[1].events.emitted && !notes.rows[1].events.draws_used);
        CHECK((notes.rows[2].events.messages[0] == ImaTimedMidiMessage{-5, 0x99, 33, 80}));
        const auto previous = c.state();
        CHECK(!c.step_notes(pool, 192, f.band, f.percussion_map, error) && c.state() == previous);
        CHECK(c.last_step()->tick == 0 && c.last_notes()->grid_time == -5 && c.last_notes()->rows.size() == 3);
        // Retain resources while the caller disappears, but keep prepared values independent of the band/map.
        const auto copied = *c.last_notes();
        std::weak_ptr<const ImaStyle> retained = f.style;
        f = {};
        CHECK(!retained.expired() && copied.rows[2].events.messages[0].key == 33);
        CHECK(c.step({}, error) && !c.last_notes()); // Admission-only steps clear old note inspection.
        c.stop();
        CHECK(retained.expired() && !c.last_notes() && copied.rows[0].events.messages[0].key == 62);
        MusicCatalog catalog;
        CHECK(!catalog.step_cursor_notes(pool) && !catalog.cursor_error.empty());
        catalog.inspect({}, {});
        CHECK(catalog.cursor_error.empty() && !catalog.cursor.last_notes());
        return true;
    }
    bool active_behavior()
    {
        std::string error;
        auto f = fixture();
        f.section->measures = 1;
        f.style->patterns.resize(1);
        auto& p = f.style->patterns.front();
        for (auto& g : p.grids)
            g.notes.clear();
        auto note = [](unsigned key, int duration)
        {
            ImaNote n;
            n.type = 9;
            n.variations = 0xffff;
            n.midi_note = std::uint8_t(key);
            n.duration = std::int16_t(duration);
            n.velocity = 90;
            return n;
        };
        p.grids[0].notes = {note(60, 500), note(61, 1000), note(62, 192), note(63, 193)};
        p.grids[1].notes = {note(65, 1000)};
        auto chord = f.section->chords[0];
        chord.beat = 1;
        chord.chord_pattern = 1;
        f.section->chords.push_back(chord);
        f.seed.random_channels = f.seed.cycle_channels = 0;
        f.seed.variations.fill(0xffff);
        CHECK(f.prepare(error));
        ImaSectionCursor c;
        CHECK(f.start(c, error) && c.step_notes({}, 0, f.band, {}, error));
        CHECK(c.last_notes()->note_offs.size() == 1 && c.last_notes()->note_offs[0].key == 62);
        CHECK(c.active_notes().size() == 3 && c.active_notes()[0].off.key == 60 && c.active_notes()[1].off.key == 61 &&
              c.active_notes()[2].off.key == 63);
        auto copied = c; // Independent state, shared immutable inputs.
        CHECK(c.step_notes({}, 192, f.band, {}, error));
        CHECK(c.last_notes()->shortened == 2 && c.last_notes()->note_offs.size() == 2);
        CHECK(c.last_notes()->note_offs[0].key == 63 && c.last_notes()->note_offs[1].key == 61);
        CHECK(c.last_notes()->note_offs[0].time == 193 && c.last_notes()->note_offs[1].time == 193);
        CHECK(c.active_notes().size() == 2 && c.active_notes()[0].off.key == 60 && c.active_notes()[1].off.key == 65 &&
              c.active_notes()[1].serial == 1);
        // A caller clock jump retains assignment, not min(deadline,tick+1), and original unsorted off order.
        CHECK(copied.step_notes({}, 1000, f.band, {}, error));
        CHECK(copied.last_notes()->note_offs.size() == 3 && copied.last_notes()->note_offs[0].time == 1001 &&
              copied.last_notes()->note_offs[1].time == 1001 && copied.last_notes()->note_offs[2].time == 500);
        const auto before = c.state();
        const auto held = c.active_notes();
        CHECK(!c.start({}, f.style, f.timeline, f.harmony, {}, error) && c.state() == before &&
              c.active_notes() == held && c.last_notes()->grid_time == 192);
        CHECK(!c.step_notes({}, 2147483647, f.band, {}, error) && c.state() == before && c.active_notes() == held);
        CHECK(c.step_notes({}, 384, f.band, {}, error) && c.last_notes()->note_offs.size() == 1 &&
              c.last_notes()->note_offs[0].key == 60 && c.last_notes()->shortened == 0);
        CHECK(c.step_notes({}, 576, f.band, {}, error) && c.state().ended && c.active_notes().size() == 1);
        CHECK(c.active_notes()[0].off.time == 1192); // No invented note flush at section end.
        CHECK(!c.step_notes({}, 768, f.band, {}, error) && c.active_notes().size() == 1);
        const auto tail = c.active_notes();
        c.stop();
        CHECK(c.active_notes().empty() && tail[0].off.key == 65);
        // Bounded capacity failure preserves old active notes, reports and clocks after candidate note preparation.
        f = fixture();
        f.style->patterns.resize(1);
        for (auto& g : f.style->patterns[0].grids)
            g.notes.clear();
        f.style->patterns[0].grids[0].notes.assign(ImaSectionCursor::active_note_limit, note(60, 2000));
        f.style->patterns[0].grids[1].notes = {note(61, 2000)};
        f.seed.random_channels = f.seed.cycle_channels = 0;
        f.seed.variations.fill(0xffff);
        CHECK(f.prepare(error) && f.start(c, error) && c.step_notes({}, 0, f.band, {}, error));
        CHECK(c.active_notes().size() == ImaSectionCursor::active_note_limit);
        const auto full = c.state();
        CHECK(!c.step_notes({}, 192, f.band, {}, error) && c.state() == full &&
              c.active_notes().size() == ImaSectionCursor::active_note_limit && c.last_notes()->grid_time == 0);
        CHECK(c.step({}, error) && c.active_notes().empty() && !c.last_notes());
        c.stop();
        // Original low-word equality exempts an old note after serial wrap as well as a newly created note.
        // Hold the explicit output clock still to isolate this boundary; this is not live transport timing.
        f = fixture();
        f.section->measures = 257;
        f.style->patterns.resize(1);
        auto& long_pattern = f.style->patterns[0];
        long_pattern.timing[0] = 3;
        long_pattern.length = 3 * 32767;
        long_pattern.grids.resize(1);
        long_pattern.grids[0].notes = {note(61, 1000)};
        chord = f.section->chords[0];
        chord.measure = 256;
        chord.chord_pattern = 1;
        f.section->chords.push_back(chord);
        f.seed.random_channels = f.seed.cycle_channels = 0;
        f.seed.variations.fill(0xffff);
        CHECK(f.prepare(error) && f.start(c, error));
        for (unsigned i = 0; i < 65536; ++i)
            CHECK(c.step_notes({}, 0, f.band, {}, error));
        CHECK(c.state().serial == 0 && c.active_notes().size() == 3);
        CHECK(c.step_notes({}, 0, f.band, {}, error) && c.last_notes()->shortened == 2 &&
              c.last_notes()->note_offs.size() == 2 && c.active_notes().size() == 1 &&
              c.active_notes()[0].serial == 0 && c.active_notes()[0].off.time == 1000);
        MusicCatalog catalog;
        catalog.cursor = std::move(c);
        std::weak_ptr<const ImaStyle> retained = f.style;
        const auto saved = catalog.cursor.active_notes();
        f = {};
        CHECK(!retained.expired());
        catalog.inspect({}, {}); // Failed replacement still unloads the prior world-owned cursor.
        CHECK(retained.expired() && !catalog.cursor.active() && catalog.cursor.active_notes().empty() &&
              !catalog.cursor.last_notes() && saved[0].off.key == 61);
        return true;
    }
    void word(std::vector<std::uint8_t>& out, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(std::uint8_t(value >> (8 * i)));
    }
    std::vector<std::uint8_t> trace(const ImaSectionCursor& c)
    {
        std::vector<std::uint8_t> out;
        const auto& s = c.state();
        const auto& r = *c.last_step();
        for (auto v : {std::uint32_t(r.tick),
                       std::uint32_t(*s.pattern),
                       std::uint32_t(r.grid_ordinal),
                       std::uint32_t(bool(r.grid)),
                       std::uint32_t(r.interval),
                       std::uint32_t(r.pattern_grids),
                       std::uint32_t(r.section.mute_mask),
                       std::uint32_t(r.section.chord),
                       std::uint32_t(r.command_restart),
                       std::uint32_t(r.pattern_selected),
                       std::uint32_t(r.chord_changed),
                       std::uint32_t(r.variations_changed),
                       std::uint32_t(r.draws_used),
                       std::uint32_t(r.admitted),
                       std::uint32_t(r.muted),
                       std::uint32_t(r.excluded_variation),
                       std::uint32_t(s.tick),
                       std::uint32_t(s.grid),
                       std::uint32_t(s.serial),
                       s.channels.previous_chord,
                       std::uint32_t(s.channels.previous_root),
                       std::uint32_t(s.ended)})
            word(out, v);
        for (auto v : s.channels.variations)
            word(out, v);
        return out;
    }
    void export_fixture(std::ostream& output, const Fixture& f, bool notes)
    {
        std::vector<std::uint8_t> b;
        auto add = [&](auto v) { word(b, static_cast<std::uint32_t>(v)); };
        add(f.section->measures);
        add(f.timeline->ticks_per_measure);
        add(f.timeline->duration);
        add(f.seed.random_channels);
        add(f.seed.cycle_channels);
        add(f.seed.previous_chord);
        add(f.seed.previous_root);
        for (auto v : f.seed.variations)
            add(v);
        if (notes)
        {
            add(f.harmony->base.chord_mask);
            add(f.harmony->base.chord_root);
            for (const auto& c : f.band.channels)
                add(c.transpose);
            add(f.percussion_map.size());
            for (auto v : f.percussion_map)
                add(v);
        }
        add(f.timeline->commands.size());
        for (auto c : f.timeline->commands)
        {
            add(c.time);
            add(c.time / f.timeline->ticks_per_measure);
            const auto& v = f.section->commands[c.source].value;
            add(std::uint32_t(v[0]) | (std::uint32_t(v[1]) << 8) | (std::uint32_t(v[2]) << 16) |
                (std::uint32_t(v[3]) << 24));
        }
        add(f.timeline->chords.size());
        for (const auto& c : f.harmony->chords)
        {
            add(c.time);
            add(c.harmony.chord_mask);
            add(c.harmony.chord_root);
            if (notes)
            {
                add(c.harmony.scale_mask);
                add(c.harmony.chord_flags);
            }
            add(c.variation_flags);
            add(f.section->chords[c.source].measure);
            add(f.section->chords[c.source].beat);
        }
        add(f.timeline->mutes.size());
        for (auto m : f.timeline->mutes)
        {
            add(m.time);
            add(m.value);
        }
        add(f.style->patterns.size());
        for (const auto& p : f.style->patterns)
        {
            add(p.flags);
            add(p.length);
            for (auto v : p.timing)
                add(v);
            for (const auto& row : prepare_ima_pattern_channels(p).variation_flags)
                for (auto v : row)
                    add(v);
            if (notes)
            {
                add(p.pitch_range_channels);
                for (unsigned ch = 0; ch < 16; ++ch)
                {
                    add(p.upper_pitch[ch]);
                    add(p.lower_pitch[ch]);
                    add(prepare_ima_pattern_channels(p).play_modes[ch]);
                }
            }
            add(p.grids.size());
            for (const auto& g : p.grids)
            {
                add(g.notes.size());
                for (auto n : g.notes)
                {
                    add(n.channel);
                    add(n.variations);
                    if (notes)
                    {
                        add(n.type);
                        add(n.time_offset);
                        add(n.pitch_modifier);
                        add(n.byte7);
                        add(n.midi_note);
                        add(n.velocity);
                        add(n.music_value);
                        add(n.duration);
                        add(n.time_range);
                        add(n.duration_range);
                        add(n.velocity_range);
                        add(n.play_mode);
                    }
                }
            }
        }
        output.write(reinterpret_cast<const char*>(b.data()), b.size());
    }
    bool walk(const Fixture& f, std::uint64_t& hash, std::size_t& steps, std::ostream* output, std::string& error,
              bool notes = false, bool active = false)
    {
        ImaSectionCursor c;
        CHECK(f.start(c, error));
        if (output)
            export_fixture(*output, f, notes);
        std::vector<std::uint8_t> records;
        while (c.active())
        {
            CHECK(c.state().steps < 65536);
            const auto pool = draws(c.state().steps);
            CHECK(notes ? c.step_notes(pool, c.state().tick + 7001, f.band, f.percussion_map, error)
                        : c.step(pool, error));
            auto b = trace(c);
            if (notes)
            {
                const auto& n = *c.last_notes();
                CHECK(n.rows.size() == c.last_step()->admitted);
                word(b, std::uint32_t(n.draws_used));
                word(b, std::uint32_t(n.suppressed));
                word(b, std::uint32_t(n.rows.size()));
                for (const auto& row : n.rows)
                {
                    word(b, std::uint32_t(row.row));
                    word(b, std::uint32_t(row.pitch.pitch));
                    word(b, row.events.emitted);
                    for (auto m : row.events.messages)
                    {
                        word(b, std::uint32_t(m.time));
                        word(b, m.status);
                        word(b, m.key);
                        word(b, m.velocity);
                    }
                }
            }
            if (active)
            {
                const auto& n = *c.last_notes();
                word(b, std::uint32_t(n.shortened));
                word(b, std::uint32_t(n.note_offs.size()));
                for (auto m : n.note_offs)
                {
                    word(b, std::uint32_t(m.time));
                    word(b, m.status);
                    word(b, m.key);
                    word(b, m.velocity);
                }
                word(b, std::uint32_t(c.active_notes().size()));
                for (auto n : c.active_notes())
                {
                    word(b, std::uint32_t(n.off.time));
                    word(b, n.off.status);
                    word(b, n.off.key);
                    word(b, n.off.velocity);
                    word(b, n.serial);
                }
            }
            for (auto x : b)
            {
                hash ^= x;
                hash *= 1099511628211ull;
            }
            ++steps;
            if (output)
                records.insert(records.end(), b.begin(), b.end());
        }
        if (output)
        {
            std::vector<std::uint8_t> size;
            word(size, std::uint32_t(c.state().steps));
            output->write(reinterpret_cast<const char*>(size.data()), size.size());
            output->write(reinterpret_cast<const char*>(records.data()), records.size());
        }
        return true;
    }
    bool generated(std::ostream* output = nullptr, bool notes = false, bool active = false)
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t steps = 0;
        for (unsigned i = 0; i < 128; ++i)
        {
            auto f = fixture(i, notes);
            f.section->measures = std::int16_t(2 + i % 7);
            command(f, 768, 0x220);
            command(f, 768, 0x200);
            command(f, 2304, 0x400);
            if (i % 3 == 0)
                command(f, 1536, 0x200); // Deliberately unsorted tail.
            if (active)
            {
                f.section->chords.front().root = std::int8_t(int(i * 2) - 128);
                constexpr std::array<std::uint32_t, 8> masks{0,           1,           0x91,        0x889,
                                                             0x80000000u, 0xffffffffu, 0x80100001u, 0xabcdefu};
                f.section->chords.front().chord_pattern = masks[i % masks.size()];
                for (auto& p : f.style->patterns)
                    for (auto& g : p.grids)
                        for (auto& n : g.notes)
                        {
                            n.music_value = 0; // Isolate active-note membership across every signed root.
                            n.duration = std::int16_t(400 + (i % 7) * 192);
                        }
            }
            auto ch = f.section->chords.front();
            ch.beat = std::int8_t(i % 3 + 1);
            ch.chord_pattern = i % 2 ? 0x89 : 0x91;
            f.section->chords.push_back(ch);
            ch.measure = 1;
            ch.beat = 1;
            ch.root = active ? std::int8_t(int(i * 2) - 127) : 2;
            f.section->chords.push_back(ch);
            f.section->mutes = {{192, std::uint16_t(i * 511), 0}, {1152, std::uint16_t(~(i * 511)), 0}};
            f.seed.random_channels = std::uint16_t(i * 997);
            f.seed.cycle_channels = std::uint16_t(~(i * 313));
            if (i % 4 == 0)
                f.style->patterns[1].flags = 0;
            if (i % 5 == 0)
                f.style->patterns[0].timing[0] = i % 10 == 0 ? 130 : 128;
            if (i % 7 == 0)
                f.style->patterns[2].grids.clear();
            CHECK(f.prepare(error) && walk(f, hash, steps, output, error, notes, active));
        }
        std::cout << "Generated cursor steps " << steps << "; FNV64 " << hash << '\n';
        CHECK(steps == 2799 && hash == (active  ? 4553459640914638122ull
                                        : notes ? 937667046537544111ull
                                                : 10075978454587385952ull));
        return true;
    }
    std::vector<std::uint8_t> read(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }
    std::string lower(std::string s)
    {
        for (auto& c : s)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
    struct DispatchDigest
    {
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t steps = 0, messages = 0, curves = 0, muted = 0, excluded = 0;
    };
    bool dispatch_walk(const Fixture& f, const ImaCurves& curves, DispatchDigest& digest, std::ostream* output,
                       std::string& error)
    {
        ImaSectionCursor c;
        CHECK(f.start(c, error));
        while (c.active())
        {
            const auto pool = draws(c.state().steps);
            const auto tick = c.state().tick + 7001;
            CHECK(c.step_notes(pool, tick, f.band, {}, error, &curves));
            const auto& report = *c.last_step();
            const auto& notes = *c.last_notes();
            CHECK(notes.curves_prepared && !notes.opaque_curve_chunks);
            CHECK(notes.dispatch.size() ==
                  notes.rows.size() - notes.suppressed + notes.curve_messages + notes.note_offs.size());
            CHECK(std::equal(notes.note_offs.begin(), notes.note_offs.end(),
                             notes.dispatch.end() - notes.note_offs.size()));
            std::vector<std::uint8_t> result;
            const auto count = notes.dispatch.size() - notes.note_offs.size();
            word(result, count);
            for (std::size_t i = 0; i < count; ++i)
                for (auto v : {std::uint32_t(notes.dispatch[i].time), std::uint32_t(notes.dispatch[i].status),
                               std::uint32_t(notes.dispatch[i].key), std::uint32_t(notes.dispatch[i].velocity)})
                    word(result, v);
            for (auto v : result)
                digest.hash = (digest.hash ^ v) * 1099511628211ull;
            ++digest.steps;
            digest.messages += count;
            digest.curves += notes.curves_admitted;
            digest.muted += notes.curves_muted;
            digest.excluded += notes.curves_excluded;
            if (output)
            {
                std::vector<std::uint8_t> b, events;
                auto add = [&](auto v) { word(b, static_cast<std::uint32_t>(v)); };
                auto event = [&](auto v) { word(events, static_cast<std::uint32_t>(v)); };
                add(tick);
                add(report.section.mute_mask);
                for (auto v : c.state().channels.variations)
                    add(v);
                for (auto ch : f.band.channels)
                    add(ch.transpose);
                add(notes.draws_used);
                for (std::size_t i = 0; i < notes.draws_used; ++i)
                    add(pool[report.draws_used + i]);
                std::size_t records = 0;
                if (report.grid)
                {
                    const auto& grid = f.style->patterns[*report.pattern.pattern].grids[*report.grid];
                    bool inside = false;
                    std::size_t row = 0;
                    for (const auto& chunk : f.style->source.chunks)
                    {
                        if (inside && chunk.depth <= 2)
                            break;
                        if (!inside)
                        {
                            inside = chunk.offset == grid.offset && chunk.depth == 2;
                            continue;
                        }
                        if (chunk.depth != 3)
                            continue;
                        if (chunk.id == "note")
                            for (std::size_t i = 0; i < (chunk.size - 2) / 18; ++i, ++row)
                            {
                                const auto& n = grid.notes[row];
                                event(0);
                                event(n.type);
                                event(n.channel);
                                event(n.time_offset);
                                event(n.variations);
                                event(n.velocity);
                                event(n.duration);
                                event(n.time_range);
                                event(n.duration_range);
                                event(n.velocity_range);
                                auto it = std::find_if(notes.rows.begin(), notes.rows.end(),
                                                       [&](const auto& x) { return x.row == row; });
                                event(it == notes.rows.end() ? 0 : it->pitch.pitch);
                                ++records;
                            }
                        else if (chunk.id == "curv")
                            for (const auto& curve : curves.curves)
                                if (curve.chunk_offset == chunk.offset)
                                {
                                    event(1);
                                    event(curve.type);
                                    event(curve.channel);
                                    event(curve.time_offset);
                                    event(curve.variations);
                                    event(curve.controller);
                                    event(curve.points.size());
                                    for (auto p : curve.points)
                                        for (auto v : {int(p.shape), int(p.flags), int(p.start), int(p.value),
                                                       int(p.end), int(p.target)})
                                            event(v);
                                    ++records;
                                }
                    }
                }
                add(records);
                b.insert(b.end(), events.begin(), events.end());
                b.insert(b.end(), result.begin(), result.end());
                output->write(reinterpret_cast<const char*>(b.data()), b.size());
            }
        }
        return true;
    }
    Fixture dispatch_fixture(unsigned i, ImaCurves& curves)
    {
        auto f = fixture(i, true);
        f.style->patterns.resize(1);
        auto& p = f.style->patterns[0];
        p.offset = 12;
        p.length = 192;
        p.grids.resize(1);
        auto& g = p.grids[0];
        g.offset = 32;
        g.curve_chunks = 1;
        f.style->source.chunks = {{"RIFF", "AAPT", 12, 480, 1},
                                  {"LIST", "AACL", 32, 400, 2},
                                  {"note", "", 44, 146, 3},
                                  {"curv", "", 198, 42, 3},
                                  {"note", "", 248, 146, 3}};
        ImaCurve c;
        c.pattern_offset = 12;
        c.grid_offset = 32;
        c.chunk_offset = 198;
        c.type = 3;
        c.channel = i % 16;
        c.variations = 1u << (i % 16);
        c.controller = 7;
        c.points = {{std::uint8_t(i % 5), std::uint8_t(i % 4), -1, 1513, 798, 8192}};
        curves.curves = {c};
        c.ordinal = 1;
        c.type = 4;
        c.channel = (i + 3) % 16;
        c.variations = 0xffff;
        c.points = {{1, 0, -5, 1, 20, 90}};
        curves.curves.push_back(c);
        curves.chunks = 1;
        curves.points = 2;
        f.section->mutes = {{0, std::uint16_t(i * 511), 0}};
        f.percussion_map.clear();
        return f;
    }
    bool dispatch_generated(std::ostream* output = nullptr)
    {
        std::string error;
        DispatchDigest d;
        for (unsigned i = 0; i < 64; ++i)
        {
            ImaCurves curves;
            auto f = dispatch_fixture(i, curves);
            CHECK(f.prepare(error) && dispatch_walk(f, curves, d, output, error));
        }
        std::cout << "Generated dispatch " << d.steps << " steps / " << d.messages << " messages / curves " << d.curves
                  << " muted " << d.muted << " excluded " << d.excluded << " FNV64 " << d.hash << '\n';
        CHECK(d.steps == 1024 && d.messages == 4348 && d.curves == 336 && d.muted == 1280 && d.excluded == 432 &&
              d.hash == 15334764557161491361ull);
        return true;
    }
    bool dispatch_behavior()
    {
        std::string error;
        ImaCurves curves;
        auto f = dispatch_fixture(0, curves);
        f.seed.random_channels = f.seed.cycle_channels = 0;
        f.seed.variations.fill(0xffff);
        CHECK(f.prepare(error));
        ImaSectionCursor c;
        CHECK(f.start(c, error));
        CHECK(c.step_notes(draws(0), 100, f.band, {}, error, &curves));
        const auto state = c.state();
        const auto saved = *c.last_notes();
        const auto active = c.active_notes();
        CHECK(saved.curves_prepared && saved.curves_admitted == 2 && saved.curve_messages > 1);
        auto bad = curves;
        bad.curves[0].ordinal = 2;
        CHECK(!c.step_notes(draws(1), 200, f.band, {}, error, &bad) && c.state() == state &&
              c.last_notes()->dispatch == saved.dispatch && c.active_notes() == active);
        bad = curves;
        bad.curves[0].points.clear();
        CHECK(!c.step_notes(draws(1), 200, f.band, {}, error, &bad) && c.state() == state);
        bad = curves;
        bad.curves[0].channel = 16;
        CHECK(!c.step_notes(draws(1), 200, f.band, {}, error, &bad) && c.state() == state);
        bad = curves;
        bad.curves[0].type = 7;
        CHECK(!c.step_notes(draws(1), 200, f.band, {}, error, &bad) && c.state() == state);
        CHECK(c.step_notes(draws(1), 200, f.band, {}, error) && !c.last_notes()->curves_prepared &&
              c.last_notes()->dispatch.empty());
        c.stop();
        curves = {};
        CHECK(!c.last_notes() && !saved.dispatch.empty());
        // Combined dispatch is bounded independently of each curve's event limit.
        ImaCurves many;
        auto large = dispatch_fixture(0, many);
        large.seed.random_channels = large.seed.cycle_channels = 0;
        large.seed.variations.fill(0xffff);
        auto curve = many.curves.front();
        curve.points = {{0, 0, 0, 0, 19200, 19200}};
        many.curves.assign(1500, curve);
        for (std::size_t i = 0; i < many.curves.size(); ++i)
            many.curves[i].ordinal = i;
        auto& chunks = large.style->source.chunks;
        chunks[3].size = 4 + 19 * 1500;
        chunks[4].offset = chunks[3].offset + 8 + chunks[3].size;
        CHECK(large.prepare(error) && large.start(c, error));
        const auto initial = c.state();
        CHECK(!c.step_notes(draws(0), 0, large.band, {}, error, &many) && c.state() == initial && !c.last_notes());
        MusicCatalog catalog;
        catalog.cursor = std::move(c);
        catalog.cursor_file = "test.sec";
        MusicAssetRecord section;
        section.filename = catalog.cursor_file;
        section.selected_band = std::make_shared<ImaBand>(large.band);
        section.section_style = large.style;
        MusicAssetRecord style;
        style.style = large.style;
        catalog.assets = {section, style}; // Failed independent curve decode must not silently skip it.
        CHECK(!catalog.step_cursor_notes(draws(0)) && catalog.cursor.state() == initial &&
              !catalog.cursor_error.empty());
        catalog.inspect({}, {});
        CHECK(!catalog.cursor.active() && catalog.cursor_error.empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root, std::ostream* output = nullptr, bool notes = false,
                      bool active = false, bool dispatch = false)
    {
        std::string error;
        DispatchDigest digest;
        std::vector<std::filesystem::path> files;
        for (const auto& p : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (p.path().extension() == ".sec")
                files.push_back(p.path());
        std::sort(files.begin(), files.end());
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t steps = 0, bound = 0, missing = 0;
        for (const auto& path : files)
        {
            Fixture f;
            CHECK(read_ima_section(read(path), *f.section, error));
            std::filesystem::path style;
            for (const auto& p : std::filesystem::directory_iterator(path.parent_path()))
                if (lower(p.path().filename().string()) == lower(f.section->source.style_reference + ".sty"))
                    style = p.path();
            if (style.empty())
            {
                CHECK(path.parent_path().filename() == "Track1");
                ++missing;
                continue;
            }
            CHECK(read_ima_style(read(style), *f.style, error) && f.style->identifier == f.section->style_id);
            if (notes)
            {
                ImaBands section_bands, style_bands;
                std::optional<ImaBandChoice> choice;
                CHECK(read_ima_bands(read(path), MusicAssetKind::Section, section_bands, error));
                CHECK(read_ima_bands(read(style), MusicAssetKind::Style, style_bands, error));
                CHECK(select_ima_band(section_bands, &style_bands, choice, error));
                f.band = (choice->scope == ImaBandScope::Section ? section_bands : style_bands).bands[choice->index];
            }
            f.seed.random_channels = 0xffff;
            ImaPersonality personality;
            CHECK(read_ima_personality(read(path.parent_path() / "SILENCE.PER"), personality, error));
            CHECK(prepare_ima_timeline(*f.section, *f.style, *f.timeline, error) &&
                  prepare_ima_harmony(*f.section, *f.timeline, personality, *f.harmony, error) &&
                  (dispatch || walk(f, hash, steps, output, error, notes, active)));
            if (dispatch)
            {
                ImaCurves curves;
                CHECK(read_ima_curves(read(style), curves, error) && dispatch_walk(f, curves, digest, output, error));
            }
            ++bound;
        }
        if (dispatch)
        {
            std::cout << "Private dispatch " << digest.steps << " steps / " << digest.messages << " messages / curves "
                      << digest.curves << " muted " << digest.muted << " excluded " << digest.excluded << " FNV64 "
                      << digest.hash << '\n';
            CHECK(bound == 132 && missing == 24 && digest.steps == 8174 && digest.messages == 9063 &&
                  digest.curves == 42 && digest.muted == 0 && digest.excluded == 2 &&
                  digest.hash == 10183239741431151107ull);
            return true;
        }
        std::cout << "Private cursor steps " << steps << "; bound/missing " << bound << '/' << missing << "; FNV64 "
                  << hash << '\n';
        CHECK(bound == 132 && missing == 24 && steps == 8174 &&
              hash == (active  ? 182961564129265722ull
                       : notes ? 5880823308775508798ull
                               : 14793402716944126194ull));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view mode = argv[1];
    if ((mode == "export" || mode == "notes_export" || mode == "active_export") && argc == 4)
    {
        // Optional transient emulator input: contains external data, never commit this output.
        std::ofstream out(argv[2], std::ios::binary);
        const bool active = mode == "active_export";
        const bool notes = mode != "export";
        return out && generated(&out, notes, active) && private_data(argv[3], &out, notes, active) && out.good() ? 0
                                                                                                                 : 1;
    }
    if (mode == "dispatch_export" && argc == 4)
    {
        std::ofstream out(argv[2], std::ios::binary);
        return out && dispatch_generated(&out) && private_data(argv[3], &out, true, false, true) && out.good() ? 0 : 1;
    }
    if (mode == "dispatch_generated")
        return dispatch_generated() ? 0 : 1;
    if (mode == "dispatch_behavior")
        return dispatch_behavior() ? 0 : 1;
    if (mode == "dispatch_private" && argc == 3)
        return private_data(argv[2], nullptr, true, false, true) ? 0 : 1;
    if (mode == "active_generated")
        return generated(nullptr, true, true) ? 0 : 1;
    if (mode == "active_private" && argc == 3)
        return private_data(argv[2], nullptr, true, true) ? 0 : 1;
    if (mode == "active_behavior")
        return active_behavior() ? 0 : 1;
    if (mode == "notes_behavior")
        return notes_behavior() ? 0 : 1;
    if (mode == "notes_generated")
        return generated(nullptr, true) ? 0 : 1;
    if (mode == "notes_private" && argc == 3)
        return private_data(argv[2], nullptr, true) ? 0 : 1;
    return (mode == "behavior"               ? behavior()
            : mode == "bounds_lifetime"      ? bounds_lifetime()
            : mode == "generated"            ? generated()
            : mode == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
