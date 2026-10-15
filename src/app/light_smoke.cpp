#include "light_smoke.h"
#include "game/original/ClientLightFX.h"
#include "game/original/KeyFramer.h"
#include <cmath>
namespace OpenBlood2
{
    bool prepare_light_smoke(GameSession& g, ObjectId& selected)
    {
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && std::string_view(r->state.m_Name) == "sublight" &&
                                                   dynamic_cast<const ClientLightFX*>(r->game.get()))
            {
                selected = id;
                return true;
            }
        return false;
    }
    bool check_light_smoke(GameSession& g, ObjectId selected)
    {
        const auto* r = g.objects.get(selected);
        if (!r)
            return false;
        const auto* fx = g.server.GetLightEffect(r->handle);
        if (!fx || !fx->visible || fx->updates < 2 || g.server.light_effects.size() != 12)
            return false;
        const KeyFramer* path = nullptr;
        for (auto id : g.objects.snapshot())
            if (const auto* f = dynamic_cast<const KeyFramer*>(g.objects.get(id)->game.get());
                f && f->object_name == "sublight")
                path = f;
        if (!path || !path->initialized || !path->active || path->keys.size() != 2 || path->targets.size() != 12)
            return false;
        for (const auto& t : path->targets)
            if (t.object == r->handle)
            {
                if (!t.moves || t.result != MoverResult::Moved)
                    return false;
                const auto& a = path->keys[0];
                const auto& b = path->keys[1];
                auto expected = vector_add(
                    t.offset, vector_add(a.position, vector_scale(vector_subtract(b.position, a.position),
                                                                  (path->time - a.time) / (b.time - a.time))));
                auto d = vector_subtract(expected, fx->position),
                     actual = vector_subtract(r->state.m_Pos, fx->position);
                // Authored SubwayA minima/maxima are equal; do not claim visible pulsing here.
                return vector_dot(d, d) < .0001f && vector_dot(actual, actual) < .0001f &&
                       std::abs(fx->radius - 200) < .001f && std::abs(fx->color.x - .858121f) < .001f;
            }
        return false;
    }
} // namespace OpenBlood2
