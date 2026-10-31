#include "game/local_effects.h"
#include "client/effect_snapshot.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/FlareProjectile.h"
#include "game/original/LeechProjectile.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
namespace OpenBlood2
{
    bool LocalEffects::publish(const GameSession& game, ClientEffects& client, std::string& error)
    {
        if (game.objects.busy())
        {
            error = "Effect publication during a server callback rejected";
            return false;
        }
        ClientEffectsFrame frame;
        frame.world = game.client_session.world.frame().world;
        frame.sequence = client.frame().sequence + 1;
        frame.time = game.server.world_time();
        const auto& flashlight = game.client_shell.flashlight;
        frame.flashlight = flashlight.Current(game.client_session.world);
        append_runtime_effects(game.client_effects, game.client_session.world, frame);
        auto add = [&](DVector position, DVector color, float radius, DDWORD flags = 0)
        {
            if (!(radius > 0) || radius > 1e7f || !std::isfinite(radius) || !physics_vector(position) ||
                !physics_vector(color) || color.x < 0 || color.y < 0 || color.z < 0 || color.x > 1 || color.y > 1 ||
                color.z > 1)
                return;
            const float brightness = std::max({color.x, color.y, color.z});
            if (!(brightness > 0))
                return;
            if (flags & (1u << 7))
            {
                ++frame.fog_lights_unsupported;
                return;
            }
            if (flags & (1u << 3))
                ++frame.shadow_lights_unshadowed;
            frame.lights.push_back({{position, color, radius}, flags});
        };
        // Presentation-only equivalent of the source's owned projectile OT_LIGHT child.
        // No extra server record, game RNG draw or saved-payload change.
        for (auto id : game.objects.snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            const auto* projectile = dynamic_cast<const CProjectile*>(r->game.get());
            if (!projectile || !projectile->configured || projectile->failed || projectile->exploded)
                continue;
            const auto position = r->state.m_Pos;
            if (auto* flare = dynamic_cast<const CFlareProjectile*>(projectile))
                add(position, flare->color, 200);
            else if (auto* flare = dynamic_cast<const CFlareAltProjectile*>(projectile))
                add(position, flare->color, 200);
            else if (auto* flare = dynamic_cast<const CFlareBurstProjectile*>(projectile))
                add(position, flare->color, 75);
            else if (auto* leech = dynamic_cast<const CLeechPrimeProjectile*>(projectile))
                add(position, leech->color, 100);
            else if (dynamic_cast<const CTeslaProjectile*>(projectile) ||
                     dynamic_cast<const CTeslaBoltProjectile*>(projectile) ||
                     dynamic_cast<const CTeslaBallProjectile*>(projectile))
                add(position, {.65f, .65f, 1}, 100);
            else if (dynamic_cast<const CNapalmProjectile*>(projectile) ||
                     dynamic_cast<const CNapalmFireball*>(projectile) ||
                     dynamic_cast<const CHowitzerAltFrag*>(projectile))
                add(position, {1, .75f, 0}, 100);
            else if (dynamic_cast<const CGroundStrikeProjectile*>(projectile))
                add(position, {1, .75f, 0}, 150);
        }
        auto variable = [&](const char* key)
        {
            const auto found = game.server.variables().find(key);
            if (found == game.server.variables().end())
                return 0.f;
            const auto& text = found->second.text;
            float value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size() && std::isfinite(value) ? value : 0.f;
        };
        frame.fog = {std::trunc(variable("FogEnable")) != 0,
                     variable("FogNearZ"),
                     variable("FogFarZ"),
                     {variable("FogR"), variable("FogG"), variable("FogB")}};
        if (const float far_z = std::trunc(variable("FarZ")); far_z > 0)
            frame.far_z = far_z;
        if (std::trunc(variable("SkyFog")) != 0)
        {
            const float near = std::trunc(variable("SkyFogNearZ")), far = std::trunc(variable("SkyFogFarZ"));
            if (far > near)
            {
                frame.sky_fog = true;
                frame.sky_fog_near = near;
                frame.sky_fog_far = far;
            }
        }
        std::set<std::uint32_t> live;
        for (auto id : game.objects.container_snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive || !r->container())
                continue;
            const auto& shape = r->container();
            auto& cached = shapes_[id.slot];
            if (cached.id != id || cached.source.lock() != shape)
            {
                cached = {id, shape, std::make_shared<const ContainerShape>(*shape)};
                ++shape_copies;
            }
            live.insert(id.slot);
            EffectFogVolume volume;
            volume.owner = id;
            volume.shape = cached.copy;
            if (const auto* brush = dynamic_cast<const VolumeBrush*>(r->game.get()))
            {
                volume.visible = (r->user_flags & FLAG_VISIBLE) != 0;
                volume.fog = {brush->fog, brush->fog_near, brush->fog_far, brush->fog_color};
            }
            frame.volumes.push_back(std::move(volume));
        }
        std::erase_if(shapes_, [&](const auto& pair) { return !live.contains(pair.first); });
        return client.receive(std::move(frame), error, true);
    }
} // namespace OpenBlood2
