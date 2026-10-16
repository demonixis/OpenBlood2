#include "random_sound_smoke.h"
#include "game/original/RandomSoundFX.h"
namespace OpenBlood2
{
    bool prepare_random_sound_smoke(GameSession& g, ObjectId& selected)
    {
        for (auto id : g.objects.snapshot())
        {
            const auto* r = g.objects.get(id);
            if (!dynamic_cast<const RandomSoundFX*>(r->game.get()))
                continue;
            selected = id;
            auto text = g.server.CreateString("ON");
            auto msg = g.server.StartMessageToObject(nullptr, r->handle, 1000);
            bool ok = msg && g.server.WriteToMessageHString(msg, text) == DE_OK && g.server.EndMessage(msg) == DE_OK;
            g.server.FreeString(text);
            return ok;
        }
        return false;
    }
    bool check_random_sound_smoke(GameSession& g, ObjectId selected)
    {
        const auto* r = g.objects.get(selected);
        const auto* c = r ? dynamic_cast<const RandomSoundFX*>(r->game.get()) : nullptr;
        const auto* v = c ? g.server.audio.get(c->sound) : nullptr;
        return c && c->attempts == 1 && !c->failures && v && v->cursor > 0 && !v->request.speaker &&
               v->request.space != SoundSpace::Local && v->clip->duration() > 0 && g.server.audio.has_output() &&
               g.server.audio.submitted_frames > 0 && g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
