#include "PolyGrid.h"
using namespace OpenBlood2;
bool PolyGrid::Setup(const PolyGridSettings& value)
{
    if (effect)
    {
        error = "PolyGrid already published";
        return false;
    }
    settings = value;
    if (!valid_polygrid(settings))
    {
        error = "Invalid PolyGrid setup";
        return false;
    }
    if (g_pServerDE->SetObjectDims(m_hObject, &settings.dims) != DE_OK)
    {
        error = "PolyGrid dimensions unavailable";
        return false;
    }
    effect = g_pServerDE->PublishPolyGrid(m_hObject, settings, error);
    return effect != 0;
}
DDWORD PolyGrid::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && data)
    {
        auto* state = static_cast<ObjectCreateStruct*>(data);
        state->m_Flags = FLAG_VISIBLE;
        // Runtime surfaces receive their parameters through Setup after construction.
        if (reason != PRECREATE_NORMAL)
        {
            setup_ = g_pServerDE->GetPropVector("Dims", &settings.dims) == DE_OK;
            g_pServerDE->GetPropVector("Color1", &settings.color1);
            g_pServerDE->GetPropVector("Color2", &settings.color2);
            const char* names[] = {"XScaleMin",      "YScaleMin", "XScaleMax", "YScaleMax", "XScaleDuration",
                                   "YScaleDuration", "XPan",      "YPan",      "Alpha"};
            float* values[] = {&settings.scale_min[0], &settings.scale_min[1], &settings.scale_max[0],
                               &settings.scale_max[1], &settings.duration[0],  &settings.duration[1],
                               &settings.pan[0],       &settings.pan[1],       &settings.alpha};
            for (unsigned i = 0; i < 9; ++i)
                g_pServerDE->GetPropReal(names[i], values[i]);
            GenericProp p;
            if (g_pServerDE->GetPropGeneric("SpriteSurfaceName", &p) == DE_OK)
                settings.sprite = p.m_String;
            const char* integers[] = {"NumPolies", "PlasmaType", "Ring1Rate", "Ring2Rate", "Ring3Rate", "Ring4Rate"};
            unsigned* destinations[] = {&settings.polies,   &settings.plasma,   &settings.rates[0],
                                        &settings.rates[1], &settings.rates[2], &settings.rates[3]};
            for (unsigned i = 0; i < 6; ++i)
                if (g_pServerDE->GetPropGeneric(integers[i], &p) == DE_OK)
                {
                    if (p.m_Long < 0)
                        return 0;
                    *destinations[i] = unsigned(p.m_Long);
                }
            if (setup_ && !valid_polygrid(settings))
                return 0;
        }
    }
    if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME)
    {
        g_pServerDE->SetObjectUserFlags(m_hObject, 1u);
        if (setup_)
            Setup(settings);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
