#include "ObjectivesTrigger.h"
DDWORD ObjectivesTrigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && (reason == 1 || reason == 2))
    {
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric("ResourceNum", &prop) == DE_OK)
        {
            if (prop.m_Long < 0 || prop.m_Long > 65535 - 6000 + 1)
                return 0;
            objectives.resource = static_cast<DDWORD>(prop.m_Long);
        }
        if (g_pServerDE->GetPropGeneric("TitleToDisplay", &prop) == DE_OK)
            objectives.title = prop.m_String;
        if (g_pServerDE->GetPropGeneric("TextToDisplay", &prop) == DE_OK)
            objectives.text = prop.m_String;
        if (g_pServerDE->GetPropGeneric("Sound", &prop) == DE_OK)
            sound = prop.m_String;
    }
    return Trigger::EngineMessageFn(message, data, reason);
}
void ObjectivesTrigger::SendMessages()
{
    // Source requires BOTH authored strings, including when ResourceNum is set.
    if (objectives.title.empty() || objectives.text.empty())
        return;
    last_delivery = g_pServerDE->SendObjectives(objectives);
    if (last_delivery == DE_OK)
        ++deliveries;
    // Original PlaySoundLocal follows the broadcast; actual audio remains pending.
    if (!sound.empty())
        ++objective_sound_requests;
}
