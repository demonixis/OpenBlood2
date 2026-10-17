#include "audio_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/AmmoPickups.h"
#include "game/original/CameraObj.h"
#include "game/original/ExitHint.h"
#include "game/original/GameStartPoint.h"
#include "game/original/Key.h"
#include "game/original/PathPoint.h"
#include "game/original/SmellHint.h"
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
    Writer dat(bool gameplay = false, bool sounds = false, bool sprites = false, bool destination = false,
               unsigned trial = 0, const Surface* effect = nullptr, unsigned surface_count = 1)
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
        w.u32(trial ? (trial == 2 ? 6 : 7) : destination ? 5 : sprites ? 6 : gameplay || sounds ? 4 : 1);
        if (trial)
        {
            Writer arrival;
            arrival.vec(trial == 3 ? 10000 : 0, 100, 0);
            if (trial != 2)
                w.append(object("GameStartPoint", {string_prop("Name", "arrival"), prop("Pos", 1, arrival),
                                                   string_prop("TriggerTarget", "entry-camera"),
                                                   string_prop("TriggerMessage", trial == 6 ? "TRIGGER" : "ON")}));
            if (trial == 6)
                w.append(object("ExitTrigger", {string_prop("Name", "entry-camera"), string_prop("NextWorld", "trial1"),
                                                prop("ObjectActivate", 5, yes)}));
            else
                w.append(object(trial == 5 ? "NotPorted" : "CameraObj", {string_prop("Name", "entry-camera")}));
            w.append(object("PathPoint", {string_prop("Name", "WALK0"), prop("Pos", 1, arrival)}));
            w.append(object("SmellHint", {string_prop("Name", "scent")}));
            w.append(object("ExitHint", {string_prop("Name", "exit")}));
            w.append(object("BulletAmmoPU", {string_prop("Name", "bullets"), prop("Pos", 1, arrival)}));
            w.append(object("Sound", {string_prop("Filename", trial == 4 ? "absent.wav" : "cue.wav")}));
        }
        else if (destination)
        {
            w.append(object("DestinationProbe", {string_prop("Name", "owned"), prop("Pos", 1, pos)}));
            w.append(object("DirLight", {string_prop("Name", "spot")}));
            w.append(object("NotPorted", {string_prop("Name", "unknown")}));
            w.append(object("Sound", {prop("TypeAction", 6, integer)}));
            w.append(object("GameStartPoint", {string_prop("Name", "arrival"), prop("Pos", 1, pos)}));
        }
        else if (sprites)
        {
            Writer no;
            no.byte(0);
            w.append(object("DetailSprite", {string_prop("Filename", "valid.spr"), prop("Pos", 1, pos)}));
            w.append(object("DetailSprite", {string_prop("Filename", "missing.spr")}));
            w.append(object("DetailSprite", {string_prop("Filename", "bad.spr")}));
            w.append(object("DetailSprite", {string_prop("Filename", "valid.spr"), prop("DifficultyMed", 5, no)}));
            w.append(object("DetailSprite", {string_prop("Filename", "valid.spr"), prop("TypeAction", 5, yes)}));
            w.append(object("DetailSprite", {string_prop("Filename", "frame-missing.spr")}));
        }
        else if (sounds)
        {
            w.append(object("Sound", {string_prop("Filename", "cue.wav"), prop("Pos", 1, pos)}));
            w.append(object("Sound", {string_prop("Filename", "missing.wav")}));
            w.append(object("Sound", {string_prop("Filename", "cue.wav"), prop("FileStream", 5, yes)}));
            w.append(object("Sound", {string_prop("Filename", "cue.wav"), prop("VoiceType", 6, integer)}));
        }
        else if (gameplay)
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
        for (unsigned n : {3u, 1u, surface_count, 0u, 1u, 0u, 3u, 0u, 0u, 0u})
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
        const auto surface_begin = w.data.size();
        w.vec();
        w.vec(1, 0, 0);
        w.vec(0, 0, 1);
        w.zero(24);
        w.u32(0xffffffff);
        w.zero(8); // Surface packed payload may contain NaN bits.
        w.u16(0);
        w.u32(0);
        w.u32(destination || trial ? 1 : 0);
        w.u32(0);
        w.byte(effect ? 1 : 0);
        if (effect)
        {
            w.str(effect->effect_name);
            w.str(effect->effect_parameters);
        }
        w.u16(20); // Serialized material flags, independent of surface flags.
        const Bytes surface_bytes(w.data.begin() + surface_begin, w.data.end());
        for (unsigned i = 1; i < surface_count; ++i)
            w.data.insert(w.data.end(), surface_bytes.begin(), surface_bytes.end());
        w.zero(12);
        w.u32(0);
        for (unsigned i = 0; i < 3; ++i)
        {
            w.u16(i);
            w.zero(3);
        }
        if (trial)
        {
            w.vec(-1000, 0, -1000);
            w.vec(1000, 0, -1000);
            w.vec(0, 0, 1000);
        }
        else
        {
            w.vec();
            w.vec(10, 0, 0);
            w.vec(0, 0, 10);
        }
        w.patch(start, static_cast<unsigned>(w.data.size()));
        w.u32(0); // No additional models.
        w.vec(trial ? -2000 : -10, trial ? -2000 : -10, trial ? -2000 : -10);
        w.vec(trial ? 2000 : 10, trial ? 2000 : 10, trial ? 2000 : 10);
        return w;
    }
    bool surface_effects()
    {
        Surface source;
        source.effect_name = "Pan";
        source.effect_parameters = "up -1024";
        World world;
        std::string error;
        auto bytes = dat(false, false, false, false, 0, &source).data;
        CHECK(read_world(bytes, world, error));
        const auto& surface = world.models[0].surfaces[0];
        CHECK(surface.has_effect && surface.effect_name == "Pan" && surface.effect_parameters == "up -1024");
        CHECK(surface.texture_flags == 20 && surface.u.x == 1 && surface.v.z == 1);
        for (std::size_t size = 0; size < bytes.size(); ++size)
            CHECK(!read_world(std::span(bytes).first(size), world, error) && world.models.empty());
        source.effect_name = "Unknown ";
        source.effect_parameters = " raw\tparameters\n";
        CHECK(read_world(dat(false, false, false, false, 0, &source).data, world, error));
        CHECK(world.models[0].surfaces[0].effect_name == source.effect_name &&
              world.models[0].surfaces[0].effect_parameters == source.effect_parameters);
        source.effect_name = "";
        source.effect_parameters = "";
        CHECK(read_world(dat(false, false, false, false, 0, &source).data, world, error));
        CHECK(world.models[0].surfaces[0].has_effect && world.models[0].surfaces[0].effect_name.empty());
        source.effect_name = "Pan";
        source.effect_parameters = std::string(4096, '0');
        CHECK(read_world(dat(false, false, false, false, 0, &source, 2046).data, world, error));
        CHECK(world.models[0].surfaces.size() == 2046);
        CHECK(!read_world(dat(false, false, false, false, 0, &source, 2047).data, world, error) &&
              world.models.empty());
        source.effect_parameters = std::string(4097, 'x');
        CHECK(!read_world(dat(false, false, false, false, 0, &source).data, world, error) && world.models.empty());
        source.effect_parameters = std::string("1\0 2", 4);
        CHECK(!read_world(dat(false, false, false, false, 0, &source).data, world, error) && world.models.empty());
        return true;
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
        CHECK(m.surfaces[0].texture_flags == 20 && m.surfaces[0].flags == 0);
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
    bool sprite_entries(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root, false));
        test_player::Directory dir;
        dir.children["worlds"].files["sprites.dat"] = dat(false, false, true).data;
        auto spr = [](std::string_view frame)
        {
            Writer w;
            w.u32(1);
            w.u32(15);
            w.u32(1);
            w.u32(0);
            w.u32(0);
            w.str(frame);
            return w.data;
        };
        dir.files["valid.spr"] = spr("frame.dtx");
        dir.files["frame-missing.spr"] = spr("absent.dtx");
        dir.files["bad.spr"] = {0, 1};
        dir.files["frame.dtx"] = {0};
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        CHECK(save(root / "spriteworld.rez", archive.b));
        std::string error;
        CHECK(game.resources.mount(root / "spriteworld.rez", error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(game.request_world("sprites", error));
            CHECK(game.entries.size() == 6 && game.objects.snapshot().size() == 1);
            CHECK(game.entries[0].state == EntryState::Created && game.entries[3].state == EntryState::Filtered);
            auto* r = game.objects.get(game.entries[0].object);
            CHECK(r && r->sprite && r->state.m_Pos.y == 6);
            std::weak_ptr<const Sprite> weak = r->sprite;
            for (auto i : {1u, 2u, 4u, 5u})
                CHECK(game.entries[i].state == EntryState::Failed && !game.entries[i].object &&
                      !game.entries[i].detail.empty());
            CHECK(game.entries[4].detail.find("TypeAction") != std::string::npos);
            CHECK(game.entries[5].detail.find("frame resource") != std::string::npos);
            CHECK(game.step(.02) && r->sprite_track.milliseconds == 20);
            CHECK(game.unload_world() && weak.expired());
        }
        return true;
    }
    bool sound_entries(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_audio::setup(game, root));
        test_player::Directory dir;
        dir.children["worlds"].files["sounds.dat"] = dat(false, true).data;
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        CHECK(save(root / "soundworld.rez", archive.b));
        std::string error;
        CHECK(game.resources.mount(root / "soundworld.rez", error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(game.request_world("sounds", error));
            CHECK(game.entries.size() == 4 && game.objects.snapshot().empty());
            CHECK(game.entries[0].state == EntryState::WorldSound && !game.entries[0].object);
            auto id = game.entries[0].sound;
            const auto* voice = game.server.audio.get(id);
            CHECK(voice && voice->world_owned && voice->position.x == 5 && voice->position.y == 6 &&
                  voice->position.z == 7);
            for (unsigned i : {1u, 3u})
                CHECK(game.entries[i].state == EntryState::Failed && !game.entries[i].sound &&
                      !game.entries[i].detail.empty());
            CHECK(game.entries[1].detail.find("Missing") != std::string::npos);
            CHECK(game.entries[3].detail.find("Rejected precreate") != std::string::npos);
            CHECK(game.entries[2].state == EntryState::WorldSound && !game.entries[2].object);
            const auto streamed = game.entries[2].sound;
            const auto* stream_voice = game.server.audio.get(streamed);
            CHECK(stream_voice && stream_voice->world_owned && stream_voice->clip->stream &&
                  stream_voice->clip->samples.empty());
            std::weak_ptr<const AudioClip> clip = stream_voice->clip;
            CHECK(game.server.audio.voices().size() == 2 && game.step(.25));
            CHECK(game.server.audio.get(id)->loops == 2);
            CHECK(game.server.audio.get(streamed)->loops == 2);
            CHECK(game.unload_world() && game.entries.empty() && !game.server.audio.get(id));
            CHECK(!game.server.audio.get(streamed) && clip.expired() && game.server.audio.cached_bytes() == 0);
        }
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

    bool destination_messages()
    {
        WorldMessage message;
        WorldRequest request;
        std::string error;
        CHECK(original_world_request("Worlds\\02_CityHub.DAT", 0, LOADTYPE_NEW_LEVEL, 3, message));
        CHECK(parse_world_request(message, request, error) && error.empty());
        CHECK(request.filename == "worlds/02_cityhub.dat" && request.difficulty == 3 &&
              request.load_type == LOADTYPE_NEW_LEVEL);
        CHECK(!decode_world_request(message, request, error) && request.filename.empty());
        CHECK(error.find("KeepAliveSave/KeepAliveLoad") != std::string::npos);
        for (std::size_t n = 0; n < message.bytes.size(); ++n)
        {
            auto truncated = message;
            truncated.bytes.resize(n);
            CHECK(!parse_world_request(truncated, request, error) && request.filename.empty());
        }
        auto bad = message;
        bad.id = 0;
        CHECK(!parse_world_request(bad, request, error));
        bad = message;
        bad.bytes.push_back(0);
        CHECK(!parse_world_request(bad, request, error));
        bad = message;
        bad.bytes[3] = 0;
        CHECK(!parse_world_request(bad, request, error));
        for (auto name : {"../next", "worlds/../next", "/next"})
        {
            CHECK(original_world_request(name, 0, LOADTYPE_NEW_LEVEL, 1, bad));
            CHECK(!parse_world_request(bad, request, error));
        }
        for (unsigned field = 0; field < 3; ++field)
        {
            bad = message;
            bad.bytes[bad.bytes.size() - 3 + field] = 255;
            CHECK(!parse_world_request(bad, request, error));
        }
        CHECK(original_world_request("next", 0, LOADTYPE_NEW_GAME, 0, message));
        CHECK(parse_world_request(message, request, error) && decode_world_request(message, request, error));
        return true;
    }
    unsigned destination_constructed = 0;
    GameSession* destination_session = nullptr;
    bool rejected_reentrant = false;
    std::unique_ptr<GameSession>* transition_owner = nullptr;
    bool transition_reentrant_rejected = false;
    struct DestinationProbe : BaseClass
    {
        DestinationProbe()
        {
            ++destination_constructed;
        }
        DDWORD EngineMessageFn(DDWORD id, void*, float) override
        {
            if (id == MID_UPDATE && transition_owner)
            {
                std::string error;
                auto* before = transition_owner->get();
                transition_reentrant_rejected =
                    GameSession::process_level_change(*transition_owner, error) == LevelChangeResult::Rejected &&
                    transition_owner->get() == before && before->pending_level_change &&
                    before->level_change.attempts == 0 && error.find("callbacks") != error.npos;
                return 1;
            }
            if (id == MID_UPDATE && destination_session)
            {
                WorldMessage message;
                std::string error;
                original_world_request("destination", 0, LOADTYPE_NEW_LEVEL, 0, message);
                const auto before = destination_session->destination_inspection.attempts;
                rejected_reentrant = !destination_session->inspect_destination(message, "arrival", error) &&
                                     error.find("callback") != std::string::npos &&
                                     destination_session->destination_inspection.attempts == before;
                const auto trials = destination_session->destination_trial.attempts;
                rejected_reentrant = rejected_reentrant &&
                                     !destination_session->rehearse_destination(message, "arrival", error) &&
                                     error.find("callbacks") != std::string::npos &&
                                     destination_session->destination_trial.attempts == trials;
            }
            return 1;
        }
    };
    ClassInfo destination_type{"DestinationProbe",
                               nullptr,
                               []() -> std::unique_ptr<BaseClass> { return std::make_unique<DestinationProbe>(); },
                               {{"Bounded", std::string{}, true, 4}}};
    bool destination_schema()
    {
        GameSession game;
        CHECK(game.initialize());
        destination_type.parent = game.objects.find_class("BaseClass");
        CHECK(game.objects.register_class(destination_type));
        const auto type = &destination_type;
        const auto diagnostic = game.objects.diagnostics();
        const auto last_error = game.objects.error();
        const auto before = destination_constructed;
        std::string error;
        CHECK(game.objects.validate_properties(type,
                                               {{"name", std::string("inherited")},
                                                {"Rotation", EulerAngles{{1, 2, 3}}},
                                                {"Bounded", std::string("four")}},
                                               error) == ObjectError::None);
        CHECK(error.empty());
        for (const auto& props : std::vector<Properties>{{{"Absent", true}},
                                                         {{"Name", 1.f}},
                                                         {{"Bounded", std::string("longer")}},
                                                         {{"Bounded", std::string("a\0b", 3)}},
                                                         {{"Pos", DVector{NAN, 0, 0}}},
                                                         {{"Rotation", EulerAngles{{0, INFINITY, 0}}}}})
            CHECK(game.objects.validate_properties(type, props, error) == ObjectError::InvalidProperty &&
                  !error.empty());
        const ClassInfo unknown{"NotRegistered", nullptr, nullptr, {}};
        CHECK(game.objects.validate_properties(&unknown, {}, error) == ObjectError::UnknownClass);
        CHECK(game.objects.validate_properties(nullptr, {}, error) == ObjectError::UnknownClass);
        CHECK(destination_constructed == before && game.objects.snapshot().empty());
        CHECK(game.objects.error() == last_error && game.objects.diagnostics() == diagnostic);
        CHECK(!game.objects.create(type, {}, Creation::World, {{"Bounded", std::string("longer")}}));
        CHECK(game.objects.error() == ObjectError::InvalidProperty && destination_constructed == before);
        auto handle = game.objects.create(type, {}, Creation::World, {{"Bounded", std::string("four")}});
        CHECK(handle && destination_constructed == before + 1 && game.objects.get(handle));
        return true;
    }
    bool destination_lifetime(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        std::string error;
        CHECK(game.enter_player(error));
        destination_type.parent = game.objects.find_class("BaseClass");
        CHECK(game.objects.register_class(destination_type));
        test_player::Directory dir;
        dir.children["worlds"].files["destination.dat"] = dat(false, false, false, true).data;
        dir.children["worlds"].files["broken.dat"] = {56, 0, 0, 0};
        auto invalid_collision = dat(false, false, false, true);
        invalid_collision.patch(invalid_collision.data.size() - 64, std::bit_cast<std::uint32_t>(1e8f));
        dir.children["worlds"].files["collision.dat"] = invalid_collision.data;
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        CHECK(save(root / "destination.rez", archive.b));
        CHECK(game.resources.mount(root / "destination.rez", error));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player);
        player->m_damage.m_fHitPoints = 73;
        const auto ids = game.objects.snapshot();
        const auto client = game.local_client;
        const auto revision = game.world_revision;
        const auto path = game.world_path;
        const auto clock = game.objects.time();
        const auto calls = game.server.service_calls;
        const auto string_count = game.server.string_count();
        const auto bullets = player->m_InventoryMgr.GetAmmoCount(1);
        const auto construction = destination_constructed;
        WorldMessage message;
        CHECK(original_world_request("destination", 0, LOADTYPE_NEW_LEVEL, 0, message));
        CHECK(game.inspect_destination(message, "arrival", error));
        const auto& d = game.destination_inspection;
        CHECK(d.scan_complete && d.attempts == 1 && d.entries.size() == 5);
        CHECK(d.schema_checked == 2 && d.editor_only == 1 && d.unknown == 1 && d.invalid == 1);
        CHECK(d.models == 1 && d.triangles == 1 && d.collision_nodes == 1);
        CHECK(d.source_world == path && d.source_revision == revision && d.requested_start == "arrival");
        CHECK(d.entries[0].has_position && d.entries[0].position.x == 5 && d.entries[0].name == "owned");
        CHECK(d.entries[3].detail.find("TypeAction") != std::string::npos);
        CHECK(destination_constructed == construction && game.server.service_calls == calls);
        CHECK(game.objects.snapshot() == ids && game.local_client == client && game.world_revision == revision);
        CHECK(game.objects.time() == clock && game.world_path == path && game.server.string_count() == string_count);
        CHECK(player->m_damage.m_fHitPoints == 73 && player->m_InventoryMgr.GetAmmoCount(1) == bullets);
        WorldRequest request;
        PreparedWorld prepared;
        CHECK(parse_world_request(message, request, error) && prepare_world(request, game.resources, prepared, error));
        CHECK(prepared.world.objects.size() == 5 && prepared.collision.triangles() == 1);
        request.filename = "worlds/collision.dat";
        CHECK(!prepare_world(request, game.resources, prepared, error));
        CHECK(error.find("primary collision") != std::string::npos);
        CHECK(prepared.world.models.empty() && prepared.collision.triangles() == 0);
        request.filename = "worlds/destination.dat";
        CHECK(prepare_world(request, game.resources, prepared, error));
        request.filename = "worlds/broken.dat";
        CHECK(!prepare_world(request, game.resources, prepared, error));
        CHECK(prepared.world.objects.empty() && prepared.world.models.empty() && prepared.collision.triangles() == 0);
        CHECK(original_world_request("broken", 0, LOADTYPE_NEW_GAME, 0, message));
        CHECK(!game.load_world(message, error) && game.objects.snapshot() == ids && game.local_client == client);
        CHECK(!game.inspect_destination(message, "arrival", error) && !d.scan_complete && d.entries.empty());
        CHECK(!game.inspect_destination(message, std::string("x\0y", 3), error));
        CHECK(!game.inspect_destination(message, std::string(101, 'x'), error));
        const auto attempts = d.attempts;
        for (unsigned i = 0; i < 256; ++i)
            CHECK(game.server.SendWorldExit({i == 255 ? "destination" : "broken"}, "arrival") == DE_OK);
        CHECK(game.pump_player_messages(error));
        CHECK(game.client_shell.world_exit.queued == 256 && game.client_shell.world_exit.received == 256);
        CHECK(d.attempts == attempts + 1 && d.scan_complete && d.destination == "worlds/destination.dat");
        CHECK(game.pump_player_messages(error) && d.attempts == attempts + 1);
        CHECK(game.objects.snapshot() == ids && game.client_shell.player == player->m_hObject);
        auto handle = game.objects.create(&destination_type);
        CHECK(handle);
        destination_session = &game;
        CHECK(game.objects.engine_message(handle, MID_UPDATE, nullptr));
        destination_session = nullptr;
        CHECK(rejected_reentrant && d.attempts == attempts + 1 && d.entries.size() == 5);
        CHECK(game.unload_world() && game.objects.snapshot().empty());
        CHECK(game.server.string_count() == 0 && game.server.client_count() == 0);
        CHECK(d.scan_complete && d.source_revision != game.world_revision && d.entries[0].name == "owned");
        return true;
    }
    bool destination_private(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
            CHECK(game.step(.016));
            auto* player = game.server_shell.Player(game.local_client);
            CHECK(player);
            const auto ids = game.objects.snapshot();
            const auto revision = game.world_revision;
            const auto time = game.objects.time();
            const auto path = game.world_path;
            const auto client = game.local_client;
            const auto calls = game.server.service_calls;
            const auto voices = game.server.audio.voices().size();
            const auto strings = game.server.string_count();
            const auto ammo = player->m_InventoryMgr.GetAmmoCount(1);
            const auto hp = player->m_damage.m_fHitPoints;
            std::vector<std::pair<HOBJECT, std::uint64_t>> updates;
            for (auto id : ids)
                updates.emplace_back(game.objects.get(id)->handle, game.objects.get(id)->updates);
            for (auto destination : {"01_SubwayA", "02_CityHub"})
            {
                WorldMessage message;
                CHECK(original_world_request(destination, 0, LOADTYPE_NEW_LEVEL, 0, message));
                CHECK(game.inspect_destination(message, "01_egressa", error));
                const auto& d = game.destination_inspection;
                std::cout << "Cycle " << cycle << ": " << d.destination << ": " << d.entries.size() << " entries / "
                          << d.schema_checked << " schema / " << d.editor_only << " editor / " << d.unknown
                          << " unknown / " << d.invalid << " invalid / " << d.models << " models / " << d.triangles
                          << " triangles / " << d.collision_nodes << " collision nodes\n";
                CHECK(d.scan_complete && d.source_world == path && d.source_revision == revision);
                if (d.destination == "worlds/01_subwaya.dat")
                    CHECK(d.entries.size() == 580 && d.schema_checked == 208 && d.editor_only == 372 &&
                          d.unknown == 0 && d.invalid == 0 && d.models == 116);
                else
                {
                    CHECK(d.entries.size() == 1026 && d.schema_checked == 475 && d.editor_only == 541 &&
                          d.unknown == 10 && d.invalid == 0 && d.models == 100 && d.triangles == 39724);
                    for (const auto& e : d.entries)
                        if (e.state == DestinationEntryState::InvalidProperties)
                            std::cout << "Schema failure " << e.class_name << " / " << e.name << ": " << e.detail
                                      << '\n';
                }
                CHECK(game.objects.snapshot() == ids && game.world_revision == revision && game.local_client == client);
                CHECK(game.objects.time() == time && game.world_path == path && game.server.service_calls == calls);
                CHECK(game.server.audio.voices().size() == voices && game.server.string_count() == strings);
                CHECK(game.client_shell.player == player->m_hObject && player->m_damage.m_fHitPoints == hp &&
                      player->m_InventoryMgr.GetAmmoCount(1) == ammo);
                for (const auto& [handle, count] : updates)
                    CHECK(game.objects.get(handle) && game.objects.get(handle)->updates == count);
                CHECK(!game.load_world(message, error) &&
                      error.find("KeepAliveSave/KeepAliveLoad") != std::string::npos);
            }
            WorldMessage broken;
            CHECK(original_world_request("16_bridge", 0, LOADTYPE_NEW_LEVEL, 0, broken));
            CHECK(!game.inspect_destination(broken, "start", error));
            CHECK(game.objects.snapshot() == ids && game.world_revision == revision && game.objects.time() == time);
            CHECK(!game.destination_inspection.scan_complete && game.destination_inspection.entries.empty());
            std::cout << "Malformed destination preserves source: " << error << '\n';
            CHECK(!game.run_world(error));
            CHECK(game.unload_world() && game.server.client_count() == 0 && game.server.string_count() == 0);
        }
        return true;
    }

    struct GlobalSnapshot
    {
        std::array<DLink*, 7> heads{&AI_Mgr::m_CabalHead,    &AI_Mgr::m_MonsterHead, &CDestructable::m_DestructableHead,
                                    &SmellHint::m_SmellHead, &ExitHint::m_ExitHead,  &AmmoPickup::m_PickupHead,
                                    &CameraObj::m_CameraHead};
        std::array<DDWORD*, 7> counts{
            &AI_Mgr::m_dwNumCabal,     &AI_Mgr::m_dwNumMonster, &CDestructable::m_dwNumDestructables,
            &SmellHint::m_dwNumSmells, &ExitHint::m_dwNumExits, &AmmoPickup::m_dwNumPickups,
            &CameraObj::m_dwNumCameras};
        std::array<DDWORD, 7> saved{};
        std::vector<std::pair<DLink*, DLink>> links;
        HOBJECT camera = g_hActiveCamera;
        bool capture()
        {
            for (std::size_t i = 0; i < heads.size(); ++i)
            {
                saved[i] = *counts[i];
                links.emplace_back(heads[i], *heads[i]);
                auto* link = heads[i]->m_pNext;
                unsigned walked = 0;
                while (link && link != heads[i] && walked <= saved[i])
                {
                    links.emplace_back(link, *link);
                    ++walked;
                    link = link->m_pNext;
                }
                CHECK(walked == saved[i] && (!link || link == heads[i]));
            }
            return true;
        }
        bool unchanged() const
        {
            CHECK(camera == g_hActiveCamera);
            for (std::size_t i = 0; i < heads.size(); ++i)
                CHECK(*counts[i] == saved[i]);
            for (const auto& [live, copy] : links)
                CHECK(live->m_pPrev == copy.m_pPrev && live->m_pNext == copy.m_pNext && live->m_pData == copy.m_pData);
            return true;
        }
    };
    bool trial_fixture(GameSession& game, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(game, root, true, true, true));
        test_player::Directory dir;
        for (unsigned i = 1; i <= 6; ++i)
            dir.children["worlds"].files["trial" + std::to_string(i) + ".dat"] =
                dat(false, false, false, false, i).data;
        dir.children["worlds"].files["broken.dat"] = {56, 0, 0, 0};
        dir.files["cue.wav"] = test_audio::wave();
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        CHECK(save(root / "trial.rez", archive.b));
        std::string error;
        CHECK(game.resources.mount(root / "trial.rez", error) && game.enter_player(error));
        // Populate the original source lists; destination destruction must not unlink these.
        for (auto name : {"SmellHint", "ExitHint", "BulletAmmoPU", "CameraObj"})
            CHECK(game.objects.create(game.objects.find_class(name), {}, Creation::World,
                                      {{"Pos", DVector{200, 100, 200}}}));
        CHECK(game.objects.finish_frame());
        CHECK(game.step(.016)); // The source must already be a viable update before any rehearsal.
        return true;
    }
    bool destination_trials(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(trial_fixture(game, root));
        auto* player = game.server_shell.Player(game.local_client);
        player->m_damage.SetHitPoints(37);
        player->m_damage.SetArmorPoints(23);
        player->m_InventoryMgr.SetAmmoCount(1, 1234);
        CHECK(player->m_InventoryMgr.AddKey({"old-key", "Old key", "icon", "selected", 2}) == InventoryResult::Ok);
        const auto ids = game.objects.snapshot();
        const auto client = game.local_client;
        const auto revision = game.world_revision;
        const auto time = game.objects.time();
        const auto strings = game.server.string_count();
        GlobalSnapshot globals;
        CHECK(globals.capture());
        std::string error;
        for (auto name : {"trial1", "trial4", "trial2", "trial3", "broken", "missing", "trial1"})
        {
            WorldMessage message;
            CHECK(original_world_request(name, 0, LOADTYPE_NEW_LEVEL, 0, message));
            const bool ok = game.rehearse_destination(message, "arrival", error);
            const auto& d = game.destination_trial;
            std::cout << name << ": loaded=" << d.world_loaded << " entered=" << d.player_entered
                      << " cleanup=" << d.cleanup_complete << " failed=" << d.failed << " error=" << error << '\n';
            CHECK(d.cleanup_complete && g_pServerDE == &game.server && globals.unchanged());
            if (std::string_view(name) == "trial1" || std::string_view(name) == "trial4")
            {
                CHECK(ok && d.world_loaded && d.player_entered && d.entry_camera_active &&
                      d.selected_start == "arrival");
                CHECK(d.created == 6 && d.sounds == (std::string_view(name) == "trial1" ? 1u : 0u));
                CHECK(d.failed == (std::string_view(name) == "trial4" ? 1u : 0u));
                CHECK(d.health == 37 && d.armor == 23 && d.bullets == 1234 && d.keys == 0);
            }
            else
                CHECK(!ok && !d.player_entered && !error.empty());
            CHECK(game.local_client == client && game.world_revision == revision && game.objects.time() == time);
            CHECK(game.objects.snapshot() == ids && game.client_shell.player == player->m_hObject);
            CHECK(player->m_damage.m_fHitPoints == 37 && player->m_InventoryMgr.GetAmmoCount(1) == 1234);
            CHECK(player->m_InventoryMgr.keys.size() == 1 && game.server.string_count() == strings);
            CHECK(game.server.list_count() == 0 && game.server.message_count() == 0);
        }
        WorldMessage message;
        CHECK(original_world_request("trial1", 0, LOADTYPE_NEW_LEVEL, 0, message));
        CHECK(!game.rehearse_destination(message, std::string("x\0y", 3), error));
        CHECK(globals.unchanged() && g_pServerDE == &game.server);
        CHECK(game.rehearse_destination(message, "arrival", error));
        const auto attempt = game.destination_trial.attempts;
        {
            GameSession other;
            CHECK(!other.initialize());
            CHECK(!other.rehearse_destination(message, "arrival", error));
        }
        CHECK(globals.unchanged() && game.destination_trial.attempts == attempt && g_pServerDE == &game.server);
        const bool resumed = game.step(.016);
        if (!resumed)
        {
            for (const auto& line : game.trace)
                std::cerr << "Source trace: " << line << '\n';
            for (const auto& line : game.objects.diagnostics())
                std::cerr << "Source diagnostic: " << line << '\n';
        }
        CHECK(resumed);
        CHECK(game.unload_world() && game.server.string_count() == 0 && game.objects.snapshot().empty());
        CHECK(game.destination_trial.player_entered && game.destination_trial.source_revision != game.world_revision);
        return true;
    }
    bool transition_images(GameSession& game, const std::filesystem::path& root)
    {
        // Transition preparation decodes images. Override the older indexed-only
        // fixture skins with real synthetic -2 DTX data, never retail assets.
        test_model::Writer image;
        image.u32(0);
        image.u32(0xfffffffeu);
        image.u16(2);
        image.u16(2);
        image.u16(1);
        image.u16(0);
        image.u32(0);
        image.u32(0);
        image.b.resize(1068);
        image.b[44] = image.b[45] = image.b[46] = image.b[47] = 255;
        image.b.insert(image.b.end(), {0, 0, 0, 0});
        test_player::Directory dir;
        for (const auto& [path, resource] : game.resources.index())
            if (path.ends_with(".dtx"))
            {
                auto* parent = &dir;
                std::size_t begin = 0, slash;
                while ((slash = path.find('/', begin)) != path.npos)
                {
                    parent = &parent->children[path.substr(begin, slash - begin)];
                    begin = slash + 1;
                }
                parent->files[path.substr(begin)] = image.b;
            }
        dir.files["t.dtx"] = image.b;
        dir.children["models"].files["path1.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].files["path1.dtx"] = image.b;
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        CHECK(save(root / "images.rez", archive.b));
        std::string error;
        CHECK(game.resources.mount(root / "images.rez", error));
        return true;
    }
    bool level_change(const std::filesystem::path& root)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(trial_fixture(*game, root));
        WorldMessage queued;
        std::string guard_error;
        CHECK(original_world_request("trial1", 0, LOADTYPE_NEW_LEVEL, 0, queued));
        CHECK(game->queue_level_change(queued, "arrival", guard_error));
        CHECK(game->objects.register_class(destination_type));
        auto probe = game->objects.create(game->objects.find_class("DestinationProbe"), {}, Creation::World);
        CHECK(probe);
        transition_owner = &game;
        CHECK(game->objects.engine_message(probe, MID_UPDATE, nullptr));
        transition_owner = nullptr;
        CHECK(transition_reentrant_rejected);
        CHECK(game->objects.remove(probe) && game->objects.finish_frame());
        game->server.SetGameConVar("GAME_DIFFICULTY", "3");
        auto* player = game->server_shell.Player(game->local_client);
        player->m_damage.m_fMaxHitPoints = 311;
        player->m_damage.m_fResistance = .42f;
        player->m_nAttribSpeed = 6;
        CHECK(player->m_InventoryMgr.AddItem(1, 1) == InventoryResult::Ok);
        player->m_InventoryMgr.medkit->count = 43;
        player->m_damage.SetHitPoints(37);
        player->m_damage.SetArmorPoints(23);
        player->m_InventoryMgr.SetAmmoCount(1, 1234);
        CHECK(player->m_InventoryMgr.AddKey({"old-key", "Old key", "icon", "selected", 2}) == InventoryResult::Ok);
        std::string error;
        WorldMessage message;
        const auto* owner = game.get();
        const auto ids = game->objects.snapshot();
        const auto client = game->local_client;
        const auto revision = game->world_revision;
        const auto time = game->objects.time();
        GlobalSnapshot globals;
        CHECK(globals.capture());
        for (const auto* name : {"trial1", "trial2", "trial3", "trial4", "trial5", "trial6", "broken", "missing"})
        {
            CHECK(original_world_request(name, 0, LOADTYPE_NEW_LEVEL, 0, message));
            CHECK(game->queue_level_change(message, "arrival", error));
            CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Rejected);
            std::cout << name << ": " << error << '\n';
            CHECK(!error.empty() && game.get() == owner && game->level_change.source_retained);
            CHECK(game->level_change.cleanup_complete && globals.unchanged() && g_pServerDE == &game->server);
            CHECK(game->objects.snapshot() == ids && game->local_client == client);
            CHECK(game->world_revision == revision && game->objects.time() == time);
            CHECK(player->m_damage.m_fHitPoints == 37 && player->m_InventoryMgr.GetAmmoCount(1) == 1234);
            CHECK(player->m_InventoryMgr.keys.size() == 1);
            CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::None);
        }
        CHECK(transition_images(*game, root));
        CHECK(original_world_request("missing", 0, LOADTYPE_NEW_LEVEL, 0, message));
        CHECK(game->queue_level_change(message, "unused", error));
        CHECK(original_world_request("trial1", 0, LOADTYPE_NEW_LEVEL, 0, message));
        CHECK(!game->queue_level_change(message, std::string("a\0b", 3), error));
        CHECK(game->queue_level_change(message, "ARRIVAL", error));
        CHECK(game->pending_level_change->start == "ARRIVAL");
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            owner = game.get();
            const auto before_revision = game->world_revision;
            const auto result = GameSession::process_level_change(game, error);
            std::cout << "Commit " << cycle << ": " << error << '\n';
            CHECK(result == LevelChangeResult::Committed && error.empty() && game.get() != owner);
            CHECK(game->world_revision == before_revision + 1 && game->objects.time() == 0);
            CHECK(game->level_change.committed == cycle + 1 && game->level_change.rejected == 8);
            CHECK(game->level_change.cleanup_complete && !game->level_change.source_retained);
            CHECK(game->level_change.checked_textures >= 5 && game->level_change.selected_start == "arrival");
            CHECK(g_pServerDE == &game->server && game->client_shell.m_bInWorld);
            player = game->server_shell.Player(game->local_client);
            CHECK(player && player->m_damage.m_fHitPoints == 37 && player->m_damage.m_fArmorPoints == 23);
            CHECK(player->m_InventoryMgr.GetAmmoCount(1) == 1234 && player->m_InventoryMgr.keys.empty());
            CHECK(player->m_damage.m_fMaxHitPoints == 311 && player->m_damage.m_fResistance == .42f);
            CHECK(player->m_nAttribSpeed == 6 && player->respawns == 0);
            CHECK(player->m_InventoryMgr.medkit && player->m_InventoryMgr.medkit->count == 43);
            CHECK(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")) == 3);
            CHECK(game->client_shell.view.object && g_hActiveCamera);
            CHECK(game->step(.016));
            CHECK(game->server.message_count() == 0 && game->server.list_count() == 0);
            CHECK(game->queue_level_change(message, "arrival", error));
        }
        CHECK(game->unload_world() && !game->pending_level_change && !game->server.string_count());
        return true;
    }
    bool level_change_private(const std::filesystem::path& root)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(game->initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game->resources.mount_game(config, error));
        CHECK(game->request_world("01_SubwayA", error) && game->enter_player(error));
        CHECK(game->step(.016));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            auto* player = game->server_shell.Player(game->local_client);
            player->m_damage.SetHitPoints(37);
            player->m_damage.SetArmorPoints(23);
            player->m_InventoryMgr.SetAmmoCount(1, 1234);
            GlobalSnapshot globals;
            CHECK(globals.capture());
            auto* owner = game.get();
            const auto ids = game->objects.snapshot();
            const auto revision = game->world_revision;
            const auto time = game->objects.time();
            std::map<SoundId, double> voices;
            for (const auto& [id, voice] : game->server.audio.voices())
                voices[id] = voice.cursor;
            WorldMessage message;
            CHECK(original_world_request("02_CityHub", 0, LOADTYPE_NEW_LEVEL, 0, message));
            CHECK(game->queue_level_change(message, "01_egressa", error));
            CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Rejected);
            std::cout << "CityHub refusal " << cycle << ": " << error << '\n';
            CHECK(game.get() == owner && game->objects.snapshot() == ids && globals.unchanged());
            CHECK(game->level_change.unknown == 10 && game->level_change.failed == 1);
            CHECK(game->level_change.cleanup_complete && game->level_change.source_retained);
            CHECK(game->world_revision == revision && game->objects.time() == time);
            CHECK(game->server.audio.voices().size() == voices.size());
            for (const auto& [id, cursor] : voices)
                CHECK(game->server.audio.get(id) && game->server.audio.get(id)->cursor == cursor);
            CHECK(original_world_request("01_SubwayA", 0, LOADTYPE_NEW_LEVEL, 0, message));
            CHECK(game->queue_level_change(message, "start", error));
            const auto result = GameSession::process_level_change(game, error);
            std::cout << "SubwayA transfer " << cycle << ": " << error << '\n';
            CHECK(result == LevelChangeResult::Committed && error.empty() && game.get() != owner);
            CHECK(game->level_change.cleanup_complete && game->world_revision == revision + 1);
            CHECK(game->level_change.checked_textures > 50 && game->objects.time() == 0);
            player = game->server_shell.Player(game->local_client);
            CHECK(player && player->m_damage.m_fHitPoints == 37 && player->m_damage.m_fArmorPoints == 23);
            CHECK(player->m_InventoryMgr.GetAmmoCount(1) == 1234);
            CHECK(game->step(.016) && g_pServerDE == &game->server);
        }
        CHECK(game->unload_world() && !game->server.string_count() && !game->server.client_count());
        return true;
    }
    bool destination_trial_private(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
            CHECK(game.step(.016));
            auto* player = game.server_shell.Player(game.local_client);
            player->m_damage.SetHitPoints(37);
            player->m_InventoryMgr.SetAmmoCount(1, 1234);
            std::vector<std::pair<AI_Mgr*, const CAnim_Sound*>> tables;
            for (auto id : game.objects.snapshot())
                if (auto* a = dynamic_cast<AI_Mgr*>(game.objects.get(id)->game.get()))
                    tables.emplace_back(a, a->AnimSound());
            CHECK(tables.size() == 16);
            CAnim_Sound* voiced_table = nullptr;
            for (const auto& [actor, table] : tables)
                if (table->GetSoundRoot() == "sounds/enemies/m_cultist")
                {
                    // Explicit diagnostic voice: exercise retained shared-table ownership during the intro.
                    voiced_table = const_cast<CAnim_Sound*>(table);
                    CHECK(voiced_table->PlayVoice(actor->m_hObject, "idle1.wav", 1000, 100));
                    break;
                }
            CHECK(voiced_table && voiced_table->m_hVoxSound);
            const auto voice_id = voiced_table->m_hVoxSound;
            CAnim_Sound* boss_table = nullptr;
            for (const auto& [actor, table] : tables)
                if (actor->npc_type == AI_Mgr::NPCType::Gideon)
                {
                    boss_table = const_cast<CAnim_Sound*>(table);
                    CHECK(boss_table->PlayVoice(actor->m_hObject, "shield.wav", 1000, 100));
                    break;
                }
            CHECK(boss_table && boss_table->m_hVoxSound);
            const auto boss_voice = boss_table->m_hVoxSound;
            const auto boss_messages = game.client_shell.boss_health_messages;
            const auto boss_fraction = game.client_shell.boss_health.fraction;
            // Explicit diagnostic audio, not a shot through the intro camera's input gate.
            auto* weapon = player->m_InventoryMgr.CurrentWeapon();
            weapon->PlayFireSound(false, game.client_shell.position);
            const auto fire_voices = weapon->fire_sounds;
            CHECK(weapon->sounds_played == 1 && game.server.audio.get(weapon->last_fire_sound));
            AI_Mgr::RuntimeTables slots;
            AI_Mgr::ExchangeRuntimeTables(slots);
            CHECK(slots[4] && slots[4].get() == boss_table);
            auto original_slots = slots;
            AI_Mgr::ExchangeRuntimeTables(slots);
            const auto ids = game.objects.snapshot();
            const auto client = game.local_client;
            const auto revision = game.world_revision;
            const auto time = game.objects.time();
            const auto strings = game.server.string_count();
            std::map<SoundId, double> voices;
            for (const auto& [id, voice] : game.server.audio.voices())
                voices[id] = voice.cursor;
            GlobalSnapshot globals;
            CHECK(globals.capture());
            for (auto name : {"02_cityhub", "01_subwaya", "16_bridge"})
            {
                WorldMessage message;
                CHECK(original_world_request(name, 0, LOADTYPE_NEW_LEVEL, 0, message));
                const bool ok = game.rehearse_destination(message, "01_egressa", error);
                const auto& d = game.destination_trial;
                std::cout << "Cycle " << cycle << " " << name << ": loaded=" << d.world_loaded
                          << " entered=" << d.player_entered << " cleanup=" << d.cleanup_complete
                          << " created=" << d.created << " unknown=" << d.unknown << " failed=" << d.failed
                          << " objects=" << d.runtime_objects << " error=" << error << '\n';
                CHECK(globals.unchanged() && g_pServerDE == &game.server && d.cleanup_complete);
                if (std::string_view(name) == "16_bridge")
                    CHECK(!ok && !d.world_loaded && !d.player_entered);
                else
                {
                    CHECK(ok && d.player_entered && d.health == 37 && d.bullets == 1234);
                    if (std::string_view(name) == "02_cityhub")
                        CHECK(d.created == 443 && d.unknown == 10 && d.failed == 1);
                    else
                        CHECK(d.created == 204 && d.unknown == 0 && d.failed == 0);
                }
                CHECK(game.objects.snapshot() == ids && game.local_client == client && game.world_revision == revision);
                CHECK(game.objects.time() == time && game.client_shell.player == player->m_hObject);
                CHECK(player->m_damage.m_fHitPoints == 37 && player->m_InventoryMgr.GetAmmoCount(1) == 1234);
                CHECK(game.server.string_count() == strings && game.server.audio.voices().size() == voices.size());
                for (const auto& [id, cursor] : voices)
                    CHECK(game.server.audio.get(id) && game.server.audio.get(id)->cursor == cursor);
                for (const auto& [ai, table] : tables)
                    CHECK(ai->AnimSound() == table);
                CHECK(voiced_table->m_hVoxSound == voice_id && game.server.audio.get(voice_id));
                CHECK(boss_table->m_hVoxSound == boss_voice && game.server.audio.get(boss_voice));
                CHECK(game.client_shell.boss_health_messages == boss_messages &&
                      game.client_shell.boss_health.fraction == boss_fraction);
                CHECK(weapon->fire_sounds == fire_voices);
                AI_Mgr::ExchangeRuntimeTables(slots);
                CHECK(slots == original_slots);
                AI_Mgr::ExchangeRuntimeTables(slots);
            }
            CHECK(game.step(.016));
            original_slots = {};
            CHECK(game.unload_world() && game.server.string_count() == 0 && game.server.client_count() == 0);
        }
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
        CHECK(game.objects.snapshot().size() == 227);
        CHECK(game.world.models[0].points.size() == 4996);
        CHECK(game.world.models[0].polygons.size() == 3367);
        CHECK(game.resources.find(game.world_path)->overrides == 1);
        CHECK(game.entries[0].state == EntryState::Created);
        CHECK(game.step(.016));
        unsigned suspended = 0;
        for (auto id : game.objects.snapshot())
            if (const auto* a = dynamic_cast<const AI_Mgr*>(game.objects.get(id)->game.get()); a && a->runtime_failed)
            {
                // No player: original enemy-list fallback can sense self. The checked
                // non-owning link contract rejects self; preserve the visible failure.
                ++suspended;
                CHECK(a->runtime_error.find("Target assignment requires distinct live objects") != std::string::npos);
                CHECK(game.objects.get(id)->activity == OBJSTATE_INACTIVE);
                std::cout << "No-player NPC " << game.objects.get(id)->state.m_Name << ": " << a->runtime_error << '\n';
            }
        std::cout << "No-player suspended=" << suspended << " dims calls=" << game.server.service_calls["SetObjectDims"]
                  << '\n';
        CHECK(suspended == 0); // Gideon supplies a real opposite-faction candidate; no self fallback.
        CHECK(game.server.service_calls["SetObjectDims"] == 57);
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
    else if (name == "level_change_private")
        ok = level_change_private(root);
    else if (name == "destination_trial_private")
        ok = destination_trial_private(root);
    else if (name == "destination_private")
        ok = destination_private(root);
    else
    {
        std::error_code ec;
        if (!root.empty())
            std::filesystem::create_directories(root, ec);
        if (ec)
            return 2;
        if (name == "surface_effects")
            ok = surface_effects();
        else if (name == "level_change")
            ok = level_change(root);
        else if (name == "destination_trials")
            ok = destination_trials(root);
        else if (name == "destination_messages")
            ok = destination_messages();
        else if (name == "destination_schema")
            ok = destination_schema();
        else if (name == "destination_lifetime")
            ok = destination_lifetime(root);
        else if (name == "rez_structure")
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
        else if (name == "sprite_entries")
            ok = sprite_entries(root);
        else if (name == "sound_entries")
            ok = sound_entries(root);
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
