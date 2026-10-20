#include "ObjectivesTrigger.h"
#include "SaveFields.h"
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
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
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
void ObjectivesTrigger::Save(HMESSAGEWRITE h)
{
    // Original ObjectivesTrigger.cpp:220-228. The original never saves m_hstrSound: a restored
    // trigger has no objective sound (preserved loss).
    namespace F = OpenBlood2::save_fields;
    g_pServerDE->WriteToMessageDWord(h, objectives.resource);
    F::write_string(h, objectives.title);
    F::write_string(h, objectives.text);
}
void ObjectivesTrigger::Load(HMESSAGEREAD h)
{
    // Original ObjectivesTrigger.cpp:239-247, with ReadProp's resource range.
    namespace F = OpenBlood2::save_fields;
    const DDWORD resource = g_pServerDE->ReadFromMessageDWord(h);
    std::string title, text;
    bool ok = F::read_string(h, title);
    ok = F::read_string(h, text) && ok;
    if (!ok)
        return;
    if (resource > 65535 - 6000 + 1)
    {
        g_pServerDE->RejectPayload(h, "saved objectives resource is outside the string table");
        return;
    }
    objectives.resource = resource;
    objectives.title = std::move(title);
    objectives.text = std::move(text);
}
