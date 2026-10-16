#include "Weapon.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <cmath>
#include <random>

using namespace OpenBlood2;
float CWeapon::CheckAmmo() const
{
    return data.m_nAmmoType == AMMO_NONE ? 1.0f
                                         : (m_pInventoryMgr ? m_pInventoryMgr->GetAmmoCount(data.m_nAmmoType) : 0.0f);
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
    auto* server = BaseClass::GetServerDE();
    if (!server || !m_pInventoryMgr || !server->IsObjectAlive(m_hOwner) || request.weapon != data.m_nType ||
        request.left != m_bLeftHand || !physics_vector(request.position) || !physics_vector(request.direction) ||
        !std::isfinite(m_pInventoryMgr->damage_multiplier) || m_pInventoryMgr->damage_multiplier < 0 ||
        m_pInventoryMgr->damage_multiplier > 1000)
        return FireResult::Invalid;
    const auto length = std::sqrt(vector_dot(request.direction, request.direction));
    if (!std::isfinite(length) || length < 1e-6f)
        return FireResult::Invalid;
    last_shot = {};
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
    const auto ammo = CheckAmmo();
    if (ammo < use)
    {
        last_shot.result = FireResult::Dry;
        return last_shot.result;
    }
    const auto spread = request.alternate ? data.m_AltSpread : data.m_Spread;
    auto direction = vector_add(forward, vector_scale(right, sample(-spread.h, spread.h) / 2000));
    direction = vector_add(direction, vector_scale(up, sample(-spread.v, spread.v) / 2000));
    direction = vector_scale(direction, 1 / std::sqrt(vector_dot(direction, direction)));
    const auto range = request.alternate ? data.m_fAltRange : data.m_fRange;
    const auto trace = [&]()
    {
        auto aim_direction = direction;
        // Source FireSpread aligns the eye only for a player. NPCs fire the
        // spread direction from their actual gun node (or body fallback).
        if (const auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(m_hOwner)))
        {
            DVector eye;
            server->GetObjectPos(m_hOwner, &eye);
            eye.y += player->eye_level;
            auto aim = vector_add(eye, vector_scale(aim_direction, range));
            SegmentHit aim_hit;
            const auto query = server->IntersectSegment(eye, aim, m_hOwner, aim_hit);
            if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
            {
                last_shot.result = FireResult::Unavailable;
                return false;
            }
            if (query == SegmentResult::Hit)
                aim = aim_hit.point;
            aim_direction = vector_subtract(aim, request.position);
            const auto aim_length = std::sqrt(vector_dot(aim_direction, aim_direction));
            if (aim_length < 1e-6f)
            {
                last_shot.result = FireResult::Invalid;
                return false;
            }
            aim_direction = vector_scale(aim_direction, 1 / aim_length);
        }
        last_shot.direction = aim_direction;
        last_shot.end = vector_add(request.position, vector_scale(aim_direction, range));
        const auto query = server->IntersectSegment(request.position, last_shot.end, m_hOwner, last_shot.hit, true);
        last_shot.collision = query;
        if (query == SegmentResult::Invalid || query == SegmentResult::Unavailable)
        {
            last_shot.result = FireResult::Unavailable;
            return false;
        }
        return true;
    };
    if (!trace())
        return last_shot.result;
    // Both selected definitions fire exactly one vector and use one unit.
    m_pInventoryMgr->SetAmmoCount(data.m_nAmmoType, ammo - use);
    m_fLastShotTime = server->GetTime();
    m_bLastFireAlt = request.alternate;
    ++shots;
    last_shot.result = FireResult::Fired;
    PlayFireSound(request.alternate, request.position);
    if (!server->IsObjectAlive(m_hOwner))
    {
        last_shot.interrupted = true;
        last_shot.hit = {};
        last_shot.collision = SegmentResult::Unavailable;
        return last_shot.result; // Accepted shot; a hearing callback removed its owner before impact.
    }
    // Keep the existing no-consumption collision preflight. After actual hearing
    // callbacks, resolve the final ray again rather than damage a stale/moved target.
    if (fire_sound_alert.eligible && !trace())
        return last_shot.result;
    if (last_shot.collision == SegmentResult::Hit && last_shot.hit.object)
    {
        Damage damage{last_shot.direction, last_shot.hit.point, last_shot.damage * m_pInventoryMgr->damage_multiplier,
                      damage_type, m_hOwner};
        auto message =
            server->StartMessageToObject(server->HandleToObject(m_hOwner), last_shot.hit.object, damage_message);
        if (!message)
            last_shot.result = FireResult::DeliveryFailed;
        else if (server->WriteDamage(message, damage) != DE_OK)
        {
            server->EndHMessageWrite(message);
            last_shot.result = FireResult::DeliveryFailed;
        }
        else if (server->EndMessage(message) != DE_OK)
            last_shot.result = FireResult::DeliveryFailed;
        else
            last_shot.damage_delivered = true;
    }
    return last_shot.result;
}
