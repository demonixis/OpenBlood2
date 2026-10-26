#include "game/presentation_effects.h"
#include "engine/container.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/FlareProjectile.h"
#include "game/original/LeechProjectile.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <charconv>
#include <cmath>
namespace OpenBlood2
{
    SceneEffects gather_scene_effects(const GameSession& game, DVector eye, DRotation view)
    {
        SceneEffects out;
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
                ++out.fog_lights_unsupported;
                return;
            }
            if (flags & (1u << 3))
                ++out.shadow_lights_unshadowed;
            out.lights.push_back({{position, color, radius}, flags});
        };
        for (const auto& light : game.server.light_effects)
            if (light.visible)
                add(light.position, light.color, light.radius, light.settings.flags);
        auto explosion = [&](const CExplosionLightFX& light)
        {
            if ((light.flags & FLAG_VISIBLE) && !light.delay)
                add(light.settings.position, light.color, light.radius);
        };
        for (const auto& light : game.server.explosion_lights.active)
            explosion(light);
        for (const auto& bolt : game.server.lightning.active)
            for (const auto& light : bolt.lights)
                explosion(light);
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
        auto fog = [&](float near, float far, DVector color)
        {
            near = std::trunc(near);
            far = std::trunc(far);
            out.uniform.fog_color = {};
            if (!std::isfinite(near) || !std::isfinite(far) || far <= near || !physics_vector(color))
                out.invalid_fog = true;
            else
            {
                auto channel = [](float c) { return std::clamp(std::trunc(c), 0.f, 255.f) / 255.f; };
                out.uniform.fog_color = {channel(color.x), channel(color.y), channel(color.z), 1};
                out.uniform.fog_eye_near = {eye.x, eye.y, eye.z, near};
                const auto f = rotate_vector(view, {0, 0, 1});
                out.uniform.fog_forward_far = {f.x, f.y, f.z, far};
            }
        };
        // ResetGlobalFog on exit: read the same server values without changing console state.
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
        if (std::trunc(variable("FogEnable")) != 0)
            fog(variable("FogNearZ"), variable("FogFarZ"), {variable("FogR"), variable("FogG"), variable("FogB")});
        // Same first-containing-volume / visible-user-flag rule as the client camera.
        for (auto id : game.objects.container_snapshot())
        {
            const auto* r = game.objects.get(id);
            if (!r || r->life != Life::Alive || !r->container() || !r->container()->contains(eye))
                continue;
            const auto* brush = dynamic_cast<const VolumeBrush*>(r->game.get());
            // Departure: the original ran "FogEnable 0" for a visible fog-off volume, so
            // CityHub's escalator Conveyors switched the world fog off abruptly underfoot.
            // A fog-off volume now keeps the current fog; fog-on volumes still replace it.
            if (brush && (r->user_flags & FLAG_VISIBLE) && brush->fog)
            {
                out.uniform.fog_color = {};
                out.invalid_fog = false;
                fog(brush->fog_near, brush->fog_far, brush->fog_color);
            }
            break;
        }
        return out;
    }
} // namespace OpenBlood2
