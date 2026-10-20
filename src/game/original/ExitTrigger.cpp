#include "ExitTrigger.h"
#include "SaveFields.h"

DDWORD ExitTrigger::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE && (reason == 1 || reason == 2))
    {
        GenericProp prop;
        if (g_pServerDE->GetPropGeneric("NextWorld", &prop) == DE_OK)
            exit.next_world = prop.m_String;
        if (g_pServerDE->GetPropGeneric("StartPointName", &prop) == DE_OK)
            start_point = prop.m_String;
        DBOOL episode = DFALSE, subworld = DFALSE;
        // Source quirk: the class declares EndEpisode, but ReadProp asks for
        // EndOfEpisode. Preserve the actual read without inventing an alias.
        g_pServerDE->GetPropBool("EndOfEpisode", &episode);
        g_pServerDE->GetPropBool("SubWorld", &subworld);
        exit.type = episode    ? OpenBlood2::WorldExitType::Episode
                    : subworld ? OpenBlood2::WorldExitType::Subworld
                               : OpenBlood2::WorldExitType::World;
    }
    if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    return Trigger::EngineMessageFn(message, data, reason);
}
void ExitTrigger::SendMessages()
{
    ++attempts;
    // The server copies a nonempty start name before the null-client broadcast.
    last_delivery = g_pServerDE->SendWorldExit(exit, start_point);
    if (last_delivery == DE_OK)
        ++deliveries; // Queued delivery, never proof of a successful transition.
}
void ExitTrigger::Save(HMESSAGEWRITE h)
{
    // Original ExitTrigger.cpp:213-221, before the Trigger block.
    namespace F = OpenBlood2::save_fields;
    F::write_string(h, exit.next_world);
    F::write_string(h, start_point);
    g_pServerDE->WriteToMessageByte(h, static_cast<DBYTE>(exit.type));
}
void ExitTrigger::Load(HMESSAGEREAD h)
{
    // Original ExitTrigger.cpp:232-240. The exit type is one of the three ENDWORLD values.
    namespace F = OpenBlood2::save_fields;
    std::string world, start;
    bool ok = F::read_string(h, world);
    ok = F::read_string(h, start) && ok;
    const DBYTE type = g_pServerDE->ReadFromMessageByte(h);
    if (!ok)
        return;
    if (type > DBYTE(OpenBlood2::WorldExitType::Episode))
    {
        g_pServerDE->RejectPayload(h, "saved exit type is not an ENDWORLD value");
        return;
    }
    exit.next_world = std::move(world);
    start_point = std::move(start);
    exit.type = static_cast<OpenBlood2::WorldExitType>(type);
}
