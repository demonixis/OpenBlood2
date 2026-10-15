#include "game/server_adapter.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

ServerDE* g_pServerDE = nullptr;
namespace OpenBlood2
{
    ServerAdapter* ServerAdapter::active_ = nullptr;
    ServerAdapter& ServerAdapter::active()
    {
        return *active_;
    }

    ServerAdapter::ServerAdapter(Objects& objects) : objects_(objects)
    {
        // Noncapturing thunks are checked against each declared callback type.
        GetClass = [](const char* name) -> HCLASS
        {
            active().trace_service("GetClass");
            auto& o = active().objects_;
            const auto type = name ? o.find_class(name) : nullptr;
            if (!type)
                o.fail(ObjectError::UnknownClass, std::string("Unknown class: ") + (name ? name : "(null)"));
            return type;
        };
        GetObjectClass = [](HOBJECT h) -> HCLASS
        {
            active().trace_service("GetObjectClass");
            auto* r = active().objects_.get(h);
            return r ? r->type : nullptr;
        };
        IsKindOf = [](HCLASS a, HCLASS b) -> DBOOL
        {
            active().trace_service("IsKindOf");
            auto& o = active().objects_;
            return o.knows(a) && o.knows(b) && Objects::is_kind_of(a, b);
        };
        CreateObject = [](HCLASS type, ObjectCreateStruct* state) -> LPBASECLASS
        {
            active().trace_service("CreateObject");
            auto& o = active().objects_;
            if (!state)
                return nullptr;
            auto* r = o.get(o.create(type, *state));
            return r ? r->game.get() : nullptr;
        };
        CreateObjectProps = [](HCLASS type, ObjectCreateStruct* state, const char* text) -> LPBASECLASS
        {
            active().trace_service("CreateObjectProps");
            auto& o = active().objects_;
            if (!state)
                return nullptr;
            auto* r = o.get(o.create_props(type, *state, text ? text : ""));
            return r ? r->game.get() : nullptr;
        };
        RemoveObject = [](HOBJECT h)
        {
            active().trace_service("RemoveObject");
            active().objects_.remove(h);
        };
        SetNextUpdate = [](HOBJECT h, float delay)
        {
            active().trace_service("SetNextUpdate");
            active().objects_.set_next_update(h, delay);
        };
        GetTime = []() -> float
        {
            active().trace_service("GetTime");
            return static_cast<float>(active().objects_.time());
        };
        GetFrameTime = []() -> float
        {
            active().trace_service("GetFrameTime");
            return static_cast<float>(active().objects_.frame_time());
        };
        HandleToObject = [](HOBJECT h) -> LPBASECLASS
        {
            active().trace_service("HandleToObject");
            auto* r = active().objects_.get(h);
            return r ? r->game.get() : nullptr;
        };
        ObjectToHandle = [](LPBASECLASS o) -> HOBJECT
        {
            active().trace_service("ObjectToHandle");
            return active().objects_.handle_of(o);
        };
        GetObjectFlags = [](HOBJECT h) -> DDWORD
        {
            active().trace_service("GetObjectFlags");
            auto* r = active().objects_.get(h);
            return r ? r->state.m_Flags : 0;
        };
        SetObjectFlags = [](HOBJECT h, DDWORD flags)
        {
            active().trace_service("SetObjectFlags");
            if (auto* r = active().objects_.get(h))
            {
                if (r->container && (flags & (FLAG_SOLID | FLAG_GRAVITY)))
                {
                    active().objects_.fail(ObjectError::Unsupported, "Container solid/gravity flags are unsupported");
                    return;
                }
                r->state.m_Flags = flags;
            }
        };
        GetNextObject = [](HOBJECT previous) -> HOBJECT
        {
            active().trace_service("GetNextObject");
            return active().objects_.next_alive(previous);
        };
        SetObjectDims = [](HOBJECT h, DVector* dims)
        {
            active().trace_service("SetObjectDims");
            auto& o = active().objects_;
            auto* r = o.get(h);
            if (!r || (r->container || r->world_model) || !dims || !std::isfinite(dims->x) || !std::isfinite(dims->y) ||
                !std::isfinite(dims->z) || dims->x < 0 || dims->y < 0 || dims->z < 0)
            {
                o.fail(ObjectError::InvalidProperty, "Invalid object dimensions");
                return;
            }
            r->dimensions = *dims;
        };
        SetObjectUserFlags = [](HOBJECT h, DDWORD flags)
        {
            active().trace_service("SetObjectUserFlags");
            if (auto* r = active().objects_.get(h))
                r->user_flags = flags;
        };
        GetObjectName = [](HOBJECT h) -> char*
        {
            active().trace_service("GetObjectName");
            auto* r = active().objects_.get(h);
            return r ? r->state.m_Name : nullptr;
        };
        GetObjectPos = [](HOBJECT h, DVector* out)
        {
            active().trace_service("GetObjectPos");
            if (out)
            {
                auto* r = active().objects_.get(h);
                *out = r ? r->state.m_Pos : DVector{};
            }
        };
        GetObjectRotation = [](HOBJECT h, DRotation* out) -> DRESULT
        {
            active().trace_service("GetObjectRotation");
            auto* r = active().objects_.get(h);
            if (out)
                *out = r ? r->state.m_Rotation : DRotation{};
            return r && out ? DE_OK : DE_INVALIDPARAMS;
        };
        GetObjectState = [](HOBJECT h) -> int
        {
            active().trace_service("GetObjectState");
            auto* r = active().objects_.get(h);
            return static_cast<int>(r ? r->activity : OBJSTATE_INACTIVE);
        };
        SetObjectState = [](HOBJECT h, int state)
        {
            active().trace_service("SetObjectState");
            auto& o = active().objects_;
            auto* r = o.get(h);
            if (!r || state < 0 || static_cast<unsigned>(state) > OBJSTATE_INACTIVE_TOUCH)
            {
                o.fail(ObjectError::Unsupported, "Invalid object state; automatic deactivation is not implemented");
                return;
            }
            r->activity = static_cast<DDWORD>(state);
        };
        GetPropGeneric = [](const char* name, GenericProp* out) -> DRESULT
        {
            active().trace_service("GetPropGeneric");
            if (!name || !out)
                return DE_INVALIDPARAMS;
            *out = {};
            const auto* p = active().objects_.property(name);
            if (!p)
                return DE_NOTFOUND;
            return generic_property(*p, *out) ? DE_OK : DE_INVALIDPARAMS;
        };
        GetPropString = [](const char* name, char* out, int count) -> DRESULT
        {
            active().trace_service("GetPropString");
            if (!out || count <= 0)
                return DE_INVALIDPARAMS;
            out[0] = 0;
            const auto* p = name ? active().objects_.property(name) : nullptr;
            if (!p)
                return DE_NOTFOUND;
            const auto* text = std::get_if<std::string>(p);
            if (!text || text->size() >= static_cast<std::size_t>(count))
                return DE_INVALIDPARAMS;
            std::memcpy(out, text->c_str(), text->size() + 1);
            return DE_OK;
        };
        GetPropReal = [](const char* name, float* out) -> DRESULT
        {
            active().trace_service("GetPropReal");
            if (!out)
                return DE_INVALIDPARAMS;
            GenericProp prop;
            auto result = active().GetPropGeneric(name, &prop);
            *out = prop.m_Float;
            return result;
        };
        GetPropBool = [](const char* name, DBOOL* out) -> DRESULT
        {
            active().trace_service("GetPropBool");
            if (!out)
                return DE_INVALIDPARAMS;
            GenericProp prop;
            auto result = active().GetPropGeneric(name, &prop);
            *out = prop.m_Bool;
            return result;
        };
        GetPropVector = [](const char* name, DVector* out) -> DRESULT
        {
            active().trace_service("GetPropVector");
            if (!name || !out)
                return DE_INVALIDPARAMS;
            *out = {};
            const auto* p = active().objects_.property(name);
            if (!p)
                return DE_NOTFOUND;
            if (const auto* v = std::get_if<DVector>(p))
            {
                *out = *v;
                return DE_OK;
            }
            return DE_INVALIDPARAMS;
        };
        GetGameConVar = [](const char* name) -> HCONVAR
        {
            active().trace_service("GetGameConVar");
            if (!name)
                return nullptr;
            auto& vars = active().variables_;
            auto it = vars.find(name);
            return it == vars.end() ? nullptr : &it->second;
        };
        SetGameConVar = [](const char* name, const char* value)
        {
            active().trace_service("SetGameConVar");
            auto& a = active();
            if (!name || !*name || std::strlen(name) > 100 || !value || std::strlen(value) > 4096 ||
                (a.variables_.size() >= 1024 && !a.variables_.contains(name)))
            {
                a.objects_.fail(ObjectError::Limit, "Invalid console variable");
                return;
            }
            a.variables_[name].text = value;
        };
        GetVarValueString = [](HCONVAR h) -> char*
        {
            active().trace_service("GetVarValueString");
            for (auto& [name, v] : active().variables_)
                if (&v == h)
                    return v.text.data();
            return nullptr;
        };
        GetVarValueFloat = [](HCONVAR h) -> float
        {
            active().trace_service("GetVarValueFloat");
            for (auto& [name, v] : active().variables_)
                if (&v == h)
                {
                    float number = 0;
                    auto [end, error] = std::from_chars(v.text.data(), v.text.data() + v.text.size(), number);
                    return error == std::errc{} && end == v.text.data() + v.text.size() && std::isfinite(number)
                               ? number
                               : 0;
                }
            return 0;
        };
        initialize_model_services();
        initialize_physics_services();
        CreateString = [](const char* text) -> HSTRING
        {
            active().trace_service("CreateString");
            return active().make_string(text);
        };
        CopyString = [](HSTRING h) -> HSTRING
        {
            active().trace_service("CopyString");
            return active().copy_string(h);
        };
        FreeString = [](HSTRING h)
        {
            active().trace_service("FreeString");
            active().free_string(h);
        };
        GetStringData = [](HSTRING h) -> char*
        {
            active().trace_service("GetStringData");
            return active().string_data(h);
        };
        CreateObjectList = []() -> ObjectList*
        {
            active().trace_service("CreateObjectList");
            return active().make_list();
        };
        AddObjectToList = [](ObjectList* l, HOBJECT h) -> ObjectLink*
        {
            active().trace_service("AddObjectToList");
            return active().add_to_list(l, h);
        };
        RemoveObjectFromList = [](ObjectList* l, HOBJECT h)
        {
            active().trace_service("RemoveObjectFromList");
            active().remove_from_list(l, h);
        };
        RelinquishList = [](ObjectList* l)
        {
            active().trace_service("RelinquishList");
            active().release_list(l);
        };
        FindNamedObjects = [](const char* name) -> ObjectList*
        {
            active().trace_service("FindNamedObjects");
            if (!name)
                return nullptr;
            auto& a = active();
            auto* list = a.make_list();
            if (!list)
                return nullptr;
            for (auto id : a.objects_.snapshot())
            {
                const auto* r = a.objects_.get(id);
                if (r->life == Life::Alive && std::strcmp(name, r->state.m_Name) == 0)
                    a.add_to_list(list, r->handle);
            }
            return list;
        };
        CreateInterObjectLink = [](HOBJECT a, HOBJECT b) -> DRESULT
        {
            active().trace_service("CreateInterObjectLink");
            return active().objects_.link(a, b) ? DE_OK : DE_ERROR;
        };
        BreakInterObjectLink = [](HOBJECT a, HOBJECT b)
        {
            active().trace_service("BreakInterObjectLink");
            active().objects_.unlink(a, b);
        };
        SaveObjects = [](const char*, ObjectList*, DDWORD, DDWORD) -> DRESULT
        {
            active().trace_service("SaveObjects");
            return active().unsupported("SaveObjects");
        };
        RestoreObjects = [](const char*, DDWORD, DDWORD) -> DRESULT
        {
            active().trace_service("RestoreObjects");
            return active().unsupported("RestoreObjects");
        };
    }
    bool ServerAdapter::bind()
    {
        if (active_ || g_pServerDE)
            return objects_.fail(ObjectError::Busy, "Original globals allow only one bound game session");
        active_ = this;
        g_pServerDE = this;
        bound_ = true;
        return true;
    }
    void ServerAdapter::unbind()
    {
        if (bound_)
        {
            objects_.clear();
            g_pServerDE = nullptr;
            active_ = nullptr;
            bound_ = false;
        }
    }
    ServerAdapter::~ServerAdapter()
    {
        unbind();
        while (!lists_.empty())
            release_list(*lists_.begin());
    }
    void ServerAdapter::set_game_settings(unsigned difficulty, unsigned type)
    {
        if (difficulty > 3 || type > 9)
        {
            objects_.fail(ObjectError::InvalidProperty, "Invalid game settings");
            return;
        }
        variables_["GAME_DIFFICULTY"].text = std::to_string(difficulty);
        variables_["GAME_GAMETYPE"].text = std::to_string(type);
    }
    DRESULT ServerAdapter::SetGlobalForce(DVector* v)
    {
        trace_service("SetGlobalForce");
        if (!v || !std::isfinite(v->x) || !std::isfinite(v->y) || !std::isfinite(v->z))
            return DE_INVALIDPARAMS;
        gravity = *v;
        return DE_OK;
    }
    DRESULT ServerAdapter::GetGlobalForce(DVector* v)
    {
        trace_service("GetGlobalForce");
        if (!v)
            return DE_INVALIDPARAMS;
        *v = gravity;
        return DE_OK;
    }
    DRESULT ServerAdapter::unsupported(const char* service)
    {
        objects_.fail(ObjectError::Unsupported, std::string(service) + ": not implemented");
        return DE_ERROR;
    }
    HSTRING ServerAdapter::make_string(const char* text)
    {
        if (!text || std::strlen(text) > 4096 || string_tokens_.size() >= 65536)
        {
            objects_.fail(ObjectError::Limit, "Invalid string or session string token limit");
            return nullptr;
        }
        string_tokens_.push_back(std::make_unique<StringHandle>());
        HSTRING h = string_tokens_.back().get();
        strings_.emplace(h, String{text, 1});
        return h;
    }
    HSTRING ServerAdapter::copy_string(HSTRING h)
    {
        auto it = strings_.find(h);
        if (it == strings_.end() || it->second.references == std::numeric_limits<unsigned>::max())
            return nullptr;
        ++it->second.references;
        return h;
    }
    void ServerAdapter::free_string(HSTRING h)
    {
        if (!h)
            return;
        auto it = strings_.find(h);
        if (it == strings_.end())
        {
            objects_.fail(ObjectError::InvalidHandle, "FreeString: stale/foreign handle");
            return;
        }
        if (--it->second.references == 0)
            strings_.erase(it);
    }
    char* ServerAdapter::string_data(HSTRING h)
    {
        auto it = strings_.find(h);
        return it == strings_.end() ? nullptr : it->second.text.data();
    }
    ObjectList* ServerAdapter::make_list()
    {
        if (list_tokens_.size() >= 65536)
        {
            objects_.fail(ObjectError::Limit, "Session list token limit");
            return nullptr;
        }
        list_tokens_.push_back(std::make_unique<ObjectList>());
        auto* list = list_tokens_.back().get();
        lists_.insert(list);
        return list;
    }
    ObjectLink* ServerAdapter::add_to_list(ObjectList* list, HOBJECT h)
    {
        if (!lists_.contains(list) || !objects_.get(h))
            return nullptr;
        auto* link = new ObjectLink{h, list->m_pFirstLink};
        list->m_pFirstLink = link;
        ++list->m_nInList;
        return link;
    }
    void ServerAdapter::remove_from_list(ObjectList* list, HOBJECT h)
    {
        if (!lists_.contains(list))
            return;
        for (auto** at = &list->m_pFirstLink; *at;)
        {
            if ((*at)->m_hObject == h)
            {
                auto* dead = *at;
                *at = dead->m_pNext;
                delete dead;
                --list->m_nInList;
            }
            else
                at = &(*at)->m_pNext;
        }
    }
    void ServerAdapter::release_list(ObjectList* list)
    {
        if (!lists_.erase(list))
            return;
        while (list->m_pFirstLink)
        {
            auto* dead = list->m_pFirstLink;
            list->m_pFirstLink = dead->m_pNext;
            delete dead;
        }
        list->m_nInList = 0;
    }
    HMESSAGEWRITE ServerAdapter::StartHMessageWrite()
    {
        trace_service("StartHMessageWrite");
        if (message_tokens_.size() >= 65536)
        {
            objects_.fail(ObjectError::Limit, "Session message token limit");
            return nullptr;
        }
        message_tokens_.push_back(std::make_unique<LMessage>());
        auto h = message_tokens_.back().get();
        messages_.emplace(h, Message{});
        return h;
    }
    ServerAdapter::Message* ServerAdapter::writable(HMESSAGEWRITE h)
    {
        auto it = messages_.find(h);
        if (it == messages_.end() || it->second.borrowed || it->second.bad)
        {
            objects_.fail(ObjectError::InvalidHandle, "Message is not writable");
            return nullptr;
        }
        return &it->second;
    }
    ServerAdapter::Message* ServerAdapter::readable(HMESSAGEREAD h, std::size_t size)
    {
        auto it = messages_.find(h);
        if (it == messages_.end() || it->second.bad)
        {
            objects_.fail(ObjectError::InvalidHandle, "Invalid message read");
            return nullptr;
        }
        auto& m = it->second;
        if (m.cursor == m.bytes.size())
            m.cursor = 0; // Original CSBase end-of-message rewind.
        if (size > m.bytes.size() - m.cursor)
        {
            m.bad = true;
            objects_.fail(ObjectError::InvalidProperty, "Truncated message read");
            return nullptr;
        }
        return &m;
    }
    DRESULT ServerAdapter::WriteToMessageDWord(HMESSAGEWRITE h, DDWORD word)
    {
        trace_service("WriteToMessageDWord");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (m->damage || m->key)
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        if (m->bytes.size() > 65536 - 4)
        {
            m->bad = true;
            objects_.fail(ObjectError::Limit, "Message exceeds 64 KiB");
            return DE_ERROR;
        }
        for (unsigned n = 0; n < 4; ++n)
            m->bytes.push_back(static_cast<std::uint8_t>(word >> (n * 8)));
        return DE_OK;
    }
    DRESULT ServerAdapter::WriteToMessageHString(HMESSAGEWRITE h, HSTRING value)
    {
        trace_service("WriteToMessageHString");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (!value)
            return WriteToMessageDWord(h, UINT32_MAX);
        auto it = strings_.find(value);
        if (it == strings_.end())
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        const auto& text = it->second.text;
        if (text.size() + 4 > 65536 - m->bytes.size())
        {
            m->bad = true;
            return DE_ERROR;
        }
        if (WriteToMessageDWord(h, static_cast<DDWORD>(text.size())) != DE_OK)
            return DE_INVALIDPARAMS;
        m->bytes.insert(m->bytes.end(), text.begin(), text.end());
        return DE_OK;
    }
    DDWORD ServerAdapter::ReadFromMessageDWord(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageDWord");
        auto* m = readable(h, 4);
        if (!m)
            return 0;
        DDWORD word = 0;
        for (unsigned n = 0; n < 4; ++n)
            word |= static_cast<DDWORD>(m->bytes[m->cursor++]) << (n * 8);
        return word;
    }
    HSTRING ServerAdapter::ReadFromMessageHString(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageHString");
        auto* m = readable(h, 4);
        if (!m)
            return nullptr;
        const auto length = ReadFromMessageDWord(h);
        if (length == UINT32_MAX)
            return nullptr;
        // Do not auto-rewind between the length prefix and its payload.
        if (length > 4096 || length > m->bytes.size() - m->cursor)
        {
            m->bad = true;
            objects_.fail(ObjectError::InvalidProperty, "Truncated/oversized message string");
            return nullptr;
        }
        std::string value(m->bytes.begin() + static_cast<std::ptrdiff_t>(m->cursor),
                          m->bytes.begin() + static_cast<std::ptrdiff_t>(m->cursor + length));
        m->cursor += length;
        if (value.find('\0') != std::string::npos)
        {
            m->bad = true;
            return nullptr;
        }
        return make_string(value.c_str()); // Read returns caller-owned HSTRING.
    }
    void ServerAdapter::EndHMessageWrite(HMESSAGEWRITE h)
    {
        trace_service("EndHMessageWrite");
        const auto it = messages_.find(h);
        if (it == messages_.end())
            return;
        if (it->second.borrowed)
        {
            objects_.fail(ObjectError::Busy, "Callback message is borrowed");
            return;
        }
        messages_.erase(it);
    }
    void ServerAdapter::EndHMessageRead(HMESSAGEREAD)
    {
        trace_service("EndHMessageRead");
        unsupported("EndHMessageRead: nested owned readers are not exposed yet");
    }
    void ServerAdapter::ResetRead(HMESSAGEREAD h)
    {
        trace_service("ResetRead");
        auto it = messages_.find(h);
        if (it != messages_.end())
            it->second.cursor = 0;
    }
    HMESSAGEWRITE ServerAdapter::StartMessageToObject(LPBASECLASS sender, HOBJECT receiver, DDWORD id)
    {
        trace_service("StartMessageToObject");
        auto* r = objects_.get(receiver);
        HOBJECT from = sender ? objects_.handle_of(sender) : nullptr;
        if (!r || r->life != Life::Alive || (sender && !from))
        {
            objects_.fail(ObjectError::InvalidHandle, "Invalid message destination/sender");
            return nullptr;
        }
        auto h = StartHMessageWrite();
        if (!h)
            return nullptr;
        auto& m = messages_.at(h);
        m.routed = true;
        m.sender = from;
        m.receiver = receiver;
        m.id = id;
        return h;
    }
    DRESULT ServerAdapter::EndMessage(HMESSAGEWRITE h)
    {
        trace_service("EndMessage");
        auto it = messages_.find(h);
        if (it == messages_.end() || it->second.borrowed)
            return DE_INVALIDPARAMS;
        auto& m = it->second;
        bool ok = m.routed && !m.bad;
        if (ok)
        {
            m.borrowed = true;
            ok = objects_.message(m.receiver, m.sender, m.id, h);
            ok = ok && !m.bad;
        }
        messages_.erase(h);
        return ok ? DE_OK : DE_ERROR;
    }
} // namespace OpenBlood2
