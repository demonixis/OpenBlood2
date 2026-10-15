#include "keyframe_smoke.h"
#include "game/original/KeyFramer.h"
namespace OpenBlood2
{
    bool prepare_keyframe_smoke(GameSession& game, ObjectId& selected)
    {
        for (auto id : game.objects.snapshot())
            if (const auto* r = game.objects.get(id);
                r && std::string_view(r->state.m_Name) == "tracking1" && dynamic_cast<const KeyFramer*>(r->game.get()))
            {
                selected = id;
                return true;
            }
        return false;
    }
    bool check_keyframe_smoke(GameSession& game, ObjectId selected)
    {
        const auto* r = game.objects.get(selected);
        const auto* f = r ? dynamic_cast<const KeyFramer*>(r->game.get()) : nullptr;
        if (!f || !f->initialized || f->keys.size() != 2 || f->targets.size() != 1 || !f->active ||
            !f->targets[0].moves || f->targets[0].result != MoverResult::Moved || f->unresolved_messages != 1)
            return false;
        const auto* target = game.objects.get(f->targets[0].object);
        if (!target || game.client_shell.view.object != target->handle)
            return false;
        const auto& a = f->keys[0];
        const auto& b = f->keys[1];
        const auto expected = vector_add(f->targets[0].offset,
                                         vector_add(a.position, vector_scale(vector_subtract(b.position, a.position),
                                                                             (f->time - a.time) / (b.time - a.time))));
        auto delta = vector_subtract(expected, game.client_shell.view.position);
        return vector_dot(delta, delta) < .0001f;
    }
} // namespace OpenBlood2
