// Original ObjectDLL/Rain.cpp, with a typed client effect publication.
#include "Rain.h"
#include "SaveFields.h"

using namespace OpenBlood2;

bool Rain::ReadProps()
{
    auto* s = GetServerDE();
    GenericProp p;
    auto real = [&](const char* name, float& value)
    {
        if (s->GetPropGeneric(name, &p) == DE_OK)
            value = p.m_Float;
    };
    auto vector = [&](const char* name, DVector& value)
    {
        if (s->GetPropGeneric(name, &p) == DE_OK)
            value = p.m_Vec;
    };
    real("Density", settings.density);
    real("Lifetime", settings.lifetime);
    real("ParticleScale", settings.scale);
    real("Spread", settings.spread);
    real("TimeLimit", settings.time_limit);
    real("Pulse", settings.pulse);
    vector("Dims", settings.dims);
    vector("Direction", settings.direction);
    vector("Color1", settings.color1);
    vector("Color2", settings.color2);
    if (s->GetPropGeneric("AddGravity", &p) == DE_OK)
        settings.gravity = p.m_Bool;
    if (s->GetPropGeneric("Triggered", &p) == DE_OK)
        triggered = p.m_Bool;
    return valid_rain(settings);
}
bool Rain::Setup(const RainSettings& value)
{
    if (!valid_rain(value) || !GetServerDE() || !GetServerDE()->IsObjectAlive(m_hObject))
        return false;
    settings = value;
    Publish();
    return effect != 0;
}
void Rain::Publish()
{
    effect = GetServerDE()->PublishRain(m_hObject, settings, error);
}
DDWORD Rain::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (message == MID_PRECREATE && reason == PRECREATE_WORLDFILE && !ReadProps())
    {
        s->RejectPrecreate("Invalid Rain properties");
        return 0;
    }
    if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        s->SetObjectUserFlags(m_hObject, s->GetObjectUserFlags(m_hObject) | USRFLG_SAVEABLE);
        if (!triggered)
            Publish();
    }
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    return CClientSFX::EngineMessageFn(message, data, reason);
}
DDWORD Rain::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    if (message == 1000)
    {
        auto* s = GetServerDE();
        const auto h = s->ReadFromMessageHString(read);
        if (h)
        {
            const auto* text = s->GetStringData(h);
            const bool start = text && same_name(text, "TRIGGER");
            s->FreeString(h);
            if (start)
                Publish();
        }
    }
    return CClientSFX::ObjectMessageFn(sender, message, read);
}
void Rain::Save(HMESSAGEWRITE h)
{
    auto* s = GetServerDE();
    s->WriteToMessageDWord(h, settings.flags);
    s->WriteToMessageFloat(h, settings.density);
    s->WriteToMessageFloat(h, settings.lifetime);
    s->WriteToMessageByte(h, settings.gravity);
    s->WriteToMessageFloat(h, settings.scale);
    s->WriteToMessageFloat(h, settings.spread);
    s->WriteToMessageByte(h, triggered);
    s->WriteToMessageVector(h, &settings.dims);
    s->WriteToMessageVector(h, &settings.direction);
    s->WriteToMessageVector(h, &settings.color1);
    s->WriteToMessageVector(h, &settings.color2);
    s->WriteToMessageFloat(h, settings.time_limit);
    s->WriteToMessageFloat(h, settings.pulse);
}
void Rain::Load(HMESSAGEREAD h)
{
    auto* s = GetServerDE();
    RainSettings decoded;
    decoded.flags = s->ReadFromMessageDWord(h);
    decoded.density = s->ReadFromMessageFloat(h);
    decoded.lifetime = s->ReadFromMessageFloat(h);
    const bool valid_gravity = save_fields::read_bool(h, decoded.gravity);
    decoded.scale = s->ReadFromMessageFloat(h);
    decoded.spread = s->ReadFromMessageFloat(h);
    bool decoded_triggered = false;
    const bool valid_trigger = save_fields::read_bool(h, decoded_triggered);
    s->ReadFromMessageVector(h, &decoded.dims);
    s->ReadFromMessageVector(h, &decoded.direction);
    s->ReadFromMessageVector(h, &decoded.color1);
    s->ReadFromMessageVector(h, &decoded.color2);
    decoded.time_limit = s->ReadFromMessageFloat(h);
    decoded.pulse = s->ReadFromMessageFloat(h);
    if (!valid_gravity || !valid_trigger || !valid_rain(decoded))
    {
        s->RejectPayload(h, "Invalid Rain state");
        return;
    }
    settings = decoded;
    triggered = decoded_triggered;
    if (!triggered)
        Publish();
}
