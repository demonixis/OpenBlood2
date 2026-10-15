// Selected original CDebris and SetupClientGibFX; see M2_EXPLOSION_SOURCES.json.
#include "Debris.h"
#include "engine/collision.h"
#include "radius_damage.h"
#include <cmath>
bool CDebris::Init(HOBJECT handle)
{
    if (!handle || !BaseClass::GetServerDE())
        return false;
    object = handle;
    return true;
}
DDWORD CDebris::EngineMessageFn(LPBASECLASS owner, DDWORD message, void* data, DFLOAT reason)
{
    auto* server = BaseClass::GetServerDE();
    if (message == MID_PRECREATE && data && (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP))
    {
        GenericProp prop;
        if (server->GetPropGeneric("Amount", &prop) == DE_OK)
            amount = static_cast<DBYTE>(prop.m_Long);
        auto boolean = [&](const char* key, bool& value)
        {
            if (server->GetPropGeneric(key, &prop) == DE_OK)
                value = prop.m_Bool != 0;
        };
        for (unsigned i = 0; i < names.size(); ++i)
            boolean(names[i], materials[i]);
        boolean("Exploding", exploding);
        boolean("Custom", custom);
        server->GetPropReal("Scale", &scale);
        auto text = [&](const char* key, std::string& value)
        {
            if (server->GetPropGeneric(key, &prop) == DE_OK)
                value = prop.m_String;
        };
        text("CustomModel", model);
        text("CustomTexture", texture);
        text("CustomSound", sound);
        valid = std::isfinite(scale) && std::abs(scale) <= 1e6f;
    }
    if (message == MID_INITIALUPDATE && reason != 3)
        for (unsigned i = 0; i < materials.size(); ++i)
            if (materials[i])
            {
                surface = surfaces[i];
                break;
            }
    return Aggregate::EngineMessageFn(owner, message, data, reason);
}
void CDebris::Create(DVector direction, float damage)
{
    using namespace OpenBlood2;
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(object) || !physics_vector(direction) || !std::isfinite(damage))
        return;
    DebrisEffect effect;
    effect.source = server->GetObjectName(object);
    server->GetObjectPos(object, &effect.position);
    server->GetObjectDims(object, &effect.dimensions);
    effect.amount = amount;
    effect.scale = scale;
    effect.model = model;
    effect.texture = texture;
    effect.sound = sound;
    direction = vector_scale(direction, -1);
    direction.y -= 1;
    const auto length = std::sqrt(vector_dot(direction, direction));
    // Source normalizes zero when the damage direction is exactly (0,-1,0).
    // Keep a finite zero launch vector for that singular input.
    effect.velocity = length > 0 ? vector_scale(direction, (50 + damage) / length) : DVector{};
    if (exploding)
    {
        ExplosionEffect explosion;
        explosion.source = effect.source;
        explosion.position = effect.position;
        explosion.targets = DamageObjectsInRadius(object, server->HandleToObject(object), effect.position,
                                                  explosion_radius, explosion_damage, DamageType::Explode);
        server->RecordExplosionEffect(explosion);
    }
    const DDWORD custom_flag = (!model.empty() || !texture.empty() || !sound.empty()) ? 0x4000 : 0;
    for (unsigned i = 0; i < materials.size(); ++i)
        if (materials[i])
        {
            // SetupClientGibFX mutates the shared input vector on EACH call.
            // Multi-material requests therefore alternate signs in source order.
            effect.velocity = vector_scale(effect.velocity, -1);
            effect.flags = surfaces[i] / 10 | 0x200 | custom_flag;
            server->RecordDebrisEffect(effect);
        }
}
