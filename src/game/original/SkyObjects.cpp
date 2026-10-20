#include "SkyObjects.h"
#include "engine/collision.h"
using namespace OpenBlood2;
DDWORD SkyObject::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data || reason == 3)
            return 0; // CF_ALWAYSLOAD: never saved; RestoreObjects refuses such a record by class name.
        auto& create = *static_cast<ObjectCreateStruct*>(data);
        create.m_ObjectType = pointer ? OT_NORMAL : OT_WORLDMODEL;
        if (reason == 1)
        {
            GenericProp p;
            if (g_pServerDE->GetPropGeneric("SkyDims", &p) == DE_OK)
                dimensions = p.m_Vec;
            if (g_pServerDE->GetPropGeneric("Index", &p) == DE_OK)
                index = p.m_Long;
            g_pServerDE->GetPropReal("InnerPercentX", &inner.x);
            g_pServerDE->GetPropReal("InnerPercentY", &inner.y);
            g_pServerDE->GetPropReal("InnerPercentZ", &inner.z);
            g_pServerDE->GetPropString("Name", create.m_Filename, MAX_CS_FILENAME_LEN);
            if (pointer && g_pServerDE->GetPropGeneric("SkyObjectName", &p) == DE_OK)
                target = p.m_String;
        }
        if (index < 0 || index >= 30 || !physics_vector(dimensions) || !physics_vector(inner) || dimensions.x < 0 ||
            dimensions.y < 0 || dimensions.z < 0 || inner.x < 0 || inner.y < 0 || inner.z < 0)
            return 0;
    }
    else if (message == MID_INITIALUPDATE)
    {
        if (dimensions.x != 0 && dimensions.y != 0 && dimensions.z != 0)
        {
            DVector p;
            g_pServerDE->GetObjectPos(m_hObject, &p);
            const DVector v{dimensions.x * inner.x, dimensions.y * inner.y, dimensions.z * inner.z};
            const SkyDefinition d{vector_subtract(p, dimensions), vector_add(p, dimensions), vector_subtract(p, v),
                                  vector_add(p, v)};
            if (!g_pServerDE->SetSkyDefinition(d))
                error = "Rejected sky definition";
        }
        if (pointer)
            g_pServerDE->SetNextUpdate(m_hObject, .001f);
        else
        {
            g_pServerDE->SetObjectFlags(m_hObject, g_pServerDE->GetObjectFlags(m_hObject) | FLAG_SKYOBJECT |
                                                       FLAG_FORCEOPTIMIZEOBJECT);
            if (!g_pServerDE->AddSkyObject(m_hObject, unsigned(index)))
                error = "Rejected sky world model";
        }
        if (!error.empty())
            g_pServerDE->RemoveObject(m_hObject);
    }
    else if (message == MID_UPDATE && pointer)
    {
        auto* list = g_pServerDE->FindNamedObjects(target.c_str());
        if (list && list->m_pFirstLink)
        {
            auto h = list->m_pFirstLink->m_hObject;
            if (g_pServerDE->AddSkyObject(h, unsigned(index)))
                g_pServerDE->SetObjectFlags(h,
                                            g_pServerDE->GetObjectFlags(h) | FLAG_SKYOBJECT | FLAG_FORCEOPTIMIZEOBJECT);
            else
                error = "Rejected sky pointer target: " + target;
        }
        else
            error = "Sky pointer target not found: " + target;
        // Source leaked empty lists; release every owned snapshot, including an empty one.
        if (list)
            g_pServerDE->RelinquishList(list);
        if (!error.empty())
            g_pServerDE->ReportSkyFailure(error);
        g_pServerDE->RemoveObject(m_hObject);
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
