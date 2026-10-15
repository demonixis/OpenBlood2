//----------------------------------------------------------
//
// MODULE  : WorldProperties.cpp
//
// PURPOSE : Blood 2 world prop class
//
// CREATED : 3/23/98
//
//----------------------------------------------------------

#include "WorldProperties.h"
#include "cpp_engineobjects_de.h"
#include <stdio.h>

// Class declaration and registration: WorldProperties.h / game/registration.cpp.

static char szWorldName[] = "WorldName";
static char szAuthor[] = "Author";
static char szStartCutscene[] = "StartCutscene";
static char szEndCutscene[] = "EndCutscene";
static char szMusicDirectory[] = "MusicDirectory";
static char szInstrumentFiles[] = "InstrumentFiles";
static char szAmbientList[] = "AmbientList";
static char szCruisingList[] = "CruisingList";
static char szHarddrivingList[] = "HarddrivingList";
static char szCDTrack[] = "CDTrack";
static char szFogEnable[] = "FogEnable";
static char szFogR[] = "FogR";
static char szFogG[] = "FogG";
static char szFogB[] = "FogB";
static char szFogNearZ[] = "FogNearZ";
static char szFogFarZ[] = "FogFarZ";
static char szSkyFog[] = "SkyFog";
static char szSkyFogNearZ[] = "SkyFogNearZ";
static char szSkyFogFarZ[] = "SkyFogFarZ";
static char szFarZ[] = "FarZ";
static char szModelAddR[] = "ModelAddR";
static char szModelAddG[] = "ModelAddG";
static char szModelAddB[] = "ModelAddB";
static char szModelDirAddR[] = "ModelDirAddR";
static char szModelDirAddG[] = "ModelDirAddG";
static char szModelDirAddB[] = "ModelDirAddB";
static char szPanSky[] = "PanSky";
static char szPanSkyTexture[] = "PanSkyTexture";
static char szPanSkyOffsetX[] = "PanSkyOffsetX";
static char szPanSkyOffsetZ[] = "PanSkyOffsetZ";
static char szPanSkyScaleX[] = "PanSkyScaleX";
static char szPanSkyScaleZ[] = "PanSkyScaleZ";
static char szEnvironmentMap[] = "EnvironmentMap";
// static char szLightScaleR[] = "LightScaleR";
// static char szLightScaleG[] = "LightScaleG";
// static char szLightScaleB[] = "LightScaleB";
static char szSoftSky[] = "SoftSky";

WorldProperties::~WorldProperties()
{
    g_pServerDE->SetGameConVar(szWorldName, "");
    g_pServerDE->SetGameConVar(szAuthor, "");
    g_pServerDE->SetGameConVar(szStartCutscene, "");
    g_pServerDE->SetGameConVar(szEndCutscene, "");
    g_pServerDE->SetGameConVar(szMusicDirectory, "");
    g_pServerDE->SetGameConVar(szInstrumentFiles, "");
    g_pServerDE->SetGameConVar(szAmbientList, "");
    g_pServerDE->SetGameConVar(szCruisingList, "");
    g_pServerDE->SetGameConVar(szHarddrivingList, "");
    g_pServerDE->SetGameConVar(szCDTrack, "");
    g_pServerDE->SetGameConVar(szFogEnable, "");
    g_pServerDE->SetGameConVar(szFogR, "");
    g_pServerDE->SetGameConVar(szFogG, "");
    g_pServerDE->SetGameConVar(szFogB, "");
    g_pServerDE->SetGameConVar(szFogNearZ, "");
    g_pServerDE->SetGameConVar(szFogFarZ, "");
    g_pServerDE->SetGameConVar(szSkyFog, "");
    g_pServerDE->SetGameConVar(szSkyFogNearZ, "");
    g_pServerDE->SetGameConVar(szSkyFogFarZ, "");
    g_pServerDE->SetGameConVar(szFarZ, "");
    g_pServerDE->SetGameConVar(szModelAddR, "");
    g_pServerDE->SetGameConVar(szModelAddG, "");
    g_pServerDE->SetGameConVar(szModelAddB, "");
    g_pServerDE->SetGameConVar(szModelDirAddR, "");
    g_pServerDE->SetGameConVar(szModelDirAddG, "");
    g_pServerDE->SetGameConVar(szModelDirAddB, "");
    g_pServerDE->SetGameConVar(szPanSky, "");
    g_pServerDE->SetGameConVar(szPanSkyTexture, "");
    g_pServerDE->SetGameConVar(szPanSkyOffsetX, "");
    g_pServerDE->SetGameConVar(szPanSkyOffsetZ, "");
    g_pServerDE->SetGameConVar(szPanSkyScaleX, "");
    g_pServerDE->SetGameConVar(szPanSkyScaleZ, "");
    g_pServerDE->SetGameConVar(szEnvironmentMap, "");
    //	g_pServerDE->SetGameConVar( szLightScaleR, "" );
    //	g_pServerDE->SetGameConVar( szLightScaleG, "" );
    //	g_pServerDE->SetGameConVar( szLightScaleB, "" );
    g_pServerDE->SetGameConVar(szSoftSky, "");
}

