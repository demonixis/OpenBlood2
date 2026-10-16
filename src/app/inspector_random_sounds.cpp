#include "game/original/RandomSoundFX.h"
#include "inspector.h"
#include <imgui.h>
namespace OpenBlood2
{
    void Inspector::draw_random_sounds(GameSession& game, ObjectSelection& selection)
    {
        auto& audio = game.server.audio;
        ImGui::TextWrapped(
            "Original RandomSoundFX: intervals replace the previous voice. OFF cancels the next start, "
            "but lets the current sound finish. Ambient uses distance without panning; positional audio is dry 3D.");
        ImGui::Text("Output %s | voices %zu | cached %.2f MiB | submitted %llu frames",
                    audio.has_output() ? "SDL stream" : "offline clock", audio.voices().size(),
                    audio.cached_bytes() / 1048576.0, (unsigned long long)audio.submitted_frames);
        if (!audio.output_error.empty())
            ImGui::TextWrapped("Output failure: %s", audio.output_error.c_str());
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!dynamic_cast<const RandomSoundFX*>(r->game.get()))
                continue;
            if (!game.objects.get(random_sound_selection))
                random_sound_selection = id;
            auto label = std::string(r->state.m_Name) + " [object " + std::to_string(id.slot) + "]";
            if (ImGui::Selectable(label.c_str(), random_sound_selection == id))
                random_sound_selection = selection.id = id;
        }
        const auto* r = game.objects.get(random_sound_selection);
        const auto* c = r ? dynamic_cast<const RandomSoundFX*>(r->game.get()) : nullptr;
        if (!c)
            return;
        ImGui::Separator();
        ImGui::Text("%s | %s | on %s | scheduled %s (%.3f s)", r->state.m_Name,
                    r->life == Life::Alive ? "alive" : "pending removal", c->on ? "yes" : "no",
                    r->scheduled ? "yes" : "no", r->scheduled ? r->due - game.objects.time() : 0);
        if (r->life == Life::Alive)
            for (const char* command : {"ON", "OFF", "TOGGLE"})
            {
                if (ImGui::Button(command))
                {
                    random_sound_request = r->id;
                    random_sound_command = command;
                }
                ImGui::SameLine();
            }
        ImGui::NewLine();
        ImGui::Text("%s | loop sequence %s | playable %u | sequence index %u | last slot %s",
                    c->sequential ? "sequential" : "random", c->loop_sequence ? "yes" : "no", c->count, c->current + 1,
                    c->attempts ? std::to_string(c->last_choice + 1).c_str() : "none");
        ImGui::Text("Wait %.3f .. %.3f s | X/Z range +/-%.1f | outer radius %.1f | volume %u%%", c->min_wait,
                    c->max_wait, c->position_radius, c->sound_radius, unsigned(c->volume));
        ImGui::Text("Commands %llu | attempted starts %llu | failed starts %llu", (unsigned long long)c->commands,
                    (unsigned long long)c->attempts, (unsigned long long)c->failures);
        if (!c->error.empty())
            ImGui::TextWrapped("Start failure: %s", c->error.c_str());
        if (const auto* v = audio.get(c->sound))
        {
            ImGui::Text("Voice %llu | %s | %s | %.3f / %.3f s", (unsigned long long)v->id,
                        v->request.space == SoundSpace::Ambient ? "ambient" : "spatial",
                        v->done ? "finished" : "playing", v->cursor / v->clip->rate, v->clip->duration());
            ImGui::Text("Fixed position %.1f %.1f %.1f | gains L/R %.3f / %.3f", v->position.x, v->position.y,
                        v->position.z, v->left, v->right);
            ImGui::Text("WAV encoding %u | %u Hz / %u channels | reverb requested %s (unavailable)", v->clip->encoding,
                        v->clip->rate, v->clip->channels, v->request.reverb_requested ? "yes" : "no");
        }
        for (unsigned i = 0; i < c->sounds.size(); ++i)
        {
            std::string authored;
            for (const auto& [key, value] : r->properties)
                if (same_name(key, "SoundFile" + std::to_string(i + 1)))
                    if (const auto* s = std::get_if<std::string>(&value))
                        authored = *s;
            ImGui::TextWrapped("%u: %s [%s; %s]", i + 1, authored.empty() ? "empty" : authored.c_str(),
                               i < c->count ? "playable slot" : "after first gap / ignored",
                               authored.empty()                ? "no resource"
                               : game.resources.find(authored) ? "archive present"
                                                               : "MISSING");
        }
        ImGui::TextWrapped("Hidden/inactive emitters are included. Archive presence is separate from decoding. "
                           "The original unused Ambient property remains visible in World. Other callers, "
                           "reverb, Doppler and complete M2 gameplay remain unavailable.");
    }
} // namespace OpenBlood2
