#include "game/original/GameStartPoint.h"
#include "game/original/Key.h"
#include "game/original/PathPoint.h"
#include "game/server_adapter.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>

using namespace OpenBlood2;
using Bytes = std::vector<std::uint8_t>;
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
    struct Writer
    {
        Bytes data;
        void byte(unsigned v)
        {
            data.push_back(static_cast<std::uint8_t>(v));
        }
        void u16(unsigned v)
        {
            byte(v);
            byte(v >> 8);
        }
        void u32(std::uint32_t v)
        {
            u16(v);
            u16(v >> 16);
        }
        void real(float f)
        {
            u32(std::bit_cast<std::uint32_t>(f));
        }
        void vec(float x = 0, float y = 0, float z = 0)
        {
            real(x);
            real(y);
            real(z);
        }
        void str(std::string_view s)
        {
            u16(static_cast<unsigned>(s.size()));
            data.insert(data.end(), s.begin(), s.end());
        }
        void cstr(std::string_view s)
        {
            data.insert(data.end(), s.begin(), s.end());
            byte(0);
        }
        void zero(std::size_t n)
        {
            data.resize(data.size() + n);
        }
        void append(const Writer& w)
        {
            data.insert(data.end(), w.data.begin(), w.data.end());
        }
        void patch(std::size_t p, std::uint32_t n)
        {
            for (unsigned i = 0; i < 4; ++i)
                data[p + i] = static_cast<std::uint8_t>(n >> (i * 8));
        }
    };
    Writer prop(std::string_view name, unsigned type, const Writer& value)
    {
        Writer p;
        p.str(name);
        p.byte(type);
        p.u32(0);
        p.u16(static_cast<unsigned>(value.data.size()));
        p.append(value);
        return p;
    }
    Writer string_prop(std::string_view name, std::string_view value)
    {
        Writer v;
        v.str(value);
        return prop(name, 0, v);
    }
    Writer object(std::string_view cls, std::initializer_list<Writer> properties)
    {
        Writer v;
        v.str(cls);
        v.u32(static_cast<unsigned>(properties.size()));
        for (const auto& p : properties)
            v.append(p);
        Writer out;
        out.u16(static_cast<unsigned>(v.data.size()));
        out.append(v);
        return out;
    }
    Writer dat(bool gameplay = false)
    {
        Writer w;
        w.u32(56);
        w.u32(48);
        w.u32(0);
        w.u32(0);
        w.zero(32);
        Writer pos;
        pos.vec(5, 6, 7);
        Writer rot;
        rot.vec(.3f, .7f, .2f);
        rot.real(1);
        Writer integer;
        integer.real(2);
        Writer value;
        value.real(1.5);
        Writer yes;
        yes.byte(1);
        w.u32(gameplay ? 4 : 1);
        if (gameplay)
        {
            w.append(object("GameStartPoint", {string_prop("Name", "Start"), prop("Pos", 1, pos),
                                               prop("Rotation", 7, rot), prop("TeamID", 6, integer)}));
            w.append(object("Key", {string_prop("Name", "Key0"), prop("TimeStamp", 3, value),
                                    string_prop("MessageTarget", "door"), string_prop("MessageName", "OPEN")}));
            w.append(object("PathPoint",
                            {string_prop("Name", "SMELL0"), prop("Pos", 1, pos), string_prop("ActionMessage", "GO")}));
            w.append(object("NotPorted", {string_prop("Name", "missing")}));
        }
        else
        {
            w.append(object("TypedFixture", {string_prop("Name", "fixture"), prop("Pos", 1, pos), prop("Color", 2, pos),
                                             prop("Rotation", 7, rot), prop("Long", 6, integer),
                                             prop("Flags", 4, integer), prop("Real", 3, value), prop("Bool", 5, yes)}));
        }
        w.patch(8, static_cast<unsigned>(w.data.size()));
        const auto start = w.data.size();
        w.u32(0);
        w.zero(32);
        w.u32(0);
        w.str("SyntheticTriangle");
        w.u32(0);
        for (unsigned n : {3, 1, 1, 0, 1, 0, 3, 0, 0, 0})
            w.u32(n);
        w.u32(0);
        w.vec();
        w.vec(10, 10, 10);
        w.vec();
        w.u32(6);
        w.u32(1);
        w.cstr("t.dtx");
        w.byte(3);
        w.byte(0); // Polygon vertex counts.
        w.vec(0, 1, 0);
        w.real(0); // Plane.
        w.vec();
        w.vec(1, 0, 0);
        w.vec(0, 0, 1);
        w.zero(24);
        w.u32(0xffffffff);
        w.zero(8); // Surface packed payload may contain NaN bits.
        w.u16(0);
        w.u32(0);
        w.u32(0);
        w.u32(0);
        w.byte(0);
        w.u16(0);
        w.zero(12);
        w.u32(0);
        for (unsigned i = 0; i < 3; ++i)
        {
            w.u16(i);
            w.zero(3);
        }
        w.vec();
        w.vec(10, 0, 0);
        w.vec(0, 0, 10);
        w.patch(start, static_cast<unsigned>(w.data.size()));
        w.u32(0); // No additional models.
        w.vec(-10, -10, -10);
        w.vec(10, 10, 10);
        return w;
    }
    Writer rez(const Bytes& payload, std::string_view name = "sample", std::uint32_t extension = 0x444154)
    {
        Writer w;
        w.zero(168);
        w.data[0] = w.data[62] = w.data[124] = 13;
        w.data[1] = w.data[63] = w.data[125] = 10;
        w.data[126] = 26;
        w.patch(127, 1);
        w.data.insert(w.data.end(), payload.begin(), payload.end());
        const auto dir = w.data.size();
        w.u32(0);
        w.u32(168);
        w.u32(static_cast<unsigned>(payload.size()));
        w.u32(0);
        w.u32(1);
        w.u32(extension);
        w.u32(0);
        w.cstr(name);
        w.cstr("");
        w.patch(131, static_cast<unsigned>(dir));
        w.patch(135, static_cast<unsigned>(w.data.size() - dir));
        return w;
    }
    bool save(const std::filesystem::path& path, const Bytes& data)
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return bool(f);
    }
    bool rez_structure()
    {
        auto fixture = rez({1, 2, 3});
        std::vector<Resource> entries;
        std::string error, key;
        CHECK(read_rez(fixture.data, entries, error));
        CHECK(entries.size() == 1);
        CHECK(entries[0].path == "sample.dat");
        CHECK(entries[0].size == 3);
        CHECK(resource_path("WORLDS\\One.DAT", key));
        CHECK(key == "worlds/one.dat");
        for (auto s : {"../x", "/x", "c:\\x", "a//b", "a/./b", "a/../b", ""})
            CHECK(!resource_path(s, key));
        return true;
    }
    bool rez_rejection()
    {
        auto valid = rez({1, 2, 3});
        std::vector<Resource> entries;
        std::string error;
        for (std::size_t n = 0; n < valid.data.size(); ++n)
            CHECK(!read_rez(std::span(valid.data).first(n), entries, error));
        for (auto offset : {0u, 126u, 127u, 131u, 135u})
        {
            auto bad = valid;
            bad.data[offset] ^= 0xff;
            CHECK(!read_rez(bad.data, entries, error));
            CHECK(entries.empty());
        }
        auto cycle = valid;
        cycle.data.resize(168);
        cycle.u32(1);
        cycle.u32(168);
        cycle.u32(18);
        cycle.u32(0);
        cycle.cstr("x");
        cycle.patch(131, 168);
        cycle.patch(135, 18);
        CHECK(!read_rez(cycle.data, entries, error));
        auto duplicate = valid;
        Writer record;
        record.data.assign(valid.data.begin() + 171, valid.data.end());
        duplicate.append(record);
        duplicate.patch(135, static_cast<unsigned>(record.data.size() * 2));
        CHECK(!read_rez(duplicate.data, entries, error));
        CHECK(!error.empty());
        auto range = valid;
        range.patch(175, UINT32_MAX);
        CHECK(!read_rez(range.data, entries, error));
        return true;
    }
    bool resource_mount(const std::filesystem::path& root)
    {
        std::string error;
        auto a = rez({1, 2, 3}), b = rez({7, 8});
        CHECK(save(root / "a.rez", a.data));
        CHECK(save(root / "b.rez", b.data));
        Resources r;
        CHECK(r.mount(root / "a.rez", error));
        CHECK(r.mount(root / "b.rez", error));
        Bytes read;
        CHECK(r.read("SAMPLE.DAT", read, error));
        CHECK(read == Bytes({7, 8}));
        CHECK(r.find("sample.dat")->overrides == 1);
        CHECK(!r.find("missing/sample.dat"));
        CHECK(!r.read("../sample.dat", read, error));
        CHECK(read.empty());
        auto bad = b;
        bad.data[127] = 2;
        CHECK(save(root / "bad.rez", bad.data));
        CHECK(!r.mount(root / "bad.rez", error));
        CHECK(r.mounts().size() == 2);
        CHECK(save(root / "b.rez", {0}));
        CHECK(!r.read("sample.dat", read, error));
        CHECK(read.empty());
        return true;
    }
    bool world_properties()
    {
        auto fixture = dat();
        World w;
        std::string error;
        CHECK(read_world(fixture.data, w, error));
        CHECK(w.objects.size() == 1);
        const auto& o = w.objects[0];
        CHECK(std::get<std::int32_t>(*o.find("Long")) == 2);
        CHECK(std::get<bool>(*o.find("Bool")));
        CHECK(std::get<DVector>(*o.find("pos")).z == 7);
        CHECK(std::get<std::string>(*o.find("Name")) == "fixture");
        GenericProp p;
        CHECK(generic_property(*o.find("Rotation"), p));
        CHECK(p.m_Vec.y == .7f);
        const auto q = rotation_from_euler({0, 1.57079632679f, 0});
        CHECK(std::abs(q.m_Vec.y - .70710678f) < 1e-6f);
        CHECK(std::abs(q.m_Spin - .70710678f) < 1e-6f);
        // Rotation's forward column agrees with original yaw*pitch*roll matrix.
        const auto a = rotation_from_euler({.3f, .7f, .2f});
        const float x = a.m_Vec.x, y = a.m_Vec.y, z = a.m_Vec.z;
        CHECK(std::abs(2 * (x * z + a.m_Spin * y) - std::cos(.3f) * std::sin(.7f)) < 1e-6f);
        CHECK(std::abs(2 * (y * z - a.m_Spin * x) + std::sin(.3f)) < 1e-6f);
        return true;
    }
    bool world_geometry()
    {
        World w;
        std::string error;
        CHECK(read_world(dat().data, w, error));
        CHECK(w.models.size() == 1);
        const auto& m = w.models[0];
        CHECK(m.points.size() == 3);
        CHECK(m.polygons[0].base_points == 3);
        CHECK(m.points[1].x == 10);
        CHECK(m.polygons[0].points == std::vector<std::uint16_t>({0, 1, 2}));
        CHECK(m.planes[0].normal.y == 1);
        CHECK(m.textures[0] == "t.dtx");
        CHECK(w.min.x == -10);
        return true;
    }
    bool world_rejection()
    {
        auto valid = dat();
        World w;
        std::string error;
        for (std::size_t n = 0; n < valid.data.size(); ++n)
            CHECK(!read_world(std::span(valid.data).first(n), w, error));
        for (auto offset : {0u, 4u, 8u, 12u, 48u})
        {
            auto bad = valid;
            bad.patch(offset, UINT32_MAX);
            CHECK(!read_world(bad.data, w, error));
            CHECK(w.objects.empty());
            CHECK(w.models.empty());
        }
        auto bad = valid;
        bad.data.push_back(0);
        CHECK(!read_world(bad.data, w, error));
        // Corrupt the first point's index in the only polygon.
        bad = valid;
        bad.data[bad.data.size() - 28 - 36 - 15] = 0xff;
        bad.data[bad.data.size() - 28 - 36 - 14] = 0xff;
        CHECK(!read_world(bad.data, w, error));
        return true;
    }
    bool mutations()
    {
        auto valid = dat();
        World w;
        std::string error;
        std::mt19937 random(2);
        for (unsigned i = 0; i < 1500; ++i)
        {
            auto changed = valid.data;
            const auto at = random() % changed.size();
            changed[at] ^= static_cast<std::uint8_t>(1u << (random() % 8));
            if (read_world(changed, w, error))
            {
                for (const auto& m : w.models)
                    for (const auto& p : m.polygons)
                        for (auto index : p.points)
                            CHECK(index < m.points.size());
            }
            else
                CHECK(w.objects.empty() && w.models.empty() && !error.empty());
        }
        return true;
    }
    bool session_lifecycle(const std::filesystem::path& root)
    {
        auto payload = dat(true);
        auto archive = rez(payload.data, "fixture");
        CHECK(save(root / "world.rez", archive.data));
        GameSession game;
        CHECK(game.initialize());
        std::string error;
        CHECK(game.resources.mount(root / "world.rez", error));
        // Local transport uses Worlds prefix; package a real directory fixture.
        const auto read32 = [&](std::size_t p)
        {
            return unsigned(archive.data[p]) | (unsigned(archive.data[p + 1]) << 8) |
                   (unsigned(archive.data[p + 2]) << 16) | (unsigned(archive.data[p + 3]) << 24);
        };
        const auto directory = read32(131), size = read32(135);
        const auto rootdir = archive.data.size();
        archive.u32(1);
        archive.u32(directory);
        archive.u32(size);
        archive.u32(0);
        archive.cstr("worlds");
        archive.patch(131, static_cast<unsigned>(rootdir));
        archive.patch(135, 23);
        CHECK(save(root / "world2.rez", archive.data));
        CHECK(game.resources.mount(root / "world2.rez", error));
        CHECK(game.request_world("fixture", error));
        CHECK(game.world.objects.size() == 4);
        CHECK(game.objects.snapshot().size() == 3);
        const auto old = game.entries[0].object;
        ObjectSelection selection{old};
        auto* start = dynamic_cast<GameStartPoint*>(game.objects.get(old)->game.get());
        CHECK(start);
        CHECK(start->GetTeamID() == 2);
        CHECK(start->GetPitchYawRoll().y == .7f);
        CHECK(std::string(game.server.GetStringData(start->GetName())) == "Start");
        CHECK(std::string(game.objects.get(old)->state.m_Name) == "Blood2StartPoint");
        auto path = game.entries[2].object;
        CHECK(game.objects.get(path)->dimensions.x == 20);
        CHECK(game.objects.get(path)->scheduled);
        CHECK(game.step(19));
        CHECK(game.objects.get(path));
        CHECK(game.step(2));
        CHECK(!game.objects.get(path));
        CHECK(!game.run_world(error));
        CHECK(game.state == SessionState::Blocked);
        CHECK(error.find("player entry: Model resource not mounted") != std::string::npos);
        CHECK(!game.request_world("../bad", error));
        CHECK(game.objects.get(old)); // Failed parse doesn't discard current world.
        CHECK(game.request_world("fixture", error));
        selection.refresh(game.objects);
        CHECK(!selection.id);
        CHECK(!game.objects.get(old));
        game.unload_world();
        CHECK(game.objects.snapshot().empty());
        CHECK(game.server.string_count() == 0);
        CHECK(game.server.message_count() == 0);
        CHECK(game.server.list_count() == 0);
        return true;
    }
    bool session_messages()
    {
        WorldMessage m;
        WorldRequest r;
        std::string error;
        CHECK(original_world_request("01_SubwayA", 0, 1, 1, m));
        CHECK(m.id == 108);
        CHECK(decode_world_request(m, r, error));
        CHECK(r.filename == "worlds/01_subwaya.dat");
        CHECK(r.difficulty == 1);
        for (std::size_t n = 0; n < m.bytes.size(); ++n)
        {
            auto bad = m;
            bad.bytes.resize(n);
            CHECK(!decode_world_request(bad, r, error));
        }
        auto bad = m;
        bad.bytes.push_back(1);
        CHECK(!decode_world_request(bad, r, error));
        CHECK(original_world_request("one", 0, 2, 1, m));
        CHECK(!decode_world_request(m, r, error));
        CHECK(original_world_request("../one", 0, 1, 1, m));
        CHECK(!decode_world_request(m, r, error));
        return true;
    }
    struct Receiver : BaseClass
    {
        int received = 0;
        std::string payload;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD id, HMESSAGEREAD read) override
        {
            if (id == 1000)
            {
                ++received;
                const auto text = g_pServerDE->ReadFromMessageHString(read);
                const auto* value = g_pServerDE->GetStringData(text);
                payload = value ? value : "";
                g_pServerDE->FreeString(text);
            }
            return 1;
        }
    };
    bool original_trigger()
    {
        GameSession game;
        CHECK(game.initialize());
        const ClassInfo receiver{"Receiver",
                                 game.objects.find_class("BaseClass"),
                                 []() -> std::unique_ptr<BaseClass> { return std::make_unique<Receiver>(); },
                                 {}};
        CHECK(game.objects.register_class(receiver));
        const auto target = game.objects.create(&receiver, {}, Creation::World, {{"Name", std::string("door")}});
        const auto start =
            game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                {{"TriggerTarget", std::string("door")}, {"TriggerMessage", std::string("OPEN")}});
        CHECK(target && start);
        auto* source = dynamic_cast<GameStartPoint*>(game.objects.get(start)->game.get());
        CHECK(source);
        auto* received = dynamic_cast<Receiver*>(game.objects.get(target)->game.get());
        CHECK(received);
        const auto strings = game.server.string_count();
        source->SendTrigger();
        CHECK(received->received == 1);
        CHECK(received->payload == "OPEN");
        CHECK(game.server.string_count() == strings);
        CHECK(game.server.list_count() == 0);
        CHECK(game.server.message_count() == 0);
        const auto remover =
            game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                {{"TriggerTarget", std::string("door")}, {"TriggerMessage", std::string("remove")}});
        game.objects.get(target)->user_flags = 7;
        dynamic_cast<GameStartPoint*>(game.objects.get(remover)->game.get())->SendTrigger();
        CHECK(game.objects.get(target)->user_flags == 0);
        CHECK(game.objects.get(target)->life == Life::Pending);
        CHECK(game.step(0));
        CHECK(!game.objects.get(target));
        std::size_t count = 0;
        HOBJECT current = nullptr;
        while ((current = game.server.GetNextObject(current)))
            ++count;
        CHECK(count == 2);
        CHECK(!game.server.GetNextObject(target));
        game.unload_world();
        CHECK(game.server.string_count() == 0);
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(c, error));
        CHECK(game.request_world("01_SubwayA", error));
        CHECK(game.world.version == 56);
        CHECK(game.world.objects.size() == 580);
        CHECK(game.world.models.size() == 116);
        CHECK(game.objects.snapshot().size() == 151);
        CHECK(game.world.models[0].points.size() == 4996);
        CHECK(game.world.models[0].polygons.size() == 3367);
        CHECK(game.resources.find(game.world_path)->overrides == 1);
        CHECK(game.entries[0].state == EntryState::Created);
        CHECK(game.step(.016));
        CHECK(game.server.service_calls["SetObjectDims"] == 30);
        CHECK(game.server.service_calls["GetPropGeneric"] > 0);
        for (const auto& [name, count] : game.server.service_calls)
            std::cout << "Service " << name << ": " << count << '\n';
        std::size_t missing = 0;
        for (const auto& use : game.resource_uses)
            if (!use.indexed)
            {
                ++missing;
                std::cout << "Missing " << use.path << '\n';
            }
        std::cout << "Resource references: " << game.resource_uses.size() << "; missing: " << missing << '\n';
        auto id = game.entries[0].object;
        CHECK(game.request_world("01_SubwayA", error));
        CHECK(!game.objects.get(id));
        CHECK(!game.run_world(error));
        std::cout << error << '\n';
        game.unload_world();
        CHECK(game.server.string_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string name = argv[1];
    bool ok = false;
    std::filesystem::path root = argc > 2 ? path_from_utf8(argv[2]) : std::filesystem::path{};
    if (name == "private")
        ok = private_world(root);
    else
    {
        std::error_code ec;
        if (!root.empty())
            std::filesystem::create_directories(root, ec);
        if (ec)
            return 2;
        if (name == "rez_structure")
            ok = rez_structure();
        else if (name == "rez_rejection")
            ok = rez_rejection();
        else if (name == "resource_mount")
            ok = resource_mount(root);
        else if (name == "world_properties")
            ok = world_properties();
        else if (name == "world_geometry")
            ok = world_geometry();
        else if (name == "world_rejection")
            ok = world_rejection();
        else if (name == "mutations")
            ok = mutations();
        else if (name == "session_lifecycle")
            ok = session_lifecycle(root);
        else if (name == "session_messages")
            ok = session_messages();
        else if (name == "original_trigger")
            ok = original_trigger();
        else
            return 2;
    }
    if (ok)
        std::cout << "PASS " << name << '\n';
    return ok ? 0 : 1;
}
