// Original ObjectDLL/ConversationTrigger.cpp; owned values and typed broadcast.
#include "ConversationTrigger.h"
DDWORD ConversationTrigger::EngineMessageFn(DDWORD m, void* data, DFLOAT reason)
{
    if (m == MID_PRECREATE && (reason == 1 || reason == 2))
    {
        for (unsigned i = 0; i < waves.size(); ++i)
        {
            GenericProp p;
            const auto name = std::string("WaveFile") + (i ? std::to_string(i + 1) : "");
            if (g_pServerDE->GetPropGeneric(name.c_str(), &p) == DE_OK)
            {
                if (!p.m_String[0])
                    break;
                waves[i] = p.m_String;
            }
            ++count; // Original increments even when property lookup fails.
        }
        DBOOL value = 0;
        if (g_pServerDE->GetPropBool("RepeatLastMessage", &value) == DE_OK)
            repeat_last = value != 0;
        if (g_pServerDE->GetPropBool("LoopToFirstMessage", &value) == DE_OK)
            loop = value != 0;
        GenericProp p;
        if (g_pServerDE->GetPropGeneric("CharacterType", &p) == DE_OK)
            character = static_cast<DBYTE>(static_cast<std::uint32_t>(p.m_Long));
    }
    return Trigger::EngineMessageFn(m, data, reason);
}
void ConversationTrigger::SendMessages()
{
    if (current >= count)
        return;
    ++attempts;
    const char* text = OpenBlood2::conversation_text(waves[current]);
    if (!text)
    {
        ++unmapped;
        error = "Conversation filename has no original text mapping";
        return;
    }
    last_delivery = g_pServerDE->SendConversation({waves[current], text, character});
    if (last_delivery == DE_OK)
    {
        ++queued;
        error.clear();
    }
    else
        error = "Conversation broadcast unavailable or queue full";
    // Source advances after the send attempt, independently of client busy/playback.
    if (++current == count)
    {
        if (repeat_last)
            --current;
        else if (loop)
            current = 0;
    }
}
