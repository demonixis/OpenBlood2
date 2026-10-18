#include "engine/audio_stream.h"
#include "engine/ima_note_event.h"
#include "engine/ima_note_pitch.h"
#include "engine/ima_selection.h"
#include "engine/ima_variation.h"
#include "inspector.h"
#include <algorithm>
#include <imgui.h>
namespace OpenBlood2
{
    namespace
    {
        void draw_section_selection(const MusicAssetRecord& asset)
        {
            if (!ImGui::TreeNode("Section selection probe (explicit tick; no playback)"))
                return;
            static std::int32_t tick = 0;
            ImGui::InputScalar("Section tick", ImGuiDataType_S32, &tick);
            ImGui::TextWrapped("Normal authored-section decision. This does not advance a clock, seek, "
                               "activate a pattern, consume RNG or send notes.");
            ImaSectionSelection selected;
            ImaPatternSelection pattern;
            std::string error;
            if (!asset.timeline || !asset.section_style)
                ImGui::TextUnformatted("Unavailable: no validated style/timeline binding.");
            else if (!select_ima_section(*asset.section, *asset.timeline, tick, selected, error) ||
                     !select_ima_pattern(*asset.section_style, selected.request, pattern, error))
                ImGui::TextWrapped("Selection refused: %s", error.c_str());
            else
            {
                ImGui::Text("Measure %d | mute mask %04x | requested measures %d / flags %08x", selected.measure,
                            selected.mute_mask, selected.request.measures, selected.request.flags);
                const auto& chord = asset.timeline->chords[selected.chord];
                ImGui::Text("Retained chord %zu | source %zu | tick %d", selected.chord, chord.source, chord.time);
                if (selected.mute)
                    ImGui::Text("Mute sample %zu | tick %d", *selected.mute,
                                asset.timeline->mutes[*selected.mute].time);
                else
                    ImGui::TextUnformatted("No prepared mute record.");
                if (asset.harmony && selected.chord < asset.harmony->chords.size())
                {
                    const auto& h = asset.harmony->chords[selected.chord];
                    ImGui::Text("Selected chord/scale %08x / %08x | roots %d / %d | pitch %02x / variation %04x",
                                h.harmony.chord_mask, h.harmony.scale_mask, h.harmony.chord_root, h.harmony.scale_root,
                                h.harmony.chord_flags, h.variation_flags);
                }
                if (pattern.pattern)
                {
                    const auto& p = asset.section_style->patterns[*pattern.pattern];
                    ImGui::Text("Pattern %zu: %s | %d measures | flags %08x", *pattern.pattern, p.name.c_str(),
                                p.timing[3], p.flags);
                }
                ImGui::Text("Decision: %s", ima_pattern_match_name(pattern.match));
            }
            ImGui::TreePop();
        }
        void draw_note_event_probe(const ImaGrid& grid, const ImaPattern& pattern)
        {
            if (grid.notes.empty() || !ImGui::TreeNode("Note event probe (explicit inputs; no playback)"))
                return;
            static int index = 0, tick = 10000, pitch = 60, transpose = 0;
            static int random[3]{0, 0, 0};
            static bool convert = true, harmony_enabled = true;
            static std::uint32_t chord_mask = 0x91, scale_mask = 0xab5;
            static int chord_root = 0, scale_root = 0, chord_flags = 0;
            ImGui::TextWrapped("Explicit diagnostic context, not the song's selected harmony. No personality "
                               "reconciliation, musical RNG, percussion remap or MIDI output occurs.");
            ImGui::SliderInt("Source note", &index, 0, static_cast<int>(grid.notes.size()) - 1);
            index = std::clamp(index, 0, static_cast<int>(grid.notes.size()) - 1);
            ImGui::InputInt("Grid tick", &tick);
            ImGui::Checkbox("Convert original note pitch", &convert);
            bool pitch_ok = true;
            if (convert)
            {
                ImGui::Checkbox("Harmonic mode active", &harmony_enabled);
                ImGui::InputScalar("Effective chord mask (hex)", ImGuiDataType_U32, &chord_mask, nullptr, nullptr,
                                   "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::InputScalar("Effective scale mask (hex)", ImGuiDataType_U32, &scale_mask, nullptr, nullptr,
                                   "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::SliderInt("Chord root", &chord_root, -128, 127);
                ImGui::SliderInt("Scale root", &scale_root, -128, 127);
                ImGui::SliderInt("Chord flags", &chord_flags, 0, 255);
                ImaNotePitch resolved;
                std::string pitch_error;
                const ImaHarmonicContext harmony{harmony_enabled,
                                                 chord_mask,
                                                 scale_mask,
                                                 static_cast<std::int8_t>(std::clamp(chord_root, -128, 127)),
                                                 static_cast<std::int8_t>(std::clamp(scale_root, -128, 127)),
                                                 static_cast<std::uint8_t>(std::clamp(chord_flags, 0, 255))};
                pitch_ok = resolve_ima_note_pitch(grid.notes[index], pattern, harmony, resolved, pitch_error);
                if (pitch_ok)
                {
                    pitch = resolved.pitch;
                    ImGui::Text("Resolved pitch %d | %s | mode %u", pitch,
                                resolved.converted ? "converted" : "raw bypass", resolved.mode);
                }
                else
                    ImGui::TextWrapped("Pitch refused: %s", pitch_error.c_str());
            }
            else
                ImGui::SliderInt("Explicit resolved pitch (0 suppresses)", &pitch, -32768, 32767);
            ImGui::SliderInt("Band transpose", &transpose, -128, 127);
            ImGui::SliderInt3("Explicit draws in consumption order", random, 0, 32767);
            const auto& note = grid.notes[index];
            const std::size_t count = pitch ? unsigned(note.time_range != 0) + unsigned(note.duration_range != 0) +
                                                  unsigned(note.velocity_range != 0)
                                            : 0;
            std::array<std::uint16_t, 3> draws{};
            for (unsigned i = 0; i < 3; ++i)
                draws[i] = static_cast<std::uint16_t>(std::clamp(random[i], 0, 32767));
            ImaNoteEvents events;
            std::string error;
            const ImaNoteEventContext context{tick,
                                              static_cast<std::int16_t>(std::clamp(pitch, -32768, 32767)),
                                              static_cast<std::int8_t>(std::clamp(transpose, -128, 127)),
                                              {}};
            if (!pitch_ok)
                ImGui::TextUnformatted("No event preparation: pitch conversion failed.");
            else if (!prepare_ima_note_events(note, context, std::span(draws).first(count), events, error))
                ImGui::TextWrapped("Probe refused: %s", error.c_str());
            else if (!events.emitted)
                ImGui::TextUnformatted("Zero resolved pitch: no events and no draws.");
            else
            {
                ImGui::Text("%u draws used; event pair prepared, not sent", events.draws_used);
                for (const auto& event : events.messages)
                    ImGui::Text("Tick %d | status %02x | key %u | velocity %u", event.time, event.status, event.key,
                                event.velocity);
            }
            ImGui::TreePop();
        }
    } // namespace
    void Inspector::draw_world_sounds(GameSession& game)
    {
        const auto& music = game.music_catalog;
        if (ImGui::TreeNodeEx("DLS instruments, samples and diagnostic notes", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& sampler = game.server.audio.instruments;
            ImGui::TextWrapped(
                "Audition creates a one-second note at the region midpoint, velocity 100, gain 20%%. "
                "This is a sample/envelope diagnostic, not original music playback. Unsupported "
                "connections refuse the note; no MIDI controller, exclusivity or sequence service is claimed.");
            ImGui::Text("Voices %zu/%u | started %llu | completed %llu | refused %llu", sampler.voices().size(),
                        DlsSampler::max_voices, (unsigned long long)sampler.started,
                        (unsigned long long)sampler.completed, (unsigned long long)sampler.rejected);
            if (!sampler.last_error.empty())
                ImGui::TextWrapped("Last note failure: %s", sampler.last_error.c_str());
            for (const auto& v : sampler.voices())
            {
                ImGui::PushID(int(v.id));
                ImGui::Text(
                    "Note %llu | instrument %zu / region %zu | key %u velocity %u | %.3f s / cursor %.2f / loops %llu",
                    (unsigned long long)v.id, v.instrument, v.region, v.key, v.velocity,
                    double(v.frames) / DlsSampler::rate, v.cursor, (unsigned long long)v.loops);
                ImGui::Text("Amplitude A/D/S/R %.3f / %.3f / %.3f / %.3f | pitch EG %.1f cents | LFO %.2f Hz",
                            v.amplitude.attack, v.amplitude.decay, v.amplitude.sustain, v.amplitude.release,
                            v.envelope_pitch, v.lfo_hz);
                ImGui::Text("Constant pan %.3f (0.1%% units) | last destination %u | legacy record %s | L/R %.3f/%.3f",
                            v.pan, v.pan_destination, v.legacy_pan ? "yes" : "no", v.left, v.right);
                if (ImGui::SmallButton("Release note"))
                    sampler.release(v.id);
                ImGui::PopID();
            }
            auto settings = [](const DlsSampleSettings& sample)
            {
                ImGui::Text("Unity note %u | fine tune %d | raw attenuation %d | options 0x%x",
                            unsigned(sample.unity_note), int(sample.fine_tune), sample.attenuation, sample.options);
                if (sample.loop)
                    ImGui::Text("Forward loop: start %u / length %u frames | %s", sample.loop->start,
                                sample.loop->length,
                                sample.loop_in_bounds ? "in bounds" : "INVALID default (region override required)");
                else
                    ImGui::TextUnformatted("One shot (no sample loop)");
            };
            auto articulation = [](const DlsArticulation& art)
            {
                ImGui::Text("Articulation graph: %s / %zu raw connections", art.present ? "present" : "absent",
                            art.connections.size());
                for (const auto& c : art.connections)
                {
                    ImGui::Text("  src 0x%x / control 0x%x -> dest 0x%x / transform 0x%x / scale %d",
                                unsigned(c.source), unsigned(c.control), unsigned(c.destination), unsigned(c.transform),
                                c.scale);
                    if (c.source == 0 && c.control == 0 && c.transform == 0 && c.destination == 2)
                        ImGui::TextUnformatted("    Legacy integer pan (0.1% units); later pan records overwrite it.");
                }
            };
            for (const auto& asset : music.assets)
            {
                if (asset.kind != MusicAssetKind::Instruments)
                    continue;
                if (!ImGui::TreeNode(asset.filename.c_str()))
                    continue;
                if (!asset.bank)
                    ImGui::TextWrapped("No decoded bank: %s",
                                       asset.bank_error.empty() ? asset.error.c_str() : asset.bank_error.c_str());
                else
                {
                    const auto& bank = *asset.bank;
                    ImGui::Text("%zu instruments / %zu waves / %zu cues / %.2f MiB PCM / %zu opaque chunks",
                                bank.instruments.size(), bank.waves.size(), bank.cues.size(),
                                bank.decoded_bytes / 1048576.0, bank.opaque_chunks);
                    for (const auto& instrument : bank.instruments)
                    {
                        const auto label = "Bank " + std::to_string(instrument.bank) + " / program " +
                                           std::to_string(instrument.program);
                        if (!ImGui::TreeNode(label.c_str()))
                            continue;
                        articulation(instrument.articulation);
                        for (std::size_t i = 0; i < instrument.regions.size(); ++i)
                        {
                            const auto& r = instrument.regions[i];
                            if (!ImGui::TreeNode(reinterpret_cast<void*>(i + 1),
                                                 "Region %zu: keys %u..%u / velocity %u..%u", i, unsigned(r.key_low),
                                                 unsigned(r.key_high), unsigned(r.velocity_low),
                                                 unsigned(r.velocity_high)))
                                continue;
                            const auto& w = bank.waves[r.wave];
                            ImGui::Text("Cue %u -> wave %u | %zu frames / %u Hz | pool offset %u", r.cue, r.wave,
                                        w.samples.size(), w.rate, w.pool_offset);
                            ImGui::Text("Region flags 0x%x / group %u | wave flags 0x%x / phase %u / channel %u",
                                        unsigned(r.options), unsigned(r.key_group), unsigned(r.wave_options),
                                        unsigned(r.phase_group), r.channel);
                            ImGui::Text("Settings: %s",
                                        r.settings_override ? "region override" : "inherited from wave");
                            settings(r.settings);
                            articulation(r.articulation);
                            if (ImGui::Button("Audition region (1 second)"))
                            {
                                std::string error;
                                sampler.play(asset.bank, std::size_t(&instrument - bank.instruments.data()),
                                             (r.key_low + r.key_high) / 2,
                                             std::clamp(100U, unsigned(r.velocity_low), unsigned(r.velocity_high)), 1,
                                             .2, error);
                            }
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("All waves (including unreferenced)"))
                    {
                        for (std::size_t i = 0; i < bank.waves.size(); ++i)
                        {
                            const auto& w = bank.waves[i];
                            if (!ImGui::TreeNode(reinterpret_cast<void*>(i + 1), "Wave %zu: %zu frames / %u Hz", i,
                                                 w.samples.size(), w.rate))
                                continue;
                            ImGui::Text("Pool offset %u", w.pool_offset);
                            if (w.settings)
                                settings(*w.settings);
                            else
                                ImGui::TextUnformatted("No wave settings; a region must supply them");
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        if (ImGui::CollapsingHeader("Original music assets (sequence playback unavailable)",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Directory: %s | %zu original requests / %zu unique assets | %zu bytes read",
                               music.plan.directory.c_str(), music.plan.requests.size(), music.assets.size(),
                               music.bytes_read);
            ImGui::TextWrapped(
                "Decoded IMA sections/bands/pattern notes and DLS sample/region data. Live pattern "
                "activation, curves, personality "
                "graphs and playlist playback are not implemented. MUSIC commands "
                "still report unsupported. Explicit DLS diagnostic notes above use the existing audio output.");
            const auto prepared =
                std::count_if(music.assets.begin(), music.assets.end(), [](const auto& a) { return bool(a.harmony); });
            const auto failed = std::count_if(music.assets.begin(), music.assets.end(),
                                              [](const auto& a) { return !a.harmony_error.empty(); });
            ImGui::Text("Harmony: %zu personality dependencies / %zu prepared sections / %zu unresolved",
                        music.personalities.size(), static_cast<std::size_t>(prepared),
                        static_cast<std::size_t>(failed));
            if (!music.error.empty())
                ImGui::TextWrapped("Music inspection: %s", music.error.c_str());
            if (ImGui::TreeNode("Original request order"))
            {
                for (const auto& request : music.plan.requests)
                    ImGui::BulletText("%s: %s", request.role.c_str(), request.filename.c_str());
                ImGui::TreePop();
            }
            if (ImGui::BeginTable("Music resources", 4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, 200)))
            {
                for (const char* title : {"External file", "Container", "Records", "Style reference / error"})
                    ImGui::TableSetupColumn(title);
                ImGui::TableHeadersRow();
                for (const auto& asset : music.assets)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(asset.filename.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s\n%zu bytes", asset.container_valid ? "RIFF valid only" : "Failed", asset.bytes);
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu chunks\n%zu patterns\n%zu note chunks", asset.chunks, asset.patterns,
                                asset.note_chunks);
                    if (asset.bank)
                        ImGui::Text("%zu instruments\n%zu waves", asset.bank->instruments.size(),
                                    asset.bank->waves.size());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", asset.error.empty() ? asset.style_reference.c_str() : asset.error.c_str());
                    if (!asset.style_error.empty())
                        ImGui::TextWrapped("IMA note decoding failed: %s", asset.style_error.c_str());
                    if (!asset.section_error.empty())
                        ImGui::TextWrapped("IMA section decoding failed: %s", asset.section_error.c_str());
                    if (!asset.timeline_error.empty())
                        ImGui::TextWrapped("IMA timeline unavailable: %s", asset.timeline_error.c_str());
                    if (!asset.harmony_error.empty())
                        ImGui::TextWrapped("IMA harmony unavailable: %s", asset.harmony_error.c_str());
                    if (!asset.bands_error.empty())
                        ImGui::TextWrapped("IMA band decoding failed: %s", asset.bands_error.c_str());
                    if (!asset.bank_error.empty())
                        ImGui::TextWrapped("DLS decoding failed: %s", asset.bank_error.c_str());
                }
                ImGui::EndTable();
            }
        }
        if (ImGui::TreeNode("IMA personality metadata (opaque graph)"))
        {
            for (const auto& p : music.personalities)
            {
                ImGui::Text("%s | %zu bytes", p.filename.c_str(), p.bytes);
                if (!p.error.empty())
                    ImGui::TextWrapped("Unavailable: %s", p.error.c_str());
                if (p.metadata)
                    ImGui::Text("%s | scale %08x | %u header bytes | %zu opaque graph chunks", p.metadata->name.c_str(),
                                p.metadata->scale_mask, p.metadata->header_bytes, p.metadata->opaque_chunks.size());
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("IMA sections (authored data only)"))
        {
            ImGui::TextWrapped("Authored records and the style-reconciled timeline are separate. Mutes are sampled "
                               "at each effective beat; chords are not final pitches. No sequence is scheduled.");
            for (const auto& asset : music.assets)
            {
                if (!asset.section || !ImGui::TreeNode(asset.filename.c_str()))
                    continue;
                const auto& section = *asset.section;
                ImGui::Text("%s | style: %s | personality label: %s", section.name.c_str(),
                            section.source.style_reference.c_str(), section.personality.c_str());
                ImGui::Text("Tempo %d + %u/65536 | %d measures | %d ticks/measure | %d ticks/beat", section.tempo,
                            section.tempo_fraction, section.measures, section.ticks_per_measure,
                            section.ticks_per_beat);
                ImaSectionTiming timing;
                std::string error;
                if (ima_section_timing(section, timing, error))
                    ImGui::Text("Authored length %d ticks | original MIDI export %u us/quarter", timing.duration_ticks,
                                timing.microseconds_per_quarter);
                else
                    ImGui::TextWrapped("Timing unavailable: %s", error.c_str());
                ImGui::Text("Raw word0 %08x / word38 %d / word48 %08x / default chord %02x", section.word0,
                            section.word38, section.word48, section.default_chord);
                ImGui::Text("%zu commands / %zu mutes / %zu chords | opaque notes %zu / curves %zu",
                            section.commands.size(), section.mutes.size(), section.chords.size(),
                            section.opaque_note_chunks, section.opaque_curve_chunks);
                if (!asset.timeline_error.empty())
                    ImGui::TextWrapped("Timeline unavailable: %s", asset.timeline_error.c_str());
                if (!asset.harmony_error.empty())
                    ImGui::TextWrapped("Harmony unavailable: %s", asset.harmony_error.c_str());
                draw_section_selection(asset);
                if (asset.harmony && ImGui::TreeNode("Prepared harmony (no live activation)"))
                {
                    const auto& h = *asset.harmony;
                    ImGui::Text("Base scale %08x | root %d | pitch flags %02x | base flag %d", h.base.scale_mask,
                                h.base.scale_root, h.base.chord_flags, int(h.base_flag));
                    for (const auto& c : h.chords)
                        ImGui::Text("source %zu | tick %d | chord %08x / scale %08x | roots %d / %d | pitch %02x / "
                                    "variation %04x",
                                    c.source, c.time, c.harmony.chord_mask, c.harmony.scale_mask, c.harmony.chord_root,
                                    c.harmony.scale_root, c.harmony.chord_flags, c.variation_flags);
                    ImGui::TextWrapped("The explicit tick probe selects a chord; live activation and sequence playback "
                                       "remain unavailable.");
                    ImGui::TreePop();
                }
                if (asset.timeline && ImGui::TreeNode("Style-reconciled timeline (not playback)"))
                {
                    const auto& t = *asset.timeline;
                    ImGui::Text("%d ticks/beat | %d ticks/measure | %d total ticks | default beat unit: %s",
                                t.ticks_per_beat, t.ticks_per_measure, t.duration, t.default_beat_unit ? "yes" : "no");
                    ImGui::Text("%zu commands / %zu sampled mutes / %zu chords / %zu discarded chord beats",
                                t.commands.size(), t.mutes.size(), t.chords.size(), t.discarded_chords.size());
                    if (ImGui::TreeNode("Reconciled commands/chords"))
                    {
                        for (const auto& c : t.commands)
                            ImGui::Text("Command source %zu: tick %d", c.source, c.time);
                        for (const auto& c : t.chords)
                            ImGui::Text("Chord source %zu: tick %d", c.source, c.time);
                        for (auto i : t.discarded_chords)
                            ImGui::Text("Discarded chord source %zu: beat %d", i, section.chords[i].beat);
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Mutes at effective beats"))
                    {
                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(t.mutes.size()), ImGui::GetTextLineHeightWithSpacing());
                        while (clipper.Step())
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                            {
                                const auto& m = t.mutes[i];
                                if (m.source == std::numeric_limits<std::size_t>::max())
                                    ImGui::Text("tick %d | value %04x | no authored mute", m.time, m.value);
                                else
                                    ImGui::Text("tick %d | value %04x | source %zu", m.time, m.value, m.source);
                            }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Commands in file order"))
                {
                    for (const auto& c : section.commands)
                    {
                        std::int16_t measure;
                        const bool valid = ima_section_command_measure(section, c.time, measure);
                        ImGui::Text("tick %d | header measure %d (%s) | raw %02x %02x %02x %02x", c.time, measure,
                                    valid ? "available" : "unavailable", c.value[0], c.value[1], c.value[2],
                                    c.value[3]);
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Mutes in file order"))
                {
                    for (const auto& m : section.mutes)
                        ImGui::Text("tick %d | raw value %04x / word6 %04x", m.time, m.value, m.word6);
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Chords in file order"))
                {
                    for (const auto& c : section.chords)
                        ImGui::Text(
                            "%s | measure %d / beat %d / root %d | flags %02x | chord %08x / scale %08x / word44 %08x",
                            c.name.c_str(), c.measure, c.beat, c.root, c.flags, c.chord_pattern, c.scale_pattern,
                            c.word44);
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("IMA bands (data and setup only)"))
        {
            ImGui::TextWrapped("Style and section bands stay separate, in file order. Collection names are labels; "
                               "no bank is loaded from them. Setup messages are planned, not played. "
                               "Effective band selection and original MUSIC playback remain unavailable.");
            for (const auto& asset : music.assets)
            {
                if (!asset.bands || !ImGui::TreeNode(asset.filename.c_str()))
                    continue;
                ImGui::Text("%zu direct bands", asset.bands->bands.size());
                for (const auto& band : asset.bands->bands)
                {
                    ImGui::PushID(static_cast<int>(band.offset));
                    if (ImGui::TreeNode("band", "%s (offset %u)", band.name.c_str(), band.offset))
                    {
                        ImGui::Text("Collection: %s | raw byte104 %d / byte105 %u", band.collection.c_str(),
                                    band.byte104, band.byte105);
                        std::array<ImaMidiSetup, 80> setup;
                        std::string error;
                        const bool valid = ima_band_setup(band, setup, error);
                        if (!valid)
                            ImGui::TextWrapped("Setup unavailable: %s", error.c_str());
                        if (ImGui::BeginTable("channels", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            for (const auto* label : {"Source -> MIDI", "Packed patch / raw bank", "Program / legacy",
                                                      "Volume / pan / transpose", "Raw byte154"})
                                ImGui::TableSetupColumn(label);
                            ImGui::TableHeadersRow();
                            for (unsigned i = 0; i < 16; ++i)
                            {
                                const auto& c = band.channels[i];
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%u -> %u%s", i, (i + 4) & 15, i == 5 ? " percussion" : "");
                                ImGui::TableNextColumn();
                                ImGui::Text("%08x / %04x", ima_band_patch(c, !band.collection.empty(), i == 5), c.bank);
                                ImGui::TableNextColumn();
                                ImGui::Text("%u / %u", c.program, c.legacy_program);
                                ImGui::TableNextColumn();
                                ImGui::Text("%u / %u / %+d", c.volume, c.pan, c.transpose);
                                ImGui::TableNextColumn();
                                ImGui::Text("%u", c.byte154);
                            }
                            ImGui::EndTable();
                        }
                        if (valid && ImGui::TreeNode("Ordered setup messages (not sent)"))
                        {
                            for (const auto& event : setup)
                                ImGui::Text("%02x %02x %02x", event.status, event.data1, event.data2);
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("IMA pattern notes (data only)"))
        {
            ImGui::TextWrapped(
                "File order and raw timing/variation/pitch fields; these are not final MIDI notes. "
                "Curves and personality rules remain undecoded. Channel defaults are prepared, not played.");
            for (const auto& asset : music.assets)
            {
                if (!asset.style || !ImGui::TreeNode(asset.filename.c_str()))
                    continue;
                const auto& style = *asset.style;
                ImGui::Text("%s: %zu patterns / %zu grids / %zu notes / %zu opaque curves", style.name.c_str(),
                            style.patterns.size(), style.grids, style.notes, style.curve_chunks);
                ImGui::Text("Meter %d/%d | grid %d ticks | authored beat/measure %d/%d ticks | tempo %d",
                            style.beats_per_measure, style.beat_unit, style.ticks_per_grid,
                            style.authored_ticks_per_beat, style.authored_ticks_per_measure, style.tempo);
                ImGui::Text("Raw word44 %04x | identifier retained; header tail uninterpreted", style.word44);
                for (const auto& pattern : style.patterns)
                {
                    ImGui::PushID(static_cast<int>(pattern.offset));
                    if (ImGui::TreeNode("pattern", "%s (offset %u)", pattern.name.c_str(), pattern.offset))
                    {
                        ImGui::Text("Length %d | raw word0 %08x / flags %08x | timing %d, %d, %d, %d", pattern.length,
                                    pattern.word0, pattern.flags, pattern.timing[0], pattern.timing[1],
                                    pattern.timing[2], pattern.timing[3]);
                        if (ImGui::TreeNode("Channel defaults and variation flags"))
                        {
                            const auto channels = prepare_ima_pattern_channels(pattern);
                            ImGui::Text(
                                "Chord/scale masks %08x / %08x | root %+d | pitch channels %04x | raw word606 %04x",
                                pattern.chord_mask, pattern.scale_mask, pattern.root, pattern.pitch_range_channels,
                                pattern.word606);
                            ImGui::TextWrapped("Authored -> effective loader values. Each row is a source channel; "
                                               "flags belong to variants 0..15. No live selection or RNG here.");
                            for (std::size_t ch = 0; ch < 16; ++ch)
                            {
                                ImGui::PushID(static_cast<int>(ch));
                                if (ImGui::TreeNode("channel", "Ch %zu: pitch %+d..%+d | mode %02x -> %02x", ch,
                                                    pattern.lower_pitch[ch], pattern.upper_pitch[ch],
                                                    pattern.play_modes[ch], channels.play_modes[ch]))
                                {
                                    for (std::size_t v = 0; v < 16; ++v)
                                        ImGui::Text("Variant %zu: %04x -> %04x", v, pattern.variation_flags[ch][v],
                                                    channels.variation_flags[ch][v]);
                                    ImGui::TreePop();
                                }
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        for (const auto& grid : pattern.grids)
                        {
                            ImGui::PushID(static_cast<int>(grid.offset));
                            if (ImGui::TreeNode("grid", "Grid %d: %zu notes / %zu opaque curves", grid.index,
                                                grid.notes.size(), grid.curve_chunks))
                            {
                                draw_note_event_probe(grid, pattern);
                                ImGuiListClipper clipper;
                                clipper.Begin(static_cast<int>(grid.notes.size()),
                                              ImGui::GetTextLineHeightWithSpacing());
                                while (clipper.Step())
                                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                                    {
                                        const auto& n = grid.notes[i];
                                        ImGui::Text("%d: type %u ch %u offset %+d dur %d var %04x MIDI %u vel %u "
                                                    "music %04x pitch %02x byte7 %02x ranges %u/%u/%u mode %02x",
                                                    i, n.type, n.channel, n.time_offset, n.duration, n.variations,
                                                    n.midi_note, n.velocity, n.music_value, n.pitch_modifier, n.byte7,
                                                    n.time_range, n.duration_range, n.velocity_range, n.play_mode);
                                    }
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        const auto& audio = game.server.audio;
        ImGui::TextWrapped("Original Sound starts an unattached loop and removes its source object. "
                           "Playback belongs to this world until unload. No activation commands are added.");
        ImGui::Text("Output: %s | cached %.2f MiB | submitted %llu frames | dropped %llu",
                    audio.has_output() ? "SDL stream" : "offline clock", audio.cached_bytes() / 1048576.0,
                    (unsigned long long)audio.submitted_frames, (unsigned long long)audio.dropped_frames);
        if (!audio.output_error.empty())
            ImGui::TextWrapped("Output error: %s", audio.output_error.c_str());
        ImGui::SeparatorText("Source entries (including failed / removed objects)");
        for (std::size_t i = 0; i < game.entries.size(); ++i)
        {
            if (game.world.objects[i].class_name != "Sound")
                continue;
            const auto& entry = game.entries[i];
            ImGui::Text("Source %zu: %s | voice %llu", i, entry_state_name(entry.state),
                        (unsigned long long)entry.sound);
            if (entry.state == EntryState::Failed)
                ImGui::TextWrapped("%s", entry.detail.c_str());
        }
        ImGui::SeparatorText("World-owned voices (independent of source visibility)");
        for (const auto& [id, voice] : audio.voices())
        {
            if (!voice.world_owned)
                continue;
            if (!audio.get(world_sound_selection))
                world_sound_selection = id;
            const auto label = "Voice " + std::to_string(id) + " | " + voice.request.resource;
            if (ImGui::Selectable(label.c_str(), world_sound_selection == id))
                world_sound_selection = id;
        }
        if (const auto* selected = audio.get(world_sound_selection); selected && selected->world_owned)
        {
            const auto& voice = *selected;
            const auto id = voice.id;
            const auto& r = voice.request;
            ImGui::Text("Voice %llu | %s | %s | wraps %llu", (unsigned long long)id,
                        r.space == SoundSpace::Ambient ? "ambient" : "spatial",
                        !voice.error.empty() ? "failed"
                        : voice.done         ? "finished"
                                             : "looping",
                        (unsigned long long)voice.loops);
            ImGui::TextWrapped("%s | %.3f / %.3f s | %u Hz / %u channels", r.resource.c_str(),
                               voice.cursor / voice.clip->rate, voice.clip->duration(), voice.clip->rate,
                               voice.clip->channels);
            if (const auto& stream = voice.clip->stream)
                ImGui::Text("Disk PCM | buffers %zu KiB | read %llu bytes / %llu refills",
                            PcmStream::buffer_bytes / 1024, (unsigned long long)stream->bytes_read,
                            (unsigned long long)stream->refills);
            else
                ImGui::TextUnformatted("Decoded whole clip");
            if (!voice.error.empty())
                ImGui::TextWrapped("Playback failure: %s", voice.error.c_str());
            ImGui::Text("Position %.1f %.1f %.1f | radii %.1f / %.1f | volume %.0f%%", voice.position.x,
                        voice.position.y, voice.position.z, r.inner_radius, r.outer_radius, r.volume * 100);
            ImGui::Text("Gains %.3f / %.3f | priority request %u (arbitration unavailable)", voice.left, voice.right,
                        unsigned(r.priority));
        }
        ImGui::TextWrapped(
            "PCM FileStream reads bounded REZ windows on the local simulation clock. Compressed streams and region "
            "loops reject. "
            "VoiceType=0 is retained historical metadata unused by the available original code; other values reject. "
            "Dry mixing, without reverb/Doppler/occlusion. M2 remains incomplete.");
    }
} // namespace OpenBlood2
