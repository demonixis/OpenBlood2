// Original ObjectDLL/ClientWeaponSFX and ClientShellDLL/WeaponFX flag dispatch.
#include "ClientWeaponSFX.h"
#include "CampaignProjectiles.h"
#include "WeaponFX.h"
#include "engine/collision.h"
#include "game/server_adapter.h"
#include <bit>
#include <cmath>

using namespace OpenBlood2;

namespace
{
    constexpr std::uint32_t known_flags =
        weapon_ballistics | WFX_MUZZLELIGHT | WFX_LEFTHANDED | WFX_PARTICLETRAIL | WFX_FLASH | WFX_SPARKS |
        WFX_SMOKE | WFX_SOUND | WFX_BLOODSPLAT | WFX_BLOODSPURT | WFX_EXPLOSION | WFX_IMPACTLIGHT | WFX_SCREENSHAKE;

    bool impact_sprites(ServerDE& server, HOBJECT owner, const ShotgunEffectSettings& shot, std::uint32_t flags,
                        std::minstd_rand& random, std::string& error)
    {
        ContainerCode code = CC_NOTHING;
        if (!server.GetPointContainerCode(shot.destination, code, true))
        {
            error = "Weapon effect destination container unavailable";
            return false;
        }
        WeaponSpriteSettings sprites;
        if ((flags & WFX_FLASH) && !IsLiquid(code))
        {
            auto& flash = sprites.flash.emplace();
            flash.position = vector_add(shot.destination, shot.normal);
            flash.normal = shot.normal;
            flash.scale1 = flash.scale2 = {.1f, .1f, 0};
            flash.duration = .15f;
            flash.resource = "Sprites/impact0" + std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".spr";
        }
        if (flags & WFX_SMOKE)
        {
            if (IsLiquid(code) && shot.surface != SURFTYPE_LIQUID)
            {
                if (shot.ammo != AMMO_BULLET && shot.ammo != AMMO_SHELL && shot.ammo != AMMO_BMG)
                    return true;
                const SparkSettings bubbles{shot.destination, vector_scale(shot.normal, 15), {125, 125, 255},
                                            {180, 180, 255}, shot.ammo == AMMO_SHELL ? 1u : 4u,
                                            1.25f, .05f, 600, 50, "SpriteTextures/Particles/ParticleBubble.dtx"};
                return server.PublishSparks(owner, bubbles, error) && error.empty();
            }
            auto& smoke = sprites.smoke.emplace();
            smoke.position = shot.destination;
            smoke.normal = shot.normal;
            smoke.offset = 3;
            smoke.gravity.y = std::uniform_real_distribution<float>(.05f, .1f)(random);
            smoke.duration = 1;
            smoke.scale1 = {.1f, .1f, 0};
            smoke.scale2 = {.35f, .35f, 0};
            smoke.alpha = .65f;
            smoke.fade = true;
            smoke.move = shot.normal.y > -.3f;
            if (shot.surface == SURFTYPE_WOOD)
                smoke.color = {1, .8f, .6f};
            smoke.resource =
                "Sprites/smokepuff" + std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".spr";
        }
        return (!sprites.flash && !sprites.smoke) || (server.PublishWeaponSprites(owner, sprites, error) && error.empty());
    }
} // namespace

