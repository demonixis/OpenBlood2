// Original ObjectDLL/ConversationTrigger.cpp; owned values and typed broadcast.
#include "ConversationTrigger.h"
#include "SaveFields.h"
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
    if (m == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (m == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
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
void ConversationTrigger::Save(HMESSAGEWRITE h)
{
    // Original ConversationTrigger.cpp:315-330.
    namespace F = OpenBlood2::save_fields;
    auto* s = g_pServerDE;
    for (const auto& wave : waves)
        F::write_string(h, wave);
    s->WriteToMessageByte(h, character);
    s->WriteToMessageByte(h, static_cast<DBYTE>(count));
    s->WriteToMessageByte(h, static_cast<DBYTE>(current));
    s->WriteToMessageByte(h, repeat_last);
    s->WriteToMessageByte(h, loop);
}
void ConversationTrigger::Load(HMESSAGEREAD h)
{
    // Original ConversationTrigger.cpp:341-356, with ReadProp's bounds: at most eight messages,
    // 199-byte wave names, and a cursor at most one past the last message.
    namespace F = OpenBlood2::save_fields;
    auto* s = g_pServerDE;
    std::array<std::string, 8> saved;
    bool ok = true;
    for (auto& wave : saved)
        ok = F::read_string(h, wave, 199) && ok;
    const DBYTE type = s->ReadFromMessageByte(h), messages = s->ReadFromMessageByte(h),
                cursor = s->ReadFromMessageByte(h);
    bool repeat = false, loops = false;
    ok = F::read_bool(h, repeat) && ok;
    ok = F::read_bool(h, loops) && ok;
    if (!ok)
        return;
    if (messages > saved.size() || cursor > messages)
    {
        s->RejectPayload(h, "saved conversation cursor is outside its eight messages");
        return;
    }
    waves = std::move(saved);
    character = type;
    count = messages;
    current = cursor;
    repeat_last = repeat;
    loop = loops;
}
