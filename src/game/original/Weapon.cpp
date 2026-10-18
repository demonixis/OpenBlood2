#include "Weapon.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
#include <random>
#include <vector>

using namespace OpenBlood2;
float CWeapon::CheckAmmo(bool alternate) const
{
    return GetAmmoType(alternate) == AMMO_NONE
               ? 1.0f
               : (m_pInventoryMgr ? m_pInventoryMgr->GetAmmoCount(GetAmmoType(alternate)) : 0.0f);
}
FireResult CWeapon::Fire(const WeaponFire& request)
{
    if (firing)
        return FireResult::Blocked;
    firing = true;
    struct ResetFire
    {
        bool& value;
        ~ResetFire()
        {
            value = false;
        }
    } reset{firing};
    BeginFire(request.alternate);
    auto* server = BaseClass::GetServerDE();
    if (!server || !m_pInventoryMgr || !server->IsObjectAlive(m_hOwner) || request.weapon != data.m_nType ||
        request.left != m_bLeftHand ||
        (inventory_bomb(data.m_nType) && (!std::isfinite(request.projectile_speed) || request.projectile_speed < 0 ||
                                          request.projectile_speed > 1750)) ||
        !physics_vector(request.position) || !physics_vector(request.direction) ||
        !std::isfinite(m_pInventoryMgr->damage_multiplier) || m_pInventoryMgr->damage_multiplier < 0 ||
        m_pInventoryMgr->damage_multiplier > 1000)
        return FireResult::Invalid;
    const auto length = std::sqrt(vector_dot(request.direction, request.direction));
    if (!std::isfinite(length) || length < 1e-6f)
        return FireResult::Invalid;
    last_shot = {};
    impacts.clear();
    impact_error.clear();
    last_shot.request = request;
    const auto forward = vector_scale(request.direction, 1 / length);
    // AlignRotation's world-up basis (stable vertical fallback).
    auto right = DVector{forward.z, 0, -forward.x};
    const auto right_length = std::sqrt(vector_dot(right, right));
    right = right_length > 1e-6f ? vector_scale(right, 1 / right_length) : DVector{1, 0, 0};
    const DVector up{forward.y * right.z - forward.z * right.y, forward.z * right.x - forward.x * right.z,
                     forward.x * right.y - forward.y * right.x};
    // Source srand(seed) / GetRandom order, with a local portable generator.
    // CRT-dependent retail random bits are intentionally not asserted identical.
    std::minstd_rand random(request.seed);
    auto sample = [&](float low, float high)
    {
        return low + (high - low) * static_cast<float>(random() - random.min()) /
                         static_cast<float>(random.max() - random.min());
    };
    last_shot.damage = request.alternate ? sample(data.m_fMinAltDamage, data.m_fMaxAltDamage)
                                         : sample(data.m_fMinDamage, data.m_fMaxDamage);
    const auto use = request.alternate ? data.m_nAltAmmoUse : data.m_nAmmoUse;
    const auto ammo = CheckAmmo(request.alternate);
    if (ammo < use)
    {
        last_shot.result = FireResult::Dry;
        return last_shot.result;
    }
    // Original FireSpread: every vector of one fire shares the damage draw. Shells cost
    // the ammo use once per fire; other ammo costs one use per vector, reduced to what
    // is available. All selected single-vector definitions keep their former behavior.
    DDWORD vectors = request.alternate ? data.m_dwAltShotsPerFire : data.m_dwShotsPerFire;
    if (!vectors || vectors > 64)
        return last_shot.result = FireResult::Invalid;
    float cost = static_cast<float>(use);
    if (GetAmmoType(request.alternate) != AMMO_SHELL)
    {
        if (static_cast<float>(vectors * use) > ammo)
            vectors = static_cast<DDWORD>(static_cast<int>(ammo) / use);
        cost = static_cast<float>(vectors * use);
    }
    const auto spread = request.alternate ? data.m_AltSpread : data.m_Spread;
    std::vector<DVector> directions(vectors);
    for (auto& direction : directions)
    {
        direction = vector_add(forward, vector_scale(right, sample(-spread.h, spread.h) / 2000));
        direction = vector_add(direction, vector_scale(up, sample(-spread.v, spread.v) / 2000));
        direction = vector_scale(direction, 1 / std::sqrt(vector_dot(direction, direction)));
    }
    const auto range = request.alternate ? data.m_fAltRange : data.m_fRange;
    const auto trace = [&](DVector direction)
    {
        auto aim_direction = direction;
        if (inventory_bomb(data.m_nType) && request.alternate)
        {
            last_shot.direction = {0, -1, 0};
            last_shot.collision = SegmentResult::Miss;
            return server->GetObjectPos(m_hOwner, &last_shot.request.position) == DE_OK;
        }
        // Source FireSpread aligns the eye only for a player. NPCs fire the
        // spread direction from their actual gun node (or body fallback).
        if (const auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(m_hOwner)))
        {
            DVector eye;
            server->GetObjectPos(m_hOwner, &eye);
            eye.y += player->eye_level;
            last_shot.request.position = request.position;
            if (inventory_bomb(data.m_nType) || data.m_nType == WEAP_FLAREGUN ||
                (data.m_nType == WEAP_ASSAULTRIFLE && request.alternate))
            {
                SegmentHit close_hit;
                const auto close = server->IntersectSegment(eye, vector_add(eye, vector_scale(forward, 60)), m_hOwner,
                                                            close_hit, false, SegmentObjects::SolidOnly,
                                                            SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
                if (close == SegmentResult::Invalid || close == SegmentResult::Unavailable)
                {
                    last_shot.result = FireResult::Unavailable;
                    return false;
                }
                if (close == SegmentResult::Hit)
                    last_shot.request.position = eye;
            }
            auto aim = vector_add(eye, vector_scale(aim_direction, range));
            SegmentHit aim_hit;
            const auto query =
                server->IntersectSegment(eye, aim, m_hOwner, aim_hit, false, SegmentObjects::SolidOrRayHit,
                                         SegmentFilter::IgnoreVolumes, SegmentStart::SkipInterior);
            if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
            {
                last_shot.result = FireResult::Unavailable;
                return false;
            }
            if (query == SegmentResult::Hit)
                aim = aim_hit.point;
            aim_direction = vector_subtract(aim, last_shot.request.position);
            const auto aim_length = std::sqrt(vector_dot(aim_direction, aim_direction));
            if (aim_length < 1e-6f)
            {
                last_shot.result = FireResult::Invalid;
                return false;
            }
            aim_direction = vector_scale(aim_direction, 1 / aim_length);
        }
        last_shot.direction = aim_direction;
        last_shot.end = vector_add(last_shot.request.position, vector_scale(aim_direction, range));
        if (data.m_nType == WEAP_GIDEON_SHIELD)
        {
            last_shot.collision = SegmentResult::Unavailable; // Visual-only source branch: no damage ray.
            return true;
        }
        const auto query =
            server->IntersectSegment(last_shot.request.position, last_shot.end, m_hOwner, last_shot.hit, false,
                                     SegmentObjects::SolidOrRayHit, SegmentFilter::None, SegmentStart::SkipInterior);
        last_shot.collision = query;
        if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
        {
            last_shot.result = FireResult::Unavailable;
            return false;
        }
        return true;
    };
    // No-consumption collision preflight of every vector.
    const auto base = last_shot;
    std::vector<WeaponShot> traced(vectors, base);
    const auto trace_all = [&]()
    {
        for (unsigned i = 0; i < vectors; ++i)
        {
            last_shot = base;
            if (!trace(directions[i]))
                return false;
            traced[i] = last_shot;
        }
        return true;
    };
    if (!trace_all())
        return last_shot.result;
    // Alternate Flare Gun consumes eight rounds; shotgun shells one or two per fire.
    m_pInventoryMgr->SetAmmoCount(GetAmmoType(request.alternate), ammo - cost);
    m_fLastShotTime = server->GetTime();
    m_bLastFireAlt = request.alternate;
    ++shots;
    last_pellets = vectors;
    last_shot.result = FireResult::Fired;
    PlayFireSound(request.alternate, request.position);
    if (!server->IsObjectAlive(m_hOwner))
    {
        last_shot.interrupted = true;
        last_shot.hit = {};
        last_shot.collision = SegmentResult::Unavailable;
        return last_shot.result; // Accepted shot; a hearing callback removed its owner before impact.
    }
    // After actual hearing callbacks, resolve the final rays again rather than damage
    // a stale/moved target.
    if (fire_sound_alert.eligible && !trace_all())
        return last_shot.result;
    const bool projectile = request.alternate ? data.m_szAltProjectileClass : data.m_szProjectileClass;
    auto result = FireResult::Fired;
    for (unsigned i = 0; i < vectors; ++i)
    {
        last_shot = traced[i];
        last_shot.result = FireResult::Fired;
        const auto vector_result = projectile ? FireProjectile() : FireVector();
        if (vector_result != FireResult::Fired)
            result = vector_result;
        if (last_shot.interrupted || !server->IsObjectAlive(m_hOwner))
            break; // A damage/impact callback removed the shooter: no further vectors.
    }
    return result;
}
