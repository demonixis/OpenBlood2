// Original Projectile.cpp:830-920 (CProjectile::Save/Load), the subclass mappings and the
// OpenBlood2 shooter tail (save_load_contracts lot 9). Loads decode and check every field before
// a member changes; a refused value fails the payload through RejectPayload.
#include "BugSprayProjectile.h"
#include "CampaignProjectiles.h"
#include "FlareProjectile.h"
#include "Grenade.h"
#include "LeechProjectile.h"
#include "OrbProjectile.h"
#include "PlayerObj.h"
#include "Projectile.h"
#include "ProximityBomb.h"
#include "RemoteBomb.h"
#include "SaveFields.h"
#include "SingularityProjectile.h"
#include "TeslaProjectile.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
namespace F = OpenBlood2::save_fields;
namespace
{
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool known_damage_type(DBYTE type)
    {
        return type <= 15 && type != 2; // DAMAGE_TYPE_FREEZE (Carbon freezer) is not ported.
    }
    // Special members the original CProjectile writes for every class; a class maps the ones it models.
    enum Special : unsigned
    {
        SpecialPosition = 1,
        SpecialArmed = 2,
        SpecialSpin = 4,
    };
    unsigned special_fields(const CProjectile& p)
    {
        if (dynamic_cast<const CGrenade*>(&p))
            return SpecialPosition | SpecialSpin;
        if (dynamic_cast<const CProximityBomb*>(&p) || dynamic_cast<const CRemoteBomb*>(&p))
            return SpecialPosition | SpecialArmed | SpecialSpin;
        if (dynamic_cast<const CTeslaBallProjectile*>(&p) || dynamic_cast<const CSingularityProjectile*>(&p))
            return SpecialArmed;
        return 0;
    }
} // namespace

