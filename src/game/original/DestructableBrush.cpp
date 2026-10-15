// Selected original brush lifecycle. Client FX and pushable-body physics remain unavailable.
#include "DestructableBrush.h"
#include "engine/collision.h"
#include <cmath>
CDestructableBrush::CDestructableBrush() : B2BaseClass(OT_WORLDMODEL)
{
    AddAggregate(&m_damage);
    AddAggregate(&debris);
}
DDWORD CDestructableBrush::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (message == MID_PRECREATE)
    {
        const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
        if (!accepted || !data || reason == 3 || !debris.valid)
            return 0;
        auto& create = *static_cast<ObjectCreateStruct*>(data);
        if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
        {
            server->GetPropReal("HitPoints", &hit_points);
            server->GetPropReal("ObjectMass", &mass);
            auto boolean = [&](const char* key, bool& value)
            {
                DBOOL result = value;
                server->GetPropBool(key, &result);
                value = result != 0;
            };
            boolean("Destructable", destructable);
            boolean("Pushable", pushable);
            boolean("BoxPhysics", box_physics);
            boolean("AllowMarks", allow_marks);
            boolean("FireThrough", fire_through);
            GenericProp prop;
            if (server->GetPropGeneric("SurfaceType", &prop) == DE_OK)
            {
                if (prop.m_Long < 0 || prop.m_Long > 255)
                    return 0;
                surface = static_cast<unsigned>(prop.m_Long);
            }
            server->GetPropGeneric("SpawnObject", &prop);
            // Do not create objects whose omitted branches change gameplay.
            if (pushable || (create.m_Flags & FLAG_GRAVITY) || prop.m_String[0])
                return 0;
        }
        if (create.m_Flags & FLAG_GRAVITY)
            return 0;
        if (!std::isfinite(hit_points) || !std::isfinite(mass) || std::abs(hit_points) > 1e6f || mass < 0 ||
            mass > 1e6f)
            return 0;
        SAFE_STRCPY(create.m_Filename, create.m_Name);
        create.m_SkinName[0] = 0;
        create.m_Flags |= FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE;
        if (box_physics)
            create.m_Flags |= FLAG_BOXPHYSICS;
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        if (!m_damage.Init(m_hObject) || !debris.Init(m_hObject))
            return 0;
        m_damage.SetHitPoints(hit_points);
        m_damage.m_fMass = mass;
        server->SetNextUpdate(m_hObject, 0);
        server->SetObjectUserFlags(m_hObject, (1u << 3) | (surface << 24));
    }
    if (message == MID_UPDATE && destructable && m_damage.IsDead())
    {
        server->RecordBrushDestruction(m_hObject, m_damage.last_damage.direction, m_damage.m_fLastDamageAmount,
                                       debris.amount);
        debris.Create(m_damage.last_damage.direction, m_damage.m_fLastDamageAmount);
        server->RemoveObject(m_hObject);
    }
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
DDWORD CDestructableBrush::ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read)
{
    const auto accepted = B2BaseClass::ObjectMessageFn(sender, message, read);
    if (message == OpenBlood2::damage_message && destructable && m_damage.IsDead() &&
        GetServerDE()->IsObjectAlive(m_hObject))
        GetServerDE()->SetNextUpdate(m_hObject, .001f);
    return accepted;
}
