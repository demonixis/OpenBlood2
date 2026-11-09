#include "game/original/ClientLightFX.h"
#include "game/original/NativeLight.h"
#include "game/original/Projectile.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool ServerAdapter::RandomFloat(float low, float high, float& value)
    {
        if (!trace_service("RandomFloat"))
            return false;
        if (!std::isfinite(low) || !std::isfinite(high) || low > high || std::abs(low) > 1e6f || std::abs(high) > 1e6f)
            return false;
        value = low + (high - low) * static_cast<float>(random_() - random_.min()) /
                          static_cast<float>(random_.max() - random_.min());
        return true;
    }
    bool ServerAdapter::PublishLightEffect(HOBJECT h, const LightSettings& settings)
    {
        if (!trace_service("PublishLightEffect"))
            return false;
        if (!IsObjectAlive(h))
            return objects_.fail(ObjectError::Unsupported, "Invalid light source");
        const auto* record = objects_.get(h);
        const bool native = dynamic_cast<LightFX*>(record->game.get());
        if (!native && !dynamic_cast<ClientLightFX*>(record->game.get()) &&
            !dynamic_cast<CProjectile*>(record->game.get()))
            return objects_.fail(ObjectError::Unsupported, "Invalid light source class");
        const auto reference = effect_reference(h);
        auto existing = std::find_if(light_effects.begin(), light_effects.end(),
                                     [reference](const auto& l) { return l.source == reference; });
        if (existing != light_effects.end() ? !native : light_effects.size() >= 4096)
            return objects_.fail(ObjectError::Unsupported, "Invalid/duplicate light source or light limit exceeded");
        CLightFX effect;
        if (!effect.Init(reference, settings, GetTime(), record->state.m_Pos))
            return objects_.fail(ObjectError::InvalidProperty, "Invalid light descriptor/pose");
        if (existing == light_effects.end())
            light_effects.push_back(std::move(effect));
        else
            *existing = std::move(effect);
        return true;
    }
    void ServerAdapter::RemoveLightEffect(HOBJECT h)
    {
        if (!trace_service("RemoveLightEffect"))
            return;
        const auto reference = effect_reference(h);
        std::erase_if(light_effects, [reference](const auto& l) { return l.source == reference; });
    }
    const CLightFX* ServerAdapter::GetLightEffect(HOBJECT h) const
    {
        const auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive)
            return nullptr;
        for (const auto& l : light_effects)
            if (l.source == effect_reference(h))
                return &l;
        return nullptr;
    }
} // namespace OpenBlood2
