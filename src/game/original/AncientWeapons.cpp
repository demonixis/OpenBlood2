// Original GameWeapons.cpp CAncientOneBeam/CAncientOneTentacle.
#include "InventoryMgr.h"
#include "Weapon.h"
#include "server_de.h"
using namespace OpenBlood2;
void CAncientOneTentacle::BeginFire(bool alternate)
{
    int sound;
    auto* s = BaseClass::GetServerDE();
    if (!s || !s->RandomInt(0, 4, sound))
        return;
    const char* slash = sound < 1   ? "Sounds\\Slash1.wav"
                        : sound < 2 ? "Sounds\\Slash2.wav"
                        : sound < 3 ? "Sounds\\Slash3.wav"
                                    : "Sounds\\Slash4.wav";
    // Original writes the opposite mode's sound, including its persistent value.
    (alternate ? fire_sound : alt_sound) = slash;
}
FireResult CAncientOneBeam::FireVector()
{
    auto* s = BaseClass::GetServerDE();
    beam = 0;
    beam_error.clear();
    explosion_error.clear();
    if (!s || !firing || !s->IsObjectAlive(m_hOwner))
        return last_shot.result = FireResult::Invalid;
    SegmentHit hit;
    const auto query =
        s->IntersectSegment(last_shot.request.position, last_shot.end, m_hOwner, hit, false, SegmentObjects::SolidOnly,
                            SegmentFilter::IgnoreAncientTentacles, SegmentStart::SkipInterior);
    last_shot.collision = query;
    last_shot.hit = hit;
    if (query != SegmentResult::Hit && query != SegmentResult::Miss)
        return last_shot.result = FireResult::Unavailable;
    beam_end = query == SegmentResult::Hit ? hit.point : last_shot.end;
    beam = s->PublishLaserBeam(m_hOwner, last_shot.request.position, beam_end, beam_error, 1 /* RED_LARGE */);
    // Both original queries use the same tentacle filter. Ignoring the owner in
    // the typed ray is equivalent to the source's skip-self continuation.
    if (query == SegmentResult::Hit)
    {
        if (hit.object)
        {
            auto message = s->StartMessageToObject(s->HandleToObject(m_hOwner), hit.object, damage_message);
            if (!message)
                return last_shot.result = FireResult::DeliveryFailed;
            Damage damage{last_shot.direction, hit.point, last_shot.damage * m_pInventoryMgr->damage_multiplier,
                          damage_type, m_hOwner};
            if (s->WriteDamage(message, damage) != DE_OK)
            {
                s->EndHMessageWrite(message);
                return last_shot.result = FireResult::DeliveryFailed;
            }
            if (s->EndMessage(message) != DE_OK)
                return last_shot.result = FireResult::DeliveryFailed;
            last_shot.damage_delivered = true;
        }
        if (!s->IsObjectAlive(m_hOwner))
        {
            last_shot.interrupted = true;
            return last_shot.result;
        }
        SceneExplosionReport r;
        r.source = s->GetObjectName(m_hOwner);
        r.position = hit.point;
        r.normal = {0, 1, 0};
        r.type = 4;
        r.model_event = s->PublishExplosionModels(m_hOwner, 4, r.position, r.normal, r.model_error);
        r.sprite_event = s->PublishExplosionSprites(m_hOwner, 4, r.position, r.normal, r.sprite_error);
        r.ring_event = s->PublishExplosionRings(m_hOwner, 4, r.position, r.normal, r.ring_error);
        r.fragment_event = s->PublishExplosionFragments(m_hOwner, 4, r.position, r.normal, r.fragment_error);
        r.flame_event = s->PublishExplosionFlames(m_hOwner, 4, r.position, r.normal, r.flame_error);
        r.light_event = s->PublishExplosionLights(m_hOwner, 4, r.position, r.normal, r.light_error);
        explosion_error =
            r.model_error + r.sprite_error + r.ring_error + r.fragment_error + r.flame_error + r.light_error;
        s->RecordSceneExplosion(r);
        // Original explosion audio lies after a loop whose every branch returns:
        // it is unreachable, so this beam has no fabricated impact sound.
    }
    return last_shot.result = beam && explosion_error.empty() ? FireResult::Fired : FireResult::Unavailable;
}
