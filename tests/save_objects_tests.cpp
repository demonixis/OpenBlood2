#include "brush_fixture.h"
#include "engine/container.h"
#include "engine/save_objects.h"
#include "game/service_coverage.h"
#include "player_fixture.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

// Engine records, clock, save list, two-pass dispatch and purity (save_load_contracts
// lot 3). Synthetic classes only; the original classes keep their Unsupported default.
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
    std::filesystem::path root;
    const char current[] = "Save\\Current\\Current.sav";
    const char autosave[] = "Save\\Current\\Auto.sav";
    const char keep[] = "Save\\Current\\Keep.sav";

    struct Hooks
    {
        // Replace the default bodies (DWord value + LoadSave reference) when set.
        std::function<void(HOBJECT, HMESSAGEWRITE, DDWORD)> save;
        std::function<void(HOBJECT, HMESSAGEREAD, DDWORD)> load;
        std::function<bool(ObjectCreateStruct&, float)> precreate; // false refuses.
        std::function<void(HOBJECT, float)> initial;
        std::vector<std::string> events;
        std::vector<float> params;
    } hooks;
    struct Probe : BaseClass
    {
        explicit Probe(DBYTE type) : BaseClass(type) {}
        std::uint32_t value = 0;
        HOBJECT ref = nullptr;
        std::string name() const
        {
            const char* text = g_pServerDE->GetObjectName(m_hObject);
            return text ? text : "";
        }
        DDWORD EngineMessageFn(DDWORD id, void* data, float param) override
        {
            switch (id)
            {
            case MID_PRECREATE:
            {
                auto& state = *static_cast<ObjectCreateStruct*>(data);
                hooks.events.push_back("precreate " + std::string(state.m_Name) + " " + std::to_string(int(param)));
                if (hooks.precreate && !hooks.precreate(state, param))
                    return 0;
                break;
            }
            case MID_INITIALUPDATE:
                hooks.events.push_back("initial " + name() + " " + std::to_string(int(param)));
                if (hooks.initial)
                    hooks.initial(m_hObject, param);
                break;
            case MID_SAVEOBJECT:
            {
                const auto h = *static_cast<HMESSAGEWRITE*>(data);
                hooks.events.push_back("save " + name());
                hooks.params.push_back(param);
                if (hooks.save)
                    hooks.save(m_hObject, h, static_cast<DDWORD>(param));
                else
                {
                    g_pServerDE->WriteToMessageDWord(h, value);
                    g_pServerDE->WriteToLoadSaveMessageObject(h, ref);
                }
                break;
            }
            case MID_LOADOBJECT:
            {
                const auto h = *static_cast<HMESSAGEREAD*>(data);
                hooks.events.push_back("load " + name());
                hooks.params.push_back(param);
                if (hooks.load)
                    hooks.load(m_hObject, h, static_cast<DDWORD>(param));
                else
                {
                    value = g_pServerDE->ReadFromMessageDWord(h);
                    g_pServerDE->ReadFromLoadSaveMessageObject(h, &ref);
                }
                break;
            }
            default:
                break;
            }
            return BaseClass::EngineMessageFn(id, data, param);
        }
    };
    template <DBYTE Type> std::unique_ptr<BaseClass> make_probe()
    {
        return std::make_unique<Probe>(Type);
    }
    ClassInfo probe_type{"SaveProbe", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::Original, 1};
    ClassInfo model_type{"SaveModelProbe", nullptr, make_probe<OT_MODEL>, {}, SaveSupport::Original, 3};
    ClassInfo sprite_type{"SaveSpriteProbe", nullptr, make_probe<OT_SPRITE>, {}, SaveSupport::Original, 1};
    ClassInfo brush_type{"SaveBrushProbe", nullptr, make_probe<OT_WORLDMODEL>, {}, SaveSupport::Original, 2};
    ClassInfo volume_type{"SaveVolumeProbe", nullptr, make_probe<OT_CONTAINER>, {}, SaveSupport::Original, 1};
    ClassInfo player_type{"SavePlayerProbe", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::Original, 1};
    ClassInfo always_type{"SaveAlwaysLoad", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::AlwaysLoad, 0};
    ClassInfo always_child_type{"SaveAlwaysLoadChild", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::Original, 1};
    ClassInfo unsupported_type{"SaveUnsupported", nullptr, make_probe<OT_NORMAL>, {}};
    ClassInfo not_saved_type{"SaveNotSaved", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::NotSaved, 0};
    ClassInfo no_version_type{"SaveNoVersion", nullptr, make_probe<OT_NORMAL>, {}, SaveSupport::Original, 0};

    bool mount_sprite(GameSession& g)
    {
        test_model::Writer spr;
        for (auto v : {3u, 10u, 1u, 0u, 0u}) // three frames, 10 fps
            spr.u32(v);
        for (const std::string frame : {"a.dtx", "b.dtx", "c.dtx"})
        {
            spr.u16(static_cast<unsigned>(frame.size()));
            for (char c : frame)
                spr.u8(static_cast<unsigned char>(c));
        }
        test_player::Directory dir;
        dir.children["sprites"].files["lamp.spr"] = spr.b;
        for (auto frame : {"a.dtx", "b.dtx", "c.dtx"})
            dir.files[frame] = {1, 2, 3}; // Indexed only, never decoded.
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::ofstream out(root / "lamp.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
        out.close();
        std::string error;
        return out.good() && g.resources.mount(root / "lamp.rez", error);
    }
    // The same synthetic world and resources in every session; no object exists afterwards.
    bool setup(GameSession& g, bool register_probes = true)
    {
        hooks = {};
        if (!test_player::setup(g, root, false, false) || !mount_sprite(g))
            return false;
        auto other = test_brush::box("other", {-50, 0, -50}, {-40, 10, -40});
        auto brush = test_brush::box("brush", {-10, 0, -10}, {10, 20, 10});
        brush.translation = {5, 10, 0};
        auto duplicate = test_brush::box("Brush", {100, 0, 100}, {110, 10, 110});
        auto volume = test_brush::box("volume", {200, 0, 200}, {240, 40, 240});
        volume.translation = {220, 20, 220};
        auto spinner = test_brush::box("spinner", {300, 0, 300}, {310, 10, 320});
        spinner.translation = {305, 5, 310};
        g.world.models = {other, brush, duplicate, volume, spinner};
        const auto base = g.objects.find_class("BaseClass");
        for (auto* type : {&probe_type, &model_type, &sprite_type, &brush_type, &volume_type, &always_type,
                           &unsupported_type, &not_saved_type, &no_version_type})
            type->parent = base;
        always_child_type.parent = &always_type;
        player_type.parent = g.objects.find_class("CPlayerObj");
        if (!register_probes)
            return g.objects.record_count() == 0;
        for (auto* type : {&probe_type, &model_type, &sprite_type, &brush_type, &volume_type, &player_type,
                           &always_type, &always_child_type, &unsupported_type, &not_saved_type, &no_version_type})
            if (!g.objects.register_class(*type))
                return false;
        return g.objects.record_count() == 0;
    }
    HOBJECT make(GameSession& g, const ClassInfo& type, const char* name, DBYTE object_type = OT_NORMAL,
                 const char* file = "", const char* skin = "", DVector position = {}, DRotation rotation = {})
    {
        ObjectCreateStruct state;
        state.m_ObjectType = object_type;
        legacy_copy(state.m_Name, name);
        legacy_copy(state.m_Filename, file);
        legacy_copy(state.m_SkinName, skin);
        state.m_Pos = position;
        state.m_Rotation = rotation;
        return g.objects.create(&type, state, Creation::Runtime);
    }
    Probe* probe(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
    }
    HOBJECT named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return r->handle;
        return nullptr;
    }
    bool save(GameSession& g, std::span<const HOBJECT> list, SaveObjectsReport& report, const char* file = current,
              DDWORD param = 0, DDWORD flags = SAVEOBJECTS_SAVEGAMECONSOLE)
    {
        return g.server.save_objects(file, list, param, flags, report);
    }
    bool records(const SaveImage& image, std::vector<SaveEngineRecord>& out)
    {
        SaveError error;
        return decode_save_records(image.objects, out, error);
    }
    DRotation turned(float yaw)
    {
        return {{0, std::sin(yaw / 2), 0}, std::cos(yaw / 2)};
    }
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool same(DRotation a, DRotation b)
    {
        return same(a.m_Vec, b.m_Vec) && a.m_Spin == b.m_Spin;
    }

    // ---- Independent encoder for the golden layouts -------------------------------------
    struct Golden
    {
        Bytes b;
        void u8(unsigned v)
        {
            b.push_back(static_cast<std::uint8_t>(v));
        }
        void u16(unsigned v)
        {
            u8(v & 255);
            u8(v >> 8);
        }
        void u32(std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                u8((v >> (8 * i)) & 255);
        }
        void f32(float v)
        {
            u32(std::bit_cast<std::uint32_t>(v));
        }
        void f64(double v)
        {
            const auto bits = std::bit_cast<std::uint64_t>(v);
            for (int i = 0; i < 8; ++i)
                u8((bits >> (8 * i)) & 255);
        }
        void vec(DVector v)
        {
            f32(v.x);
            f32(v.y);
            f32(v.z);
        }
        void rot(DRotation q)
        {
            vec(q.m_Vec);
            f32(q.m_Spin);
        }
        void text(std::string_view s)
        {
            u32(static_cast<std::uint32_t>(s.size()));
            b.insert(b.end(), s.begin(), s.end());
        }
        void blob(const Bytes& v)
        {
            u32(static_cast<std::uint32_t>(v.size()));
            b.insert(b.end(), v.begin(), v.end());
        }
        void record(const SaveEngineRecord& r)
        {
            text(r.class_name);
            u16(r.type);
            u32(r.flags);
            u32(r.user_flags);
            vec(r.position);
            rot(r.rotation);
            vec(r.scale);
            text(r.name);
            text(r.filename);
            text(r.skin);
            u16(r.container_code);
            u32(r.world_model_index);
            rot(r.world_model_basis);
            vec(r.color);
            f32(r.alpha);
            vec(r.dimensions);
            vec(r.velocity);
            vec(r.acceleration);
            u8(r.body);
            f32(r.body_properties.mass);
            f32(r.body_properties.friction);
            u8(r.body_properties.pushable);
            u8(r.body_properties.rotating_debris);
            u8(r.body_properties.polygon_bounds);
            u8(r.blocking_priority);
            u32(r.activity);
            u8(r.auto_deactivate);
            f64(r.deactivation_period);
            f64(r.deactivation_remaining);
            u8(r.scheduled);
            f64(r.update_remaining);
            u32(r.animation);
            f64(r.animation_time);
            u8(r.animation_looping);
            u8(r.animation_done);
            u8(r.animation_first);
            blob(r.hidden_nodes);
            f64(r.sprite_milliseconds);
            u32(r.sprite_frame);
            u8(r.sprite_has_frame);
            u8(r.in_sky);
            u32(r.sky_slot);
            u8(r.local_client);
            blob(r.payload);
        }
    };
    SaveEngineRecord full_record()
    {
        SaveEngineRecord r;
        r.class_name = "SaveBrushProbe";
        r.type = OT_WORLDMODEL;
        r.flags = 0x80000001u;
        r.user_flags = 0x12345678u;
        r.position = {1.5f, -2.25f, 1e6f};
        r.rotation = turned(.5f);
        r.scale = {1, 2, -0.0f};
        r.name = std::string(100, 'n');
        r.filename = "brush";
        r.skin = "skin";
        r.container_code = 0xBEEF;
        r.world_model_index = 1;
        r.world_model_basis = turned(-.25f);
        r.color = {0, .5f, 1};
        r.alpha = .25f;
        r.dimensions = {10, 0, 3};
        r.velocity = {-1, 2, -3};
        r.acceleration = {4, -5, 6};
        r.body = save_records::world_body;
        r.body_properties = {12.5f, 3, true, false, true};
        r.blocking_priority = 200;
        r.activity = OBJSTATE_AUTODEACTIVATE_NOW;
        r.auto_deactivate = true;
        r.deactivation_period = 7.5;
        r.deactivation_remaining = 0.1;
        r.scheduled = true;
        r.update_remaining = 1.0 / 3.0;
        r.animation = save_records::no_index;
        r.animation_time = 12.5;
        r.animation_looping = false;
        r.animation_done = true;
        r.animation_first = false;
        r.hidden_nodes = {0, 1, 1};
        r.sprite_milliseconds = 250.25;
        r.sprite_frame = 4095;
        r.sprite_has_frame = false;
        r.in_sky = true;
        r.sky_slot = 29;
        r.local_client = true;
        r.payload = {1, 2, 3, 0, 255};
        return r;
    }

    // ---- Cases --------------------------------------------------------------------------
    bool codec()
    {
        // CLCK golden layout and bounds.
        SaveClock clock{12.75, 0.05, 2.5, {0, -1000, 5}, 48271};
        Bytes bytes;
        SaveError error;
        CHECK(encode_save_clock(clock, bytes, error));
        Golden g;
        g.f64(12.75);
        g.f64(0.05);
        g.f64(2.5);
        g.vec({0, -1000, 5});
        g.u32(48271);
        CHECK(bytes == g.b && bytes.size() == 40);
        SaveClock back;
        CHECK(decode_save_clock(bytes, back, error) && back == clock);
        for (std::size_t n = 0; n < bytes.size(); ++n)
        {
            CHECK(!decode_save_clock(std::span(bytes).first(n), back, error) && error.fault == SaveFault::Truncated);
            CHECK(back == clock); // Output changes only on success.
        }
        auto extended = bytes;
        extended.push_back(0);
        CHECK(!decode_save_clock(extended, back, error) && error.fault == SaveFault::Format);
        for (auto bad : {SaveClock{-1, 0, 0, {}, 1}, SaveClock{1, -1, 0, {}, 1}, SaveClock{1, 0, 2, {}, 1},
                         SaveClock{NAN, 0, 0, {}, 1}, SaveClock{1, 0, 0, {INFINITY, 0, 0}, 1},
                         SaveClock{1, 0, 0, {}, 0}, SaveClock{1, 0, 0, {}, 2147483647u}})
        {
            CHECK(!encode_save_clock(bad, bytes, error) && error.fault == SaveFault::Format);
            Golden raw;
            raw.f64(bad.time);
            raw.f64(bad.frame_time);
            raw.f64(bad.world_origin);
            raw.vec(bad.global_force);
            raw.u32(bad.generator);
            CHECK(!decode_save_clock(raw.b, back, error) && error.fault == SaveFault::Format);
        }

        // OBJS golden layout of a record with every field nondefault.
        const auto full = full_record();
        SaveEngineRecord plain;
        plain.class_name = "SaveProbe";
        std::vector<SaveEngineRecord> list{full, plain};
        CHECK(encode_save_records(list, bytes, error));
        Golden objs;
        objs.u32(2);
        objs.record(full);
        objs.record(plain);
        CHECK(bytes == objs.b);
        std::vector<SaveEngineRecord> decoded;
        CHECK(decode_save_records(bytes, decoded, error) && decoded == list);
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(!decode_save_records(std::span(bytes).first(n), decoded, error) &&
                  error.fault == SaveFault::Truncated && decoded == list);
        extended = bytes;
        extended.push_back(0);
        CHECK(!decode_save_records(extended, decoded, error) && error.fault == SaveFault::Format);

        // Every field rule refuses on encode and on decode of an independently encoded record.
        std::vector<std::function<void(SaveEngineRecord&)>> invalid{
            [](auto& r) { r.class_name.clear(); },
            [](auto& r) { r.class_name = std::string(65, 'c'); },
            [](auto& r) { r.type = 5; },
            [](auto& r) { r.position.x = NAN; },
            [](auto& r) { r.scale.z = INFINITY; },
            [](auto& r) { r.rotation = {{0, 0, 0}, 1.01f}; },
            [](auto& r) { r.name = std::string(101, 'n'); },
            [](auto& r) { r.skin = std::string("a\0b", 3); },
            [](auto& r) { r.world_model_index = save_records::no_index; },
            [](auto& r) { r.filename.clear(); },
            [](auto& r) { r.world_model_basis = {{0, 0, 0}, 2}; },
            [](auto& r)
            {
                r.type = OT_NORMAL;
                r.body = 0;
            }, // BSP binding on a plain object.
            [](auto& r) { r.color.y = 1.5f; },
            [](auto& r) { r.alpha = -0.1f; },
            [](auto& r) { r.dimensions.y = -1; },
            [](auto& r) { r.velocity.x = INFINITY; },
            [](auto& r) { r.acceleration.z = NAN; },
            [](auto& r) { r.body = save_records::world_body | save_records::model_body; },
            [](auto& r) { r.body = 8; },
            [](auto& r) { r.body_properties.mass = -1; },
            [](auto& r) { r.body_properties.friction = NAN; },
            [](auto& r) { r.activity = 4; },
            [](auto& r) { r.deactivation_period = 2e6; },
            [](auto& r) { r.deactivation_remaining = -1; },
            [](auto& r) { r.deactivation_period = 0; }, // Automatic deactivation needs a period.
            [](auto& r) { r.update_remaining = NAN; },
            [](auto& r) { r.update_remaining = 2e9; },
            [](auto& r)
            {
                r.scheduled = false;
                r.update_remaining = 1;
            },
            [](auto& r) { r.animation_time = -1; },
            [](auto& r) { r.hidden_nodes = {0, 2}; },
            [](auto& r) { r.hidden_nodes.assign(4097, 0); },
            [](auto& r) { r.sprite_milliseconds = NAN; },
            [](auto& r) { r.sprite_frame = 4096; },
            [](auto& r) { r.sky_slot = 30; },
            [](auto& r)
            {
                r.in_sky = false;
                r.sky_slot = 3;
            },
            [](auto& r) { r.payload.assign(65537, 0); },
        };
        for (std::size_t i = 0; i < invalid.size(); ++i)
        {
            auto r = full;
            invalid[i](r);
            std::string why;
            CHECK(!valid_save_record(r, why) && !why.empty());
            Bytes kept{9};
            CHECK(!encode_save_records(std::vector{r}, kept, error) && kept == Bytes{9});
            Golden raw;
            raw.u32(1);
            raw.record(r);
            CHECK(!decode_save_records(raw.b, decoded, error));
            CHECK(error.fault == SaveFault::Format || error.fault == SaveFault::Bound);
        }
        // Booleans are exactly 0 or 1; counts are bounded before allocating.
        {
            Golden raw;
            raw.u32(1);
            raw.record(full);
            auto flipped = raw.b;
            const auto local = flipped.size() - (4 + full.payload.size()) - 1;
            CHECK(flipped[local] == 1);
            flipped[local] = 2;
            CHECK(!decode_save_records(flipped, decoded, error) && error.fault == SaveFault::Format);
            Golden count;
            count.u32(65537);
            CHECK(!decode_save_records(count.b, decoded, error) && error.fault == SaveFault::Bound);
            Golden twice;
            twice.u32(2);
            twice.record(full);
            twice.record(full); // Two local-client records.
            CHECK(!decode_save_records(twice.b, decoded, error) && error.fault == SaveFault::Format);
            CHECK(!encode_save_records(std::vector{full, full}, bytes, error));
        }

        // LINK and ATCH layouts and index rules.
        std::vector<SaveLink> links{{0, 1}, {1, 0}};
        CHECK(encode_save_links(links, 2, bytes, error));
        Golden l;
        l.u32(2);
        for (auto v : {0u, 1u, 1u, 0u})
            l.u32(v);
        CHECK(bytes == l.b);
        std::vector<SaveLink> link_back;
        CHECK(decode_save_links(bytes, 2, link_back, error) && link_back == links);
        CHECK(!decode_save_links(bytes, 1, link_back, error) && error.fault == SaveFault::Format);
        CHECK(!encode_save_links(std::vector<SaveLink>{{0, 0}}, 2, bytes, error));
        Golden dup;
        dup.u32(2);
        for (auto v : {0u, 1u, 0u, 1u})
            dup.u32(v);
        CHECK(!decode_save_links(dup.b, 2, link_back, error) && error.fault == SaveFault::Format);
        std::vector<SaveAttachment> attachments{{0, 1, "hand", {1, 2, 3}, turned(1)}, {1, 2, "", {}, {}}};
        CHECK(encode_save_attachments(attachments, 3, bytes, error));
        Golden a;
        a.u32(2);
        for (const auto& at : attachments)
        {
            a.u32(at.parent);
            a.u32(at.child);
            a.text(at.node);
            a.vec(at.offset);
            a.rot(at.rotation);
        }
        CHECK(bytes == a.b);
        std::vector<SaveAttachment> attachment_back;
        CHECK(decode_save_attachments(bytes, 3, attachment_back, error) && attachment_back == attachments);
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(!decode_save_attachments(std::span(bytes).first(n), 3, attachment_back, error));
        for (auto bad : {SaveAttachment{0, 0, "", {}, {}}, SaveAttachment{0, 3, "", {}, {}},
                         SaveAttachment{0, 1, std::string(101, 'x'), {}, {}},
                         SaveAttachment{0, 1, "", {NAN, 0, 0}, {}}, SaveAttachment{0, 1, "", {}, {{0, 0, 0}, 0}}})
            CHECK(!encode_save_attachments(std::vector{bad}, 3, bytes, error));
        Golden twice_attached;
        twice_attached.u32(2);
        for (std::uint32_t parent : {0u, 2u})
        {
            twice_attached.u32(parent);
            twice_attached.u32(1);
            twice_attached.text("");
            twice_attached.vec({});
            twice_attached.rot({});
        }
        CHECK(!decode_save_attachments(twice_attached.b, 3, attachment_back, error) &&
              error.fault == SaveFault::Format);
        return true;
    }

    bool deadlines()
    {
        // restored_deadline is the exact inverse of remaining_delay at the saved clock.
        std::mt19937_64 random(7);
        std::uniform_real_distribution<double> clock(0, 1e5), delay(0, 1e3), tiny(0, 1e-9);
        std::uint64_t adjusted = 0;
        for (int i = 0; i < 200000; ++i)
        {
            const double t = i % 7 == 0 ? 0.1 * (i % 13) : clock(random);
            double deadline = t + (i % 5 == 0 ? tiny(random) : delay(random));
            if (i % 11 == 0)
                deadline = std::nextafter(t + std::ldexp(1.0, -(i % 60)), INFINITY);
            const double remaining = remaining_delay(t, deadline);
            CHECK(remaining >= 0 && std::isfinite(remaining));
            const double restored = restored_deadline(t, remaining);
            CHECK(remaining_delay(t, restored) == remaining);
            adjusted += restored != t + remaining;
        }
        std::cout << "deadlines: " << adjusted << " rounded sums adjusted to an exact neighbour\n";
        CHECK(remaining_delay(10, 5) == 0 && remaining_delay(10, 10) == 0 && restored_deadline(10, 0) == 10);

        // The registry clock is restorable only while empty and idle.
        GameSession g;
        CHECK(setup(g));
        CHECK(g.objects.restore_clock(42.5, .125) && g.objects.time() == 42.5 && g.objects.frame_time() == .125);
        CHECK(!g.objects.restore_clock(-1, 0) && !g.objects.restore_clock(1, NAN) && g.objects.time() == 42.5);
        auto h = make(g, probe_type, "clock");
        CHECK(h && !g.objects.restore_clock(1, 0) && g.objects.time() == 42.5);
        CHECK(g.objects.remove(h) && !g.objects.restore_clock(1, 0)); // Pending removal still present.
        CHECK(g.objects.finish_frame() && g.objects.restore_clock(1, 0) && g.objects.time() == 1);
        // A restore started inside a callback is refused before anything changes.
        bool refused = false;
        hooks.initial = [&](HOBJECT, float)
        {
            RestoreObjectsReport report;
            refused = !g.server.restore_objects(current, 0, 0, report) && !report.error.empty();
            refused = refused && !g.objects.restore_clock(5, 0);
        };
        CHECK(make(g, probe_type, "inside") && refused && g.objects.time() == 1);
        return true;
    }

    // Builds the nondefault world used by the round-trip cases. Returns the save list.
    struct World
    {
        HOBJECT model = nullptr, sprite = nullptr, brush = nullptr, volume = nullptr, plain = nullptr;
        HOBJECT child = nullptr, player = nullptr, always = nullptr, always_child = nullptr, unsaved = nullptr;
        HOBJECT spinner = nullptr;
        std::vector<HOBJECT> list;
    };
    bool build(GameSession& g, World& w)
    {
        auto& s = g.server;
        CHECK(g.objects.advance(10.5));
        s.reset_world_time(); // World-time origin 10.5.
        CHECK(g.objects.advance(2.25));
        DVector force{0, -1000, 5};
        CHECK(s.SetGlobalForce(&force) == DE_OK);
        int drawn = 0;
        for (int i = 0; i < 3; ++i)
            CHECK(s.RandomInt(0, 1000, drawn));
        s.SetGameConVar("SaveProbeVar", "value with spaces");

        w.always = make(g, always_type, "always");
        w.model = make(g, model_type, "model", OT_MODEL, "models\\characters\\caleb.abc",
                       "skins\\characters\\caleb1.dtx", {12.5f, 3.25f, -7}, turned(.75f));
        w.sprite = make(g, sprite_type, "sprite", OT_SPRITE, "sprites\\lamp.spr", "", {1, 2, 3});
        w.brush = make(g, brush_type, "brush", OT_WORLDMODEL, "brush");
        w.volume = make(g, volume_type, "volume", OT_CONTAINER, "volume");
        w.plain = make(g, probe_type, "plain", OT_NORMAL, "", "", {-4, 5, -6});
        w.child = make(g, probe_type, "child");
        w.player = make(g, player_type, "player");
        w.always_child = make(g, always_child_type, "always child");
        w.unsaved = make(g, probe_type, "unsaved");
        w.spinner = make(g, brush_type, "spinner", OT_WORLDMODEL, "spinner");
        if (!(w.always && w.model && w.sprite && w.brush && w.volume && w.plain && w.child && w.player &&
              w.always_child && w.unsaved))
            for (const auto& d : g.objects.diagnostics())
                std::cerr << "diagnostic: " << d << "\n";
        CHECK(w.always && w.model && w.sprite && w.brush && w.volume && w.plain && w.child && w.player &&
              w.always_child && w.unsaved);

        auto* model = g.objects.get(w.model);
        CHECK(model->model && model->model->animations.size() > 2 && model->model->nodes.size() == 2);
        s.SetObjectFlags(w.model, FLAG_VISIBLE | FLAG_SOLID | FLAG_GRAVITY);
        s.SetObjectUserFlags(w.model, 0x1234 | 8); // USRFLG_SAVEABLE
        CHECK(s.ScaleObject(w.model, {2, 3, 4}));
        CHECK(s.SetObjectColor(w.model, {.25f, .5f, .75f}, .5f));
        model->dimensions = {10, 20, 30};
        model->velocity = {1, -2, 3};
        model->acceleration = {-4, 5, -6};
        CHECK(s.ConfigureCharacterBody(w.model, 250, 3));
        CHECK(s.SetBlockingPriority(w.model, 9));
        CHECK(s.SetDeactivationTime(w.model, 7.5f));
        s.SetObjectState(w.model, OBJSTATE_INACTIVE_TOUCH);
        s.SetNextUpdate(w.model, 1.25f);
        model->state.m_ContainerCode = 7;
        model->animation = {2, 25, false, false, false, model->animation.revision};
        CHECK(s.SetModelNodeHideStatus(w.model, "hand", DTRUE) == DE_OK);

        auto* sprite = g.objects.get(w.sprite);
        CHECK(sprite->sprite && sprite->sprite->frames.size() == 3);
        sprite->sprite_track = {120.5, 2, false};
        CHECK(s.SetObjectColor(w.sprite, {1, .5f, .25f}, .75f));
        // The brush replaces the sprite's slot; the sprite keeps INSKY membership.
        CHECK(s.AddSkyObject(w.sprite, 4) && s.AddSkyObject(w.brush, 4));

        auto* brush = g.objects.get(w.brush);
        CHECK(brush->world_model && brush->world_model_index == 1 && brush->world_model_candidates == 2);
        brush->state.m_Pos = vector_add(brush->state.m_Pos, {0, 0, 30}); // Translated, unrotated.
        s.SetObjectFlags(w.brush, FLAG_BOXPHYSICS);
        CHECK(s.ConfigureWorldBody(w.brush, {50, 2, true, false, false})); // A pushable dynamic body.
        // A rotated world model: the posed shell differs from its authored base.
        const auto pivot = g.objects.get(w.spinner)->state.m_Pos;
        CHECK(s.RotateWorldModel(w.spinner, {{0, 0, 0}, {0, .5f, 0}, pivot, pivot}) == MoverResult::Moved);
        CHECK(g.objects.get(w.spinner)->world_model != g.objects.get(w.spinner)->world_model_base);
        auto* volume = g.objects.get(w.volume);
        CHECK(volume->container() && volume->world_model_index == 3);
        s.SetObjectState(w.volume, OBJSTATE_INACTIVE);

        probe(g, w.plain)->value = 0xC0FFEE;
        probe(g, w.plain)->ref = w.model;         // Listed: remapped.
        probe(g, w.model)->ref = w.always;        // Always-load: NULL.
        probe(g, w.sprite)->ref = w.unsaved;      // Not listed: NULL.
        probe(g, w.brush)->ref = w.always_child;  // Inherits always-load: NULL.
        probe(g, w.child)->ref = w.child;         // Itself.
        s.SetNextUpdate(w.plain, 0.001f);
        s.SetNextUpdate(w.child, 0.5f);
        CHECK(g.objects.advance(0.25)); // child and plain overdue? plain updated; child still pending.
        s.SetObjectState(w.child, OBJSTATE_INACTIVE);
        s.SetNextUpdate(w.child, 0.05f);
        CHECK(g.objects.advance(0.1)); // Inactive: the child's update stays overdue.
        CHECK(g.objects.get(w.child)->scheduled && g.objects.get(w.child)->due < g.objects.time());

        CHECK(s.CreateInterObjectLink(w.model, w.plain) == DE_OK);
        CHECK(s.CreateInterObjectLink(w.plain, w.model) == DE_OK);
        CHECK(s.CreateInterObjectLink(w.plain, w.unsaved) == DE_OK); // Unsaved end: dropped.
        CHECK(s.CreateInterObjectLink(w.always, w.plain) == DE_OK);  // Always-load end: dropped.
        const DVector offset{1, 2, 3};
        const auto rotation = turned(.3f);
        HATTACHMENT attachment = 0;
        CHECK(s.CreateAttachment(w.model, w.child, "hand", &offset, &rotation, &attachment) == DE_OK);
        CHECK(s.CreateAttachment(w.model, w.unsaved, nullptr, &offset, &rotation, &attachment) == DE_OK);
        auto client = s.connect_client();
        CHECK(client && s.SetClientUserData(client, w.player) == DE_OK);
        CHECK(g.objects.finish_frame());

        // The shell lists in its own order; the engine writes creation order.
        w.list = {w.unsaved, w.spinner, w.player, w.always_child, w.child,  w.plain,
                  w.volume,  w.brush,   w.sprite, w.model,        w.always, w.plain};
        w.list.erase(w.list.begin()); // `unsaved` is not listed.
        return true;
    }

    bool roundtrip()
    {
        SaveStore store;
        SaveObjectsReport saved;
        SaveClock clock;
        std::vector<int> draws;
        double update_remaining = 0, deactivation_remaining = 0;
        DVector spinner_min{}, spinner_max{};
        DRotation spinner_rotation{};
        {
            GameSession g;
            CHECK(setup(g));
            World w;
            CHECK(build(g, w));
            clock = g.server.save_clock();
            spinner_min = g.objects.get(w.spinner)->world_model->min;
            spinner_max = g.objects.get(w.spinner)->world_model->max;
            spinner_rotation = g.objects.get(w.spinner)->state.m_Rotation;
            hooks.events.clear();
            CHECK(save(g, w.list, saved));
            CHECK(saved.error.empty() && saved.file == "Current");
            CHECK(saved.listed == 11 && saved.saved == 8 && saved.always_load == 2 && saved.duplicates == 1);
            CHECK(saved.links == 2 && saved.attachments == 1);
            // Creation order: model, sprite, brush, volume, plain, child, player, spinner.
            CHECK((hooks.events == std::vector<std::string>{"save model", "save sprite", "save brush", "save volume",
                                                            "save plain", "save child", "save player",
                                                            "save spinner"}));
            const auto* image = g.server.save_store.find(SaveStoreSlot::Current);
            CHECK(image && image->records == 8 && image->console_saved && image->param == 0);
            CHECK((image->classes == std::vector<SaveClass>{{"SaveBrushProbe", 2},
                                                            {"SaveModelProbe", 3},
                                                            {"SavePlayerProbe", 1},
                                                            {"SaveProbe", 1},
                                                            {"SaveSpriteProbe", 1},
                                                            {"SaveVolumeProbe", 1}}));
            const auto cons = std::find_if(image->console.begin(), image->console.end(),
                                           [](const auto& v) { return v.name == "SaveProbeVar"; });
            CHECK(cons != image->console.end() && cons->value == "value with spaces");
            std::vector<SaveEngineRecord> list;
            CHECK(records(*image, list) && list.size() == 8);
            CHECK(list[7].world_model_index == 4 && same(list[7].world_model_basis, DRotation{}) &&
                  same(list[7].rotation, spinner_rotation));
            const auto& model = list[0];
            CHECK(model.class_name == "SaveModelProbe" && model.type == OT_MODEL && model.name == "model");
            CHECK(model.flags == (FLAG_VISIBLE | FLAG_SOLID | FLAG_GRAVITY) && model.user_flags == (0x1234 | 8));
            CHECK(std::abs(model.update_remaining - 0.9) < 1e-9 && model.scheduled);
            CHECK(model.deactivation_period == 7.5 && std::abs(model.deactivation_remaining - 7.15) < 1e-9);
            update_remaining = model.update_remaining;
            deactivation_remaining = model.deactivation_remaining;
            CHECK(model.activity == OBJSTATE_INACTIVE_TOUCH && model.auto_deactivate);
            CHECK(model.body == save_records::character_body && model.body_properties.mass == 250);
            CHECK(model.hidden_nodes == (std::vector<std::uint8_t>{0, 1}) && model.animation == 2);
            CHECK(list[1].in_sky && list[1].sky_slot == save_records::no_index);
            CHECK(list[2].in_sky && list[2].sky_slot == 4 && list[2].world_model_index == 1);
            CHECK(list[3].world_model_index == 3 && list[3].activity == OBJSTATE_INACTIVE);
            CHECK(list[5].scheduled && list[5].update_remaining == 0); // Overdue while inactive.
            CHECK(list[6].local_client && !list[0].local_client);
            CHECK(saved.classes.size() == 6 && saved.classes[3].name == "SaveProbe" &&
                  saved.classes[3].records == 2 && saved.classes[3].references == 2);
            store = g.server.save_store;
            for (int i = 0; i < 4; ++i)
            {
                int value = 0;
                CHECK(g.server.RandomInt(0, 1 << 30, value));
                draws.push_back(value);
            }
        }
        GameSession g;
        CHECK(setup(g));
        g.server.save_store = store;
        std::vector<ObjectCreateStruct> creates;
        std::vector<std::size_t> links_at_load;
        hooks.precreate = [&](ObjectCreateStruct& state, float reason)
        {
            CHECK(reason == PRECREATE_SAVEGAME);
            creates.push_back(state);
            return true;
        };
        hooks.load = [&](HOBJECT self, HMESSAGEREAD h, DDWORD)
        {
            auto* p = probe(g, self);
            p->value = g.server.ReadFromMessageDWord(h);
            g.server.ReadFromLoadSaveMessageObject(h, &p->ref);
            links_at_load.push_back(g.objects.links().size() + g.objects.attachments().size());
        };
        RestoreObjectsReport restored;
        CHECK(g.server.restore_objects("save/current/CURRENT.SAV", 0, RESTOREOBJECTS_RESTORETIME, restored));
        CHECK(restored.error.empty() && restored.records == 8 && restored.objects.size() == 8);
        CHECK(restored.clock_restored && restored.console_restored && restored.links == 2 &&
              restored.attachments == 1);
        CHECK(g.server.save_clock() == clock && g.objects.time() == clock.time);
        CHECK(g.server.world_time() == clock.time - clock.world_origin);
        CHECK(std::string(g.server.GetVarValueString(g.server.GetGameConVar("SaveProbeVar"))) == "value with spaces");
        // Pass 1 for every record precedes every Load; links and attachments follow the Loads.
        std::vector<std::string> passes;
        for (const auto& e : hooks.events)
            passes.push_back(e.substr(0, e.find(' ')));
        CHECK(std::count(passes.begin(), passes.end(), "precreate") == 8);
        CHECK(std::find(passes.begin(), passes.end(), "load") - passes.begin() == 16);
        CHECK((links_at_load == std::vector<std::size_t>(8, 0)));
        CHECK(creates.size() == 8 && creates[0].m_ObjectType == OT_MODEL && std::string(creates[0].m_Name) == "model");
        CHECK(std::string(creates[0].m_Filename) == "models\\characters\\caleb.abc" &&
              std::string(creates[0].m_SkinName) == "skins\\characters\\caleb1.dtx");
        CHECK(creates[0].m_Flags == (FLAG_VISIBLE | FLAG_SOLID | FLAG_GRAVITY) && creates[0].m_ContainerCode == 7);
        CHECK(creates[0].m_NextUpdate == static_cast<float>(update_remaining) && same(creates[0].m_Scale, {2, 3, 4}));
        CHECK(creates[5].m_NextUpdate == 0); // Overdue: due at once, no float hint.

        // Restored objects follow creation order and every engine field.
        const auto model = named(g, "model"), plain = named(g, "plain"), child = named(g, "child"),
                   sprite = named(g, "sprite"), brush = named(g, "brush"), volume = named(g, "volume");
        CHECK(model && plain && child && sprite && brush && volume && restored.objects[0] == model);
        CHECK(restored.local_client == named(g, "player"));
        for (std::size_t i = 1; i < restored.objects.size(); ++i)
            CHECK(g.objects.get(restored.objects[i])->creation > g.objects.get(restored.objects[i - 1])->creation);
        const auto* m = g.objects.get(model);
        CHECK(m->reason == Creation::Restore && m->state.m_ObjectType == OT_MODEL && m->model);
        CHECK(same(m->state.m_Pos, {12.5f, 3.25f, -7}) && same(m->state.m_Rotation, turned(.75f)));
        CHECK(same(m->state.m_Scale, {2, 3, 4}) && m->user_flags == (0x1234 | 8));
        CHECK(same(m->color, {.25f, .5f, .75f}) && m->alpha == .5f && same(m->dimensions, {10, 20, 30}));
        CHECK(same(m->velocity, {1, -2, 3}) && same(m->acceleration, {-4, 5, -6}));
        CHECK(m->character_body && !m->world_body && m->body_properties.mass == 250 &&
              m->body_properties.friction == 3 && m->blocking_priority == 9);
        CHECK(m->activity == OBJSTATE_INACTIVE_TOUCH && m->auto_deactivate && m->deactivation_period == 7.5);
        CHECK(remaining_delay(clock.time, m->deactivate_at) == deactivation_remaining && m->scheduled);
        CHECK(remaining_delay(clock.time, m->due) == update_remaining); // Exact, not the float hint.
        CHECK(m->animation.animation == 2 && m->animation.time == 25 && !m->animation.looping &&
              !m->animation.done && !m->animation.first);
        CHECK(m->hidden_nodes == (std::vector<std::uint8_t>{0, 1}) && m->state.m_ContainerCode == 7);
        const auto* sp = g.objects.get(sprite);
        CHECK(sp->sprite_track.milliseconds == 120.5 && sp->sprite_track.frame == 2 && !sp->sprite_track.has_frame);
        CHECK(same(sp->color, {1, .5f, .25f}) && sp->alpha == .75f);
        const auto sky_slot = std::find(g.server.sky.objects.begin(), g.server.sky.objects.end(),
                                        g.objects.get(brush)->id) - g.server.sky.objects.begin();
        CHECK(sky_slot == 4 && g.server.sky.registered.size() == 2);
        CHECK(std::find(g.server.sky.objects.begin(), g.server.sky.objects.end(), sp->id) ==
              g.server.sky.objects.end());
        const auto* b = g.objects.get(brush);
        CHECK(b->world_model && b->world_model == b->world_model_base && b->world_model_index == 1);
        CHECK(b->world_body && b->body_properties.pushable && b->body_properties.mass == 50 &&
              b->body_properties.friction == 2 && b->state.m_Flags == FLAG_BOXPHYSICS);
        CHECK(same(b->state.m_Pos, {5, 10, 30}) && same(b->world_model->origin, {5, 10, 0}));
        CHECK(g.objects.get(volume)->container() && g.objects.get(volume)->activity == OBJSTATE_INACTIVE);
        // The rotated world model rebuilds its base at the authored basis and poses the saved rotation once.
        const auto* spinner = g.objects.get(named(g, "spinner"));
        CHECK(spinner && same(spinner->world_model_basis, DRotation{}) &&
              same(spinner->state.m_Rotation, spinner_rotation));
        CHECK(spinner->world_model != spinner->world_model_base && spinner->world_model_index == 4);
        CHECK(same(spinner->world_model->min, spinner_min) && same(spinner->world_model->max, spinner_max));
        const auto* c = g.objects.get(child);
        CHECK(c->scheduled && c->due == g.objects.time() && c->activity == OBJSTATE_INACTIVE && c->attachment);
        // Payloads and LoadSave references remapped through the new index table.
        CHECK(probe(g, plain)->value == 0xC0FFEE && probe(g, plain)->ref == model);
        CHECK(!probe(g, model)->ref && !probe(g, sprite)->ref && !probe(g, brush)->ref);
        CHECK(probe(g, child)->ref == child);
        const auto& links = g.objects.links();
        CHECK((links == std::vector<std::pair<HOBJECT, HOBJECT>>{{model, plain}, {plain, model}}));
        HATTACHMENT found = 0;
        CHECK(g.objects.find_attachment(model, child, &found) == DE_OK && g.objects.attachments().size() == 1);
        const auto& attached = g.objects.attachments().at(found);
        CHECK(attached.node == "hand" && same(attached.offset, {1, 2, 3}) && same(attached.rotation, turned(.3f)));

        // A save immediately after the load reproduces the loaded bytes (no rewrites here),
        // once the local client is associated again as the restored-client branch does.
        auto client = g.server.connect_client();
        CHECK(client && g.server.SetClientUserData(client, restored.local_client) == DE_OK);
        SaveObjectsReport again;
        std::vector<HOBJECT> list(restored.objects.rbegin(), restored.objects.rend());
        CHECK(save(g, list, again, autosave));
        const auto* first = store.find(SaveStoreSlot::Current);
        const auto* second = g.server.save_store.find(SaveStoreSlot::Auto);
        CHECK(second->objects == first->objects && second->links == first->links &&
              second->attachments == first->attachments && second->clock == first->clock);
        CHECK(second->classes == first->classes && second->console == first->console);
        for (int expected : draws)
        {
            int value = 0;
            CHECK(g.server.RandomInt(0, 1 << 30, value) && value == expected); // Generator restored (D10).
        }
        return true;
    }

    bool order()
    {
        GameSession g;
        CHECK(setup(g));
        auto a = make(g, probe_type, "a"), b = make(g, probe_type, "b"), c = make(g, probe_type, "c");
        CHECK(g.objects.remove(a) && g.objects.finish_frame());
        auto d = make(g, probe_type, "d"); // Reuses a's slot; still created last.
        CHECK(g.objects.get(d)->id.slot == 0 && g.objects.get(d)->creation == 3);
        auto pending = make(g, probe_type, "pending");
        auto always = make(g, always_type, "always"), inherited = make(g, always_child_type, "inherited");
        probe(g, b)->ref = pending;
        probe(g, c)->ref = inherited;
        probe(g, d)->ref = b;
        CHECK(g.objects.remove(pending));
        SaveObjectsReport report;
        hooks.events.clear();
        std::vector<HOBJECT> list{d, inherited, c, pending, always, b, d, c};
        CHECK(save(g, list, report));
        CHECK((hooks.events == std::vector<std::string>{"save b", "save c", "save d"}));
        CHECK(report.saved == 3 && report.pending == 1 && report.always_load == 2 && report.duplicates == 2);
        std::vector<SaveEngineRecord> saved;
        CHECK(records(*g.server.save_store.find(SaveStoreSlot::Current), saved));
        CHECK(saved[0].name == "b" && saved[1].name == "c" && saved[2].name == "d");
        // b's pending target and c's inherited always-load target are NULL; d references b (index 0).
        CHECK(saved[0].payload == (Bytes{0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF}));
        CHECK(saved[1].payload == (Bytes{0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF}));
        CHECK(saved[2].payload == (Bytes{0, 0, 0, 0, 0, 0, 0, 0}));
        CHECK(g.objects.finish_frame());

        // Unsupported, NotSaved and versionless classes refuse the whole save, naming them.
        const auto previous = *g.server.save_store.find(SaveStoreSlot::Current);
        auto u = make(g, unsupported_type, "u"), n = make(g, not_saved_type, "n"), v = make(g, no_version_type, "v");
        std::vector<HOBJECT> refused{b, u, n, v, u};
        hooks.events.clear();
        CHECK(!save(g, refused, report));
        CHECK(hooks.events.empty() && report.refused.size() == 3);
        CHECK(report.refused[0] == "SaveNoVersion (Original without a payload version)" &&
              report.refused[1] == "SaveNotSaved (NotSaved)" && report.refused[2] == "SaveUnsupported (Unsupported)");
        CHECK(report.error.find("SaveUnsupported") != std::string::npos);
        CHECK(*g.server.save_store.find(SaveStoreSlot::Current) == previous); // Previous image kept.
        // An original class keeps the Unsupported default until its lot declares it.
        CHECK(save_support(g.objects.find_class("CPlayerObj")) == SaveSupport::Unsupported);
        auto spot = g.objects.create(g.objects.find_class("CameraSpot"), {}, Creation::World, {});
        std::vector<HOBJECT> original{spot, b};
        CHECK(spot && !save(g, original, report) &&
              report.refused == std::vector<std::string>{"CameraSpot (Unsupported)"});
        // A stale handle in the list is an explicit refusal.
        CHECK(g.objects.remove(u) && g.objects.finish_frame());
        std::vector<HOBJECT> stale{b, u};
        CHECK(!save(g, stale, report) && report.error.find("stale") != std::string::npos);
        return true;
    }

    bool dispatch()
    {
        SaveStore store;
        {
            GameSession g;
            CHECK(setup(g));
            auto a = make(g, probe_type, "a"), b = make(g, probe_type, "b");
            probe(g, a)->ref = b; // A forward reference.
            probe(g, b)->ref = a;
            CHECK(g.server.CreateInterObjectLink(a, b) == DE_OK);
            DDWORD seen = 99;
            hooks.save = [&](HOBJECT self, HMESSAGEWRITE h, DDWORD param)
            {
                seen = param;
                g.server.WriteToLoadSaveMessageObject(h, probe(g, self)->ref);
                g.server.WriteToMessageObject(h, probe(g, self)->ref); // Plain: NULL, counted per class (D7).
            };
            std::vector<HOBJECT> list{a, b};
            SaveObjectsReport report;
            CHECK(save(g, list, report, keep, 1, 0));
            CHECK(seen == 1 && hooks.params == (std::vector<float>{1, 1}));
            CHECK(report.classes.size() == 1 && report.classes[0].plain_objects == 2 &&
                  report.classes[0].null_references == 0 && report.classes[0].references == 2);
            const auto* image = g.server.save_store.find(SaveStoreSlot::Keep);
            CHECK(image && image->param == 1 && !image->console_saved && image->console.empty());
            store = g.server.save_store;
        }
        // Keep-alive restore into a populated world keeps its clock (no RESTORETIME).
        GameSession g;
        CHECK(setup(g));
        g.server.save_store = store;
        CHECK(g.objects.advance(3.5));
        auto existing = make(g, probe_type, "existing");
        const auto before = g.server.save_clock();
        std::vector<std::string> order;
        bool forward = false;
        hooks.initial = [&](HOBJECT self, float reason)
        {
            if (reason == INITIALUPDATE_SAVEGAME)
            {
                // Initial update wins for flags and the next update; the engine overwrites user flags.
                g.server.SetObjectFlags(self, FLAG_VISIBLE);
                g.server.SetObjectUserFlags(self, 0xFFFF);
                g.server.SetNextUpdate(self, 0.5f);
            }
        };
        hooks.load = [&](HOBJECT self, HMESSAGEREAD h, DDWORD param)
        {
            order.push_back(probe(g, self)->name() + " " + std::to_string(param));
            g.server.ReadFromLoadSaveMessageObject(h, &probe(g, self)->ref);
            CHECK(!g.server.ReadFromMessageObject(h)); // The plain NULL reads back as NULL.
            // Every restored object already exists, including forward references.
            forward = forward || (probe(g, self)->ref && g.objects.get(probe(g, self)->ref));
            if (probe(g, self)->name() == "a") // A Load recreating its link is not duplicated.
                g.server.CreateInterObjectLink(self, probe(g, self)->ref);
            return true;
        };
        RestoreObjectsReport restored;
        CHECK(g.server.restore_objects(keep, 1, 0, restored));
        CHECK((order == std::vector<std::string>{"a 1", "b 1"}) && forward);
        CHECK(!restored.clock_restored && !restored.console_restored && g.server.save_clock() == before);
        CHECK(restored.links == 0 && restored.existing_links == 1 && g.objects.links().size() == 1);
        const auto a = restored.objects[0];
        const auto* r = g.objects.get(a);
        CHECK(r->state.m_Flags == FLAG_VISIBLE && r->user_flags == 0 && r->due == g.objects.time() + 0.5f);
        CHECK(probe(g, a)->ref == restored.objects[1] && g.objects.get(existing));
        // RESTORETIME with existing objects requires the restored clock to be current.
        CHECK(!g.server.restore_objects(keep, 1, RESTOREOBJECTS_RESTORETIME, restored));
        CHECK(restored.error.find("saved clock") != std::string::npos);
        return true;
    }

    // The lot-4 order: console and clock on the empty registry, then the restore-mode world's
    // always-load objects, then RestoreObjects with RESTORETIME on a populated registry.
    bool phases()
    {
        SaveStore store;
        SaveClock clock;
        {
            GameSession g;
            CHECK(setup(g));
            CHECK(g.objects.advance(6.25));
            g.server.SetGameConVar("PhaseVar", "saved");
            auto a = make(g, probe_type, "a");
            g.server.SetNextUpdate(a, 2);
            std::vector<HOBJECT> list{a};
            SaveObjectsReport report;
            CHECK(save(g, list, report));
            clock = g.server.save_clock();
            store = g.server.save_store;
        }
        {
            GameSession g;
            CHECK(setup(g));
            g.server.save_store = store;
            std::string error;
            CHECK(!g.server.restore_image_state(SaveStoreSlot::Auto, true, error) && !error.empty());
            g.server.SetGameConVar("PhaseVar", "fresh");
            CHECK(g.server.restore_image_state(SaveStoreSlot::Current, true, error));
            CHECK(g.server.save_clock() == clock);
            CHECK(std::string(g.server.GetVarValueString(g.server.GetGameConVar("PhaseVar"))) == "saved");
            double authored_time = -1;
            hooks.initial = [&](HOBJECT, float) { authored_time = g.objects.time(); };
            auto authored = make(g, always_type, "authored"); // Starts from the restored clock (D9).
            CHECK(authored && authored_time == clock.time);
            hooks.initial = nullptr;
            CHECK(!g.server.restore_image_state(SaveStoreSlot::Current, true, error));
            RestoreObjectsReport report;
            CHECK(g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report));
            CHECK(!report.clock_restored && !report.console_restored && report.records == 1);
            CHECK(g.objects.get(report.objects[0])->due == clock.time + 2 && g.objects.get(authored));
        }
        GameSession g;
        CHECK(setup(g));
        g.server.save_store = store;
        std::string error;
        CHECK(g.server.restore_image_state(SaveStoreSlot::Current, true, error));
        CHECK(make(g, always_type, "authored") && g.objects.advance(0.05)); // Simulation advanced.
        RestoreObjectsReport report;
        CHECK(!g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report));
        CHECK(report.error.find("saved clock") != std::string::npos && g.objects.record_count() == 1);
        return true;
    }

    bool purity()
    {
        // The pure set is sorted, unique and part of the reviewed catalogue.
        std::set<std::string_view> catalogue;
        for (const auto& definition : service_catalog())
            catalogue.insert(definition.name);
        GameSession g;
        CHECK(setup(g));
        auto& s = g.server;
        auto target = make(g, probe_type, "target"), saved = make(g, probe_type, "saved");
        auto other = make(g, probe_type, "other");
        std::vector<HOBJECT> list{saved, target};
        SaveObjectsReport report;
        CHECK(save(g, list, report)); // A pure save first.
        const auto image = *s.save_store.find(SaveStoreSlot::Current);

        struct Attempt
        {
            const char* service;
            std::function<bool()> call;
        };
        DVector v{1, 2, 3};
        DRotation q{};
        HATTACHMENT attachment = 0;
        std::string error;
        SoundRequest sound;
        sound.resource = "sounds\\none.wav";
        std::vector<Attempt> attempts{
            {"CreateObject", [&] { CHECK(!make(g, probe_type, "created")); return true; }},
            {"RemoveObject", [&] { s.RemoveObject(other); return true; }},
            {"SetNextUpdate", [&] { s.SetNextUpdate(other, 1); return true; }},
            {"SetObjectState", [&] { s.SetObjectState(other, OBJSTATE_INACTIVE); return true; }},
            {"SetDeactivationTime", [&] { CHECK(!s.SetDeactivationTime(other, 2)); return true; }},
            {"PingObjects", [&] { CHECK(!s.PingObjects(other)); return true; }},
            {"StartMessageToObject", [&] { CHECK(!s.StartMessageToObject(nullptr, other, 1000)); return true; }},
            {"CreateInterObjectLink", [&] { CHECK(s.CreateInterObjectLink(other, target) != DE_OK); return true; }},
            {"BreakInterObjectLink", [&] { s.BreakInterObjectLink(other, target); return true; }},
            {"CreateAttachment",
             [&] { CHECK(s.CreateAttachment(target, other, nullptr, &v, &q, &attachment) != DE_OK); return true; }},
            {"RandomInt", [&] { int x = 0; CHECK(!s.RandomInt(0, 9, x)); return true; }},
            {"RandomFloat", [&] { float x = 0; CHECK(!s.RandomFloat(0, 9, x)); return true; }},
            {"PlayClientSound", [&] { CHECK(!s.PlayClientSound(sound, v, error)); return true; }},
            {"PlayObjectLoopSound", [&] { CHECK(!s.PlayObjectLoopSound(other, sound, error)); return true; }},
            {"PublishSparks", [&] { CHECK(!s.PublishSparks(other, {}, error)); return true; }},
            {"SetObjectFlags", [&] { s.SetObjectFlags(other, FLAG_VISIBLE); return true; }},  // Detected.
            {"SetGameConVar", [&] { s.SetGameConVar("PureProbe", "1"); return true; }},       // Detected.
            {"CastRay",
             [&]
             {
                 SegmentHit hit;
                 s.CastRay(v, {4, 5, 6}, hit);
                 return true;
             }},
            {"FindNamedObjects", [&] { if (auto* l = s.FindNamedObjects("x")) s.RelinquishList(l); return true; }},
            {"SaveObjects", [&] { CHECK(s.SaveObjects(current, nullptr, 0, 0) != DE_OK); return true; }},
        };
        for (const auto& attempt : attempts)
        {
            const auto records = g.objects.snapshot();
            const auto clock = s.save_clock();
            const auto handles = g.objects.handle_count();
            const auto strings = s.string_count(), messages = s.message_count(), voices = s.audio.voices().size();
            const auto scheduled = g.objects.get(other)->scheduled;
            const auto activity = g.objects.get(other)->activity;
            bool body_ok = false;
            hooks.save = [&](HOBJECT self, HMESSAGEWRITE h, DDWORD)
            {
                if (self == saved)
                    body_ok = attempt.call();
                g.server.WriteToMessageDWord(h, 1);
            };
            CHECK(!save(g, list, report));
            CHECK(body_ok);
            CHECK(report.failed.impure >= 1 || report.failed.error.find("purity") != std::string::npos);
            CHECK(report.error.find("purity") != std::string::npos);
            CHECK(report.error.find(attempt.service) != std::string::npos);
            // Nothing the contract protects changed, and the previous image survived.
            CHECK(g.objects.snapshot() == records && s.save_clock() == clock && g.objects.handle_count() == handles);
            CHECK(s.string_count() == strings && s.message_count() == messages && s.audio.voices().size() == voices);
            CHECK(g.objects.get(other)->scheduled == scheduled && g.objects.get(other)->activity == activity);
            CHECK(g.objects.get(other)->life == Life::Alive && g.objects.links().empty() &&
                  !g.objects.get(other)->attachment);
            CHECK(*s.save_store.find(SaveStoreSlot::Current) == image && !g.objects.pure_callback());
        }
        // Strings, queries and nested messages remain available, and leave no residue.
        hooks.save = [&](HOBJECT self, HMESSAGEWRITE h, DDWORD)
        {
            HSTRING text = s.CreateString("name");
            s.WriteToMessageHString(h, text);
            s.FreeString(text);
            DVector p{};
            s.GetObjectPos(self, &p);
            s.WriteToMessageVector(h, &p);
            s.WriteToMessageFloat(h, s.GetTime());
            auto nested = s.StartHMessageWrite();
            s.WriteToMessageByte(nested, 1);
            s.WriteToMessageHMessageWrite(h, nested);
            s.EndHMessageWrite(nested);
        };
        const auto strings = s.string_count(), messages = s.message_count();
        CHECK(save(g, list, report) && s.string_count() == strings && s.message_count() == messages);
        // Consuming a pending owned reader (the original client blob) is the allowed mutation.
        auto writer = s.StartHMessageWrite(), nested = s.StartHMessageWrite();
        s.WriteToMessageByte(nested, 7);
        CHECK(s.WriteToMessageHMessageWrite(writer, nested) == DE_OK);
        s.EndHMessageWrite(nested);
        HMESSAGEREAD blob = s.ReadFromMessageHMessageRead(writer);
        s.EndHMessageWrite(writer);
        CHECK(blob && s.message_count() == messages + 1);
        hooks.save = [&](HOBJECT self, HMESSAGEWRITE h, DDWORD)
        {
            if (self == saved && blob)
            {
                s.WriteToMessageHMessageRead(h, blob);
                s.EndHMessageRead(blob);
                blob = nullptr;
            }
            s.WriteToMessageByte(h, 1);
        };
        CHECK(save(g, list, report) && !blob && s.message_count() == messages);
        // The pure set is catalogued, sorted and excludes every mutating category.
        std::cout << "purity: " << attempts.size() << " impure services refused\n";
        for (auto name : {"CreateObject", "RemoveObject", "SetNextUpdate", "StartMessageToObject", "EndMessage",
                          "RandomInt", "PlayLocalSound", "PublishPolyGrid", "SetObjectState"})
            CHECK(catalogue.contains(name));
        return true;
    }

    bool failures()
    {
        SaveStore store;
        {
            GameSession g;
            CHECK(setup(g));
            CHECK(g.objects.advance(4));
            auto a = make(g, probe_type, "a"), b = make(g, probe_type, "b"), c = make(g, probe_type, "c");
            auto brush = make(g, brush_type, "brush", OT_WORLDMODEL, "brush");
            probe(g, a)->ref = c;
            CHECK(g.server.CreateInterObjectLink(a, b) == DE_OK);
            std::vector<HOBJECT> list{a, b, c, brush};
            SaveObjectsReport report;
            CHECK(save(g, list, report));
            store = g.server.save_store;
        }
        // Each failure leaves the registry, clock, console and the stored image unchanged.
        auto attempt = [&](const std::function<void(GameSession&, SaveImage&)>& prepare, const char* expected,
                           DDWORD flags = RESTOREOBJECTS_RESTORETIME, const char* file = current)
        {
            GameSession g;
            CHECK(setup(g));
            g.server.save_store = store;
            auto image = *store.find(SaveStoreSlot::Current);
            prepare(g, image);
            g.server.save_store.put(SaveStoreSlot::Current, image);
            const auto before = g.objects.snapshot();
            const auto clock = g.server.save_clock();
            const auto variables = g.server.variables().size();
            const auto pending = g.objects.pending_count();
            const auto handles = g.objects.handle_count();
            RestoreObjectsReport report;
            CHECK(!g.server.restore_objects(file, 0, flags, report));
            if (report.error.find(expected) == std::string::npos)
                std::cerr << "unexpected error: " << report.error << "\n";
            CHECK(report.error.find(expected) != std::string::npos);
            CHECK(g.objects.snapshot() == before && g.server.save_clock() == clock);
            CHECK(g.server.variables().size() == variables && report.objects.empty() && !report.local_client);
            CHECK(g.objects.pending_count() == pending && g.objects.links().empty() && !g.objects.pure_callback());
            CHECK(g.server.save_store.find(SaveStoreSlot::Current) != nullptr);
            std::cout << "refused (" << g.objects.handle_count() - handles << " tokens consumed): " << report.error
                      << "\n";
            return true;
        };
        auto none = [](GameSession&, SaveImage&) {};
        CHECK(attempt(none, "not the original", RESTOREOBJECTS_RESTORETIME, "Save\\Current\\Other.sav"));
        CHECK(attempt(none, "no saved Auto image", RESTOREOBJECTS_RESTORETIME, autosave));
        CHECK(attempt(none, "unknown RestoreObjects flags", 2));
        CHECK(attempt([](auto& g, auto&) { CHECK(g.objects.remove(make(g, probe_type, "gone"))); return true; },
                      "deferred removals"));
        // Corruption is refused while decoding, before anything changes.
        CHECK(attempt([](auto&, auto& image) { image.objects.pop_back(); }, "OBJS"));
        CHECK(attempt([](auto&, auto& image) { image.objects[0] = 0xFF; }, "OBJS"));
        CHECK(attempt([](auto&, auto& image) { image.clock.resize(8); }, "CLCK"));
        CHECK(attempt([](auto&, auto& image) { image.links[4] = 9; }, "LINK"));
        CHECK(attempt([](auto&, auto& image) { image.attachments.push_back(0); }, "ATCH"));
        CHECK(attempt([](auto&, auto& image) { ++image.records; }, "record count"));
        CHECK(attempt([](auto&, auto& image) { image.classes[1].payload_version = 7; }, "payload version"));
        CHECK(attempt([](auto&, auto& image) { image.classes.erase(image.classes.begin()); }, "payload version"));
        CHECK(attempt([](auto&, auto& image) { image.classes.push_back({"Zz", 1}); }, "absent from the records"));
        CHECK(attempt([](auto& g, auto&) { g.world.models.erase(g.world.models.begin()); }, "BSP binding"));
        // A class this build does not register.
        {
            GameSession g;
            CHECK(setup(g, false));
            g.server.save_store = store;
            RestoreObjectsReport report;
            CHECK(!g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report));
            CHECK(report.error.find("not registered") != std::string::npos && g.objects.snapshot().empty());
        }
        // Failures after objects exist roll back everything this restore created.
        int created = 0;
        CHECK(attempt(
            [&](auto& g, auto&)
            {
                hooks.precreate = [&](ObjectCreateStruct& state, float) { return std::string(state.m_Name) != "c"; };
                hooks.initial = [&](HOBJECT, float reason)
                {
                    if (reason == INITIALUPDATE_SAVEGAME && make(g, probe_type, "spawned child"))
                        ++created;
                };
                g.server.SetGameConVar("Existing", "x");
            },
            "record 2 (SaveProbe) was refused"));
        CHECK(created == 2);
        CHECK(attempt([&](auto& g, auto&)
                      { hooks.load = [&](HOBJECT, HMESSAGEREAD h, DDWORD) { g.server.ReadFromMessageDWord(h); }; },
                      "under-read"));
        CHECK(attempt(
            [&](auto& g, auto&)
            {
                hooks.load = [&](HOBJECT self, HMESSAGEREAD h, DDWORD)
                {
                    g.server.ReadFromMessageDWord(h);
                    HOBJECT ref = nullptr;
                    g.server.ReadFromLoadSaveMessageObject(h, &ref);
                    if (probe(g, self)->name() == "a")
                        g.server.ReadFromMessageDWord(h); // Over-read.
                };
            },
            "MID_LOADOBJECT failed for record 0"));
        CHECK(attempt(
            [&](auto& g, auto&)
            {
                hooks.load = [&](HOBJECT self, HMESSAGEREAD h, DDWORD)
                {
                    g.server.ReadFromMessageDWord(h);
                    HOBJECT ref = nullptr;
                    g.server.ReadFromLoadSaveMessageObject(h, &ref);
                    if (probe(g, self)->name() == "c")
                        g.server.RemoveObject(named(g, "b")); // The saved link loses an end.
                };
            },
            "link 0"));
        // A precreate that changes the next-update hint uses its own value.
        {
            GameSession g;
            CHECK(setup(g));
            g.server.save_store = store;
            hooks.precreate = [](ObjectCreateStruct& state, float)
            {
                state.m_NextUpdate = 2;
                return true;
            };
            RestoreObjectsReport report;
            CHECK(g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report));
            CHECK(g.objects.get(report.objects[0])->due == g.objects.time() + 2);
        }
        return true;
    }

    bool store_rules()
    {
        SaveStoreSlot slot{};
        CHECK(ServerAdapter::save_store_slot("save/current/auto.SAV", slot) && slot == SaveStoreSlot::Auto);
        CHECK(ServerAdapter::save_store_slot("SAVE\\CURRENT\\KEEP.SAV", slot) && slot == SaveStoreSlot::Keep);
        CHECK(!ServerAdapter::save_store_slot("Keep.sav", slot) && !ServerAdapter::save_store_slot("", slot));
        CHECK(!ServerAdapter::save_store_slot("Save\\Current\\Current.sav ", slot));
        GameSession g;
        CHECK(setup(g));
        auto a = make(g, probe_type, "a");
        std::vector<HOBJECT> list{a};
        SaveObjectsReport report;
        CHECK(!save(g, list, report, current, 2) && report.error.find("dwParam") != std::string::npos);
        CHECK(!save(g, list, report, current, 0, 4) && report.error.find("flags") != std::string::npos);
        CHECK(save(g, list, report, current, 0, SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS));
        const auto image = *g.server.save_store.find(SaveStoreSlot::Current);
        // Console values over the CONS bound refuse the save and keep the stored image.
        g.server.SetGameConVar("TooLong", std::string(1025, 'x').c_str());
        CHECK(!save(g, list, report) && report.error.find("CONS") != std::string::npos);
        CHECK(*g.server.save_store.find(SaveStoreSlot::Current) == image);
        CHECK(save(g, list, report, current, 0, 0)); // Without the console flag it is not saved.
        CHECK(!g.server.save_store.find(SaveStoreSlot::Current)->console_saved);
        // The SDK thunks read the shell's ObjectList.
        auto* objects = g.server.CreateObjectList();
        g.server.AddObjectToList(objects, a);
        CHECK(g.server.SaveObjects(keep, objects, 1, 0) == DE_OK);
        CHECK(g.server.SaveObjects(nullptr, objects, 1, 0) == DE_INVALIDPARAMS);
        g.server.RelinquishList(objects);
        CHECK(g.server.SaveObjects(keep, objects, 1, 0) == DE_INVALIDPARAMS); // Released list.
        CHECK(g.server.save_store.find(SaveStoreSlot::Keep)->records == 1 && g.server.saves == 3);
        // The store survives world unload; restoring Keep into the emptied registry works.
        CHECK(g.unload_world());
        CHECK(g.server.save_store.find(SaveStoreSlot::Keep) && g.objects.record_count() == 0);
        CHECK(g.server.RestoreObjects(keep, 1, 0) == DE_OK && g.objects.record_count() == 1);
        CHECK(g.server.restores == 1 && g.server.last_restore.records == 1);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    root = argv[2];
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    const std::map<std::string, bool (*)()> cases{{"codec", codec},       {"deadlines", deadlines},
                                                  {"roundtrip", roundtrip}, {"order", order},
                                                  {"dispatch", dispatch}, {"phases", phases}, {"purity", purity},
                                                  {"failures", failures}, {"store", store_rules}};
    const auto found = cases.find(argv[1]);
    return found != cases.end() && found->second() ? 0 : 1;
}
