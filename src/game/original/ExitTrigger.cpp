#include "ExitTrigger.h"

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