DDWORD WorldProperties::EngineMessageFn(DDWORD messageID, void* pData, DFLOAT lData)
{
    switch (messageID)
    {
        case MID_PRECREATE:
        {
            char buf[501];

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szWorldName, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szWorldName, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szAuthor, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szAuthor, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szStartCutscene, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szStartCutscene, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szEndCutscene, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szEndCutscene, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szMusicDirectory, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szMusicDirectory, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szInstrumentFiles, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szInstrumentFiles, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szAmbientList, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szAmbientList, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szCruisingList, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szCruisingList, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szHarddrivingList, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szHarddrivingList, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szCDTrack, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szCDTrack, buf);
            }

            DBOOL bVal;
            if (g_pServerDE->GetPropBool(szFogEnable, &bVal) == DE_OK)
            {
                sprintf(buf, "%d", bVal);
                g_pServerDE->SetGameConVar(szFogEnable, buf);
            }

            DFLOAT fVal;
            if (g_pServerDE->GetPropReal(szFogNearZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szFogNearZ, buf);
            }

            if (g_pServerDE->GetPropReal(szFogFarZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szFogFarZ, buf);
            }

            DVector vVec;
            if (g_pServerDE->GetPropVector("FogColor", &vVec) == DE_OK)
            {
                sprintf(buf, "%f", vVec.x);
                g_pServerDE->SetGameConVar(szFogR, buf);

                sprintf(buf, "%f", vVec.y);
                g_pServerDE->SetGameConVar(szFogG, buf);

                sprintf(buf, "%f", vVec.z);
                g_pServerDE->SetGameConVar(szFogB, buf);
            }

            if (g_pServerDE->GetPropReal(szFarZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szFarZ, buf);
            }

            if (g_pServerDE->GetPropBool(szSkyFog, &bVal) == DE_OK)
            {
                sprintf(buf, "%d", bVal);
                g_pServerDE->SetGameConVar(szSkyFog, buf);
            }

            if (g_pServerDE->GetPropReal(szSkyFogNearZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szSkyFogNearZ, buf);
            }

            if (g_pServerDE->GetPropReal(szSkyFogFarZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szSkyFogFarZ, buf);
            }

            if (g_pServerDE->GetPropVector("ModelAdd", &vVec) == DE_OK)
            {
                sprintf(buf, "%f", vVec.x);
                g_pServerDE->SetGameConVar(szModelAddR, buf);

                sprintf(buf, "%f", vVec.y);
                g_pServerDE->SetGameConVar(szModelAddG, buf);

                sprintf(buf, "%f", vVec.z);
                g_pServerDE->SetGameConVar(szModelAddB, buf);
            }

            if (g_pServerDE->GetPropVector("ModelDirAdd", &vVec) == DE_OK)
            {
                sprintf(buf, "%f", vVec.x);
                g_pServerDE->SetGameConVar(szModelDirAddR, buf);

                sprintf(buf, "%f", vVec.y);
                g_pServerDE->SetGameConVar(szModelDirAddG, buf);

                sprintf(buf, "%f", vVec.z);
                g_pServerDE->SetGameConVar(szModelDirAddB, buf);
            }

            if (g_pServerDE->GetPropBool(szPanSky, &bVal) == DE_OK)
            {
                sprintf(buf, "%d", bVal);
                g_pServerDE->SetGameConVar(szPanSky, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szEnvironmentMap, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szEnvironmentMap, buf);
            }

            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szPanSkyTexture, buf, 500) == DE_OK)
            {
                if (buf[0])
                    g_pServerDE->SetGameConVar(szPanSkyTexture, buf);
            }

            if (g_pServerDE->GetPropReal(szPanSkyOffsetX, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szPanSkyOffsetX, buf);
            }

            if (g_pServerDE->GetPropReal(szPanSkyOffsetZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szPanSkyOffsetZ, buf);
            }

            if (g_pServerDE->GetPropReal(szPanSkyScaleX, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szPanSkyScaleX, buf);
            }

            if (g_pServerDE->GetPropReal(szPanSkyScaleZ, &fVal) == DE_OK)
            {
                sprintf(buf, "%f", fVal);
                g_pServerDE->SetGameConVar(szPanSkyScaleZ, buf);
            }

            if (g_pServerDE->GetPropVector("Gravity", &vVec) == DE_OK)
            {
                g_pServerDE->SetGlobalForce(&vVec);
            }

            /*			if (g_pServerDE->GetPropVector("LightScale", &vVec) == DE_OK)
                        {
                            sprintf(buf, "%f", vVec.x);
                            g_pServerDE->SetGameConVar( szLightScaleR, buf );

                            sprintf(buf, "%f", vVec.y);
                            g_pServerDE->SetGameConVar( szLightScaleG, buf );

                            sprintf(buf, "%f", vVec.z);
                            g_pServerDE->SetGameConVar( szLightScaleB, buf );
                        }
            */
            buf[0] = '\0';
            if (g_pServerDE->GetPropString(szSoftSky, buf, 500) == DE_OK && buf[0])
                g_pServerDE->SetGameConVar(szSoftSky, buf);
            else
                g_pServerDE->SetGameConVar(szSoftSky, "textures\\environmentmaps\\clouds\\clouds");

            break;
        }
        case MID_INITIALUPDATE:
        {
            g_pServerDE->SetObjectState(m_hObject, OBJSTATE_INACTIVE);
            break;
        }

        default:
            break;
    }

    return B2BaseClass::EngineMessageFn(messageID, pData, lData);
}
