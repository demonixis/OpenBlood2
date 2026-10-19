// Original ObjectDLL/GameProjectiles.cpp CNapalmProjectile (Monolith).
#include "NapalmProjectile.h"
using namespace OpenBlood2;
namespace
{
    void publish(HOBJECT owner, SceneExplosionReport& r, DDWORD type, DVector position, DVector normal,
                 const char* sound, float radius)
    {
        auto* s = g_pServerDE;
        r = {};
        r.source = s->GetObjectName(owner);
        r.position = position;
        r.normal = normal;
        r.type = type;
        r.model_event = s->PublishExplosionModels(owner, type, position, normal, r.model_error);
        r.sprite_event = s->PublishExplosionSprites(owner, type, position, normal, r.sprite_error);
        r.ring_event = s->PublishExplosionRings(owner, type, position, normal, r.ring_error);
        r.fragment_event = s->PublishExplosionFragments(owner, type, position, normal, r.fragment_error);
        r.flame_event = s->PublishExplosionFlames(owner, type, position, normal, r.flame_error);
        r.light_event = s->PublishExplosionLights(owner, type, position, normal, r.light_error);
        SoundRequest request;
        r.sound = request.resource = sound;
        request.inner_radius = radius * .1f; // PlaySoundFromPos.
        request.outer_radius = radius;
        request.priority = 1; // SOUNDPRIORITY_MISC_MEDIUM.
        r.voice = s->PlayDetachedSound(owner, request, position, r.sound_error);
        s->RecordSceneExplosion(r);
    }
} // namespace
CNapalmProjectile::~CNapalmProjectile()
{
    if (flight_sound && g_pServerDE)
        g_pServerDE->KillSound(flight_sound);
}
bool CNapalmProjectile::InitialUpdate()
{
    return CProjectile::InitialUpdate() && g_pServerDE->ScaleObject(m_hObject, {});
}
bool CNapalmProjectile::Update()
{
    // Hidden (scale zero) for its first 0.01 s, then 0.25.
    const float scale = g_pServerDE->GetTime() - m_fStartTime < .01f ? 0 : .25f;
    if (!flight_sound)
    {
        // PlaySoundFromObject(.., 750, MISC_MEDIUM, loop, handle): attached and retained.
        SoundRequest request;
        request.resource = "Sounds\\Weapons\\napalm\\projectile.wav";
        request.speaker = m_hObject;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 750;
        request.inner_radius = 75;
        request.priority = 1;
        request.reverb_requested = true;
        request.loop = true;
        flight_sound = g_pServerDE->PlayObjectLoopSound(m_hObject, request, sound_error);
    }
    if (!g_pServerDE->ScaleObject(m_hObject, {scale, scale, scale}))
        return Fail("Napalm projectile scale rejected");
    return CProjectile::Update();
}
void CNapalmProjectile::AddExplosion(DVector position, DVector normal)
{
    if (flight_sound)
    {
        g_pServerDE->KillSound(flight_sound);
        flight_sound = 0;
    }
    publish(m_hObject, effect, 4 /* EXP_NAPALM_PRIMARY */, position, normal, "Sounds\\Weapons\\Napalm\\impact.wav",
            2000);
}
