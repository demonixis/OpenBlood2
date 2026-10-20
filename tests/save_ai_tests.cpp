#include "engine/config.h"
#include "engine/save_fingerprint.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CivilianAI.h"
#include "game/original/Corpse.h"
#include "game/original/CultistAI.h"
#include "game/original/Fanatic.h"
#include "game/original/PathMgr.h"
#include "game/original/SmellHint.h"
#include "save_ai_fixture.h"
#include "save_world_fixture.h"
#include <array>
#include <bit>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include "scratch_dir.h"

// Original AI_Mgr, CultistAI, CivilianAI, Fanatic, CCorpse and AI_Mgr-only subclass Save/Load
// (save_load_contracts lot 11): the byte layout, checked refusals and the ledge departure on a
// synthetic AI_Mgr, then round trips and identical continuation of every lot-11 class on the
// retail models (private data), and whole-world restores of the mounted worlds.
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
    std::filesystem::path root;
    constexpr const char* current = "Save\\Current\\Current.sav";
    constexpr DDWORD save_flags = SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS;
    constexpr std::uint32_t none = 0xffffffffu;

    // An independent reader of the original layouts: each field is recorded by name with its offset,
    // so a test checks values, order and consumption, and patches a field.
    struct Field
    {
        std::string name;
        std::size_t offset = 0, size = 0;
        std::uint32_t u = 0;
        float f = 0;
        std::optional<std::string> text;
    };
    struct Decoder
    {
        explicit Decoder(std::span<const std::uint8_t> bytes, std::size_t start = 0) : data(bytes), at(start) {}
        std::span<const std::uint8_t> data;
        std::size_t at = 0;
        bool ok = true;
        std::vector<Field> fields;
        std::uint32_t raw(std::size_t n)
        {
            if (!ok || data.size() - at < n)
            {
                ok = false;
                return 0;
            }
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < n; ++i)
                value |= std::uint32_t(data[at + i]) << (8 * i);
            at += n;
            return value;
        }
        Field& add(std::string name, std::size_t size)
        {
            Field field;
            field.name = std::move(name);
            field.offset = at;
            field.size = size;
            fields.push_back(std::move(field));
            return fields.back();
        }
        std::uint32_t integer(std::string name, std::size_t size)
        {
            auto& field = add(std::move(name), size);
            return field.u = raw(size);
        }
        std::uint32_t b(std::string name)
        {
            return integer(std::move(name), 1);
        }
        std::uint32_t d(std::string name)
        {
            return integer(std::move(name), 4);
        }
        float f(std::string name)
        {
            auto& field = add(std::move(name), 4);
            field.u = raw(4);
            return field.f = std::bit_cast<float>(field.u);
        }
        void v(const std::string& name)
        {
            for (auto axis : {".x", ".y", ".z"})
                f(name + axis);
        }
        void s(std::string name) // HString or String: u32 length, 0xFFFFFFFF for NULL.
        {
            auto& field = add(std::move(name), 4);
            const auto length = raw(4);
            field.u = length;
            if (length == none || !ok)
                return;
            if (data.size() - at < length)
            {
                ok = false;
                return;
            }
            field.text = std::string(data.begin() + at, data.begin() + at + length);
            field.size += length;
            at += length;
        }
        const Field* find(std::string_view name) const
        {
            for (const auto& field : fields)
                if (field.name == name)
                    return &field;
            return nullptr;
        }
        std::uint32_t u(std::string_view name) const
        {
            const auto* field = find(name);
            return field ? field->u : none;
        }
        float real(std::string_view name) const
        {
            const auto* field = find(name);
            return field ? field->f : NAN;
        }
    };
    // AI_Mgr::Save (AI_Mgr.cpp:6759-6842) and the OpenBlood2 tail.
    void ai_block(Decoder& d)
    {
        d.d("target");
        d.v("target_pos");
        d.d("track");
        d.v("track_pos");
        d.v("guard");
        d.v("dest");
        d.d("state");
        d.d("last_state");
        for (int i = 0; i < 6; ++i)
            d.f("stimulus" + std::to_string(i));
        d.d("dodge");
        d.s("spot_target");
        d.s("spot_message");
        d.s("relay");
        for (auto name : {"hearing", "smelling", "sensing", "seeing", "walk", "run", "jump", "roll"})
            d.f(name);
        d.f("time_start");
        d.f("load_time_start");
        d.d("corpse");
        d.f("attack_load");
        d.b("animating");
        d.b("jumping");
        d.d("metacmd");
        d.d("cur_metacmd");
        d.d("flags");
        d.d("block_flags");
        d.b("shutdown");
        d.f("last_update");
        d.d("injured");
        d.d("enemy_attach");
        d.v("scale");
        d.f("ledge");
        d.d("animation");
        d.v("dims");
        d.b("start_fire");
        d.f("script_index");
        d.f("wait_end");
        d.d("script_flags");
        d.b("movement");
        d.b("update_command");
        d.b("current.command");
        d.s("current.args");
        const auto commands = d.f("commands");
        for (int i = 0; d.ok && i < int(commands); ++i)
        {
            d.b("command" + std::to_string(i));
            d.s("args" + std::to_string(i));
        }
        const auto points = d.f("points");
        d.b("delete_elements");
        for (int i = 0; d.ok && i < int(points); ++i)
        {
            const auto p = "point" + std::to_string(i);
            d.v(p);
            d.s(p + ".name");
            d.s(p + ".target");
            d.s(p + ".message");
        }
        d.d("tail.attacker");
        d.d("tail.smell");
        d.d("tail.ammo");
        d.b("tail.remove");
        d.b("tail.target_valid");
        d.b("tail.track_valid");
        d.b("tail.plan_failed");
        d.b("tail.script_outcome");
        d.b("tail.motion_command");
        d.b("tail.failed");
        d.s("tail.error");
    }
    using save_ai::is_ai;
    using save_ai::subclass_bytes;
    bool decode(const SaveImage& image, std::vector<SaveEngineRecord>& records)
    {
        SaveError error;
        return decode_save_records(image.objects, records, error);
    }
    // The re-save rewrites listed by save_load_contracts: AI_Mgr::Load adds GetTime() to the raw
    // m_fLastUpdate (AI_Mgr.cpp:6912); CCorpse's PostPropRead restores its constant flags; lot 5's
    // player rewrites (flags, velocity, node reset, zoom and the blob's load type).
    void listed_rewrites(std::vector<SaveEngineRecord>& records)
    {
        for (auto& r : records)
            if (is_ai(r.class_name))
            {
                Decoder d{r.payload, subclass_bytes(r.class_name)};
                ai_block(d);
                if (const auto* field = d.find("last_update"); d.ok && field)
                    std::fill_n(r.payload.begin() + std::ptrdiff_t(field->offset), 4, std::uint8_t(0));
            }
            else if (r.class_name == "CCorpse")
                r.flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_MODELKEYS | FLAG_REMOVEIFOUTSIDE;
            else if (r.class_name == "CPlayerObj")
            {
                r.flags &= ~FLAG_SHADOW;
                r.velocity = {};
                r.hidden_nodes.clear();
                r.payload.clear();
                // Lot 5 (observed, not listed there): the client attach resets the acceleration a
                // moving player had, and the restored player's first update restarts its clip.
                r.acceleration = {};
                r.animation = 0;
                r.animation_time = 0;
                r.animation_looping = r.animation_done = r.animation_first = false;
            }
    }
    bool same(DVector a, DVector b)
    {
        return std::bit_cast<std::array<std::uint32_t, 3>>(a) == std::bit_cast<std::array<std::uint32_t, 3>>(b);
    }
    // Relative float clocks may restore one ulp away from the absolute value (lot 8): accept exactly
    // a one-ulp float difference in a payload and nothing else.
    bool within_clock_ulp(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b)
    {
        if (a.size() != b.size())
            return false;
        auto value = [](const std::vector<std::uint8_t>& p, std::size_t at)
        {
            std::uint32_t bits = 0;
            for (unsigned i = 0; i < 4; ++i)
                bits |= std::uint32_t(p[at + i]) << (8 * i);
            return std::bit_cast<float>(bits);
        };
        for (std::size_t k = 0; k < a.size(); ++k)
        {
            if (a[k] == b[k])
                continue;
            bool explained = false;
            for (std::size_t start = k >= 3 ? k - 3 : 0; start <= k && start + 4 <= a.size() && !explained; ++start)
            {
                const float x = value(a, start), y = value(b, start);
                if (std::isfinite(x) && std::isfinite(y) && std::nextafter(x, y) == y)
                {
                    explained = true;
                    k = start + 3;
                }
            }
            if (!explained)
                return false;
        }
        return true;
    }
    // Compares two images record by record, naming the first difference.
    bool same_image(const SaveImage& a, const SaveImage& b, bool clock, bool clock_ulp = false)
    {
        std::vector<SaveEngineRecord> x, y;
        CHECK(decode(a, x) && decode(b, y));
        listed_rewrites(x);
        listed_rewrites(y);
        if (x.size() != y.size())
        {
            std::cerr << "records: " << x.size() << " vs " << y.size() << '\n';
            for (const auto& r : x)
                std::cerr << "  saved " << r.class_name << '\n';
            for (const auto& r : y)
                std::cerr << "  other " << r.class_name << '\n';
            return false;
        }
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            if (clock_ulp && x[i].payload != y[i].payload && within_clock_ulp(x[i].payload, y[i].payload))
                y[i].payload = x[i].payload;
            if (!(x[i] == y[i]))
            {
                std::cerr << "record " << i << " (" << x[i].class_name << ") differs";
                if (x[i].payload != y[i].payload)
                {
                    std::cerr << " payload " << x[i].payload.size() << " vs " << y[i].payload.size();
                    std::size_t k = 0;
                    while (k < std::min(x[i].payload.size(), y[i].payload.size()) && x[i].payload[k] == y[i].payload[k])
                        ++k;
                    std::cerr << " at byte " << k;
                    if (is_ai(x[i].class_name))
                    {
                        Decoder d{x[i].payload, subclass_bytes(x[i].class_name)};
                        ai_block(d);
                        for (const auto& field : d.fields)
                            if (k >= field.offset && k < field.offset + field.size)
                                std::cerr << " (" << field.name << ")";
                    }
                }
                if (!same(x[i].position, y[i].position))
                    std::cerr << " position " << x[i].position.x << "," << x[i].position.y << "," << x[i].position.z
                              << " vs " << y[i].position.x << "," << y[i].position.y << "," << y[i].position.z;
                if (!same(x[i].velocity, y[i].velocity))
                    std::cerr << " velocity";
                if (x[i].update_remaining != y[i].update_remaining || x[i].scheduled != y[i].scheduled)
                    std::cerr << " delay " << x[i].update_remaining << " vs " << y[i].update_remaining;
                if (x[i].flags != y[i].flags || x[i].user_flags != y[i].user_flags)
                    std::cerr << " flags";
                const auto& p = x[i];
                const auto& q = y[i];
                const std::pair<const char*, bool> fields[]{
                    {"animation", p.animation != q.animation || p.animation_time != q.animation_time ||
                                      p.animation_looping != q.animation_looping ||
                                      p.animation_done != q.animation_done || p.animation_first != q.animation_first},
                    {"rotation", std::memcmp(&p.rotation, &q.rotation, sizeof(DRotation)) != 0},
                    {"scale", !same(p.scale, q.scale)},
                    {"dimensions", !same(p.dimensions, q.dimensions)},
                    {"acceleration", !same(p.acceleration, q.acceleration)},
                    {"color", !same(p.color, q.color) || p.alpha != q.alpha},
                    {"body", p.body != q.body || p.body_properties.mass != q.body_properties.mass ||
                                 p.body_properties.friction != q.body_properties.friction ||
                                 p.blocking_priority != q.blocking_priority},
                    {"activity", p.activity != q.activity || p.auto_deactivate != q.auto_deactivate ||
                                     p.deactivation_period != q.deactivation_period ||
                                     p.deactivation_remaining != q.deactivation_remaining},
                    {"hidden", p.hidden_nodes != q.hidden_nodes},
                    {"names", p.name != q.name || p.filename != q.filename || p.skin != q.skin},
                    {"container", p.container_code != q.container_code},
                    {"sky", p.in_sky != q.in_sky || p.sky_slot != q.sky_slot},
                    {"client", p.local_client != q.local_client}};
                for (const auto& [field, differs] : fields)
                    if (differs)
                        std::cerr << ' ' << field;
                if (p.animation_time != q.animation_time)
                    std::cerr << " (anim ms " << p.animation_time << " vs " << q.animation_time << ")";
                if (p.activity != q.activity || p.scheduled != q.scheduled)
                    std::cerr << " (activity " << p.activity << "/" << p.auto_deactivate << "/" << p.scheduled << " vs "
                              << q.activity << "/" << q.auto_deactivate << "/" << q.scheduled << ")";
                if (p.deactivation_remaining != q.deactivation_remaining)
                    std::cerr << " (deactivation " << p.deactivation_remaining << " vs " << q.deactivation_remaining
                              << ")";
                std::cerr << '\n';
                return false;
            }
        }
        CHECK(a.links == b.links && a.attachments == b.attachments);
        CHECK(a.classes == b.classes && a.records == b.records);
        if (clock)
            CHECK(a.clock == b.clock);
        return true;
    }

    // ------------------------------------------------------------------------------------------
    // Headless: a synthetic AI_Mgr declared Original. Diagnostic actors run no production
    // initialization, so the probe binds its aggregates itself (labelled fixture, not an NPC).
    class SaveAIProbe : public AI_Mgr
    {
      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT value) override
        {
            if (message == MID_INITIALUPDATE)
            {
                m_InventoryMgr.Init(m_hObject, nullptr);
                m_damage.Init(m_hObject, &m_InventoryMgr);
                if (value != INITIALUPDATE_SAVEGAME)
                    m_MoveObj.Init(m_hObject);
            }
            return AI_Mgr::EngineMessageFn(message, data, value);
        }
    };
    const ClassInfo* probe_class(GameSession& g)
    {
        static const ClassInfo type{"SaveAIProbe",
                                    nullptr,
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<SaveAIProbe>(); },
                                    {},
                                    SaveSupport::Original,
                                    1};
        auto* registered = g.objects.find_class(type.name);
        if (registered)
            return registered;
        auto copy = type;
        copy.parent = g.objects.find_class("AI_Mgr");
        static std::vector<std::unique_ptr<ClassInfo>> owned;
        owned.push_back(std::make_unique<ClassInfo>(copy));
        return g.objects.register_class(*owned.back()) ? owned.back().get() : nullptr;
    }
    bool headless_setup(GameSession& g)
    {
        CHECK(test_player::setup(g, root, false, true));
        CHECK(probe_class(g));
        return true;
    }
    SaveAIProbe* probe(GameSession& g, DVector pos, const char* name)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = pos;
        state.m_Flags = FLAG_SOLID | FLAG_GRAVITY;
        SAFE_STRCPY(state.m_Name, name);
        SAFE_STRCPY(state.m_Filename, "Models\\Characters\\Caleb.abc");
        SAFE_STRCPY(state.m_SkinName, "Skins\\Characters\\Caleb1.dtx");
        auto h = g.objects.create(probe_class(g), state);
        auto* r = g.objects.get(h);
        if (!r)
            return nullptr;
        r->user_flags |= USRFLG_SAVEABLE;
        r->dimensions = {1, 2, 1};
        return dynamic_cast<SaveAIProbe*>(r->game.get());
    }
    HOBJECT world(GameSession& g, const char* type, Properties props)
    {
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, std::move(props));
    }
    std::vector<HOBJECT> saveable(GameSession& g)
    {
        std::vector<HOBJECT> list;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && (r->user_flags & USRFLG_SAVEABLE))
                list.push_back(r->handle);
        return list;
    }
    bool save(GameSession& g, SaveImage& image)
    {
        SaveObjectsReport report;
        if (!g.server.save_objects(current, saveable(g), 0, save_flags, report))
        {
            std::cerr << "save: " << report.error << '\n';
            return false;
        }
        image = *g.server.save_store.find(SaveStoreSlot::Current);
        return true;
    }
    bool restore(GameSession& g, const SaveImage& image, RestoreObjectsReport& report)
    {
        g.server.save_store.put(SaveStoreSlot::Current, image);
        return g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report);
    }
    template <class T> T* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<T*>(r->game.get()) : nullptr;
    }
    // The saved probe "a" in a nondefault state with references to a saved probe, always-load
    // objects (unlisted: NULL) and an owned script path.
    struct Built
    {
        SaveImage image;
        float now = 0, last_update = 0, ledge = 0, time_start = 0;
    };
    bool build_probe(GameSession& g, Built& out)
    {
        auto* a = probe(g, {0, 10, 0}, "a");
        auto* b = probe(g, {80, 10, 0}, "b");
        CHECK(a && b);
        // Always-load objects: path points (an owned snapshot) and a smell hint (unlisted).
        CHECK(world(g, "PathPoint",
                    {{"Name", std::string("route1")},
                     {"Pos", DVector{0, 0, 200}},
                     {"ActionTarget", std::string("b")},
                     {"ActionMessage", std::string("TRIGGER")}}));
        CHECK(world(g, "PathPoint", {{"Name", std::string("route2")}, {"Pos", DVector{100, 0, 200}}}));
        auto hint = world(g, "SmellHint", {{"Pos", DVector{0, 0, 50}}});
        CHECK(hint);
        PathMgr paths(g.server);
        CHECK(paths.BuildPathList());
        CHECK(g.step(.05));
        // Run the looping script to its FOLLOWPATH command (labelled zero animation table).
        CHECK(a->InitStatics(std::make_shared<CAnim_Sound>()));
        CHECK(a->PrepareScript("SCRIPT 1; SETMOVEMENT RUN; FOLLOWPATH route; WAIT 3; BOGUS x;"));
        CHECK(a->AI_STATE_Script(paths) && a->AI_STATE_Script(paths));
        CHECK(a->CurrentScriptCommand().command == AI_Mgr::ScriptCommand::FollowPath && a->ScriptPath().size() == 2);
        CHECK(a->SetNewTarget(b->m_hObject) && a->RefreshTargetPosition());
        CHECK(a->SetTrackedObject(hint) && a->SetSmellCursor(hint));
        a->m_damage.RestoreLastDamager(b->m_hObject);
        a->script_index = 2;
        a->script_outcome = AI_Mgr::ScriptOutcome::Running;
        a->m_nLastState = AI_Mgr::StateGuard;
        a->m_fStimuli = {.5f, .25f, 0, .75f, 1, .9f};
        a->m_nDodgeFlags = 0x14;
        a->spot_target = "spotter";
        a->trigger_relay_target = "relay";
        a->m_fHearingDist = 1500;
        a->m_fSeeingDist = 2000;
        a->m_fSensingDist = 300;
        a->m_fSmellingDist = 50;
        out.now = g.server.GetTime();
        a->m_fTimeStart = out.time_start = out.now - 1.25f;
        a->m_fLoadTimeStart = out.now - .5f;
        a->m_nCorpseType = 31;
        a->m_fAttackLoadTime = .7f;
        a->m_bAnimating = true;
        a->Metacmd = 3;
        a->m_nCurMetacmd = 7;
        a->m_dwFlags = 0x2f;
        a->block_flags = 0x12;
        a->m_bSetShutdown = true;
        a->m_fLastUpdate = out.last_update = out.now - .01f;
        a->m_nInjuredLeg = 10;
        a->m_vScale = {1.1f, .9f, 1.1f};
        a->last_ledge_check = out.ledge = out.now - .01f; // Just checked: throttled for 0.04 s more.
        a->m_nCurAnimation = 0;
        a->m_vDims = {12, 40, 12};
        a->m_vGuardPos = {5, 6, 7};
        a->m_vDestPos = {-5, 0, 9};
        a->navigation_plan_failed = true;
        CHECK(save(g, out.image));
        return true;
    }
    bool layout()
    {
        Built built;
        {
            GameSession g;
            CHECK(headless_setup(g));
            CHECK(build_probe(g, built));
        }
        std::vector<SaveEngineRecord> records;
        CHECK(decode(built.image, records) && records.size() == 2 && records[0].class_name == "SaveAIProbe");
        Decoder d{records[0].payload};
        ai_block(d);
        CHECK(d.ok);
        // References: the listed probe by index, always-load objects as the NULL sentinel.
        CHECK(d.u("target") == 1 && d.u("track") == none && d.u("enemy_attach") == none);
        CHECK(d.u("tail.attacker") == 1 && d.u("tail.smell") == none && d.u("tail.ammo") == none);
        CHECK(d.u("state") == AI_Mgr::StateScript && d.u("last_state") == AI_Mgr::StateGuard);
        CHECK(d.real("stimulus5") == .9f && d.u("dodge") == 0x14);
        CHECK(d.find("spot_target")->text == "spotter" && d.find("spot_message")->u == none &&
              d.find("relay")->text == "relay");
        CHECK(d.real("hearing") == 1500 && d.real("smelling") == 50 && d.real("sensing") == 300 &&
              d.real("seeing") == 2000);
        // Relative clocks, raw absolute clocks.
        CHECK(d.real("time_start") == built.time_start - built.now && d.real("last_update") == built.last_update &&
              d.real("ledge") == built.ledge);
        CHECK(d.u("corpse") == 31 && d.u("animating") == 1 && d.u("jumping") == 0 && d.u("start_fire") == 0);
        CHECK(d.u("metacmd") == 3 && d.u("cur_metacmd") == 7 && d.u("flags") == 0x2f && d.u("block_flags") == 0x12);
        CHECK(d.u("shutdown") == 1 && d.u("injured") == 10 && d.real("dims.y") == 40 && d.real("scale.y") == .9f);
        // Script state: loop flag 1, the index as a float, commands with NULL empty args.
        CHECK(d.real("script_index") == 2 && d.u("script_flags") == 1 && d.u("movement") == 1);
        CHECK(d.u("update_command") == 0 && d.u("current.command") == 2 && d.find("current.args")->text == "route");
        CHECK(d.real("commands") == 4 && d.u("command0") == 1 && d.find("args0")->text == "RUN");
        CHECK(d.u("command3") == 0 && d.find("args3")->text == "x"); // BOGUS keeps the original DONE mapping.
        // The original PathMgr enumerates in reverse: route2, then route1 with its action.
        CHECK(d.real("points") == 2 && d.u("delete_elements") == 0 && d.find("point0.name")->text == "route2");
        CHECK(d.find("point1.target")->text == "b" && d.find("point1.message")->text == "TRIGGER");
        CHECK(d.find("point0.target")->u == none && d.find("point0.message")->u == none);
        CHECK(d.u("tail.target_valid") == 1 && d.u("tail.track_valid") == 0 && d.u("tail.plan_failed") == 1);
        CHECK(d.u("tail.script_outcome") == unsigned(AI_Mgr::ScriptOutcome::Running) && d.u("tail.failed") == 0);
        CHECK(d.u("tail.motion_command") == unsigned(AI_Mgr::MotionCommand::FollowPath));
        // CBaseCharacter writes nothing; CMovement follows with its owner reference.
        Decoder movement{records[0].payload, d.at};
        CHECK(movement.d("owner") == 0);

        // Restore into a fresh session and save again.
        GameSession g;
        CHECK(headless_setup(g));
        RestoreObjectsReport report;
        if (!restore(g, built.image, report))
        {
            std::cerr << "restore: " << report.error << '\n';
            return false;
        }
        auto* a = get<SaveAIProbe>(g, report.objects[0]);
        auto* b = get<SaveAIProbe>(g, report.objects[1]);
        CHECK(a && b && a->restored_loads == 1);
        const float now = g.server.GetTime();
        CHECK(now == built.now);
        CHECK(a->GetTarget() == b->m_hObject && !a->GetTrackedObject() && !a->smell_cursor);
        CHECK(a->m_damage.LastDamagerIdentity() == b->m_hObject); // Attacker memory (port tail).
        CHECK(a->m_nState == AI_Mgr::StateScript && a->script_index == 2 && a->ScriptCommands().size() == 4);
        CHECK(a->ScriptCommands()[3].name == "DONE" && a->ScriptCommands()[3].argument == "x");
        CHECK(a->CurrentScriptCommand().command == AI_Mgr::ScriptCommand::FollowPath);
        CHECK(a->ScriptPath().size() == 2 && a->ScriptPath()[1].action_message == "TRIGGER" &&
              !a->ScriptPath()[0].action_target && !a->ScriptPath()[1].source);
        CHECK(a->m_fLastUpdate == built.last_update + now); // Original +GetTime() quirk.
        CHECK(a->last_ledge_check == built.ledge);          // Departure: raw, not +GetTime().
        CHECK(a->m_fTimeStart - now == built.time_start - built.now);
        CHECK(a->spot_target == "spotter" && a->spot_message.empty() && a->m_nInjuredLeg == 10);
        CHECK(a->navigation_plan_failed && a->target_position_valid && a->m_bSetShutdown);
        CHECK(g.objects.link_count(a->m_hObject) >= 1); // The target link came back with LINK.
        SaveImage again;
        CHECK(save(g, again));
        CHECK(same_image(built.image, again, false));
        std::vector<SaveEngineRecord> resaved;
        CHECK(decode(again, resaved));
        Decoder r{resaved[0].payload};
        ai_block(r);
        CHECK(r.ok && r.real("last_update") == built.last_update + now); // The listed rewrite, exactly.
        // Departure (decision 4): the ledge throttle runs from the saved deadline. The original's
        // +GetTime() would suppress ledge checks for about the saved game time.
        CHECK(g.step(.05) && g.step(.05));
        bool edge = false;
        const auto queries = a->ledge_queries;
        CHECK(a->m_MoveObj.initialized && a->IsLedge({0, 0, 1}, edge) && a->ledge_queries == queries + 1);
        std::cout << "AI_Mgr block " << d.at << " bytes; record " << records[0].payload.size()
                  << " bytes; re-save identical apart from m_fLastUpdate\n";
        return true;
    }
    // Every decoded field is checked before a member changes; a refusal restores nothing.
    bool refusals()
    {
        Built built;
        {
            GameSession g;
            CHECK(headless_setup(g));
            CHECK(build_probe(g, built));
        }
        std::vector<SaveEngineRecord> records;
        CHECK(decode(built.image, records));
        Decoder d{records[0].payload};
        ai_block(d);
        CHECK(d.ok);
        struct Patch
        {
            const char* field;
            std::uint32_t value;
            const char* reason;
        };
        const Patch patches[]{
            {"state", 50, "AI state"},
            {"last_state", 999 + 1, "AI state"},
            {"animating", 2, "boolean"},
            {"jumping", 1, "jump or start-fire"},
            {"start_fire", 1, "jump or start-fire"},
            {"enemy_attach", 1, "enemy attachment"},
            {"movement", 3, "script movement"},
            {"delete_elements", 1, "ownership flag"},
            {"current.command", 10, "current script command"},
            {"script_index", std::bit_cast<std::uint32_t>(7.f), "index past the list"},
            {"script_index", std::bit_cast<std::uint32_t>(1.5f), "script command index"},
            {"stimulus2", std::bit_cast<std::uint32_t>(INFINITY), "stimulus"},
            {"seeing", std::bit_cast<std::uint32_t>(-1.f), "distance or speed"},
            {"scale.x", 0, "scale"},
            {"tail.script_outcome", 9, "script outcome"},
            {"tail.smell", 1, "SmellHint"},
            {"target", 0, "self reference"},
        };
        for (const auto& patch : patches)
        {
            auto image = built.image;
            auto edited = records;
            const auto* field = d.find(patch.field);
            CHECK(field);
            for (std::size_t i = 0; i < field->size && i < 4; ++i)
                edited[0].payload[field->offset + i] = std::uint8_t(patch.value >> (8 * i));
            SaveError error;
            CHECK(encode_save_records(edited, image.objects, error));
            GameSession g;
            CHECK(headless_setup(g));
            RestoreObjectsReport report;
            CHECK(!restore(g, image, report));
            if (report.error.find(patch.reason) == std::string::npos)
            {
                std::cerr << patch.field << ": " << report.error << '\n';
                return false;
            }
            CHECK(g.objects.finish_frame());
            for (auto id : g.objects.snapshot())
                CHECK(!get<AI_Mgr>(g, g.objects.get(id)->handle)); // Rolled back.
        }
        std::cout << std::size(patches) << " corrupt AI_Mgr fields refused before any change\n";
        return true;
    }

    // ------------------------------------------------------------------------------------------
    // Private: a synthetic DAT world (one floor triangle, a start point, path points and exit hints)
    // mounted over the retail data, through GameSession::save_game/load_game with the local player.
    std::filesystem::path data_dir;
    using save_world::object;
    using save_world::text;
    using save_world::vector;
    bool write_arena()
    {
        test_player::Directory dir;
        dir.children["worlds"].files["ai_arena.dat"] =
            save_world::dat({object("GameStartPoint", {text("Name", "start"), vector("Pos", {0, 10, 700})}),
                             object("PathPoint", {text("Name", "route1"), vector("Pos", {-300, 0, -300})}),
                             object("PathPoint", {text("Name", "route2"), vector("Pos", {300, 0, -300})}),
                             object("PathPoint", {text("Name", "route3"), vector("Pos", {0, 0, 300})}),
                             object("ExitHint", {text("Name", "exit1"), vector("Pos", {-500, 10, -700})}),
                             object("ExitHint", {text("Name", "exit2"), vector("Pos", {500, 10, -700})})},
                            "arena.dtx");
        dir.files["arena.dtx"] = save_world::dtx();
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        std::ofstream out(root / "ai_arena.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(archive.b.data()), static_cast<std::streamsize>(archive.b.size()));
        return !ec && out.good();
    }
    bool arena(std::unique_ptr<GameSession>& game)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize());
        Config config;
        config.data_dir = data_dir;
        CHECK(game->resources.mount_game(config, error));
        CHECK(game->resources.mount(root / "ai_arena.rez", error));
        if (!game->request_world("ai_arena", error) || !game->run_world(error))
        {
            std::cerr << "arena: " << error << '\n';
            return false;
        }
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, DVector pos, Properties p = {})
    {
        p.insert_or_assign("Pos", pos);
        p.try_emplace("Name", std::string("npc"));
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, std::move(p));
        auto* a = g.server.IsObjectAlive(h) ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        if (!a || !a->RuntimeReady())
            for (const auto& diagnostic : g.objects.diagnostics())
                std::cerr << diagnostic << '\n';
        return a && a->RuntimeReady() ? a : nullptr;
    }
    HOBJECT named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return r->handle;
        return nullptr;
    }
    bool steps(GameSession& g, unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
            if (!g.step(1.0 / 60))
            {
                for (const auto& d : g.objects.diagnostics())
                    std::cerr << d << '\n';
                return false;
            }
        return true;
    }
    bool damage(GameSession& g, HOBJECT from, HOBJECT to, float amount, DamageType type = DamageType::Normal)
    {
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(from), to, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{0, 0, 1}, {}, amount, type, from}) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    bool trigger(GameSession& g, HOBJECT to, const char* text)
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, to, 1000);
        CHECK(str && msg && g.server.WriteToMessageHString(msg, str) == DE_OK);
        g.server.FreeString(str);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    // One round trip of the current arena: save, record `after` steps of the saved world, load the
    // save (replacing that world), check it, save again (identical apart from the listed rewrites),
    // then match every recorded image. `diverges` names an original loss the continuation cannot
    // reproduce; the trip then stops comparing at that step and reports it.
    struct Trip
    {
        unsigned after = 90;
        std::function<bool(GameSession&)> check;
        std::function<bool(GameSession&)> continued;
    };
    bool round_trip(const char* label, std::unique_ptr<GameSession>& game, const Trip& trip)
    {
        GameSave saved;
        std::string error;
        if (!game->save_game(saved, error))
        {
            std::cerr << label << " save: " << error << '\n';
            return false;
        }
        // CPlayerObj::m_fNextPingTime is not saved (an original loss outside this lot): the
        // restored player pings the NPCs at once. Carry it over so the comparison isolates AI state.
        CHECK(player(*game));
        const float next_ping = player(*game)->m_fNextPingTime;
        std::vector<GameSave> expected(trip.after);
        for (auto& image : expected)
            CHECK(steps(*game, 1) && game->save_game(image, error));
        if (!GameSession::load_game(game, saved, error))
        {
            std::cerr << label << " load refused at " << game->game_load.stage << ": " << error << '\n';
            return false;
        }
        CHECK(player(*game) && player(*game)->m_fNextPingTime == 0);
        player(*game)->m_fNextPingTime = next_ping;
        if (trip.check && !trip.check(*game))
        {
            std::cerr << label << ": restored state check failed\n";
            return false;
        }
        GameSave again;
        CHECK(game->save_game(again, error));
        if (!same_image(saved.image, again.image, false))
        {
            std::cerr << label << ": the restored world saves different bytes\n";
            return false;
        }
        for (unsigned i = 0; i < trip.after; ++i)
        {
            GameSave y;
            CHECK(steps(*game, 1) && game->save_game(y, error));
            if (!same_image(expected[i].image, y.image, true, true))
            {
                std::cerr << label << ": the worlds diverge after " << i + 1 << " steps\n";
                return false;
            }
        }
        if (trip.continued)
            CHECK(trip.continued(*game));
        std::cout << label << ": " << saved.image.records << " records, " << saved.image.objects.size()
                  << " OBJS bytes, " << trip.after << " identical steps\n";
        return true;
    }
    AI_Mgr* first_ai(GameSession& g, std::string_view type)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && r->type->name == type)
                return dynamic_cast<AI_Mgr*>(r->game.get());
        return nullptr;
    }
    // Every lot-11 class saved and loaded idle, then continuing identically.
    bool classes()
    {
        const char* types[]{"CultistAI",  "CivilianAI", "Fanatic",    "ShikariAI",   "SoulDrudge",   "MadScientistAI",
                            "Behemoth",   "Prophet",    "DrudgeLord", "GabriellaAI", "GabriellaREV", "OpheliaAI",
                            "OpheliaREV", "IshmaelAI",  "IshmaelREV", "CalebAI",     "DeathShroud"};
        for (auto type : types)
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            // Far from the player (out of sight range): the idle and passive branches.
            auto* a = npc(*game, type, {0, 40, -800});
            CHECK(a);
            a->m_fSeeingDist = a->m_fHearingDist = a->m_fSensingDist = a->m_fSmellingDist = 100;
            CHECK(steps(*game, 30));
            const auto state = a->m_nState;
            const auto weapon = a->m_InventoryMgr.CurrentWeapon() ? a->m_InventoryMgr.CurrentWeapon()->data.m_nType : 0;
            Trip trip;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, type);
                CHECK(b && b->RuntimeReady() && b->m_nState == state && b->restored_loads == 1);
                CHECK((b->m_InventoryMgr.CurrentWeapon() ? b->m_InventoryMgr.CurrentWeapon()->data.m_nType : 0) ==
                      weapon);
                CHECK(b->AnimSound() && b->m_MoveObj.initialized);
                return true;
            };
            if (!round_trip(type, game, trip))
                return false;
        }
        return true;
    }
    // Damage aimed at the actor's torso from the player, as a vector weapon delivers it.
    bool hit(GameSession& g, HOBJECT from, HOBJECT to, float amount, DamageType type = DamageType::Normal)
    {
        DVector torso, origin;
        DRotation rotation;
        CHECK(g.server.QueryModelNodeTransform(to, "torso", &torso, &rotation) == ServerDE::NodeQuery::Found);
        CHECK(g.server.GetObjectPos(from, &origin) == DE_OK);
        auto direction = vector_subtract(torso, origin);
        direction = vector_scale(direction, 1 / std::sqrt(vector_dot(direction, direction)));
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(from), to, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {direction, torso, amount, type, from}) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    template <class F> bool until(GameSession& g, unsigned limit, F done)
    {
        for (unsigned i = 0; i < limit; ++i)
        {
            if (done())
                return true;
            CHECK(steps(g, 1));
        }
        return done();
    }
    void narrow(AI_Mgr& a)
    {
        a.m_fSeeingDist = a.m_fHearingDist = a.m_fSensingDist = a.m_fSmellingDist = 100;
    }
    // Representative states: a target attack, a script mid-path, a recoil with its unconsumed attacker
    // memory, a civilian escape, a lit Fanatic and a burning corpse.
    bool states()
    {
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            // The original player Load zeroes its velocity: a knocked-back target would move apart in
            // the two worlds. This fixture keeps the player still (its own lot 5 covers that rule).
            player(*game)->m_damage.apply_damage_physics = false;
            auto* a = npc(*game, "CultistAI", {0, 40, 250});
            CHECK(a);
            const auto target = player(*game)->m_hObject;
            CHECK(until(*game, 600, [&] { return a->GetTarget() == target && a->fire_shots > 0; }));
            const auto state = a->m_nState;
            CHECK(state == AI_Mgr::StateAttackClose || state == AI_Mgr::StateAttackFar);
            Trip trip;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->GetTarget() == player(g)->m_hObject && b->m_nState == state);
                return true;
            };
            trip.continued = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->RuntimeReady() && b->GetTarget() == player(g)->m_hObject && b->fire_shots > 0);
                return true;
            };
            CHECK(round_trip("Cultist attacking the player", game, trip));
        }
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = npc(*game, "CultistAI", {0, 40, -100});
            CHECK(a);
            narrow(*a);
            CHECK(steps(*game, 10));
            CHECK(trigger(*game, a->m_hObject, "SCRIPT; SETMOVEMENT WALK; FOLLOWPATH route; WAIT 30;"));
            CHECK(until(*game, 1200, [&] { return a->ScriptPath().size() == 2 && a->path_arrivals == 1; }));
            CHECK(a->m_nState == AI_Mgr::StateScript);
            const auto left = a->ScriptPath().size();
            const auto destination = a->ScriptPath().front().position;
            Trip trip;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->m_nState == AI_Mgr::StateScript && b->ScriptPath().size() == left);
                CHECK(b->CurrentScriptCommand().command == AI_Mgr::ScriptCommand::FollowPath);
                CHECK(same(b->ScriptPath().front().position, destination) && b->ScriptCommands().size() == 1);
                return true;
            };
            trip.continued = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->RuntimeReady() && b->script_motion_updates > 0);
                return true;
            };
            CHECK(round_trip("Cultist scripted mid-path", game, trip));
        }
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = npc(*game, "CultistAI", {0, 40, -100});
            CHECK(a);
            narrow(*a);
            CHECK(steps(*game, 10));
            CHECK(hit(*game, player(*game)->m_hObject, a->m_hObject, 10));
            CHECK(a->m_nState == AI_Mgr::StateRecoil && a->m_damage.LastDamagerIdentity());
            CHECK(until(*game, 60, [&] { return a->recoil_starts == 1 || a->m_nState != AI_Mgr::StateRecoil; }));
            if (a->m_nState != AI_Mgr::StateRecoil || a->recoil_starts != 1)
                std::cerr << "recoil state " << a->m_nState << " starts " << a->recoil_starts << " node "
                          << a->recoil_node << " error " << a->runtime_error << '\n';
            CHECK(a->m_nState == AI_Mgr::StateRecoil && a->recoil_starts == 1 && a->m_bAnimating);
            const float health = a->m_damage.m_fHitPoints;
            Trip trip;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->m_nState == AI_Mgr::StateRecoil && b->m_damage.m_fHitPoints == health);
                // The recoil leaves the attacker memory unconsumed: the port tail brings it back.
                CHECK(b->m_damage.LastDamagerIdentity() == player(g)->m_hObject);
                return true;
            };
            trip.after = 150;
            trip.continued = [&](GameSession& g)
            {
                // After the recoil the memory selects the attacker, as in the saved world.
                auto* b = first_ai(g, "CultistAI");
                CHECK(b && b->GetTarget() == player(g)->m_hObject && !b->m_damage.LastDamagerIdentity());
                return true;
            };
            CHECK(round_trip("Cultist recoiling", game, trip));
        }
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = npc(*game, "CivilianAI", {0, 40, 300}, {{"Scared", true}});
            CHECK(a);
            // FindVisible never accepts an ExitHint (CanSee needs a ray hit on the target), so a scared
            // civilian's EscapeRun falls through to EscapeHide: it faces the player and begs.
            CHECK(until(*game, 600, [&] { return a->m_nState == AI_Mgr::StateEscapeHide && a->m_bAnimating; }));
            CHECK(a->GetTarget() == player(*game)->m_hObject);
            const auto phase = a->Metacmd;
            Trip trip;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CivilianAI");
                CHECK(b && b->m_nState == AI_Mgr::StateEscapeHide && b->Metacmd == phase && b->m_bAnimating);
                CHECK(b->GetTarget() == player(g)->m_hObject && dynamic_cast<CivilianAI*>(b)->m_bScared);
                return true;
            };
            CHECK(round_trip("Civilian escaping", game, trip));
        }
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = dynamic_cast<Fanatic*>(npc(*game, "Fanatic", {0, 40, -600}));
            CHECK(a);
            narrow(*a);
            CHECK(steps(*game, 10));
            CHECK(a->SetNewTarget(player(*game)->m_hObject) && a->RefreshTargetPosition() &&
                  a->SetNewState(AI_Mgr::StateSpecial1));
            CHECK(until(*game, 600, [&] { return a->m_fStartTime != 0 && a->Metacmd == 3 && a->smoke_effect; }));
            const float start = a->m_fStartTime;
            Trip trip;
            trip.after = 60;
            trip.check = [&](GameSession& g)
            {
                auto* b = dynamic_cast<Fanatic*>(first_ai(g, "Fanatic"));
                CHECK(b && b->m_fStartTime == start && g.server.IsObjectAlive(b->m_hSmokeSource) && b->m_bStartSmoke);
                return true;
            };
            trip.continued = [&](GameSession& g)
            {
                auto* b = dynamic_cast<Fanatic*>(first_ai(g, "Fanatic"));
                CHECK(b && b->smoke_publications == 1 && b->m_hAttach); // Republished by the next update.
                return true;
            };
            CHECK(round_trip("Fanatic lit and sprinting", game, trip));
        }
        {
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = npc(*game, "CivilianAI", {0, 40, -100});
            CHECK(a);
            narrow(*a);
            CHECK(steps(*game, 10));
            const auto victim = a->m_hObject;
            CHECK(hit(*game, player(*game)->m_hObject, victim, 1000, DamageType::Fire));
            CCorpse* corpse = nullptr;
            CHECK(until(*game, 600,
                        [&]
                        {
                            for (auto id : game->objects.snapshot())
                                if (auto* c = get<CCorpse>(*game, game->objects.get(id)->handle))
                                    corpse = c;
                            return corpse && corpse->fire_source && !game->server.IsObjectAlive(victim);
                        }));
            const float health = corpse->m_damage.m_fHitPoints;
            const bool limbs = corpse->limb_loss;
            const auto root_dir = corpse->sound_root;
            const auto spheres = corpse->measurements.m_fHitSpheres;
            Trip trip;
            trip.after = 0; // The source keeps burning; the restored corpse has lost its fire (original).
            trip.check = [&](GameSession& g)
            {
                CCorpse* c = nullptr;
                for (auto id : g.objects.snapshot())
                    if (auto* x = get<CCorpse>(g, g.objects.get(id)->handle))
                        c = x;
                CHECK(c && c->initialized && c->configured && c->measurements.hit_spheres_valid);
                CHECK(!c->fire_source && !c->smoke_source && !c->burn_attachment && !c->burn_sound);
                CHECK(c->m_damage.m_fHitPoints == health && c->limb_loss == limbs && c->sound_root == root_dir);
                CHECK(c->measurements.m_fHitSpheres == spheres && !c->dead);
                return true;
            };
            trip.continued = [&](GameSession& g) { return steps(g, 120) && g.server.unavailable_services.empty(); };
            CHECK(round_trip("Civilian corpse burning", game, trip));
        }
        {
            // A civilian between its corpse transfer and its deferred self-removal (m_bRemoveMe).
            std::unique_ptr<GameSession> game;
            CHECK(arena(game));
            auto* a = npc(*game, "CivilianAI", {0, 40, -100});
            CHECK(a);
            narrow(*a);
            CHECK(steps(*game, 10));
            const auto victim = a->m_hObject;
            // Hits below the 90 % gib threshold, in separate updates (each resets the percentage).
            const float part = a->m_damage.m_fMaxHitPoints * .6f;
            bool transferred = false;
            for (int i = 0; i < 6 && !transferred; ++i)
            {
                CHECK(game->server.IsObjectAlive(victim) && hit(*game, player(*game)->m_hObject, victim, part));
                until(*game, 30,
                      [&]
                      {
                          transferred = game->server.IsObjectAlive(victim) && a->corpses_created == 1;
                          return transferred || !game->server.IsObjectAlive(victim);
                      });
            }
            CHECK(transferred); // Not gibbed: the corpse transfer happened and removal is pending.
            CHECK(a->m_bRemoveMe && game->server.IsObjectAlive(victim));
            auto corpses = [](GameSession& g)
            {
                unsigned n = 0;
                for (auto id : g.objects.snapshot())
                    n += get<CCorpse>(g, g.objects.get(id)->handle) != nullptr;
                return n;
            };
            Trip trip;
            trip.after = 30;
            trip.check = [&](GameSession& g)
            {
                auto* b = first_ai(g, "CivilianAI");
                CHECK(b && b->m_bRemoveMe && corpses(g) == 1);
                return true;
            };
            trip.continued = [&](GameSession& g)
            {
                // The restored actor removes itself instead of transferring a second corpse.
                CHECK(!first_ai(g, "CivilianAI") && corpses(g) == 1);
                return true;
            };
            CHECK(round_trip("Civilian awaiting removal", game, trip));
        }
        return true;
    }
    // Real worlds (private data): for every mounted world, the classes still blocking a whole save;
    // a world with none saves, loads, saves the same bytes again and runs on.
    bool worlds()
    {
        std::vector<std::string> names;
        {
            GameSession g;
            CHECK(g.initialize());
            Config config;
            config.data_dir = data_dir;
            std::string error;
            CHECK(g.resources.mount_game(config, error));
            for (const auto& [path, resource] : g.resources.index())
            {
                (void)resource;
                if (path.starts_with("worlds/") && path.ends_with(".dat"))
                    names.push_back(path.substr(7, path.size() - 11));
            }
        }
        unsigned entered = 0, whole = 0, ai_worlds = 0, npc_restores = 0;
        std::map<std::string, unsigned> totals;
        std::vector<std::string> restored;
        for (const auto& name : names)
        {
            auto game = std::make_unique<GameSession>();
            CHECK(game->initialize());
            Config config;
            config.data_dir = data_dir;
            std::string error;
            CHECK(game->resources.mount_game(config, error));
            if (!game->request_world(name, error))
            {
                std::cout << name << ": not entered (" << error.substr(0, 120) << ")\n";
                continue;
            }
            ++entered;
            game->server_shell.inspection_start = true; // Multiplayer maps: the local-inspection marker.
            if (!game->enter_player(error))
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
            else if (auto* p = player(*game); p && p->ctf_team)
                p->ctf_team = 0; // CTF match state is never saved (D12).
            for (int i = 0; i < 30; ++i)
                CHECK(game->step(.05));
            std::map<std::string, unsigned> blocking;
            unsigned npcs = 0;
            for (auto id : game->objects.snapshot())
            {
                const auto* r = game->objects.get(id);
                if (!r || r->life != Life::Alive)
                    continue;
                npcs += save_support(r->type) == SaveSupport::Original && is_ai(r->type->name);
                if (save_support(r->type) == SaveSupport::Unsupported && (r->user_flags & USRFLG_SAVEABLE))
                    ++blocking[r->type->name];
            }
            ai_worlds += npcs > 0;
            std::string list;
            for (const auto& [type, count] : blocking)
            {
                list += (list.empty() ? "" : ", ") + type + " x" + std::to_string(count);
                ++totals[type];
            }
            std::cout << name << ": " << npcs << " lot-11 NPCs; "
                      << (blocking.empty() ? "no blocking class" : "blocked by " + list) << '\n';
            if (!blocking.empty() && npcs)
            {
                // Every supported object through the real restore-mode world. References to the
                // classes of pending lots read back as NULL; the NPCs must resume.
                std::vector<HOBJECT> covered;
                for (auto id : game->objects.snapshot())
                    if (const auto* r = game->objects.get(id); r && r->life == Life::Alive &&
                                                               save_support(r->type) == SaveSupport::Original &&
                                                               (r->user_flags & USRFLG_SAVEABLE))
                        covered.push_back(r->handle);
                SaveObjectsReport report;
                if (!game->server.save_objects(current, covered, 0, save_flags, report))
                {
                    std::cerr << name << " supported-object save refused: " << report.error << '\n';
                    for (auto h : covered)
                        if (auto* a = get<AI_Mgr>(*game, h); a && !a->m_damage.spawn_valid)
                            std::cerr << "  invalid authored SpawnObject: " << a->m_damage.spawn_object << '\n';
                    return false;
                }
                GameSave partial{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
                SaveError fingerprint;
                CHECK(build_fingerprint(game->resources, partial.world_path, partial.image.classes, partial.fingerprint,
                                        fingerprint));
                if (!GameSession::load_game(game, partial, error))
                {
                    std::cerr << name << " supported-object load refused at " << game->game_load.stage << ": " << error
                              << '\n';
                    return false;
                }
                unsigned restored_npcs = 0;
                for (auto h : game->server.last_restore.objects)
                    if (auto* a = get<AI_Mgr>(*game, h))
                    {
                        CHECK(a->restored_loads == 1);
                        restored_npcs += a->RuntimeReady();
                    }
                CHECK(steps(*game, 30) && game->server.unavailable_services.empty());
                unsigned running = 0;
                for (auto id : game->objects.snapshot())
                {
                    const auto* r = game->objects.get(id);
                    if (auto* a = get<AI_Mgr>(*game, r->handle); a && a->restored_loads)
                    {
                        if (a->runtime_failed)
                            std::cout << "  restored " << r->type->name << " failed: " << a->runtime_error << '\n';
                        running += a->RuntimeReady();
                    }
                }
                std::cout << "  restored " << covered.size() << " supported records, " << restored_npcs
                          << " NPCs ready; " << running << " running after 30 steps\n";
                CHECK(restored_npcs > 0);
                ++npc_restores;
            }
            if (!blocking.empty())
                continue;
            GameSave save;
            if (!game->save_game(save, error))
            {
                std::cerr << name << " save refused: " << error << '\n';
                return false;
            }
            const SaveImage first = save.image;
            if (!GameSession::load_game(game, save, error))
            {
                std::cerr << name << " load refused at " << game->game_load.stage << ": " << error << '\n';
                return false;
            }
            GameSave again;
            CHECK(game->save_game(again, error) && same_image(first, again.image, false));
            CHECK(steps(*game, 60) && game->server.unavailable_services.empty());
            for (auto id : game->objects.snapshot())
                if (auto* a = get<AI_Mgr>(*game, game->objects.get(id)->handle); a && a->runtime_failed)
                {
                    std::cerr << name << ": restored NPC failed: " << a->runtime_error << '\n';
                    return false;
                }
            ++whole;
            restored.push_back(name);
        }
        std::cout << entered << " of " << names.size() << " worlds entered, " << ai_worlds << " with lot-11 NPCs, "
                  << npc_restores << " restored their supported objects; " << whole << " restored whole:";
        for (const auto& n : restored)
            std::cout << ' ' << n;
        std::cout << "\nWorlds blocked per remaining class:";
        for (const auto& [type, count] : totals)
            std::cout << ' ' << type << '=' << count;
        std::cout << '\n';
        return entered == names.size() && npc_restores == ai_worlds;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string mode = argv[1];
    root = argv[2];
    if (!OpenBlood2::Testing::reset_scratch_dir(root))
        return 2;
    const std::map<std::string, std::function<bool()>> headless{{"layout", layout}, {"refusals", refusals}};
    if (auto it = headless.find(mode); it != headless.end())
        return it->second() ? 0 : 1;
    if (argc < 4)
        return 2;
    data_dir = argv[3];
    if (!write_arena())
        return 1;
    const std::map<std::string, std::function<bool()>> private_cases{
        {"classes", classes}, {"states", states}, {"worlds", worlds}};
    auto it = private_cases.find(mode);
    return it != private_cases.end() && it->second() ? 0 : 1;
}
