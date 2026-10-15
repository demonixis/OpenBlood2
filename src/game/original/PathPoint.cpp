// ----------------------------------------------------------------------- //
//
// MODULE  : PathPoint.cpp
//
// PURPOSE : PathPoint object (will be placed in the world with Dedit
//           but removed by the engine (with locations and special actions
//           stored in the PATHMGR. (Thanks to Kevin Stephens, who wrote this)
//
// CREATED : 10/17/97
//
// ----------------------------------------------------------------------- //

#include "PathPoint.h"
#include <cstring>
#include <stdio.h>

// Registration is in game/registration.cpp.

void BPrint(char*);

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::PathPoint()
//
//	PURPOSE:	Initialize object
//
// ----------------------------------------------------------------------- //

PathPoint::PathPoint() : B2BaseClass(OT_MODEL)
{
    m_bActive = DTRUE;
    m_hstrActionTarget = DNULL;
    m_hstrActionMessage = DNULL;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::~PathPoint()
//
//	PURPOSE:	Deallocate object
//
// ----------------------------------------------------------------------- //

PathPoint::~PathPoint()
{
    CServerDE* pServerDE = GetServerDE();
    if (!pServerDE)
        return;

    if (m_hstrActionTarget)
        pServerDE->FreeString(m_hstrActionTarget);
    if (m_hstrActionMessage)
        pServerDE->FreeString(m_hstrActionMessage);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::EngineMessageFn
//
//	PURPOSE:	Handle engine messages
//
// ----------------------------------------------------------------------- //

DDWORD PathPoint::EngineMessageFn(DDWORD messageID, void* pData, DFLOAT fData)
{
    switch (messageID)
    {
        case MID_UPDATE:
        {
            if (!Update((DVector*)pData))
            {
                CServerDE* pServerDE = GetServerDE();
                if (!pServerDE)
                    break;

                pServerDE->RemoveObject(m_hObject);
            }
        }
        break;

        case MID_TOUCHNOTIFY:
        {
            ObjectTouch((HOBJECT)pData);
        }
        break;

        case MID_PRECREATE:
        {
            DDWORD dwRet = B2BaseClass::EngineMessageFn(messageID, pData, fData);

            if (fData == 1.0f)
                ReadProp((ObjectCreateStruct*)pData);

            PostPropRead((ObjectCreateStruct*)pData);

            return dwRet;
        }
        break;

        case MID_INITIALUPDATE:
        {
            InitialUpdate((DVector*)pData);
        }
        break;

        default:
            break;
    }

    return B2BaseClass::EngineMessageFn(messageID, pData, fData);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PostPropRead()
//
//	PURPOSE:	Update properties
//
// ----------------------------------------------------------------------- //

void PathPoint::PostPropRead(ObjectCreateStruct* pStruct)
{
    if (!pStruct)
        return;

    const char* pFilename = "Models\\path1.abc";
    const char* pSkin = "Skins\\path1.dtx";
    SAFE_STRCPY(pStruct->m_Filename, pFilename);
    SAFE_STRCPY(pStruct->m_SkinName, pSkin);

    // Set the Update!
    pStruct->m_NextUpdate = 0.001f;

    if (std::strncmp(pStruct->m_Name, "SMELL", 5) == 0)
    {
        pStruct->m_Flags = FLAG_TOUCH_NOTIFY;
        //	    pStruct->m_Flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY;
        pStruct->m_NextUpdate = 0.0f;
    }
    else
    {
        if (g_pServerDE->GetVarValueFloat(g_pServerDE->GetGameConVar("DebugCutScene")) == 1.0f)
            pStruct->m_Flags = FLAG_VISIBLE | FLAG_TOUCH_NOTIFY;
        else
            pStruct->m_Flags = FLAG_TOUCH_NOTIFY;
    }
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::ReadProp
//
//	PURPOSE:	Set property value
//
// ----------------------------------------------------------------------- //

DBOOL PathPoint::ReadProp(ObjectCreateStruct* pStruct)
{
    CServerDE* pServerDE = GetServerDE();
    if (!pServerDE || !pStruct)
        return DFALSE;

    char buf[MAX_CS_FILENAME_LEN];

    buf[0] = '\0';
    pServerDE->GetPropString("ActionTarget", buf, MAX_CS_FILENAME_LEN);
    if (buf[0])
        m_hstrActionTarget = pServerDE->CreateString(buf);

    buf[0] = '\0';
    pServerDE->GetPropString("ActionMessage", buf, MAX_CS_FILENAME_LEN);
    if (buf[0])
        m_hstrActionMessage = pServerDE->CreateString(buf);

    return DTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::ObjectTouch
//
//	PURPOSE:	Handle object touch
//
// ----------------------------------------------------------------------- //

void PathPoint::ObjectTouch(HOBJECT)
{
    CServerDE* pServerDE = GetServerDE();
    if (!pServerDE)
        return;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::InitialUpdate
//
//	PURPOSE:	Initial update
//
// ----------------------------------------------------------------------- //

DBOOL PathPoint::InitialUpdate(DVector*)
{
    CServerDE* pServerDE = GetServerDE();
    if (!pServerDE)
        return DFALSE;

    m_bActive = DTRUE;
    pServerDE->SetNextUpdate(m_hObject, 0.001f);

    if (std::strncmp(pServerDE->GetObjectName(m_hObject), "SMELL", 5) == 0)
    {
        // Secs since server started
        m_fStartTime = pServerDE->GetTime();
    }

    // Set the dims...
    DVector vDims;
    VEC_SET(vDims, 20, 20, 20);
    pServerDE->SetObjectDims(m_hObject, &vDims);

    // If we are a SMELL, set next update to expire time.
    DFLOAT fUpdate = 0.0f;
    if (std::strncmp(pServerDE->GetObjectName(m_hObject), "SMELL", 5) == 0)
    {
        fUpdate = 20.0f;
    }
    pServerDE->SetNextUpdate(m_hObject, fUpdate);

    return DTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathPoint::Update
//
//	PURPOSE:	Handle Update
//
// ----------------------------------------------------------------------- //

DBOOL PathPoint::Update(DVector*)
{
    return DFALSE;
}
