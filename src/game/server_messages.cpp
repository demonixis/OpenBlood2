#include "game/server_adapter.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>

// Original CSBase message primitives on owned, bounded byte payloads, and the
// MID_SAVEOBJECT/MID_LOADOBJECT payload dispatch. See save_load_contracts
// "Message primitives" and message_lifetime_contracts.
namespace OpenBlood2
{
    namespace
    {
        // Shared only for unique identities. No payloads or original game state
        // cross sessions, including a suspended source and prepared destination.
        std::atomic<std::uint64_t> next_message_identity{1};
        MessageHandle allocate_message_identity()
        {
            auto identity = next_message_identity.load(std::memory_order_relaxed);
            while (identity != UINT64_MAX)
                if (next_message_identity.compare_exchange_weak(identity, identity + 1, std::memory_order_relaxed))
                    return MessageHandle{identity};
            return nullptr; // Exhaustion never wraps to a previous/stale identity.
        }
        constexpr std::size_t max_message_bytes = 65536;
        constexpr std::size_t max_read_strings = max_message_bytes / 4;
        const char empty_text[] = "";
        void put(std::uint8_t* out, std::uint32_t value, unsigned size)
        {
            for (unsigned n = 0; n < size; ++n)
                out[n] = static_cast<std::uint8_t>(value >> (n * 8));
        }
        std::uint32_t get(const std::uint8_t* in, unsigned size)
        {
            std::uint32_t value = 0;
            for (unsigned n = 0; n < size; ++n)
                value |= static_cast<std::uint32_t>(in[n]) << (n * 8);
            return value;
        }
        void put_float(std::uint8_t* out, float value)
        {
            put(out, std::bit_cast<std::uint32_t>(value), 4); // Raw binary32 bits, NaN included.
        }
        float get_float(const std::uint8_t* in)
        {
            return std::bit_cast<float>(get(in, 4));
        }
    } // namespace

