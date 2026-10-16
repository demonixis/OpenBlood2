#include "sprite_smoke.h"
#include "game/original/DetailSprite.h"
namespace OpenBlood2
{
    bool check_sprite_smoke(GameSession& g)
    {
        unsigned count = 0;
        for (const auto& e : g.entries)
        {
            const auto* r = g.objects.get(e.object);
            if (!r || !dynamic_cast<const DetailSprite*>(r->game.get()))
                continue;
            if (e.state != EntryState::Created || !r->sprite || r->sprite->frames.size() != 1 || r->sprite->fps != 15 ||
                r->sprite_track.milliseconds <= 0 || r->sprite_track.frame != 0 ||
                !g.resources.find(r->sprite->frames[0]) || r->state.m_Pos.y != 129 || r->state.m_Scale.x != .5f)
                return false;
            ++count;
        }
        return count == 12;
    }
} // namespace OpenBlood2
