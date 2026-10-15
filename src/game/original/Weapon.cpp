#include "Weapon.h"
#include "InventoryMgr.h"
#include "engine/collision.h"
#include <cmath>
#include <random>

using namespace blood2;
float CWeapon::CheckAmmo() const { return data.m_nAmmoType==AMMO_NONE ? 1.0f : (m_pInventoryMgr ? m_pInventoryMgr->GetAmmoCount(data.m_nAmmoType) : 0.0f); }
FireResult CWeapon::Fire(const WeaponFire& request) {
    auto* server=BaseClass::GetServerDE();
    if (!server || !m_pInventoryMgr || !server->HandleToObject(m_hOwner) || request.weapon!=data.m_nType || request.left!=m_bLeftHand ||
        !physics_vector(request.position) || !physics_vector(request.direction)) return FireResult::Invalid;
    const auto length=std::sqrt(vector_dot(request.direction,request.direction));
    if (!std::isfinite(length) || length<1e-6f) return FireResult::Invalid;
    last_shot={}; last_shot.request=request;
    const auto forward=vector_scale(request.direction,1/length);
    // AlignRotation's world-up basis (stable vertical fallback).
    auto right=DVector{forward.z,0,-forward.x};
    const auto right_length=std::sqrt(vector_dot(right,right));
    right=right_length>1e-6f ? vector_scale(right,1/right_length) : DVector{1,0,0};
    const DVector up{forward.y*right.z-forward.z*right.y,forward.z*right.x-forward.x*right.z,forward.x*right.y-forward.y*right.x};
    // Source srand(seed) / GetRandom order, with a local portable generator.
    // CRT-dependent retail random bits are intentionally not asserted identical.
    std::minstd_rand random(request.seed);
    auto sample=[&](float low,float high) { return low+(high-low)*static_cast<float>(random()-random.min())/static_cast<float>(random.max()-random.min()); };
    last_shot.damage=request.alternate ? sample(data.m_fMinAltDamage,data.m_fMaxAltDamage) : sample(data.m_fMinDamage,data.m_fMaxDamage);
    const auto use=request.alternate ? data.m_nAltAmmoUse : data.m_nAmmoUse;
    const auto ammo=CheckAmmo();
    if (ammo<use) { last_shot.result=FireResult::Dry; return last_shot.result; }
    const auto spread=request.alternate ? data.m_AltSpread : data.m_Spread;
    auto direction=vector_add(forward,vector_scale(right,sample(-spread.h,spread.h)/2000));
    direction=vector_add(direction,vector_scale(up,sample(-spread.v,spread.v)/2000));
    direction=vector_scale(direction,1/std::sqrt(vector_dot(direction,direction)));
    const auto range=request.alternate ? data.m_fAltRange : data.m_fRange;
    // FireSpread -> AlignFireVector: project from Caleb's standing eye, then
    // aim the muzzle at that intersection. Player input owns the muzzle origin.
    DVector eye; server->GetObjectPos(m_hOwner,&eye); eye.y+=36;
    auto aim=vector_add(eye,vector_scale(direction,range)); SegmentHit aim_hit;
    auto query=server->IntersectSegment(eye,aim,m_hOwner,aim_hit);
    if (query==SegmentResult::Invalid || query==SegmentResult::Unavailable) { last_shot.result=FireResult::Unavailable; return last_shot.result; }
    if (query==SegmentResult::Hit) aim=aim_hit.point;
    direction=vector_subtract(aim,request.position);
    const auto aim_length=std::sqrt(vector_dot(direction,direction));
    if (aim_length<1e-6f) { last_shot.result=FireResult::Invalid; return last_shot.result; }
    direction=vector_scale(direction,1/aim_length);
    last_shot.direction=direction; last_shot.end=vector_add(request.position,vector_scale(direction,range));
    query=server->IntersectSegment(request.position,last_shot.end,m_hOwner,last_shot.hit);
    last_shot.collision=query;
    if (query==SegmentResult::Invalid || query==SegmentResult::Unavailable) { last_shot.result=FireResult::Unavailable; return last_shot.result; }
    // Both selected definitions fire exactly one vector and use one unit.
    m_pInventoryMgr->SetAmmoCount(data.m_nAmmoType,ammo-use);
    m_fLastShotTime=server->GetTime(); m_bLastFireAlt=request.alternate; ++shots;
    last_shot.result=FireResult::Fired;
    if (query==SegmentResult::Hit && last_shot.hit.object) {
        Damage damage{direction,last_shot.hit.point,last_shot.damage,damage_type,m_hOwner};
        auto message=server->StartMessageToObject(server->HandleToObject(m_hOwner),last_shot.hit.object,damage_message);
        if (!message) last_shot.result=FireResult::DeliveryFailed;
        else if (server->WriteDamage(message,damage)!=DE_OK) { server->EndHMessageWrite(message); last_shot.result=FireResult::DeliveryFailed; }
        else if (server->EndMessage(message)!=DE_OK) last_shot.result=FireResult::DeliveryFailed;
        else last_shot.damage_delivered=true;
    }
    return last_shot.result;
}
