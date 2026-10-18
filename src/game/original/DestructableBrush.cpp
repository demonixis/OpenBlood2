// Selected original brush lifecycle; bounded BoxPhysics body services support Pushable/Gravity.
#include "DestructableBrush.h"
#include "WorldModelDebris.h"
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
        {
            if (accepted && data && reason != 3)
                server->RejectPrecreate("invalid debris Scale");
            return 0;
        }
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
                {
                    server->RejectPrecreate("SurfaceType out of range");
                    return 0;
                }
                surface = static_cast<unsigned>(prop.m_Long);
            }
            // Reject unsupported spawn rules before admitting this brush.
            if (!m_damage.spawn_valid)
            {
                server->RejectPrecreate("SpawnObject not accepted: " + m_damage.spawn_object);
                return 0;
            }
        }

        if (!std::isfinite(hit_points) || !std::isfinite(mass) || std::abs(hit_points) > 1e6f || mass < 0 ||
            mass > 1e6f)
        {
            server->RejectPrecreate("invalid HitPoints or ObjectMass");
            return 0;
        }
        SAFE_STRCPY(create.m_Filename, create.m_Name);
        create.m_SkinName[0] = 0;
        create.m_Flags |= FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE;
        if (box_physics)
            create.m_Flags |= FLAG_BOXPHYSICS;
        if (pushable)
        {
            allow_marks = false;
            create.m_Flags |= FLAG_GRAVITY;
        }
        if ((create.m_Flags & FLAG_GRAVITY) &&
            ((!dynamic_debris && !(create.m_Flags & FLAG_BOXPHYSICS)) || (create.m_Flags & FLAG_GOTHRUWORLD)))
        {
            // Polygon dynamics / world bypass are not this body solver.
            server->RejectPrecreate(create.m_Flags & FLAG_GOTHRUWORLD ? "gravity with GOTHRUWORLD"
                                                                      : "gravity without BoxPhysics (polygon dynamics)");
            return 0;
        }
        return 1;
    }
    if (message == MID_INITIALUPDATE)
    {
        if (!m_damage.Init(m_hObject) || !debris.Init(m_hObject))
            return 0;
        m_damage.SetHitPoints(hit_points);
        m_damage.m_fMass = mass;
        m_damage.apply_damage_physics = pushable;
        server->SetBlockingPriority(m_hObject, pushable ? 40 : 100);
        if (dynamic_debris || (server->GetObjectFlags(m_hObject) & FLAG_GRAVITY))
        {
            // Source SetMass clamps its local parameter, but uses the unclamped
            // member in this formula. Preserve friction above mass 500 too.
            if (!server->ConfigureWorldBody(m_hObject, {mass, 5 + mass * 10 / 500, pushable, dynamic_debris}))
            {
                server->RemoveObject(m_hObject);
                return 0;
            }
        }
        server->SetNextUpdate(m_hObject, 0);
        server->SetObjectUserFlags(m_hObject, (1u << 3) | (pushable ? (1u << 4) : 0) | (surface << 24));
    }
    if (message == MID_UPDATE && destructable && m_damage.IsDead())
    {
        debris.Create(m_damage.last_damage.direction, m_damage.m_fLastDamageAmount);
        CreateWorldModelDebris();
        server->RecordBrushDestruction(m_hObject, m_damage.last_damage.direction, m_damage.m_fLastDamageAmount,
                                       debris.amount, world_debris_started, world_debris_failed);
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

void CDestructableBrush::CreateWorldModelDebris()
{
    auto* server = GetServerDE();
    const auto* raw = server->GetObjectName(m_hObject);
    const std::string name = raw ? raw : "";
    if (name.empty())
        return;
    // Source contiguous numbering, all same-name matches, stop on the first
    // empty list (not on a wrong-class match). Own the name across callbacks.
    for (unsigned n = 0; n < 65536; ++n)
    {
        const auto key = name + "debris" + std::to_string(n);
        if (key.size() > 100)
            return; // No legal object name can match.
        auto* list = server->FindNamedObjects(key.c_str());
        if (!list)
        {
            ++world_debris_failed;
            return;
        }
        const bool found = list->m_pFirstLink != nullptr;
        for (auto* link = list->m_pFirstLink; link; link = link->m_pNext)
            if (server->IsObjectAlive(link->m_hObject))
                if (auto* piece = dynamic_cast<WorldModelDebris*>(server->HandleToObject(link->m_hObject)))
                {
                    if (piece->Start())
                        ++world_debris_started;
                    else
                        ++world_debris_failed;
                }
        server->RelinquishList(list); // Also release the source's leaked terminal empty list.
        if (!found)
            return;
    }
    ++world_debris_failed; // Bounded traversal; never report truncated work as complete.
}
