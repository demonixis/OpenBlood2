#include "game/original/CinematicTrigger.h"
#include "inspector.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_cinematics(GameSession& game, ObjectSelection& selection)
    {
        auto& audio = game.server.audio;
        ImGui::TextWrapped("Original CinematicTrigger sequences: cue message precedes sound; decoded sample completion "
                           "advances the next delay. TRIGGER rearms cue 1's delay without resetting playback.");
        ImGui::Text("Output: %s | voices %zu | cached clips %zu (%.1f MiB)",
                    audio.has_output() ? "SDL stream" : "offline clock", audio.voices().size(), audio.cached_clips(),
                    audio.cached_bytes() / 1048576.0);
        ImGui::Text("PCM frames mixed / submitted / dropped: %llu / %llu / %llu",
                    (unsigned long long)audio.mixed_frames, (unsigned long long)audio.submitted_frames,
                    (unsigned long long)audio.dropped_frames);
        ImGui::TextWrapped("Dry stereo output only: original reverb request is retained but not rendered. Spatial "
                           "gains are replacement linear attenuation/panning, without occlusion or Doppler.");
        if (!audio.output_error.empty())
            ImGui::TextWrapped("Output failure: %s", audio.output_error.c_str());
        ImGui::Text("Last PCM peak %.4f | listener %.1f %.1f %.1f", audio.peak, game.client_shell.listener_position.x,
                    game.client_shell.listener_position.y, game.client_shell.listener_position.z);
        if (!audio.mixed.empty())
            ImGui::PlotLines("Left PCM", audio.mixed.data(), int(audio.mixed.size() / 2), 0, nullptr, -1, 1,
                             ImVec2(0, 40), 2 * sizeof(float));
        ImGui::Separator();
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            const auto* c = dynamic_cast<const CinematicTrigger*>(r->game.get());
            if (!c)
                continue;
            if (!game.objects.get(cinematic_selection))
                cinematic_selection = id;
            const auto label = std::string(r->state.m_Name) + " [cue " + std::to_string(c->current + 1) + "]##" +
                               std::to_string(id.slot);
            if (ImGui::Selectable(label.c_str(), cinematic_selection == id))
                cinematic_selection = selection.id = id;
        }
        const auto* r = game.objects.get(cinematic_selection);
        const auto* c = r ? dynamic_cast<const CinematicTrigger*>(r->game.get()) : nullptr;
        if (!c)
            return;
        ImGui::Text("%s | %s | scheduled %s", r->state.m_Name, r->life == Life::Alive ? "alive" : "pending removal",
                    r->scheduled ? "yes" : "no");
        ImGui::Text("Commands %llu | starts %llu | messages %llu | failed starts %llu", (unsigned long long)c->commands,
                    (unsigned long long)c->starts, (unsigned long long)c->messages, (unsigned long long)c->failures);
        if (r->life == Life::Alive && ImGui::Button("Send original TRIGGER"))
            cinematic_request = r->id;
        if (!c->error.empty())
            ImGui::TextWrapped("Cue diagnostic: %s", c->error.c_str());
        if (const auto* v = audio.get(c->sound))
        {
            ImGui::Separator();
            ImGui::Text("Voice %llu | %s | %s", (unsigned long long)v->id,
                        v->request.space == SoundSpace::Local ? "local" : "attached 3D", v->done ? "done" : "playing");
            ImGui::Text("WAV encoding %u | %u Hz / %u channels | %.3f / %.3f s", v->clip->encoding, v->clip->rate,
                        v->clip->channels, v->cursor / v->clip->rate, v->clip->duration());
            ImGui::Text("Gains L/R %.3f / %.3f | radii %.0f / %.0f | speaker lost %s", v->left, v->right,
                        v->request.inner_radius, v->request.outer_radius, v->speaker_lost ? "yes" : "no");
            const auto* speaker = game.objects.get(v->request.speaker);
            ImGui::Text("Speaker %s | position %.1f %.1f %.1f", speaker ? speaker->state.m_Name : "none", v->position.x,
                        v->position.y, v->position.z);
        }
        for (std::size_t i = 0; i < c->cues.size(); ++i)
        {
            const auto& cue = c->cues[i];
            if (ImGui::TreeNode(
                    (std::to_string(i + 1) + (i == c->current ? " (current) " : " ") +
                     (cue.sound.empty() ? "[empty: ends sequence]"
                                        : cue.sound + (game.resources.find(cue.sound) ? " [present]" : " [MISSING]")))
                        .c_str()))
            {
                ImGui::Text("Delay %.4f s | speaker %s", cue.delay, cue.speaker.empty() ? "self" : cue.speaker.c_str());
                ImGui::TextWrapped("Message %s -> %s", cue.target.c_str(), cue.message.c_str());
                ImGui::TreePop();
            }
        }
        ImGui::TextWrapped("Inactive and hidden controllers are included. Cue presence does not imply successful "
                           "decoding; World lists missing classes. This "
                           "subset does not complete M2 or implement general game audio/music.");
    }
} // namespace OpenBlood2
