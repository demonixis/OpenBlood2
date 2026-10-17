#include "cinematic_smoke.h"
#include "game/original/CinematicTrigger.h"
#include "game/original/ConversationTrigger.h"
#include <algorithm>
namespace OpenBlood2
{
    bool prepare_cinematic_smoke(GameSession& g, ObjectId& selected)
    {
        for (auto id : g.objects.snapshot())
        {
            const auto* r = g.objects.get(id);
            if (std::string_view(r->state.m_Name) != "cintrig1" ||
                !dynamic_cast<const CinematicTrigger*>(r->game.get()))
                continue;
            selected = id;
            auto text = g.server.CreateString("TRIGGER");
            auto msg = g.server.StartMessageToObject(nullptr, r->handle, 1000);
            bool ok = msg && g.server.WriteToMessageHString(msg, text) == DE_OK && g.server.EndMessage(msg) == DE_OK;
            g.server.FreeString(text);
            return ok;
        }
        // CityHub's authored ConversationTrigger has no mapped text. Keep it intact;
        // exercise playback with an explicitly named diagnostic and a real installed WAV.
        bool authored = false;
        for (auto id : g.objects.snapshot())
            authored |= dynamic_cast<const ConversationTrigger*>(g.objects.get(id)->game.get()) != nullptr;
        if (!authored)
            return false;
        std::string file, error;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (path.ends_with("/1acaleb1.wav"))
            {
                file = path;
                break;
            }
        }
        if (file.empty())
            return false;
        std::replace(file.begin(), file.end(), '/', '\\');
        if (!g.client_shell.m_bInWorld)
            return false;
        auto h = g.objects.create(g.objects.find_class("ConversationTrigger"), {}, Creation::World,
                                  {{"Name", std::string("Diagnostic conversation (retail WAV)")}, {"WaveFile", file}});
        if (!h)
            return false;
        selected = g.objects.get(h)->id;
        auto text = g.server.CreateString("TRIGGER");
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(g.client_shell.player), h, 1000);
        bool ok = msg && g.server.WriteToMessageHString(msg, text) == DE_OK && g.server.EndMessage(msg) == DE_OK;
        g.server.FreeString(text);
        if (!ok || !g.pump_player_messages(error) || !g.objects.advance(.25))
            return false;
        g.client_shell.communication.Update(g.server, g.client_shell.player);
        return g.client_shell.communication.voice != 0;
    }
    bool check_cinematic_smoke(GameSession& g, ObjectId selected)
    {
        const auto* r = g.objects.get(selected);
        if (const auto* conversation = r ? dynamic_cast<const ConversationTrigger*>(r->game.get()) : nullptr)
        {
            const auto& comm = g.client_shell.communication;
            const auto* voice = g.server.audio.get(comm.voice);
            return conversation->queued == 1 && conversation->current == 1 && !conversation->unmapped &&
                   comm.received == 1 && comm.started == 1 && !comm.busy && !comm.sound_failures &&
                   comm.message.text == "Bring it on!" && voice && voice->cursor > 0 &&
                   voice->request.space == SoundSpace::Local && !voice->request.streaming &&
                   g.server.audio.has_output() && g.server.audio.submitted_frames > 0 &&
                   g.server.audio.output_error.empty();
        }
        const auto* c = r ? dynamic_cast<const CinematicTrigger*>(r->game.get()) : nullptr;
        const auto* v = c ? g.server.audio.get(c->sound) : nullptr;
        return c && c->starts == 1 && !c->failures && c->current == 0 && v && !v->done && v->cursor > 0 &&
               v->request.space == SoundSpace::Local && v->clip->duration() > .2 && g.server.audio.has_output() &&
               g.server.audio.submitted_frames > 0 && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
