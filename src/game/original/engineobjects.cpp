#include "cpp_engineobjects_de.h"
#include "game/original/BuiltinModel.h"

#include "engineobjects_de.h"
#include "serverobj_de.h"

DDWORD OpenBlood2::BuiltinModel::EngineMessageFn(DDWORD message, void* data, float reason)
{
    // Original mdl_MessageFn forces the type even for a non-normal incoming type.
    if (message == MID_PRECREATE)
        static_cast<ObjectCreateStruct*>(data)->m_ObjectType = OT_MODEL;
    return bc_EngineMessageFn(this, message, data, reason);
}

void bc_AddAggregate(LPBASECLASS pObject, LPAGGREGATE pAggregate)
{
    pAggregate->m_pNextAggregate = pObject->m_pFirstAggregate;
    pObject->m_pFirstAggregate = pAggregate;
}

DDWORD bc_EngineMessageFn(LPBASECLASS pObject, DDWORD messageID, void* pData, float fData)
{
    LPAGGREGATE pAggregate;
    ObjectCreateStruct* pStruct;
    GenericProp genProp;

    // Handle ReadProp.
    switch (messageID)
    {
        case MID_PRECREATE:
        {
            // Get the props.
            if (fData == 1.0f || fData == 2.0f)
            {
                pStruct = (ObjectCreateStruct*)pData;

                if (g_pServerDE->GetPropGeneric("Name", &genProp) == DE_OK)
                {
                    SAFE_STRCPY(pStruct->m_Name, genProp.m_String);
                    pStruct->m_Name[MAX_CS_FILENAME_LEN] = '\0';
                }
                if (g_pServerDE->GetPropGeneric("Pos", &genProp) == DE_OK)
                {
                    VEC_COPY(pStruct->m_Pos, genProp.m_Vec);
                }
                if (g_pServerDE->GetPropGeneric("Rotation", &genProp) == DE_OK)
                {
                    ROT_COPY(pStruct->m_Rotation, genProp.m_Rotation);
                }
                if (g_pServerDE->GetPropGeneric("Flags", &genProp) == DE_OK)
                {
                    pStruct->m_Flags = genProp.m_Long;
                }
                if (g_pServerDE->GetPropGeneric("Visible", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_VISIBLE;
                    else
                        pStruct->m_Flags &= ~FLAG_VISIBLE;
                }
                if (g_pServerDE->GetPropGeneric("Shadow", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_SHADOW;
                    else
                        pStruct->m_Flags &= ~FLAG_SHADOW;
                }
                if (g_pServerDE->GetPropGeneric("RotateableSprite", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_ROTATEABLESPRITE;
                    else
                        pStruct->m_Flags &= ~FLAG_ROTATEABLESPRITE;
                }
                if (g_pServerDE->GetPropGeneric("Chromakey", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_SPRITECHROMAKEY;
                    else
                        pStruct->m_Flags &= ~FLAG_SPRITECHROMAKEY;
                }
                if (g_pServerDE->GetPropGeneric("Solid", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_SOLID;
                    else
                        pStruct->m_Flags &= ~FLAG_SOLID;
                }
                if (g_pServerDE->GetPropGeneric("Gravity", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_GRAVITY;
                    else
                        pStruct->m_Flags &= ~FLAG_GRAVITY;
                }
                if (g_pServerDE->GetPropGeneric("TouchNotify", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_TOUCH_NOTIFY;
                    else
                        pStruct->m_Flags &= ~FLAG_TOUCH_NOTIFY;
                }
                if (g_pServerDE->GetPropGeneric("Rayhit", &genProp) == DE_OK)
                {
                    if (genProp.m_Bool)
                        pStruct->m_Flags |= FLAG_RAYHIT;
                    else
                        pStruct->m_Flags &= ~FLAG_RAYHIT;
                }
                if (g_pServerDE->GetPropGeneric("Filename", &genProp) == DE_OK)
                {
                    SAFE_STRCPY(pStruct->m_Filename, genProp.m_String);
                    pStruct->m_Filename[MAX_CS_FILENAME_LEN] = '\0';
                }
                if (g_pServerDE->GetPropGeneric("Skin", &genProp) == DE_OK)
                {
                    SAFE_STRCPY(pStruct->m_SkinName, genProp.m_String);
                    pStruct->m_SkinName[MAX_CS_FILENAME_LEN] = '\0';
                }
                break;
            }
        }
    }

    // Call the aggregates.
    pAggregate = pObject->m_pFirstAggregate;
    while (pAggregate)
    {
        pAggregate->m_EngineMessageFn(pObject, pAggregate, messageID, pData, fData);
        pAggregate = pAggregate->m_pNextAggregate;
    }

    // Default return is 1.
    return 1;
}

DDWORD bc_ObjectMessageFn(LPBASECLASS pObject, HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead)
{
    LPAGGREGATE pAggregate;

    // Call the aggregates.
    pAggregate = pObject->m_pFirstAggregate;
    while (pAggregate)
    {
        pAggregate->m_ObjectMessageFn(pObject, pAggregate, hSender, messageID, hRead);
        pAggregate = pAggregate->m_pNextAggregate;
    }

    return 1;
}
