#include "world_sound_smoke.h"
namespace OpenBlood2
{
    bool check_world_sound_smoke(GameSession& g)
    {
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
        return count == 4 && g.server.audio.has_output() && g.server.audio.submitted_frames > 0 &&
               g.server.audio.output_error.empty();
    }
} // namespace OpenBlood2
