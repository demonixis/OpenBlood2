// Original WeaponFX.cpp PlayImpactSound (977-992), GetImpactSound (1104-1152).
#include "WeaponFX.h"
#include "engine/collision.h"
#include "game/original/Weapon.h"
#include "game/weapon_sprites.h"
#include "server_de.h"
#include <cmath>
std::string CWeaponFX::GetImpactSound(std::minstd_rand& random) const
{
    const char* material = nullptr;
    switch (m_eSurfaceType)
    {
        case SURFTYPE_STONE:
        case SURFTYPE_BUILDING:
            material = "stone";
            break;
        case SURFTYPE_METAL:
            material = "metal";
            break;
        case SURFTYPE_WOOD:
            material = "wood";
            break;
        case SURFTYPE_ENERGY:
            material = "energy";
            break;
        case SURFTYPE_GLASS:
            material = "glass";
            break;
        case SURFTYPE_TERRAIN:
            material = "terrain";
            break;
        case SURFTYPE_CLOTH:
            material = "cloth";
            break;
        case SURFTYPE_PLASTIC:
            material = "plastic";
            break;
        case SURFTYPE_FLESH:
            material = "flesh";
            break;
        case SURFTYPE_LIQUID:
            material = "water";
            break;
        default:
            return {}; // No random draw for unknown/sky/fire-through.
    }
    // SharedDefs.h defines B2_LITE: NRES(4) is 3. Own the string instead of writing a literal.
    return std::string("Sounds/Weapons/impacts/") + material + "/impact" +
           std::to_string(std::uniform_int_distribution<int>(0, 2)(random) + 1) + ".wav";
}
bool CWeaponFX::PlayImpactSound(ServerDE& server, HOBJECT source, std::minstd_rand& random)
{
    resource = GetImpactSound(random);
    error.clear();
    sound = 0;
    if (resource.empty())
        return true; // Original intentional silence, not a successful voice.
    OpenBlood2::SoundRequest request;
    request.resource = resource;
    request.outer_radius = 500;
    request.inner_radius = 50;
    request.priority = 0; // SOUNDPRIORITY_MISC_LOW.
    sound = server.PlayDetachedSound(source, request, m_vDestPos, error);
    return sound != 0;
}

// Original Weapon::UpdateWeaponFX selection; WeaponFX::CreateObject/CreateFlash/CreateSmoke recipes.
bool CWeaponFX::CreateSprites(ServerDE& server, HOBJECT source, DVector normal, std::minstd_rand& random)
{
    sprite_event = 0;
    sprite_error.clear();
    if ((m_nAmmoType != AMMO_BULLET && m_nAmmoType != AMMO_SHELL && m_nAmmoType != AMMO_BMG) ||
        (m_eSurfaceType != SURFTYPE_WOOD && m_eSurfaceType != SURFTYPE_STONE))
        return true; // No authored flash/smoke branch; not successful visual creation.
    ContainerCode code = CC_NOTHING;
    if (!OpenBlood2::physics_vector(normal) || OpenBlood2::vector_dot(normal, normal) < 1e-12f ||
        !server.GetPointContainerCode(m_vDestPos, code, true))
    {
        sprite_error = "Impact normal/container query unavailable";
        return false;
    }
    normal = OpenBlood2::vector_scale(normal, 1 / std::sqrt(OpenBlood2::vector_dot(normal, normal)));
    OpenBlood2::WeaponSpriteSettings settings;
    if (IsLiquid(code))
        settings.bubbles_unavailable = true; // Flash suppressed; original smoke substitutes SparkFX bubbles.
    else
    {
        auto& flash = settings.flash.emplace();
        flash.position = OpenBlood2::vector_add(m_vDestPos, normal);
        flash.normal = normal;
        flash.scale1 = flash.scale2 = {.1f, .1f, 0};
        flash.duration = .15f;
        flash.resource = "Sprites/impact0" + std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".spr";
        auto& smoke = settings.smoke.emplace();
        smoke.position = m_vDestPos;
        smoke.normal = normal;
        smoke.offset = 3;
        smoke.gravity.y = std::uniform_real_distribution<float>(.05f, .1f)(random);
        smoke.duration = 1;
        smoke.scale1 = {.1f, .1f, 0};
        smoke.scale2 = {.35f, .35f, 0};
        smoke.alpha = .65f;
        smoke.fade = true;
        smoke.move = normal.y > -.3f;
        if (m_eSurfaceType == SURFTYPE_WOOD)
            smoke.color = {1, .8f, .6f};
        smoke.resource =
            "Sprites/smokepuff" + std::to_string(std::uniform_int_distribution<int>(0, 2)(random) + 1) + ".spr";
    }
    sprite_event = server.PublishWeaponSprites(source, settings, sprite_error);
    return sprite_event && sprite_error.empty();
}