void CProjectile::SaveBlock(ProjectileBlock& b) const
{
    b.dir = m_vDir;
    b.velocity = m_fVelocity;
    b.flags = flags;
    b.damage_type = static_cast<DBYTE>(damage_type);
    b.radius = static_cast<DDWORD>(m_nRadius);
    b.start_time = m_fStartTime;
    b.damage = m_fDamage;
    b.life = m_fLifeTime;
    b.explode = m_bExplode;
    b.shooter = m_hFiredFrom;
}
std::string CProjectile::CheckBlock(const ProjectileBlock& b) const
{
    if (!physics_vector(b.dir) || !F::finite(b.velocity) || b.velocity < 0 || !std::isfinite(b.start_time) ||
        !F::finite(b.damage) || b.damage < 0 || !F::finite(b.life) || b.radius > 1000000)
        return "projectile direction, speed, clock, damage, lifetime or radius cannot be represented";
    if (!known_damage_type(b.damage_type))
        return "unknown projectile damage type " + std::to_string(b.damage_type);
    // Values the port does not model: the constructor's, or 0 where the original left them unset.
    const ProjectileBlock fixed;
    if (b.remove || b.damage_message || b.shockwave || b.explosion || b.sparks ||
        !same(b.shockwave_min, fixed.shockwave_min) || !same(b.shockwave_max, fixed.shockwave_max) ||
        !same(b.light_color, fixed.light_color) || b.shockwave_duration != 0 || !b.shockwave_file.empty() ||
        !same(b.dims, fixed.dims))
        return "projectile block holds presentation values the port does not model";
    const auto special = special_fields(*this);
    if (special & SpecialPosition ? !physics_vector(b.last_position) : !same(b.last_position, {}))
        return "projectile last position is invalid";
    if (!(special & SpecialArmed) && b.armed)
        return "this projectile class has no armed state";
    if (special & SpecialSpin ? !F::finite(b.pitch_velocity) || !F::finite(b.yaw_velocity) ||
                                    !F::finite(b.pitch, 1e9f) || !F::finite(b.yaw, 1e9f)
                              : b.pitch_velocity != 0 || b.yaw_velocity != 0 || b.pitch != 0 || b.yaw != 0)
        return "projectile spin is invalid";
    return {};
}
std::string CProjectile::LoadBlock(const ProjectileBlock& b)
{
    m_vDir = b.dir;
    m_fVelocity = b.velocity;
    flags = b.flags;
    damage_type = static_cast<DamageType>(b.damage_type);
    m_nRadius = static_cast<int>(b.radius);
    m_fStartTime = b.start_time;
    m_fDamage = b.damage;
    m_fLifeTime = b.life;
    m_bExplode = b.explode;
    m_hFiredFrom = b.shooter;
    // Unsaved, as in the original: hit object, weapon type, contact and the explosion flags.
    configured = true;
    return {};
}
void CProjectile::SaveObject(HMESSAGEWRITE h)
{
    auto* s = g_pServerDE;
    if (!configured || failed)
    {
        s->RejectPayload(h, "an unconfigured or failed projectile cannot be saved");
        return;
    }
    ProjectileBlock b;
    SaveBlock(b);
    s->WriteToMessageVector(h, &b.dir);
    s->WriteToMessageFloat(h, b.velocity);
    s->WriteToMessageDWord(h, b.flags);
    s->WriteToMessageByte(h, b.damage_type);
    s->WriteToMessageDWord(h, b.radius);
    s->WriteToMessageFloat(h, b.start_time - s->GetTime());
    s->WriteToMessageByte(h, b.remove);
    s->WriteToMessageFloat(h, b.damage);
    s->WriteToMessageFloat(h, b.life);
    s->WriteToMessageDWord(h, b.damage_message);
    s->WriteToMessageByte(h, b.shockwave);
    s->WriteToMessageByte(h, b.explosion);
    s->WriteToMessageByte(h, b.sparks);
    s->WriteToMessageVector(h, &b.shockwave_min);
    s->WriteToMessageVector(h, &b.shockwave_max);
    s->WriteToMessageVector(h, &b.light_color);
    s->WriteToMessageFloat(h, b.shockwave_duration);
    F::write_string(h, b.shockwave_file);
    s->WriteToMessageVector(h, &b.dims);
    s->WriteToMessageByte(h, b.explode);
    s->WriteToMessageVector(h, &b.last_position);
    s->WriteToMessageByte(h, b.armed);
    s->WriteToMessageFloat(h, b.pitch_velocity);
    s->WriteToMessageFloat(h, b.yaw_velocity);
    s->WriteToMessageFloat(h, b.pitch);
    s->WriteToMessageFloat(h, b.yaw);
    s->WriteToLoadSaveMessageObject(h, b.shooter); // OpenBlood2 tail.
}
void CProjectile::LoadObject(HMESSAGEREAD h)
{
    auto* s = g_pServerDE;
    const float now = s->GetTime();
    ProjectileBlock b;
    s->ReadFromMessageVector(h, &b.dir);
    b.velocity = s->ReadFromMessageFloat(h);
    b.flags = s->ReadFromMessageDWord(h);
    b.damage_type = s->ReadFromMessageByte(h);
    b.radius = s->ReadFromMessageDWord(h);
    const float start = s->ReadFromMessageFloat(h);
    bool ok = F::read_bool(h, b.remove);
    b.damage = s->ReadFromMessageFloat(h);
    b.life = s->ReadFromMessageFloat(h);
    b.damage_message = s->ReadFromMessageDWord(h);
    ok = F::read_bool(h, b.shockwave) && ok;
    ok = F::read_bool(h, b.explosion) && ok;
    ok = F::read_bool(h, b.sparks) && ok;
    s->ReadFromMessageVector(h, &b.shockwave_min);
    s->ReadFromMessageVector(h, &b.shockwave_max);
    s->ReadFromMessageVector(h, &b.light_color);
    b.shockwave_duration = s->ReadFromMessageFloat(h);
    ok = F::read_string(h, b.shockwave_file) && ok;
    s->ReadFromMessageVector(h, &b.dims);
    ok = F::read_bool(h, b.explode) && ok;
    s->ReadFromMessageVector(h, &b.last_position);
    ok = F::read_bool(h, b.armed) && ok;
    b.pitch_velocity = s->ReadFromMessageFloat(h);
    b.yaw_velocity = s->ReadFromMessageFloat(h);
    b.pitch = s->ReadFromMessageFloat(h);
    b.yaw = s->ReadFromMessageFloat(h);
    s->ReadFromLoadSaveMessageObject(h, &b.shooter);
    if (!ok)
        return;
    b.start_time = std::isfinite(start) ? F::restored_time(start, now) : start;
    if (const auto reason = CheckBlock(b); !reason.empty())
    {
        s->RejectPayload(h, reason);
        return;
    }
    if (const auto reason = LoadBlock(b); !reason.empty())
        s->RejectPayload(h, reason); // A service failure after the change: the restore is undone.
}

