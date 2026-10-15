#include "game/original/ClientLightFX.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::RandomFloat(float low, float high, float& value)
    {
        trace_service("RandomFloat");
        if (!std::isfinite(low) || !std::isfinite(high) || low > high || std::abs(low) > 1e6f || std::abs(high) > 1e6f)
            return false;
        value = low + (high - low) * static_cast<float>(random_() - random_.min()) /
                          static_cast<float>(random_.max() - random_.min());
        return true;
    }
    bool ServerAdapter::PublishLightEffect(HOBJECT h, const LightSettings& settings)
    {
        trace_service("PublishLightEffect");
        if (!IsObjectAlive(h) || !dynamic_cast<ClientLightFX*>(objects_.get(h)->game.get()) ||
            light_effects.size() >= 4096 ||
            std::any_of(light_effects.begin(), light_effects.end(), [h](const auto& l) { return l.source == h; }))
            return objects_.fail(ObjectError::Unsupported, "Invalid/duplicate light source or light limit exceeded");
        CLightFX effect;
        if (!effect.Init(h, settings, GetTime(), objects_.get(h)->state.m_Pos))
            return objects_.fail(ObjectError::InvalidProperty, "Invalid light descriptor/pose");
        light_effects.push_back(std::move(effect));
        return true;
    }
    void ServerAdapter::RemoveLightEffect(HOBJECT h)
    {
        trace_service("RemoveLightEffect");
        std::erase_if(light_effects, [h](const auto& l) { return l.source == h; });
    }
    const CLightFX* ServerAdapter::GetLightEffect(HOBJECT h) const
    {
        const auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive)
            return nullptr;
        for (const auto& l : light_effects)
            if (l.source == h)
                return &l;
        return nullptr;
    }
    bool ServerAdapter::advance_lights()
    {
        std::erase_if(light_effects, [this](const auto& l) { return !IsObjectAlive(l.source); });
        for (auto& l : light_effects)
        {
            const auto* r = objects_.get(l.source);
            if (!l.Update(GetTime(), r->state.m_Pos, (r->user_flags & 1) != 0))
                return objects_.fail(ObjectError::InvalidProperty, "Light update produced invalid state");
        }
        return true;
    }
} // namespace OpenBlood2
