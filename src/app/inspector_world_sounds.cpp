#include "engine/audio_stream.h"
#include "inspector.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_world_sounds(GameSession& game)
    {
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
