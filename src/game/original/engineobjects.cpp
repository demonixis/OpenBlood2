#include "cpp_engineobjects_de.h"
#include "game/original/BuiltinModel.h"
#include "game/original/BuiltinSprite.h"

#include "engineobjects_de.h"
#include "serverobj_de.h"

DDWORD OpenBlood2::BuiltinModel::EngineMessageFn(DDWORD message, void* data, float reason)
{
    // Original mdl_MessageFn forces the type even for a non-normal incoming type.
    if (message == MID_PRECREATE)
        static_cast<ObjectCreateStruct*>(data)->m_ObjectType = OT_MODEL;
    return bc_EngineMessageFn(this, message, data, reason);
}

DDWORD OpenBlood2::BuiltinContainer::EngineMessageFn(DDWORD message, void* data, float reason)
{
    if (message == MID_PRECREATE)
    {
        auto* state = static_cast<ObjectCreateStruct*>(data);
        state->m_ObjectType = OT_CONTAINER;
        if (reason == PRECREATE_WORLDFILE)
        {
            GenericProp prop;
            long value = g_pServerDE->GetPropGeneric("ContainerCode", &prop) == DE_OK ? prop.m_Long : 0;
            // The source truncates to D_WORD; an out-of-range code has no defined meaning here.
            if (value < 0 || value > 0xFFFF)
            {
                g_pServerDE->RejectPrecreate("ContainerCode outside the original D_WORD range");
                return 0;
            }
            code = static_cast<D_WORD>(value);
            state->m_ContainerCode = code;
            g_pServerDE->GetPropString("Name", state->m_Filename, MAX_CS_FILENAME_LEN);
        }
    }
    return bc_EngineMessageFn(this, message, data, reason);
}

namespace
{
    // The source casts unchecked floats to DBYTE. Out-of-range values have no
    // defined result, so they reject creation instead of guessing a wrap.
    bool sprite_byte(float value, DBYTE& out)
    {
        if (!(value >= 0 && value < 256))
            return false;
        out = static_cast<DBYTE>(value);
        return true;
    }
} // namespace

DDWORD OpenBlood2::BuiltinSprite::EngineMessageFn(DDWORD message, void* data, float reason)
{
    GenericProp prop;
    switch (message)
    {
        case MID_PRECREATE:
        {
            auto* state = static_cast<ObjectCreateStruct*>(data);
            state->m_ObjectType = OT_SPRITE;
            // Original spr_MessageFn reads these for every creation reason and
            // ignores the getter results; the registered defaults are always present.
            if (g_pServerDE->GetPropGeneric("Color", &prop) != DE_OK || !sprite_byte(prop.m_Color.x, m_Color[0]) ||
                !sprite_byte(prop.m_Color.y, m_Color[1]) || !sprite_byte(prop.m_Color.z, m_Color[2]))
            {
                error = "Sprite Color is unavailable or outside 0..255";
                return 0;
            }
            if (g_pServerDE->GetPropGeneric("Scale", &prop) != DE_OK)
            {
                error = "Sprite Scale is unavailable";
                return 0;
            }
            VEC_COPY(state->m_Scale, prop.m_Vec);
            if (g_pServerDE->GetPropGeneric("Alpha", &prop) != DE_OK || !sprite_byte(prop.m_Float * 255.0f, m_Color[3]))
            {
                error = "Sprite Alpha is unavailable or outside 0..1";
                return 0;
            }
            break;
        }
        case MID_INITIALUPDATE:
        {
            // Source quirk: blue is taken from the green byte.
            const DVector color{m_Color[0] / 255.0f, m_Color[1] / 255.0f, m_Color[1] / 255.0f};
            color_applied = g_pServerDE->SetObjectColor(m_hObject, color, m_Color[3] / 255.0f);
            if (!color_applied)
                error = "Sprite SetObjectColor failed";
            break;
        }
    }
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
