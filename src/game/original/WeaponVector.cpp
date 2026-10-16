// Selected original Weapon.cpp FireVector, AddImpact and material/impact-audio path.
#include "DestructableBrush.h"
#include "Door.h"
#include "InventoryMgr.h"
#include "PlayerObj.h"
#include "VolumeBrush.h"
#include "Weapon.h"
#include "engine/collision.h"
using namespace OpenBlood2;
FireResult CWeapon::FireVector()
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !firing)
        return FireResult::Invalid;
    auto from = last_shot.request.position;
    const auto end = last_shot.end, direction = last_shot.direction;
    const bool marks =
        data.m_nAmmoType == AMMO_BULLET || data.m_nAmmoType == AMMO_SHELL || data.m_nAmmoType == AMMO_BMG;
    for (unsigned count = 0; count < 64; ++count)
    {
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return last_shot.result;
        }
        if (last_shot.collision == SegmentResult::Miss)
            return last_shot.result;
        if (last_shot.collision != SegmentResult::Hit)
        {
            impact_error = "Impact continuation collision unavailable";
            return last_shot.result = FireResult::Unavailable;
        }
        auto& impact = impacts.emplace_back();
        ++impact_contacts;
        impact.hit = last_shot.hit;
        impact.from = from;
        const auto h = impact.hit.object;
        auto* object = h ? server->HandleToObject(h) : nullptr;
        if (h && !server->IsObjectAlive(h))
        {
            impact_error = "Impact object no longer alive";
            return last_shot.result = FireResult::Unavailable;
        }
        impact.surface = h ? server->GetObjectUserFlags(h) >> 24 : impact.hit.texture_flags;
        impact.stopping_surface = impact.surface;
        bool damage = false, alert_before = false, alert_after = false;
        if (auto* volume = dynamic_cast<VolumeBrush*>(object))
        {
            impact.branch = WeaponImpactBranch::Volume;
            impact.continued = true;
            impact.effect_requested =
                volume->info.code == CC_WATER || volume->info.code == CC_BLOOD || volume->info.code == CC_ACID;
        }
        else if (!h && impact.surface != SURFTYPE_FIRETHROUGH)
        {
            impact.branch = WeaponImpactBranch::World;
            impact.effect_requested = alert_before = true;
            impact.continued = impact.surface == SURFTYPE_GLASS && data.m_nType != WEAP_MELEE;
        }
        else if (auto* brush = dynamic_cast<CDestructableBrush*>(object); brush && marks)
        {
            impact.branch = WeaponImpactBranch::Brush;
            impact.continued = brush->fire_through;
            if (!brush->fire_through)
            {
                impact.stopping_surface = brush->GetSurfaceType();
                impact.effect_requested = alert_before = damage = true;
                impact.continued = impact.stopping_surface == SURFTYPE_GLASS;
            }
        }
        else if (auto* door = dynamic_cast<Door*>(object))
        {
            impact.branch = WeaponImpactBranch::Door;
            impact.continued = door->fire_through;
            impact.effect_requested = alert_before = damage = !door->fire_through;
        }
        else
        {
            // Original primary-world FIRETHROUGH falls into this general branch and stops.
            impact.effect_requested = true;
            damage = h != nullptr; // No damage aggregate/actor exists for the primary-world sentinel.
            alert_after = !dynamic_cast<CBaseCharacter*>(object);
        }
        if (impact.effect_requested)
        {
            impact.effect.m_nAmmoType = data.m_nAmmoType;
            impact.effect.m_eSurfaceType = static_cast<SurfaceType>(static_cast<DBYTE>(impact.surface));
            impact.effect.m_vDestPos = vector_add(impact.hit.point, vector_scale(impact.hit.normal, 1.25f));
            if (!impact.effect.PlayImpactSound(*server, m_hOwner, impact_random))
                ++impact_sound_failures;
            if (impact.effect.sound)
                ++impact_sounds;
            // Other UpdateWeaponFX/AddImpact visuals and marks remain explicitly unavailable.
        }
        auto alert = [&]
        {
            if (!SendSoundTrigger(m_hOwner, 1, impact.hit.point, 200, impact.alert))
                impact_error = impact.alert.error;
        };
        if (alert_before)
            alert();
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return last_shot.result;
        }
        if (damage && !server->IsObjectAlive(h))
            impact.target_removed = true;
        else if (damage)
        {
            OpenBlood2::Damage value{direction, impact.hit.point, last_shot.damage * m_pInventoryMgr->damage_multiplier,
                                     damage_type, m_hOwner};
            auto message = server->StartMessageToObject(server->HandleToObject(m_hOwner), h, damage_message);
            if (!message)
                return last_shot.result = FireResult::DeliveryFailed;
            if (server->WriteDamage(message, value) != DE_OK)
            {
                server->EndHMessageWrite(message);
                return last_shot.result = FireResult::DeliveryFailed;
            }
            if (server->EndMessage(message) != DE_OK)
                return last_shot.result = FireResult::DeliveryFailed;
            impact.damage_delivered = last_shot.damage_delivered = true;
        }
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return last_shot.result;
        }
        if (alert_after)
            alert();
        if (!server->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return last_shot.result;
        }
        if (!impact.continued)
            return last_shot.result;
        // Original one-unit advance; retain original endpoint/range, do not restart a full-length ray.
        const auto next = vector_add(impact.hit.point, direction);
        if (vector_dot(vector_subtract(next, from), direction) <= 0)
        {
            impact_error = "Impact continuation made no progress";
            return last_shot.result = FireResult::Unavailable;
        }
        from = next;
        if (vector_dot(vector_subtract(end, from), direction) <= 0)
            return last_shot.result;
        last_shot.collision =
            server->IntersectSegment(from, end, m_hOwner, last_shot.hit, false, SegmentObjects::SolidOrRayHit,
                                     SegmentFilter::None, SegmentStart::SkipInterior);
    }
    if (last_shot.collision == SegmentResult::Miss)
        return last_shot.result;
    impact_error = "Impact continuation exceeds 64 contacts";
    return last_shot.result = FireResult::Unavailable;
}
