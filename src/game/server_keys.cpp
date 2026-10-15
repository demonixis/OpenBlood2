#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool same_name(std::string_view a, std::string_view b)
    {
        return !CaseLess{}(a, b) && !CaseLess{}(b, a);
    }
    bool ServerAdapter::IsObjectAlive(HOBJECT h)
    {
        trace_service("IsObjectAlive");
        const auto* r = objects_.get(h);
        return r && r->life == Life::Alive;
    }
    DRESULT ServerAdapter::WriteKeyMessage(HMESSAGEWRITE h, const KeyMessage& value)
    {
        trace_service("WriteKeyMessage");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        auto valid = [](const std::string& s) { return s.size() <= 255 && s.find('\0') == std::string::npos; };
        if (!m->routed ||
            (m->id != key_pickup_message && m->id != key_query_message && m->id != key_response_message &&
             m->id != picked_up_message) ||
            !m->bytes.empty() || m->damage || m->key || !IsObjectAlive(m->sender) || !valid(value.item.name) ||
            value.item.name.empty() || !valid(value.item.display_name) || !valid(value.item.icon) ||
            !valid(value.item.highlighted_icon))
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        m->key = value;
        return DE_OK;
    }
    bool ServerAdapter::ReadKeyMessage(HMESSAGEREAD h, KeyMessage& value)
    {
        trace_service("ReadKeyMessage");
        value = {};
        auto it = messages_.find(h);
        if (it == messages_.end())
            return false;
        auto& m = it->second;
        if (!m.borrowed || m.bad || !m.key || !IsObjectAlive(m.sender))
        {
            m.bad = true;
            return false;
        }
        value = *m.key;
        return true;
    }
} // namespace OpenBlood2
