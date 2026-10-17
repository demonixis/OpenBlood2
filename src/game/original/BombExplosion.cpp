#include "BombExplosion.h"
#include "cpp_engineobjects_de.h"
using namespace OpenBlood2;
void OpenBlood2::PublishBombExplosion(HOBJECT source, DVector p, DVector n, unsigned type, const char* sound,
                                      float damage, float radius)
{
    SceneExplosionReport r;
    r.source = g_pServerDE->GetObjectName(source);
    r.position = p;
    r.normal = n;
    r.type = type;
    r.damage = damage;
    r.radius = radius;
    r.model_event = g_pServerDE->PublishExplosionModels(source, type, p, n, r.model_error);
    r.sprite_event = g_pServerDE->PublishExplosionSprites(source, type, p, n, r.sprite_error);
    r.ring_event = g_pServerDE->PublishExplosionRings(source, type, p, n, r.ring_error);
    r.fragment_event = g_pServerDE->PublishExplosionFragments(source, type, p, n, r.fragment_error);
    r.flame_event = g_pServerDE->PublishExplosionFlames(source, type, p, n, r.flame_error);
    r.light_event = g_pServerDE->PublishExplosionLights(source, type, p, n, r.light_error);
    SoundRequest request;
    r.sound = request.resource = sound;
    request.inner_radius = 100;
    request.outer_radius = 1000;
    request.priority = 1;
    r.voice = g_pServerDE->PlayDetachedSound(source, request, p, r.sound_error);
    g_pServerDE->RecordSceneExplosion(r);
}
