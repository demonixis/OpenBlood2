#include "game/original/CameraSpot.h"
#include "game/server_adapter.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Save-message primitives (save_load_contracts "Message primitives", lot 1).
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    using Bytes = std::vector<std::uint8_t>;
    struct Hooks
    {
        std::function<void(HMESSAGEWRITE, DDWORD)> save;
        std::function<void(HMESSAGEREAD, DDWORD)> load;
        float last_param = -1;
    } hooks;
    struct SaveProbe : CameraSpot
    {
        DDWORD EngineMessageFn(DDWORD id, void* data, float param) override
        {
            const auto result = CameraSpot::EngineMessageFn(id, data, param);
            if (id == MID_SAVEOBJECT || id == MID_LOADOBJECT)
                hooks.last_param = param;
            if (id == MID_SAVEOBJECT && hooks.save)
                hooks.save(*static_cast<HMESSAGEWRITE*>(data), static_cast<DDWORD>(param));
            if (id == MID_LOADOBJECT && hooks.load)
                hooks.load(*static_cast<HMESSAGEREAD*>(data), static_cast<DDWORD>(param));
            return result;
        }
    };
    std::unique_ptr<BaseClass> probe_factory()
    {
        return std::make_unique<SaveProbe>();
    }
    ClassInfo probe_type{"SaveMessageProbe", nullptr, probe_factory, {}};
    bool setup(GameSession& session)
    {
        hooks = {};
        if (!session.initialize())
            return false;
        probe_type.parent = session.objects.find_class("CameraSpot");
        return session.objects.register_class(probe_type);
    }
    HOBJECT probe(GameSession& session, const char* name)
    {
        return session.objects.create(&probe_type, {}, Creation::World, {{"Name", std::string(name)}});
    }
    void put(Bytes& out, std::uint32_t value, unsigned size = 4)
    {
        for (unsigned n = 0; n < size; ++n)
            out.push_back(static_cast<std::uint8_t>(value >> (n * 8)));
    }
    void put_float(Bytes& out, float value)
    {
        put(out, std::bit_cast<std::uint32_t>(value));
    }
    void put_text(Bytes& out, std::string_view text)
    {
        put(out, static_cast<std::uint32_t>(text.size()));
        out.insert(out.end(), text.begin(), text.end());
    }
    // One instance of every save primitive. The listed target has record index 1.
    struct Fixture
    {
        HOBJECT saved = nullptr, listed = nullptr, unlisted = nullptr, pending = nullptr;
        SaveIndex index;
    };
    const float nan_value = std::bit_cast<float>(0x7FC00001u);
    void write_all(ServerAdapter& s, HMESSAGEWRITE h, const Fixture& f)
    {
        s.WriteToMessageByte(h, 0xAB);
        s.WriteToMessageWord(h, 0x1234);
        s.WriteToMessageDWord(h, 0x89ABCDEF);
        s.WriteToMessageFloat(h, 1.5f);
        s.WriteToMessageFloat(h, -0.0f);
        s.WriteToMessageFloat(h, nan_value);
        const DVector v{1, -2, 0.5f};
        s.WriteToMessageVector(h, &v);
        const DRotation r{{0.5f, -0.5f, 0.5f}, -0.5f};
        s.WriteToMessageRotation(h, &r);
        s.WriteToMessageString(h, "ab");
        s.WriteToMessageString(h, "");
        s.WriteToMessageHString(h, nullptr);
        HSTRING x = s.CreateString("x");
        s.WriteToMessageHString(h, x);
        s.FreeString(x);
        s.WriteToLoadSaveMessageObject(h, f.listed);
        s.WriteToLoadSaveMessageObject(h, nullptr);
        s.WriteToLoadSaveMessageObject(h, f.unlisted);
        s.WriteToLoadSaveMessageObject(h, f.pending);
        s.WriteToMessageObject(h, f.listed); // Plain reference inside a save: NULL (D7).
        auto data = s.StartHMessageWrite();
        s.WriteToMessageByte(data, 7);
        s.WriteToMessageDWord(data, 9);
        s.WriteToMessageHMessageWrite(h, data);
        s.EndHMessageWrite(data);
    }
    Bytes golden()
    {
        Bytes b{0xAB};
        put(b, 0x1234, 2);
        put(b, 0x89ABCDEF);
        put_float(b, 1.5f);
        put(b, 0x80000000u);
        put(b, 0x7FC00001u);
        for (float f : {1.0f, -2.0f, 0.5f, 0.5f, -0.5f, 0.5f, -0.5f})
            put_float(b, f);
        put_text(b, "ab");
        put_text(b, "");
        put(b, 0xFFFFFFFFu);
        put_text(b, "x");
        put(b, 1);
        for (int n = 0; n < 4; ++n)
            put(b, 0xFFFFFFFFu);
        put(b, 5);
        b.push_back(7);
        put(b, 9);
        return b;
    }
    bool make_fixture(GameSession& s, Fixture& f)
    {
        f.saved = probe(s, "saved");
        f.listed = probe(s, "listed");
        f.unlisted = probe(s, "unlisted");
        f.pending = probe(s, "pending");
        CHECK(f.saved && f.listed && f.unlisted && f.pending);
        CHECK(f.index.add(f.saved) && f.index.add(f.listed) && f.index.add(f.pending));
        CHECK(!f.index.add(f.saved) && !f.index.add(nullptr) && f.index.size() == 3);
        CHECK(s.objects.remove(f.pending)); // Going away: saved as NULL even though listed.
        return true;
    }
    bool save_all(GameSession& s, const Fixture& f, Bytes& bytes, SavePayloadReport& report)
    {
        hooks.save = [&](HMESSAGEWRITE h, DDWORD) { write_all(s.server, h, f); };
        const bool ok = s.server.save_object_payload(f.saved, 0, f.index, bytes, report);
        hooks.save = {};
        return ok;
    }
    struct Loaded
    {
        DBYTE byte = 0;
        D_WORD word = 0;
        DDWORD dword = 0;
        float a = 0, b = 0, c = 0;
        DVector v{};
        DRotation r{};
        std::string s1, s2;
        bool null_hstring = false;
        std::string hstring;
        HOBJECT refs[4]{};
        HOBJECT plain = reinterpret_cast<HOBJECT>(1);
        DBYTE nested_byte = 0;
        DDWORD nested_word = 0;
    };
    void read_all(ServerAdapter& s, HMESSAGEREAD h, Loaded& out)
    {
        out.byte = s.ReadFromMessageByte(h);
        out.word = s.ReadFromMessageWord(h);
        out.dword = s.ReadFromMessageDWord(h);
        out.a = s.ReadFromMessageFloat(h);
        out.b = s.ReadFromMessageFloat(h);
        out.c = s.ReadFromMessageFloat(h);
        s.ReadFromMessageVector(h, &out.v);
        s.ReadFromMessageRotation(h, &out.r);
        out.s1 = s.ReadFromMessageString(h);
        char buffer[1] = {'z'};
        s.ReadFromMessageStringBuffer(h, buffer, 1); // Empty string fits a one-byte buffer.
        out.s2 = buffer;
        HSTRING null = s.ReadFromMessageHString(h);
        out.null_hstring = !null;
        HSTRING text = s.ReadFromMessageHString(h);
        if (text)
            out.hstring = s.GetStringData(text);
        s.FreeString(text);
        for (auto& ref : out.refs)
            s.ReadFromLoadSaveMessageObject(h, &ref);
        out.plain = s.ReadFromMessageObject(h);
        auto nested = s.ReadFromMessageHMessageRead(h);
        out.nested_byte = s.ReadFromMessageByte(nested);
        out.nested_word = s.ReadFromMessageDWord(nested);
        s.EndHMessageRead(nested);
    }

    bool golden_layout()
    {
        GameSession s;
        CHECK(setup(s));
        Fixture f;
        CHECK(make_fixture(s, f));
        Bytes bytes;
        SavePayloadReport report;
        CHECK(save_all(s, f, bytes, report));
        CHECK(report.error.empty() && hooks.last_param == 0);
        CHECK(bytes == golden() && report.bytes == bytes.size());
        CHECK(report.class_name == "SaveMessageProbe" && report.references == 1 && report.null_references == 3);
        CHECK(report.plain_objects == 1 && report.nested == 1);
        CHECK(s.server.message_count() == 0 && s.server.string_count() == 0);
        // dwParam 1 (keep-alive) reaches the callback as fData.
        CHECK(s.server.save_object_payload(f.saved, 1, f.index, bytes, report) && hooks.last_param == 1);
        CHECK(bytes.empty() && report.bytes == 0);
        return true;
    }
    bool round_trip()
    {
        Bytes bytes;
        {
            GameSession s;
            CHECK(setup(s));
            Fixture f;
            CHECK(make_fixture(s, f));
            SavePayloadReport report;
            CHECK(save_all(s, f, bytes, report));
        }
        // A fresh session: identities restart, references resolve through the new table.
        GameSession s;
        CHECK(setup(s));
        auto restored = probe(s, "restored");
        auto target = probe(s, "target");
        auto third = probe(s, "third");
        SaveIndex index;
        CHECK(index.add(restored) && index.add(target) && index.add(third));
        Loaded out;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { read_all(s.server, h, out); };
        SavePayloadReport report;
        CHECK(s.server.load_object_payload(restored, 0, bytes, index, report));
        CHECK(report.error.empty() && report.consumed == bytes.size() && report.bytes == bytes.size());
        CHECK(out.byte == 0xAB && out.word == 0x1234 && out.dword == 0x89ABCDEF);
        CHECK(out.a == 1.5f && std::bit_cast<std::uint32_t>(out.b) == 0x80000000u);
        CHECK(std::bit_cast<std::uint32_t>(out.c) == 0x7FC00001u);
        CHECK(out.v.x == 1 && out.v.y == -2 && out.v.z == 0.5f);
        CHECK(out.r.m_Vec.x == 0.5f && out.r.m_Vec.y == -0.5f && out.r.m_Vec.z == 0.5f && out.r.m_Spin == -0.5f);
        CHECK(out.s1 == "ab" && out.s2.empty() && out.null_hstring && out.hstring == "x");
        CHECK(out.refs[0] == target && !out.refs[1] && !out.refs[2] && !out.refs[3] && !out.plain);
        CHECK(out.nested_byte == 7 && out.nested_word == 9);
        CHECK(report.references == 1 && report.null_references == 3 && report.plain_objects == 1 && report.nested == 1);
        CHECK(s.server.message_count() == 0 && s.server.string_count() == 0);
        return true;
    }
    bool truncation()
    {
        GameSession s;
        CHECK(setup(s));
        Fixture f;
        CHECK(make_fixture(s, f));
        Bytes bytes;
        SavePayloadReport report;
        CHECK(save_all(s, f, bytes, report));
        Loaded out;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { read_all(s.server, h, out); };
        // Every strict prefix fails: no end-of-message rewind inside MID_LOADOBJECT.
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            const Bytes prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
            CHECK(!s.server.load_object_payload(f.saved, 0, prefix, f.index, report));
            CHECK(!report.error.empty() && report.consumed <= size);
            CHECK(s.server.message_count() == 0 && s.server.string_count() == 0);
        }
        // Under-read: one trailing byte is a failure (D5), naming the counts.
        auto longer = bytes;
        longer.push_back(0);
        CHECK(!s.server.load_object_payload(f.saved, 0, longer, f.index, report));
        CHECK(report.error.find("under-read") != std::string::npos && report.consumed == bytes.size());
        // A body that reads nothing from a nonempty payload also fails.
        hooks.load = {};
        CHECK(!s.server.load_object_payload(f.saved, 0, bytes, f.index, report) && report.consumed == 0);
        CHECK(s.server.load_object_payload(f.saved, 0, {}, f.index, report) && report.error.empty());
        // Over-read on an exact payload: the first error latches and later reads fail.
        hooks.load = [&](HMESSAGEREAD h, DDWORD)
        {
            s.server.ReadFromMessageDWord(h);
            s.server.ReadFromMessageByte(h);
            s.server.ResetRead(h);
            s.server.ReadFromMessageByte(h);
        };
        const Bytes four{1, 2, 3, 4};
        CHECK(!s.server.load_object_payload(f.saved, 0, four, f.index, report));
        CHECK(report.error.find("over-read") != std::string::npos);
        // Ordinary messages keep the original end-of-message rewind.
        auto plain = s.server.StartHMessageWrite();
        s.server.WriteToMessageDWord(plain, 5);
        CHECK(s.server.ReadFromMessageDWord(plain) == 5 && s.server.ReadFromMessageDWord(plain) == 5);
        s.server.EndHMessageWrite(plain);
        return true;
    }
    bool overflow()
    {
        GameSession s;
        CHECK(setup(s));
        Fixture f;
        CHECK(make_fixture(s, f));
        auto& server = s.server;
        Bytes bytes;
        SavePayloadReport report;
        // The 64 KiB payload bound fails the save; a field never lands partially.
        DRESULT last = DE_OK;
        std::size_t written = 0;
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            for (unsigned i = 0; i < 16383; ++i)
                server.WriteToMessageDWord(h, i);
            written = 16383 * 4;
            const std::string text(5, 'q');
            last = server.WriteToMessageString(h, text.c_str()); // 9 bytes into 4 remaining.
        };
        CHECK(!server.save_object_payload(f.saved, 0, f.index, bytes, report) && last == DE_ERROR);
        CHECK(bytes.empty() && report.error.find("64 KiB") != std::string::npos && written == 65532);
        // Exactly 64 KiB is accepted.
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            for (unsigned i = 0; i < 16384; ++i)
                server.WriteToMessageDWord(h, i);
        };
        CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) && bytes.size() == 65536);
        // Strings: 4,096 bytes accepted, 4,097 refused on write.
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            const std::string text(4096, 's');
            server.WriteToMessageString(h, text.c_str());
        };
        CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) && bytes.size() == 4100);
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            const std::string text(4097, 's');
            last = server.WriteToMessageString(h, text.c_str());
        };
        CHECK(!server.save_object_payload(f.saved, 0, f.index, bytes, report) && last == DE_ERROR);
        // Oversized and NUL-bearing string payloads fail on read.
        Bytes oversized;
        put(oversized, 4097);
        oversized.resize(oversized.size() + 4097, 's');
        bool null_read = false;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { null_read = *server.ReadFromMessageString(h) == '\0'; };
        CHECK(!server.load_object_payload(f.saved, 0, oversized, f.index, report) && null_read);
        Bytes with_nul;
        put_text(with_nul, std::string_view("a\0b", 3));
        CHECK(!server.load_object_payload(f.saved, 0, with_nul, f.index, report));
        // A fixed buffer must hold the string and its terminator.
        Bytes three;
        put_text(three, "abc");
        char small[3] = {};
        DRESULT copied = DE_OK;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { copied = server.ReadFromMessageStringBuffer(h, small, 3); };
        CHECK(!server.load_object_payload(f.saved, 0, three, f.index, report) && copied == DE_ERROR && !small[0]);
        char exact[4] = {};
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { copied = server.ReadFromMessageStringBuffer(h, exact, 4); };
        CHECK(server.load_object_payload(f.saved, 0, three, f.index, report) && copied == DE_OK);
        CHECK(std::string(exact) == "abc");
        // Nested messages: 16 KiB accepted, one byte more refused on write and read.
        for (const std::size_t size : {std::size_t{16384}, std::size_t{16385}})
        {
            hooks.save = [&](HMESSAGEWRITE h, DDWORD)
            {
                auto data = server.StartHMessageWrite();
                for (std::size_t n = 0; n < size; ++n)
                    server.WriteToMessageByte(data, 1);
                last = server.WriteToMessageHMessageWrite(h, data);
                server.EndHMessageWrite(data);
            };
            CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) == (size == 16384));
            CHECK(last == (size == 16384 ? DE_OK : DE_ERROR));
        }
        Bytes nested;
        put(nested, 16385);
        nested.resize(nested.size() + 16385);
        HMESSAGEREAD reader = nullptr;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { reader = server.ReadFromMessageHMessageRead(h); };
        CHECK(!server.load_object_payload(f.saved, 0, nested, f.index, report) && !reader);
        Bytes short_nested;
        put(short_nested, 8);
        put(short_nested, 1);
        CHECK(!server.load_object_payload(f.saved, 0, short_nested, f.index, report) && !reader);
        // A load payload above the message bound is refused before dispatch.
        Bytes huge(65537);
        bool called = false;
        hooks.load = [&](HMESSAGEREAD, DDWORD) { called = true; };
        CHECK(!server.load_object_payload(f.saved, 0, huge, f.index, report) && !called);
        CHECK(server.message_count() == 0 && server.string_count() == 0);
        return true;
    }
    bool references()
    {
        GameSession s;
        CHECK(setup(s));
        Fixture f;
        CHECK(make_fixture(s, f));
        auto& server = s.server;
        SavePayloadReport report;
        Bytes payload;
        put(payload, 2);
        HOBJECT read = reinterpret_cast<HOBJECT>(1);
        DRESULT result = DE_OK;
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { result = server.ReadFromLoadSaveMessageObject(h, &read); };
        // Index 2 is the pending object: it still exists until the frame ends.
        CHECK(server.load_object_payload(f.saved, 0, payload, f.index, report) && read == f.pending);
        CHECK(s.objects.finish_frame());
        CHECK(!server.load_object_payload(f.saved, 0, payload, f.index, report) && !read && result == DE_ERROR);
        payload.clear();
        put(payload, 3); // Outside the restored list.
        CHECK(!server.load_object_payload(f.saved, 0, payload, f.index, report) && !read);
        CHECK(report.error.find("outside the restored list") != std::string::npos);
        payload.clear();
        put(payload, 0xFFFFFFFFu);
        CHECK(server.load_object_payload(f.saved, 0, payload, f.index, report) && !read);
        CHECK(report.null_references == 1 && report.references == 0);
        // Null destinations are refused.
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { result = server.ReadFromLoadSaveMessageObject(h, nullptr); };
        CHECK(!server.load_object_payload(f.saved, 0, payload, f.index, report) && result == DE_INVALIDPARAMS);
        // A plain object field must hold the NULL sentinel written by saves.
        hooks.load = [&](HMESSAGEREAD h, DDWORD) { read = server.ReadFromMessageObject(h); };
        CHECK(server.load_object_payload(f.saved, 0, payload, f.index, report) && !read && report.plain_objects == 1);
        payload.clear();
        put(payload, 0);
        CHECK(!server.load_object_payload(f.saved, 0, payload, f.index, report) && !read);
        // A stale handle saves as NULL.
        hooks.save = [&](HMESSAGEWRITE h, DDWORD) { server.WriteToLoadSaveMessageObject(h, f.pending); };
        Bytes bytes;
        CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) && report.null_references == 1);
        CHECK(bytes == Bytes(4, 0xFF));
        CHECK(server.message_count() == 0);
        return true;
    }
    bool context()
    {
        GameSession s;
        CHECK(setup(s));
        Fixture f;
        CHECK(make_fixture(s, f));
        auto& server = s.server;
        SavePayloadReport report;
        Bytes bytes;
        // Outside save/load dispatch: reference primitives are refused and latch.
        auto plain = server.StartHMessageWrite();
        CHECK(server.WriteToLoadSaveMessageObject(plain, f.listed) == DE_ERROR);
        CHECK(server.WriteToMessageDWord(plain, 1) == DE_INVALIDPARAMS); // Latched.
        server.EndHMessageWrite(plain);
        plain = server.StartHMessageWrite();
        server.WriteToMessageDWord(plain, 0);
        HOBJECT read = reinterpret_cast<HOBJECT>(1);
        CHECK(server.ReadFromLoadSaveMessageObject(plain, &read) == DE_ERROR && !read);
        server.EndHMessageWrite(plain);
        plain = server.StartHMessageWrite();
        CHECK(server.WriteToMessageObject(plain, f.listed) == DE_ERROR);
        CHECK(server.unavailable_services.contains("WriteToMessageObject outside MID_SAVEOBJECT"));
        server.EndHMessageWrite(plain);
        plain = server.StartHMessageWrite();
        server.WriteToMessageDWord(plain, 0xFFFFFFFFu);
        CHECK(!server.ReadFromMessageObject(plain));
        CHECK(server.unavailable_services.contains("ReadFromMessageObject outside MID_LOADOBJECT"));
        server.EndHMessageWrite(plain);
        // A nested message created inside the save is ordinary: references are refused
        // and the refusal fails the whole save.
        DRESULT nested_result = DE_OK;
        hooks.save = [&](HMESSAGEWRITE, DDWORD)
        {
            auto data = server.StartHMessageWrite();
            nested_result = server.WriteToLoadSaveMessageObject(data, f.listed);
            server.EndHMessageWrite(data);
        };
        CHECK(!server.save_object_payload(f.saved, 0, f.index, bytes, report) && nested_result == DE_ERROR);
        CHECK(report.error.find("outside MID_SAVEOBJECT") != std::string::npos);
        // The save writer is borrowed: it cannot be released, routed or reused after dispatch.
        HMESSAGEWRITE kept = nullptr;
        DRESULT routed = DE_OK;
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            kept = h;
            server.EndHMessageWrite(h);
            server.EndHMessageRead(h);
            routed = server.EndMessage(h);
            server.WriteToMessageByte(h, 3);
        };
        CHECK(!server.save_object_payload(f.saved, 0, f.index, bytes, report) && routed == DE_INVALIDPARAMS);
        CHECK(report.error.find("borrowed") != std::string::npos);
        hooks.save = [&](HMESSAGEWRITE h, DDWORD)
        {
            kept = h;
            server.WriteToMessageByte(h, 3);
        };
        CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) && bytes == Bytes{3});
        CHECK(server.WriteToMessageByte(kept, 4) == DE_INVALIDPARAMS && server.message_count() == 0);
        // The load reader is not writable.
        DRESULT load_write = DE_OK;
        hooks.load = [&](HMESSAGEREAD h, DDWORD)
        {
            load_write = server.WriteToMessageByte(h, 1);
            server.ReadFromMessageByte(h);
        };
        CHECK(!server.load_object_payload(f.saved, 0, Bytes{1}, f.index, report) && load_write == DE_INVALIDPARAMS);
        // A save reader cannot resolve load references and vice versa.
        hooks.save = [&](HMESSAGEWRITE h, DDWORD) { server.ReadFromLoadSaveMessageObject(h, &read); };
        CHECK(!server.save_object_payload(f.saved, 0, f.index, bytes, report));
        // Nested dispatch from a callback, dead objects and an unbound adapter are refused.
        bool nested_ok = true;
        hooks.save = [&](HMESSAGEWRITE, DDWORD)
        {
            SavePayloadReport inner;
            Bytes other;
            nested_ok = server.save_object_payload(f.listed, 0, f.index, other, inner);
        };
        CHECK(server.save_object_payload(f.saved, 0, f.index, bytes, report) && !nested_ok);
        hooks.save = {};
        CHECK(s.objects.remove(f.unlisted) && !server.save_object_payload(f.unlisted, 0, f.index, bytes, report));
        CHECK(report.error.find("not alive") != std::string::npos);
        CHECK(!server.load_object_payload(nullptr, 0, {}, f.index, report));
        {
            GameSession other;
            CHECK(!other.initialize()); // Only one bound session per thread.
            CHECK(!other.server.save_object_payload(f.saved, 0, f.index, bytes, report));
            CHECK(report.error.find("not the bound session") != std::string::npos);
        }
        CHECK(server.message_count() == 0 && server.string_count() == 0);
        return true;
    }
    bool nested_messages()
    {
        GameSession s;
        CHECK(setup(s));
        auto& server = s.server;
        // Client-blob style: writer → nested in a message → read back as an owned reader.
        auto blob = server.StartHMessageWrite();
        server.WriteToMessageString(blob, "objectives");
        auto outer = server.StartHMessageWrite();
        CHECK(server.WriteToMessageHMessageWrite(outer, blob) == DE_OK);
        CHECK(server.WriteToMessageHMessageWrite(outer, outer) == DE_INVALIDPARAMS); // Latches outer.
        server.EndHMessageWrite(outer);
        outer = server.StartHMessageWrite();
        CHECK(server.WriteToMessageHMessageWrite(outer, blob) == DE_OK);
        server.EndHMessageWrite(blob);
        auto reader = server.ReadFromMessageHMessageRead(outer);
        CHECK(reader && server.message_count() == 2);
        CHECK(server.WriteToMessageByte(reader, 1) == DE_INVALIDPARAMS);      // Readers are read-only.
        server.EndHMessageWrite(reader);                                     // Wrong release call: refused.
        CHECK(server.message_count() == 2 && server.ReadFromMessageString(reader) == std::string("objectives"));
        // Forward a reader (PlayerObj Load → SMSG_CLIENTSAVEDATA) without consuming its owner.
        auto forward = server.StartHMessageWrite();
        CHECK(server.WriteToMessageHMessageRead(forward, reader) == DE_OK);
        CHECK(server.WriteToMessageHMessageWrite(forward, reader) == DE_INVALIDPARAMS); // Reader is not a writer.
        server.EndHMessageWrite(forward);
        forward = server.StartHMessageWrite();
        auto writer = server.StartHMessageWrite();
        CHECK(server.WriteToMessageHMessageRead(forward, writer) == DE_INVALIDPARAMS); // Writer is not a reader.
        server.EndHMessageRead(writer); // Not a nested reader: refused.
        CHECK(server.message_count() == 4);
        server.EndHMessageWrite(writer);
        server.EndHMessageWrite(forward);
        server.EndHMessageRead(reader);
        server.EndHMessageRead(reader); // Stale: no effect.
        server.EndHMessageWrite(outer);
        CHECK(server.message_count() == 0);
        CHECK(server.message_stats.created == server.message_stats.released);
        // Routed messages and typed side-cars never nest.
        auto target = probe(s, "target");
        auto routed = server.StartMessageToObject(nullptr, target, 1);
        outer = server.StartHMessageWrite();
        CHECK(server.WriteToMessageHMessageWrite(outer, routed) == DE_INVALIDPARAMS);
        server.EndHMessageWrite(outer);
        server.EndHMessageWrite(routed);
        // Nested readers created during a load are released with the load (and leak-checked by ASan).
        Bytes payload;
        put(payload, 2);
        payload.push_back(4);
        payload.push_back(5);
        SaveIndex index;
        CHECK(index.add(target));
        SavePayloadReport report;
        DBYTE values[2]{};
        hooks.load = [&](HMESSAGEREAD h, DDWORD)
        {
            auto nested = server.ReadFromMessageHMessageRead(h);
            values[0] = server.ReadFromMessageByte(nested);
            values[1] = server.ReadFromMessageByte(nested);
            server.EndHMessageRead(nested);
        };
        CHECK(server.load_object_payload(target, 0, payload, index, report) && values[0] == 4 && values[1] == 5);
        CHECK(report.nested == 1 && server.message_count() == 0);
        // Object references inside a nested message are ordinary and refused.
        hooks.load = [&](HMESSAGEREAD h, DDWORD)
        {
            auto nested = server.ReadFromMessageHMessageRead(h);
            HOBJECT object = nullptr;
            server.ReadFromLoadSaveMessageObject(nested, &object);
            server.EndHMessageRead(nested);
        };
        payload.clear();
        put(payload, 4);
        put(payload, 0);
        CHECK(!server.load_object_payload(target, 0, payload, index, report));
        CHECK(server.message_count() == 0 && server.string_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::pair<const char*, bool (*)()> cases[] = {{"golden_layout", golden_layout},
                                                        {"round_trip", round_trip},
                                                        {"truncation", truncation},
                                                        {"overflow", overflow},
                                                        {"references", references},
                                                        {"context", context},
                                                        {"nested", nested_messages}};
    if (argc != 2)
        return 2;
    for (const auto& [name, test] : cases)
        if (std::string_view(argv[1]) == name)
        {
            if (!test())
                return 1;
            std::cout << "PASS " << name << '\n';
            return 0;
        }
    return 2;
}