// Subclass mappings: the CProjectile special members each class models, and what its skipped
// initial update published that the original's engine-saved effect message replays.
void CGrenade::SaveBlock(ProjectileBlock& b) const
{
    CProjectile::SaveBlock(b);
    b.last_position = last_position;
    b.pitch_velocity = pitch_velocity;
    b.yaw_velocity = yaw_velocity;
    b.pitch = pitch;
    b.yaw = yaw;
}
std::string CGrenade::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    last_position = b.last_position;
    pitch_velocity = b.pitch_velocity;
    yaw_velocity = b.yaw_velocity;
    pitch = b.pitch;
    yaw = b.yaw;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 1, trail_error); // m_nBounceCount restarts at 5.
    return {};
}
void CProximityBomb::SaveBlock(ProjectileBlock& b) const
{
    CProjectile::SaveBlock(b);
    b.last_position = last_position;
    b.armed = armed;
    b.pitch_velocity = pitch_velocity;
    b.yaw_velocity = yaw_velocity;
    b.pitch = pitch;
    b.yaw = yaw;
}
std::string CProximityBomb::LoadBlock(const ProjectileBlock& b)
{
    // Unsaved, as in the original: the get-away time, the hit-character flag and both sound
    // flags restart from the constructor, so an armed bomb plays its arm sound again.
    CProjectile::LoadBlock(b);
    last_position = b.last_position;
    armed = b.armed;
    pitch_velocity = b.pitch_velocity;
    yaw_velocity = b.yaw_velocity;
    pitch = b.pitch;
    yaw = b.yaw;
    return {};
}
void CRemoteBomb::SaveBlock(ProjectileBlock& b) const
{
    CProjectile::SaveBlock(b);
    b.last_position = last_position;
    b.armed = armed;
    b.pitch_velocity = pitch_velocity;
    b.yaw_velocity = yaw_velocity;
    b.pitch = pitch;
    b.yaw = yaw;
}
std::string CRemoteBomb::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    last_position = b.last_position;
    armed = b.armed;
    pitch_velocity = b.pitch_velocity;
    yaw_velocity = b.yaw_velocity;
    pitch = b.pitch;
    yaw = b.yaw;
    // Departure (with the shooter tail): a charge its player has not signalled is listed again,
    // in load order, so the restored player can still detonate it. Its m_bFalling is lost.
    if (!m_bExplode && g_pServerDE->IsObjectAlive(m_hFiredFrom))
        if (auto* player = dynamic_cast<CPlayerObj*>(g_pServerDE->HandleToObject(m_hFiredFrom)))
            if (!player->RestoreRemoteBomb(m_hObject))
                return "remote charge could not rejoin its player: " + player->remote_error;
    return {};
}
void CTeslaBallProjectile::SaveBlock(ProjectileBlock& b) const
{
    CProjectile::SaveBlock(b);
    b.armed = armed;
}
std::string CTeslaBallProjectile::LoadBlock(const ProjectileBlock& b)
{
    // The attract clock is the subclass's shadow m_fStartTime (constructor 0): an armed ball
    // expires on its first update after a restore, as in the original.
    CProjectile::LoadBlock(b);
    armed = b.armed;
    exploded = contacted = armed;
    return {};
}
void CSingularityProjectile::SaveBlock(ProjectileBlock& b) const
{
    CProjectile::SaveBlock(b);
    b.armed = armed;
}
std::string CSingularityProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b); // The shadow arm clock is lost too (see CTeslaBallProjectile).
    armed = b.armed;
    return {};
}
std::string COrbProjectile::LoadBlock(const ProjectileBlock& b)
{
    // Unsaved, as in the original: tracking, attachment and the hit clocks. A saved alternate orb
    // never holds its player's camera (the player's save refuses an active Orb), so the camera
    // is not created again.
    CProjectile::LoadBlock(b);
    create_camera = false;
    if (!alternate)
        trail = g_pServerDE->PublishObjectParticles(m_hObject, 33, trail_error);
    return {};
}
std::string CLeechPrimeProjectile::LoadBlock(const ProjectileBlock& b)
{
    // The curve and amplitude come from the skipped initial update: the restored leech flies
    // straight, as the original's unsaved m_fCurve/m_fAmp do.
    CProjectile::LoadBlock(b);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 3, trail_error, trail_scale);
    return {};
}
std::string CBugSprayProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, alternate_ ? 32 : 31, trail_error);
    return {};
}
std::string CVomitProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 3, trail_error, .25f);
    return {};
}
std::string CSkullProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    g_pServerDE->PublishObjectParticles(m_hObject, 3, trail_error);
    return {};
}
std::string CFlareProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 6, trail_error);
    return {};
}
std::string CFlareBurstProjectile::LoadBlock(const ProjectileBlock& b)
{
    // The original leaves m_vColor uninitialized on a restore; the port keeps the engine colour
    // its last update set.
    CProjectile::LoadBlock(b);
    float alpha = 1;
    color_ready = g_pServerDE->GetObjectColor(m_hObject, color, alpha);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 8, trail_error);
    return {};
}

std::string CNapalmProjectile::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    smoke = g_pServerDE->PublishObjectParticles(m_hObject, 2, smoke_error);
    return {};
}
std::string CHowitzerAltFrag::LoadBlock(const ProjectileBlock& b)
{
    CProjectile::LoadBlock(b);
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 1, trail_error);
    return {};
}

std::string CEnergyBlastProjectile::LoadBlock(const ProjectileBlock& b)
{
    if (auto failure = CProjectile::LoadBlock(b); !failure.empty())
        return failure;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 4, trail_error, .5f);
    return {};
}

std::string CGroundStrikeProjectile::LoadBlock(const ProjectileBlock& b)
{
    if (auto failure = CProjectile::LoadBlock(b); !failure.empty())
        return failure;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 27, trail_error, .5f);
    return {};
}

std::string CShikariLoogieProjectile::LoadBlock(const ProjectileBlock& b)
{
    if (auto failure = CProjectile::LoadBlock(b); !failure.empty())
        return failure;
    trail = g_pServerDE->PublishObjectParticles(m_hObject, 31, trail_error, .25f);
    return {};
}