bool CClientWeaponSFX::Setup(const LegacyWeaponEffect& settings, std::string& error)
{
    error.clear();
    auto* server = dynamic_cast<ServerAdapter*>(GetServerDE());
    auto shot = settings.shot;
    const auto flags = settings.flags;
    DRotation rotation;
    const auto valid_color = [](DVector c)
    {
        return physics_vector(c) && c.x >= 0 && c.x <= 255 && c.y >= 0 && c.y <= 255 && c.z >= 0 && c.z <= 255;
    };
    if (!server || !server->IsObjectAlive(m_hObject) || (flags & ~known_flags) || !physics_vector(shot.source) ||
        !physics_vector(shot.destination) || !physics_vector(shot.forward) || !physics_vector(shot.normal) ||
        vector_dot(shot.forward, shot.forward) < 1e-12f || !align_rotation(shot.normal, {0, 1, 0}, rotation) ||
        shot.ammo > AMMO_TIMEBOMB || !valid_color(shot.color1) || !valid_color(shot.color2) ||
        !std::isfinite(settings.damage) || settings.damage < 0 || settings.damage > 100000)
    {
        error = "Invalid legacy weapon effect";
        return false;
    }
    shot.normal = vector_scale(shot.normal, 1 / std::sqrt(vector_dot(shot.normal, shot.normal)));
    shot.left = (flags & WFX_LEFTHANDED) != 0;
    shot.eject = (flags & WFX_EJECTSHELL) != 0;
    auto report = [&](bool success, std::string_view detail, std::string_view fallback)
    {
        if (success && detail.empty())
            return;
        if (!error.empty())
            error += ", ";
        error += detail.empty() ? fallback : detail;
    };
    std::minstd_rand random(std::bit_cast<std::uint32_t>(server->GetTime()) ^
                            std::bit_cast<std::uint32_t>(shot.destination.z));
    std::string detail;
    if (flags & weapon_ballistics)
    {
        const auto event = server->PublishBallistics(m_hObject, shot, detail, flags);
        report(event != 0, detail, "Ballistic effects unavailable");
    }
    if (flags & (WFX_FLASH | WFX_SMOKE))
    {
        detail.clear();
        const bool applied = impact_sprites(*server, m_hObject, shot, flags, random, detail);
        report(applied, detail, "Impact sprites unavailable");
    }
    CWeaponFX impact;
    impact.m_vDestPos = shot.destination;
    impact.m_nAmmoType = shot.ammo;
    impact.m_eSurfaceType = shot.surface;
    if (flags & WFX_SPARKS)
    {
        const bool applied = impact.CreateSparks(*server, m_hObject, shot.normal, settings.damage, true, random);
        report(applied, impact.spark_error, "Impact sparks unavailable");
    }
    if (flags & WFX_SOUND)
    {
        const bool applied = impact.PlayImpactSound(*server, m_hObject, random);
        report(applied, impact.error, "Impact sound unavailable");
    }
    if (flags & WFX_BLOODSPLAT)
    {
        detail.clear();
        const BloodSplatSettings splat{shot.destination, rotation, std::uniform_real_distribution<float>(.05f, .15f)(random),
                                       0, "Sprites/bloodsplat" +
                                              std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".spr"};
        const auto event = server->PublishClientBloodSplat(splat, "weapon effect", detail);
        report(event != 0, detail, "Blood splat unavailable");
    }
    if (flags & WFX_BLOODSPURT)
    {
        impact.blood_requested = true; // Setup already carries the source's selection result.
        const bool applied = impact.CreateBloodSpurt(*server, m_hObject, shot.normal, settings.damage, random);
        report(applied, impact.blood_error, "Blood spurt unavailable");
    }
    if (flags & WFX_EXPLOSION)
    {
        bool applied = false;
        // Original SFXMsgIds.h ends at Naga's ground pound (31), or Flayer shatter (36) in the expansion.
        if (settings.explosion <= (IsAddonBuild() ? 36 : 31))
        {
            const auto fx = PublishCampaignExplosion(m_hObject, shot.destination, shot.normal, settings.explosion);
            applied = (fx.model_event && fx.model_error.empty()) || (fx.sprite_event && fx.sprite_error.empty()) ||
                      (fx.ring_event && fx.ring_error.empty()) || (fx.fragment_event && fx.fragment_error.empty()) ||
                      (fx.flame_event && fx.flame_error.empty()) || (fx.light_event && fx.light_error.empty());
        }
        report(applied, {}, "Explosion recipe unavailable");
    }
    if (flags & WFX_SCREENSHAKE)
        report(false, {}, "Weapon screen shake unavailable");
    return error.empty();
}