    MessageHandle ServerAdapter::create_message()
    {
        if (messages_.size() >= max_live_messages)
        {
            ++message_stats.refused;
            refuse(ObjectError::Limit, "Concurrent message limit");
            return nullptr;
        }
        const auto h = allocate_message_identity();
        if (!h)
        {
            ++message_stats.refused;
            refuse(ObjectError::Limit, "Message identity exhausted");
            return nullptr;
        }
        messages_.emplace(h, Message{});
        ++message_stats.created;
        message_stats.peak_live = std::max(message_stats.peak_live, messages_.size());
        return h;
    }
    ServerAdapter::PayloadDispatch* ServerAdapter::payload(HMESSAGEWRITE h, bool load)
    {
        return payload_ && payload_->message == h && payload_->load == load ? &*payload_ : nullptr;
    }
    void ServerAdapter::latch(Message& m, const std::string& detail)
    {
        if (!m.bad)
        {
            m.bad = true;
            m.failure = detail;
        }
        if (!payload_)
            return;
        // Any message failure inside a save/load callback fails that dispatch.
        if (payload_->report->error.empty())
            payload_->report->error = detail;
        const auto it = messages_.find(payload_->message);
        if (it != messages_.end() && !it->second.bad)
        {
            it->second.bad = true;
            it->second.failure = detail;
        }
    }
    bool ServerAdapter::reject(Message& m, ObjectError error, std::string detail)
    {
        latch(m, detail);
        objects_.fail(error, std::move(detail));
        return false;
    }
    bool ServerAdapter::refuse(ObjectError error, std::string detail)
    {
        if (payload_ && payload_->report->error.empty())
            payload_->report->error = detail;
        if (payload_)
            if (const auto it = messages_.find(payload_->message); it != messages_.end() && !it->second.bad)
            {
                it->second.bad = true;
                it->second.failure = detail;
            }
        objects_.fail(error, std::move(detail));
        return false;
    }
    HMESSAGEWRITE ServerAdapter::StartHMessageWrite()
    {
        trace_service("StartHMessageWrite");
        return create_message();
    }
    ServerAdapter::Message* ServerAdapter::writable(HMESSAGEWRITE h)
    {
        auto it = messages_.find(h);
        // A callback borrows its message; only the active save payload stays writable.
        if (it == messages_.end() || it->second.reader || it->second.bad ||
            (it->second.borrowed && !payload(h, false)))
        {
            refuse(ObjectError::InvalidHandle, "Message is not writable");
            return nullptr;
        }
        return &it->second;
    }
    ServerAdapter::Message* ServerAdapter::readable(HMESSAGEREAD h, std::size_t size)
    {
        auto it = messages_.find(h);
        if (it == messages_.end() || it->second.bad)
        {
            refuse(ObjectError::InvalidHandle, "Invalid message read");
            return nullptr;
        }
        auto& m = it->second;
        const bool load = payload(h, true) != nullptr;
        if (m.cursor == m.bytes.size() && !load)
            m.cursor = 0; // Original CSBase end-of-message rewind; disabled inside MID_LOADOBJECT.
        if (size > m.bytes.size() - m.cursor)
        {
            reject(m, ObjectError::InvalidProperty,
                   load ? "MID_LOADOBJECT over-read: " + std::to_string(size) + " bytes at " +
                              std::to_string(m.cursor) + " of " + std::to_string(m.bytes.size())
                        : std::string("Truncated message read"));
            return nullptr;
        }
        return &m;
    }
    DRESULT ServerAdapter::append(HMESSAGEWRITE h, const std::uint8_t* data, std::size_t size)
    {
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (m->damage || m->key || m->pickup || m->immobilize)
        {
            latch(*m, "Typed message cannot carry byte fields");
            return DE_INVALIDPARAMS;
        }
        if (size > max_message_bytes - m->bytes.size())
        {
            reject(*m, ObjectError::Limit, "Message exceeds 64 KiB");
            return DE_ERROR;
        }
        m->bytes.insert(m->bytes.end(), data, data + size); // Whole field or nothing.
        return DE_OK;
    }
    bool ServerAdapter::take(HMESSAGEREAD h, std::uint8_t* data, std::size_t size)
    {
        auto* m = readable(h, size);
        if (!m)
            return false;
        std::memcpy(data, m->bytes.data() + m->cursor, size);
        m->cursor += size;
        return true;
    }
    bool ServerAdapter::read_text(HMESSAGEREAD h, bool nullable, std::string& text, bool& null)
    {
        null = false;
        std::uint8_t prefix[4];
        if (!take(h, prefix, 4))
            return false;
        const auto length = get(prefix, 4);
        if (nullable && length == save_null_reference)
            return null = true;
        auto& m = messages_.at(h);
        // Do not auto-rewind between the length prefix and its payload.
        if (length > max_message_string || length > m.bytes.size() - m.cursor)
            return reject(m, ObjectError::InvalidProperty, "Truncated/oversized message string");
        text.assign(m.bytes.begin() + static_cast<std::ptrdiff_t>(m.cursor),
                    m.bytes.begin() + static_cast<std::ptrdiff_t>(m.cursor + length));
        m.cursor += length;
        if (text.find('\0') != std::string::npos)
        {
            latch(m, "Message string contains NUL");
            return false;
        }
        return true;
    }
    DRESULT ServerAdapter::WriteToMessageFloat(HMESSAGEWRITE h, float value)
    {
        trace_service("WriteToMessageFloat");
        std::uint8_t bytes[4];
        put_float(bytes, value);
        return append(h, bytes, 4);
    }
    DRESULT ServerAdapter::WriteToMessageByte(HMESSAGEWRITE h, DBYTE value)
    {
        trace_service("WriteToMessageByte");
        return append(h, &value, 1);
    }
    DRESULT ServerAdapter::WriteToMessageWord(HMESSAGEWRITE h, D_WORD value)
    {
        trace_service("WriteToMessageWord");
        std::uint8_t bytes[2];
        put(bytes, value, 2);
        return append(h, bytes, 2);
    }
    DRESULT ServerAdapter::WriteToMessageDWord(HMESSAGEWRITE h, DDWORD word)
    {
        trace_service("WriteToMessageDWord");
        std::uint8_t bytes[4];
        put(bytes, word, 4);
        return append(h, bytes, 4);
    }
    DRESULT ServerAdapter::WriteToMessageString(HMESSAGEWRITE h, const char* text)
    {
        trace_service("WriteToMessageString");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        // A null pointer writes an empty string (InventoryMgr.cpp:1685 passes DNULL).
        const std::size_t length = text ? strnlen(text, max_message_string + 1) : 0;
        if (length > max_message_string)
        {
            reject(*m, ObjectError::Limit, "Message string exceeds 4096 bytes");
            return DE_ERROR;
        }
        std::vector<std::uint8_t> field(4 + length);
        put(field.data(), static_cast<std::uint32_t>(length), 4);
        std::memcpy(field.data() + 4, text ? text : empty_text, length);
        return append(h, field.data(), field.size());
    }
    DRESULT ServerAdapter::WriteToMessageVector(HMESSAGEWRITE h, const DVector* value)
    {
        trace_service("WriteToMessageVector");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (!value)
            {
                reject(*m, ObjectError::InvalidProperty, "Null message vector");
                return DE_INVALIDPARAMS;
            }
        std::uint8_t bytes[12];
        put_float(bytes, value->x);
        put_float(bytes + 4, value->y);
        put_float(bytes + 8, value->z);
        return append(h, bytes, 12);
    }
    DRESULT ServerAdapter::WriteToMessageRotation(HMESSAGEWRITE h, const DRotation* value)
    {
        trace_service("WriteToMessageRotation");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (!value)
            {
                reject(*m, ObjectError::InvalidProperty, "Null message rotation");
                return DE_INVALIDPARAMS;
            }
        std::uint8_t bytes[16];
        put_float(bytes, value->m_Vec.x);
        put_float(bytes + 4, value->m_Vec.y);
        put_float(bytes + 8, value->m_Vec.z);
        put_float(bytes + 12, value->m_Spin);
        return append(h, bytes, 16);
    }
    DRESULT ServerAdapter::WriteToMessageHString(HMESSAGEWRITE h, HSTRING value)
    {
        trace_service("WriteToMessageHString");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (!value)
        {
            std::uint8_t null[4];
            put(null, save_null_reference, 4);
            return append(h, null, 4);
        }
        auto it = strings_.find(value);
        if (it == strings_.end())
        {
            latch(*m, "Stale or foreign message HSTRING");
            return DE_INVALIDPARAMS;
        }
        const auto& text = it->second.text;
        if (text.size() + 4 > max_message_bytes - m->bytes.size())
        {
            latch(*m, "Message exceeds 64 KiB");
            return DE_ERROR;
        }
        std::vector<std::uint8_t> field(4 + text.size());
        put(field.data(), static_cast<std::uint32_t>(text.size()), 4);
        std::copy(text.begin(), text.end(), field.begin() + 4);
        return append(h, field.data(), field.size()) == DE_OK ? DE_OK : DE_INVALIDPARAMS;
    }
    DRESULT ServerAdapter::nest(HMESSAGEWRITE h, HMESSAGEREAD data, bool reader)
    {
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        const auto it = messages_.find(data);
        // Writers nest standalone StartHMessageWrite data; readers nest an owned
        // nested reader or a borrowed callback reader. Save/load payloads,
        // routed sends and typed side-car messages never nest.
        const bool valid =
            it != messages_.end() && data != h && !it->second.bad && !payload(data, false) && !payload(data, true) &&
            !it->second.damage && !it->second.key && !it->second.pickup && !it->second.immobilize &&
            (reader ? it->second.reader || it->second.borrowed
                    : !it->second.reader && !it->second.borrowed && !it->second.routed);
        if (!valid)
            {
                reject(*m, ObjectError::InvalidHandle, "Invalid nested message");
                return DE_INVALIDPARAMS;
            }
        const auto& bytes = it->second.bytes;
        if (bytes.size() > max_nested_message)
            {
                reject(*m, ObjectError::Limit, "Nested message exceeds 16 KiB");
                return DE_ERROR;
            }
        std::vector<std::uint8_t> field(4 + bytes.size());
        put(field.data(), static_cast<std::uint32_t>(bytes.size()), 4);
        std::copy(bytes.begin(), bytes.end(), field.begin() + 4);
        const auto result = append(h, field.data(), field.size());
        if (result == DE_OK)
            if (auto* active = payload(h, false))
                ++active->report->nested;
        return result;
    }
    DRESULT ServerAdapter::WriteToMessageHMessageWrite(HMESSAGEWRITE h, HMESSAGEWRITE data)
    {
        trace_service("WriteToMessageHMessageWrite");
        return nest(h, data, false);
    }
    DRESULT ServerAdapter::WriteToMessageHMessageRead(HMESSAGEWRITE h, HMESSAGEREAD data)
    {
        trace_service("WriteToMessageHMessageRead");
        return nest(h, data, true);
    }
    DRESULT ServerAdapter::WriteToMessageObject(HMESSAGEWRITE h, HOBJECT)
    {
        trace_service("WriteToMessageObject");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        if (auto* active = payload(h, false))
        {
            // Departure D7: a runtime reference would be stale after a restore.
            std::uint8_t bytes[4];
            put(bytes, save_null_reference, 4);
            const auto result = append(h, bytes, 4);
            if (result == DE_OK)
                ++active->report->plain_objects;
            return result;
        }
        latch(*m, "WriteToMessageObject outside MID_SAVEOBJECT");
        return unsupported("WriteToMessageObject outside MID_SAVEOBJECT");
    }
    DRESULT ServerAdapter::WriteToLoadSaveMessageObject(HMESSAGEWRITE h, HOBJECT object)
    {
        trace_service("WriteToLoadSaveMessageObject");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        auto* active = payload(h, false);
        if (!active)
        {
            reject(*m, ObjectError::Unsupported, "WriteToLoadSaveMessageObject outside MID_SAVEOBJECT");
            return DE_ERROR;
        }
        std::uint32_t index = save_null_reference;
        const auto* record = objects_.get(object);
        // NULL, stale, going-away and unlisted targets (always-load, unsaved) read back NULL.
        const bool listed = record && record->life == Life::Alive && active->index->find(object, index);
        if (!listed)
            index = save_null_reference;
        std::uint8_t bytes[4];
        put(bytes, index, 4);
        const auto result = append(h, bytes, 4);
        if (result == DE_OK)
            ++(listed ? active->report->references : active->report->null_references);
        return result;
    }
    float ServerAdapter::ReadFromMessageFloat(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageFloat");
        std::uint8_t bytes[4];
        return take(h, bytes, 4) ? get_float(bytes) : 0.0f;
    }
    DBYTE ServerAdapter::ReadFromMessageByte(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageByte");
        DBYTE value = 0;
        return take(h, &value, 1) ? value : DBYTE{0};
    }
    D_WORD ServerAdapter::ReadFromMessageWord(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageWord");
        std::uint8_t bytes[2];
        return take(h, bytes, 2) ? static_cast<D_WORD>(get(bytes, 2)) : D_WORD{0};
    }
    DDWORD ServerAdapter::ReadFromMessageDWord(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageDWord");
        std::uint8_t bytes[4];
        return take(h, bytes, 4) ? get(bytes, 4) : 0;
    }
    const char* ServerAdapter::ReadFromMessageString(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageString");
        std::string text;
        bool null = false;
        if (!read_text(h, false, text, null))
            return empty_text;
        auto& m = messages_.at(h);
        if (m.strings.size() >= max_read_strings)
        {
            reject(m, ObjectError::Limit, "Message string read limit");
            return empty_text;
        }
        m.strings.push_back(std::move(text));
        return m.strings.back().c_str();
    }
    DRESULT ServerAdapter::ReadFromMessageStringBuffer(HMESSAGEREAD h, char* buffer, DDWORD capacity)
    {
        trace_service("ReadFromMessageStringBuffer");
        if (!buffer || !capacity)
        {
            if (const auto it = messages_.find(h); it != messages_.end())
                reject(it->second, ObjectError::InvalidProperty, "Null message string destination");
            return DE_INVALIDPARAMS;
        }
        buffer[0] = '\0';
        std::string text;
        bool null = false;
        if (!read_text(h, false, text, null))
            return DE_ERROR;
        if (text.size() >= capacity)
            {
                reject(messages_.at(h), ObjectError::Limit, "Message string exceeds its destination");
                return DE_ERROR;
            }
        std::memcpy(buffer, text.c_str(), text.size() + 1);
        return DE_OK;
    }
    void ServerAdapter::ReadFromMessageVector(HMESSAGEREAD h, DVector* value)
    {
        trace_service("ReadFromMessageVector");
        if (!value)
        {
            if (const auto it = messages_.find(h); it != messages_.end())
                reject(it->second, ObjectError::InvalidProperty, "Null message vector destination");
            return;
        }
        std::uint8_t bytes[12];
        *value = take(h, bytes, 12) ? DVector{get_float(bytes), get_float(bytes + 4), get_float(bytes + 8)} : DVector{};
    }
    void ServerAdapter::ReadFromMessageRotation(HMESSAGEREAD h, DRotation* value)
    {
        trace_service("ReadFromMessageRotation");
        if (!value)
        {
            if (const auto it = messages_.find(h); it != messages_.end())
                reject(it->second, ObjectError::InvalidProperty, "Null message rotation destination");
            return;
        }
        std::uint8_t bytes[16];
        *value = take(h, bytes, 16) ? DRotation{{get_float(bytes), get_float(bytes + 4), get_float(bytes + 8)},
                                                get_float(bytes + 12)}
                                    : DRotation{};
    }
    HOBJECT ServerAdapter::ReadFromMessageObject(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageObject");
        auto* active = payload(h, true);
        if (!active)
        {
            const auto it = messages_.find(h);
            if (it == messages_.end())
                refuse(ObjectError::InvalidHandle, "Invalid message read");
            else
            {
                latch(it->second, "ReadFromMessageObject outside MID_LOADOBJECT");
                unsupported("ReadFromMessageObject outside MID_LOADOBJECT");
            }
            return nullptr;
        }
        std::uint8_t bytes[4];
        if (!take(h, bytes, 4))
            return nullptr;
        // Saves write plain references only as the NULL sentinel (D7).
        if (get(bytes, 4) != save_null_reference)
            {
                reject(messages_.at(h), ObjectError::InvalidProperty, "Plain object field is not NULL");
                return nullptr;
            }
        ++active->report->plain_objects;
        return nullptr;
    }
    HSTRING ServerAdapter::ReadFromMessageHString(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageHString");
        std::string text;
        bool null = false;
        if (!read_text(h, true, text, null) || null)
            return nullptr;
        return make_string(text.c_str()); // Read returns caller-owned HSTRING.
    }
    DRESULT ServerAdapter::ReadFromLoadSaveMessageObject(HMESSAGEREAD h, HOBJECT* object)
    {
        trace_service("ReadFromLoadSaveMessageObject");
        if (object)
            *object = nullptr;
        const auto it = messages_.find(h);
        auto* active = payload(h, true);
        if (!active)
        {
            if (it != messages_.end())
                reject(it->second, ObjectError::Unsupported, "ReadFromLoadSaveMessageObject outside MID_LOADOBJECT");
            else
                refuse(ObjectError::InvalidHandle, "Invalid message read");
            return DE_ERROR;
        }
        if (!object)
        {
            reject(it->second, ObjectError::InvalidProperty, "Null LoadSave object destination");
            return DE_INVALIDPARAMS;
        }
        std::uint8_t bytes[4];
        if (!take(h, bytes, 4))
            return DE_ERROR;
        const auto index = get(bytes, 4);
        if (index == save_null_reference)
        {
            ++active->report->null_references;
            return DE_OK;
        }
        const auto target = active->index->at(index);
        if (!target || !objects_.get(target))
        {
            reject(it->second, ObjectError::InvalidProperty,
                   "LoadSave reference " + std::to_string(index) + " is outside the restored list");
            return DE_ERROR;
        }
        *object = target;
        ++active->report->references;
        return DE_OK;
    }
    HMESSAGEREAD ServerAdapter::ReadFromMessageHMessageRead(HMESSAGEREAD h)
    {
        trace_service("ReadFromMessageHMessageRead");
        std::uint8_t prefix[4];
        if (!take(h, prefix, 4))
            return nullptr;
        const auto length = get(prefix, 4);
        auto& m = messages_.at(h); // Element references survive map growth below.
        if (length > max_nested_message || length > m.bytes.size() - m.cursor)
            {
                reject(m, ObjectError::InvalidProperty, "Truncated/oversized nested message");
                return nullptr;
            }
        const auto nested = create_message();
        if (!nested)
        {
            latch(m, "Nested message reader refused");
            return nullptr;
        }
        auto& reader = messages_.at(nested);
        reader.reader = true;
        reader.bytes.assign(m.bytes.begin() + static_cast<std::ptrdiff_t>(m.cursor),
                            m.bytes.begin() + static_cast<std::ptrdiff_t>(m.cursor + length));
        m.cursor += length;
        if (auto* active = payload(h, true))
            ++active->report->nested;
        return nested;
    }
    void ServerAdapter::EndHMessageWrite(HMESSAGEWRITE h)
    {
        trace_service("EndHMessageWrite");
        const auto it = messages_.find(h);
        if (it == messages_.end())
            return;
        if (it->second.borrowed)
        {
            refuse(ObjectError::Busy, "Callback message is borrowed");
            return;
        }
        if (it->second.reader)
        {
            refuse(ObjectError::InvalidHandle, "Nested reader requires EndHMessageRead");
            return;
        }
        messages_.erase(it);
        ++message_stats.released;
    }
    void ServerAdapter::EndHMessageRead(HMESSAGEREAD h)
    {
        trace_service("EndHMessageRead");
        const auto it = messages_.find(h);
        if (it == messages_.end())
            return;
        if (it->second.borrowed)
        {
            refuse(ObjectError::Busy, "Callback message is borrowed");
            return;
        }
        if (!it->second.reader)
        {
            refuse(ObjectError::InvalidHandle, "EndHMessageRead requires a nested reader");
            return;
        }
        messages_.erase(it);
        ++message_stats.released;
    }
    void ServerAdapter::RejectPayload(HMESSAGEWRITE h, std::string_view reason)
    {
        trace_service("RejectPayload");
        if (!payload_ || payload_->message != h)
            return;
        if (const auto it = messages_.find(h); it != messages_.end())
            latch(it->second, payload_->report->class_name + ": " + std::string(reason));
    }
    void ServerAdapter::ResetRead(HMESSAGEREAD h)
    {
        trace_service("ResetRead");
        auto it = messages_.find(h);
        if (it != messages_.end())
            it->second.cursor = 0;
    }
    bool ServerAdapter::MessageFullyRead(HMESSAGEREAD h)
    {
        trace_service("MessageFullyRead");
        const auto it = messages_.find(h);
        return it != messages_.end() && !it->second.bad && it->second.cursor == it->second.bytes.size();
    }
    HMESSAGEWRITE ServerAdapter::StartMessageToObject(LPBASECLASS sender, HOBJECT receiver, DDWORD id)
    {
        if (!trace_service("StartMessageToObject"))
            return nullptr;
        auto* r = objects_.get(receiver);
        HOBJECT from = sender ? objects_.handle_of(sender) : nullptr;
        if (!r || r->life != Life::Alive || (sender && !from))
        {
            refuse(ObjectError::InvalidHandle, "Invalid message destination/sender");
            return nullptr;
        }
        auto h = create_message();
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
        if (it == messages_.end() || it->second.borrowed || it->second.reader)
            return DE_INVALIDPARAMS;
        auto& m = it->second;
        bool ok = m.routed && !m.bad && (m.id != immobilize_message || m.immobilize.has_value());
        if (ok)
        {
            m.borrowed = true;
            ok = objects_.message(m.receiver, m.sender, m.id, h);
            ok = ok && !m.bad;
        }
        messages_.erase(h);
        ++message_stats.released;
        return ok ? DE_OK : DE_ERROR;
    }
    bool ServerAdapter::dispatch_ready(HOBJECT object, SavePayloadReport& report)
    {
        report = {};
        const auto* record = objects_.get(object);
        if (record && record->type)
            report.class_name = record->type->name;
        if (!bound_ || active_ != this || g_pServerDE != this)
            report.error = "Save/load dispatch: adapter is not the bound session";
        else if (payload_)
            report.error = "Save/load dispatch: another payload dispatch is active";
        else if (objects_.busy())
            report.error = "Save/load dispatch: an object callback is in progress";
        else if (!record || record->life != Life::Alive)
            report.error = "Save/load dispatch: object is not alive";
        else
            return true;
        return false;
    }
    bool ServerAdapter::save_object_payload(HOBJECT object, DDWORD param, const SaveIndex& index,
                                            std::vector<std::uint8_t>& bytes, SavePayloadReport& report)
    {
        bytes.clear();
        if (!dispatch_ready(object, report))
            return false;
        const auto h = create_message();
        if (!h)
        {
            report.error = "Save payload message refused";
            return false;
        }
        messages_.at(h).borrowed = true; // The callback cannot release or route its writer.
        // Purity (save_load_contracts "Save sequence" 5): the registry refuses creation,
        // removal, scheduling, links, attachments and nested dispatch; trace_service
        // refuses every other service outside the pure set.
        const auto refusals = objects_.pure_refusals();
        if (!objects_.begin_pure_callback())
        {
            messages_.erase(h);
            ++message_stats.released;
            report.error = "Save payload purity scope refused";
            return false;
        }
        payload_ = PayloadDispatch{h, false, &index, &report};
        HMESSAGEWRITE argument = h;
        const bool delivered = objects_.engine_message(object, MID_SAVEOBJECT, &argument, static_cast<float>(param));
        payload_.reset();
        objects_.end_pure_callback();
        const auto it = messages_.find(h);
        auto& m = it->second;
        const bool pure = objects_.pure_refusals() == refusals;
        if (!pure && report.error.empty())
            report.error = "MID_SAVEOBJECT purity: " + objects_.first_pure_refusal() + " refused";
        const bool ok = delivered && !m.bad && pure;
        if (ok)
        {
            bytes = std::move(m.bytes);
            report.bytes = bytes.size();
        }
        else if (report.error.empty())
            report.error = !m.failure.empty() ? m.failure : std::string("MID_SAVEOBJECT dispatch failed");
        messages_.erase(it);
        ++message_stats.released;
        return ok;
    }
    bool ServerAdapter::load_object_payload(HOBJECT object, DDWORD param, std::span<const std::uint8_t> bytes,
                                            const SaveIndex& index, SavePayloadReport& report)
    {
        if (!dispatch_ready(object, report))
            return false;
        report.bytes = bytes.size();
        if (bytes.size() > max_message_bytes)
        {
            report.error = "Load payload exceeds 64 KiB";
            return false;
        }
        const auto h = create_message();
        if (!h)
        {
            report.error = "Load payload message refused";
            return false;
        }
        auto& reader = messages_.at(h);
        reader.bytes.assign(bytes.begin(), bytes.end());
        reader.borrowed = true;
        payload_ = PayloadDispatch{h, true, &index, &report};
        HMESSAGEREAD argument = h;
        const bool delivered = objects_.engine_message(object, MID_LOADOBJECT, &argument, static_cast<float>(param));
        payload_.reset();
        const auto it = messages_.find(h);
        auto& m = it->second;
        report.consumed = m.cursor;
        // Departure D5: an under-read is a failure, like an over-read.
        const bool ok = delivered && !m.bad && m.cursor == m.bytes.size();
        if (!ok && report.error.empty())
            report.error = m.bad        ? m.failure
                           : !delivered ? std::string("MID_LOADOBJECT dispatch failed")
                                        : "MID_LOADOBJECT under-read: consumed " + std::to_string(m.cursor) + " of " +
                                              std::to_string(m.bytes.size()) + " bytes";
        messages_.erase(it);
        ++message_stats.released;
        return ok;
    }
    DRESULT ServerAdapter::SendClientSaveData(HMESSAGEREAD blob)
    {
        if (!trace_service("SendClientSaveData"))
            return DE_ERROR;
        const auto it = messages_.find(blob);
        if (it == messages_.end() || it->second.bad || it->second.bytes.size() > max_nested_message)
        {
            refuse(ObjectError::InvalidHandle, "SendClientSaveData requires a valid bounded blob");
            return DE_INVALIDPARAMS;
        }
        if (client_save_data)
        {
            refuse(ObjectError::Limit, "A second client save blob: saves hold one local player (D6)");
            return DE_ERROR;
        }
        client_save_data = it->second.bytes; // The whole blob, like WriteToMessageHMessageRead.
        return DE_OK;
    }
    HMESSAGEREAD ServerAdapter::take_client_save_data()
    {
        if (!client_save_data)
            return nullptr;
        const auto h = create_message();
        if (!h)
            return nullptr;
        // SMSG_CLIENTSAVEDATA carries the blob as a nested message (LoadClientData reads it back).
        auto& m = messages_.at(h);
        m.reader = true;
        std::uint8_t prefix[4];
        put(prefix, static_cast<std::uint32_t>(client_save_data->size()), 4);
        m.bytes.assign(prefix, prefix + 4);
        m.bytes.insert(m.bytes.end(), client_save_data->begin(), client_save_data->end());
        client_save_data.reset();
        return h;
    }
} // namespace OpenBlood2
