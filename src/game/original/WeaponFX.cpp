// Original WeaponFX.cpp PlayImpactSound (977-992), GetImpactSound (1104-1152).
#include "WeaponFX.h"
#include "ParticleExplosionFX.h"
#include "SparksFX.h"
#include "engine/collision.h"
#include "game/original/Weapon.h"
#include "game/weapon_sprites.h"
#include "server_de.h"
#include <cmath>
using namespace OpenBlood2;
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

// Original Weapon::UpdateWeaponFX and CWeaponFX::CreateBloodSpurt.
bool CWeaponFX::SelectBloodSpurt(float damage, std::minstd_rand& random)
{
    blood_roll = -1;
    blood_requested = false;
    blood_error.clear();
    blood_events[0] = blood_events[1] = 0;
    if (m_eSurfaceType != SURFTYPE_FLESH)
        return true;
    if (!std::isfinite(damage) || damage < 0)
    {
        blood_error = "Blood-spurt selection unavailable";
        return false;
    }
    // Preserve the original draw/condition; use the existing cosmetic stream so
    // newly connected visuals cannot perturb established AI/damage randomness.
    blood_roll = std::uniform_int_distribution<int>(0, 3)(random);
    // The random draw precedes the damage >25 test even for a high-damage shot.
    blood_requested = !blood_roll || damage > 25;
    return true;
}
bool CWeaponFX::CreateBloodSpurt(ServerDE& server, HOBJECT source, DVector normal, float damage,
                                 std::minstd_rand& random)
{
    if (!blood_requested)
        return blood_error.empty();
    if (!server.IsObjectAlive(source) || !physics_vector(normal) || vector_dot(normal, normal) < 1e-12f ||
        !std::isfinite(damage) || damage < 0)
    {
        blood_error = "Invalid weapon blood-spurt source/normal/damage";
        return false;
    }
    normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
    OpenBlood2::BloodSpurtSettings s;
    s.position = m_vDestPos;
    s.min_velocity = vector_scale(normal, 30);
    s.max_velocity = vector_scale(normal, 50);
    s.min_velocity.y = 40;
    s.max_velocity.y = 50;
    s.min_velocity.x -= 20;
    s.min_velocity.z -= 20;
    s.max_velocity.x += 20;
    s.max_velocity.z += 20;
    s.lifetime = 1;
    s.texture = std::uniform_int_distribution<int>(1, 3)(random);
    blood_events[0] = server.PublishClientBloodSpurt(s, "weapon entry", true, blood_error);
    s.min_velocity = vector_scale(normal, -2.5f * damage);
    s.max_velocity = vector_scale(normal, -3 * damage);
    s.min_velocity.y += 30;
    s.max_velocity.y += 40;
    s.min_velocity.x -= 20;
    s.min_velocity.z -= 20;
    s.max_velocity.x += 20;
    s.max_velocity.z += 20;
    s.lifetime = 1.2f;
    s.fade_time = .8f;
    std::string second_error;
    blood_events[1] = server.PublishClientBloodSpurt(s, "weapon exit", true, second_error);
    if (!second_error.empty())
        blood_error = second_error;
    return blood_error.empty();
}

// Original UpdateWeaponFX flags and CreateSparks recipes. No liquid substitution in this branch.
bool CWeaponFX::CreateSparks(ServerDE& server, HOBJECT source, DVector normal, float damage, bool melee,
                             std::minstd_rand& random)
{
    spark_event = 0;
    spark_error.clear();
    sparks_big = false;
    sparks_requested = melee || m_eSurfaceType == SURFTYPE_FLESH ||
                       (m_eSurfaceType == SURFTYPE_METAL &&
                        (m_nAmmoType == AMMO_BULLET || m_nAmmoType == AMMO_SHELL || m_nAmmoType == AMMO_BMG));
    if (!sparks_requested)
        return true;
    if (m_eSurfaceType != SURFTYPE_FLESH && m_eSurfaceType != SURFTYPE_METAL)
    {
        spark_error = "Original melee spark recipe has zero duration/count for this surface";
        return false;
    }
    if (!physics_vector(normal) || vector_dot(normal, normal) < 1e-12f || !std::isfinite(damage) || damage < 0)
    {
        spark_error = "Invalid spark impact normal/damage";
        return false;
    }
    normal = vector_scale(normal, 1 / std::sqrt(vector_dot(normal, normal)));
    SparkSettings s;
    s.position = m_vDestPos;
    s.gravity = -150;
    if (m_eSurfaceType == SURFTYPE_FLESH)
    {
        s.direction = vector_scale(normal, 50);
        s.color1 = {192, 0, 0};
        s.color2 = {255, 0, 0};
        s.texture = "spritetextures/particles/blooddrop_" +
                    std::to_string(std::uniform_int_distribution<int>(1, 3)(random)) + ".dtx";
        s.count = damage < 50 ? static_cast<unsigned>(damage * 4) : 200;
        s.duration = 2;
        s.emission_radius = 5;
        s.radius = 200;
    }
    else
    {
        sparks_big = std::uniform_int_distribution<int>(0, 19)(random) == 0;
        s.direction = vector_scale(normal, 75);
        s.color1 = {255, 255, 0};
        s.color2 = {255, 255, 255};
        s.texture = "spritetextures/particles/particle2.dtx";
        s.count = damage < 50 ? static_cast<unsigned>(damage * (sparks_big ? 5 : 2)) : (sparks_big ? 250 : 100);
        s.duration = sparks_big ? 2 : .5f;
        s.emission_radius = .3f;
        s.radius = 175;
    }
    spark_event = server.PublishSparks(source, s, spark_error);
    return spark_event && spark_error.empty();
}
