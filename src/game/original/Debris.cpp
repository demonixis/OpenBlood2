// Selected original CDebris and SetupClientGibFX; see m2_explosion_sources.json.
#include "Debris.h"
#include "SaveFields.h"
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
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
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
namespace
{
    // Original stream order of the material flags; `names` holds the emission order.
    constexpr std::array<unsigned, 9> saved_materials{0, 1, 2, 8, 5, 3, 4, 6, 7};
} // namespace
void CDebris::Save(HMESSAGEWRITE h)
{
    // Original Debris.cpp:462-490. Texture2/Model2 are never assigned (NULL), and the
    // explosion radius and damage keep their constructor values.
    auto* s = BaseClass::GetServerDE();
    namespace F = OpenBlood2::save_fields;
    s->WriteToLoadSaveMessageObject(h, object);
    F::write_string(h, sound);
    F::write_string(h, texture);
    s->WriteToMessageHString(h, nullptr);
    F::write_string(h, model);
    s->WriteToMessageHString(h, nullptr);
    s->WriteToMessageByte(h, amount);
    s->WriteToMessageFloat(h, scale);
    s->WriteToMessageByte(h, exploding);
    for (auto i : saved_materials)
        s->WriteToMessageByte(h, materials[i]);
    s->WriteToMessageByte(h, custom);
    s->WriteToMessageDWord(h, surface);
    s->WriteToMessageFloat(h, explosion_radius);
    s->WriteToMessageFloat(h, explosion_damage);
}
void CDebris::Load(HMESSAGEREAD h)
{
    // Original Debris.cpp:501-532; decoded and checked before any member changes.
    auto* s = BaseClass::GetServerDE();
    namespace F = OpenBlood2::save_fields;
    HOBJECT self = nullptr;
    std::string saved_sound, saved_texture, unused_texture, saved_model, unused_model;
    std::array<bool, 9> saved{};
    bool saved_exploding = false, saved_custom = false;
    s->ReadFromLoadSaveMessageObject(h, &self);
    bool ok = F::read_string(h, saved_sound);
    ok = F::read_string(h, saved_texture) && ok;
    ok = F::read_string(h, unused_texture) && ok;
    ok = F::read_string(h, saved_model) && ok;
    ok = F::read_string(h, unused_model) && ok;
    const DBYTE saved_amount = s->ReadFromMessageByte(h);
    const float saved_scale = s->ReadFromMessageFloat(h);
    ok = F::read_bool(h, saved_exploding) && ok;
    for (auto i : saved_materials)
        ok = F::read_bool(h, saved[i]) && ok;
    ok = F::read_bool(h, saved_custom) && ok;
    const DDWORD saved_surface = s->ReadFromMessageDWord(h);
    const float radius = s->ReadFromMessageFloat(h), damage = s->ReadFromMessageFloat(h);
    if (!ok)
        return;
    // Values the port keeps as constructor constants or never assigns must be unchanged.
    if (!unused_texture.empty() || !unused_model.empty() || !F::finite(saved_scale) || saved_surface > 255 ||
        radius != explosion_radius || damage != explosion_damage)
    {
        s->RejectPayload(h, "debris block holds values the port cannot represent");
        return;
    }
    object = self;
    sound = std::move(saved_sound);
    texture = std::move(saved_texture);
    model = std::move(saved_model);
    amount = saved_amount;
    scale = saved_scale;
    exploding = saved_exploding;
    materials = saved;
    custom = saved_custom;
    surface = saved_surface;
}
