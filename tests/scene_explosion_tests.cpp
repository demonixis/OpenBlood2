#include "audio_fixture.h"
#include "game/original/ClientExplosionSFX.h"
#include "game/original/Destructable.h"
#include "game/original/radius_damage.h"
#include <cmath>
#include <functional>
#include <limits>
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
    CClientExplosionSFX* make(GameSession& g, Properties props = {}, Creation reason = Creation::World)
    {
        auto h = g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, reason, props);
        return h ? dynamic_cast<CClientExplosionSFX*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool command(GameSession& g, HOBJECT target, const char* text)
    {
        auto* from = g.server.HandleToObject(target);
        auto s = g.server.CreateString(text);
        auto m = g.server.StartMessageToObject(from, target, 1000);
        const auto written = g.server.WriteToMessageHString(m, s);
        g.server.FreeString(s);
        return written == DE_OK && g.server.EndMessage(m) == DE_OK;
    }
    struct Target : BaseClass
    {
        CDestructable damage;
        unsigned hits = 0;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD m, void* data, float reason) override
        {
            if (m == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject);
                damage.SetHitPoints(1000);
            }
            return BaseClass::EngineMessageFn(m, data, reason);
        }
        DDWORD ObjectMessageFn(HOBJECT from, DDWORD m, HMESSAGEREAD read) override
        {
            if (m == damage_message)
                ++hits;
            return BaseClass::ObjectMessageFn(from, m, read);
        }
    };
    Target* target(GameSession& g, DVector p)
    {
        static ClassInfo type{"SceneExplosionTarget",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                              {}};
        if (!g.objects.find_class(type.name))
        {
            type.parent = g.objects.find_class("BaseClass");
            if (!g.objects.register_class(type))
                return nullptr;
        }
        ObjectCreateStruct c;
        c.m_Pos = p;
        return dynamic_cast<Target*>(g.server.HandleToObject(g.objects.create(&type, c)));
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        for (auto reason : {Creation::Runtime, Creation::World, Creation::String})
        {
            auto* e = make(g, {}, reason);
            CHECK(e && e->damage == 50 && e->radius == 200 && e->type == 0 && e->normal.y == 1 &&
                  e->sound == "Sounds\\exp_tnt.wav");
            auto* r = g.objects.get(e->m_hObject);
            CHECK(r->scheduled && std::abs(r->due - g.objects.time() - .1) < 1e-6 &&
                  r->state.m_Flags & FLAG_FORCECLIENTUPDATE && r->state.m_UserData == 0);
            CHECK(g.server.GetObjectUserFlags(e->m_hObject) & 8u);
        }
        auto* e = make(g, {{"Name", std::string("authored")},
                           {"Pos", DVector{1, 2, 3}},
                           {"Damage", -5.f},
                           {"DamageRadius", -10.f},
                           {"Direction", DVector{0, 7, 0}},
                           {"ExplosionType", std::int32_t(-1)},
                           {"WaveFile", std::string{}}});
        CHECK(e && e->position.x == 1 && e->normal.y == 7 && e->type == 0xffffffffu && e->damage == -5 &&
              e->radius == -10);
        CHECK(command(g, e->m_hObject, "TRIGGER") && e->targets.null_damager && !e->targets.cancelled && !e->voice);
        CHECK(!g.objects.create(g.objects.find_class("CClientExplosionSFX"), {}, Creation::Restore));
        auto filtered = make(g, {{"DifficultyMed", false}});
        CHECK(filtered && filtered->filtered && g.objects.get(filtered->m_hObject)->life == Life::Pending);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool commands(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto* e = make(g, {{"Pos", DVector{10, 20, 30}}, {"WaveFile", std::string("long.wav")}});
        CHECK(e);
        auto h = e->m_hObject;
        auto due = g.objects.get(h)->due;
        for (const auto* t : {"ON", "TRIGGERextra", " TRIGGER", "TRIGGER "})
            CHECK(command(g, h, t));
        CHECK(e->commands == 4 && e->activations == 0 && !e->triggered && g.server.scene_explosions.empty());
        CHECK(g.step(.06));
        DVector moved{500, 30, 700};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK);
        CHECK(command(g, h, "tRiGgEr") && command(g, h, "TRIGGER"));
        CHECK(e->activations == 2 && g.server.scene_explosions.size() == 2 && g.server.audio.voices().size() == 2);
        CHECK(g.objects.get(h)->due == due && e->position.x == 10);
        for (const auto& [id, v] : g.server.audio.voices())
            CHECK(v.position.x == 10 && v.request.inner_radius == 100 && v.request.outer_radius == 1000 &&
                  v.request.priority == 2 && v.world_owned && v.auto_remove && !v.request.loop &&
                  v.request.reverb_requested);
        CHECK(g.step(.039) && g.objects.get(h));
        CHECK(g.step(.002) && !g.objects.get(h));
        CHECK(g.server.scene_explosions.size() == 2 && g.server.audio.voices().size() == 2 &&
              g.server.string_count() == 0 && g.server.message_count() == 0);
        CHECK(g.step(1) && g.server.audio.voices().empty());
        CHECK(g.unload_world() && g.server.scene_explosions.empty() && g.server.audio.cached_bytes() == 0);
        return true;
    }
    bool damage(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto* e = make(g, {{"Pos", DVector{0, 100, 0}}, {"WaveFile", std::string{}}});
        auto* a = target(g, {0, 100, 0});
        auto* b = target(g, {150, 100, 0});
        auto* edge = target(g, {200, 100, 0});
        auto* far = target(g, {201, 100, 0});
        CHECK(e && a && b && edge && far && command(g, e->m_hObject, "TRIGGER"));
        CHECK(e->targets.null_damager && !e->targets.cancelled && e->targets.candidates == 4 &&
              e->targets.in_radius == 3 && e->targets.delivered == 0);
        for (auto* t : {a, b, edge, far})
            CHECK(t->hits == 0 && t->damage.m_fHitPoints == 1000);
        // Existing non-null brush/weapon rule must still deliver the same original falloff.
        auto r = DamageObjectsInRadius(e->m_hObject, e, {0, 100, 0}, 200, 100, DamageType::Explode);
        CHECK(!r.null_damager && !r.cancelled && r.delivered == 3 && a->damage.m_fHitPoints == 900 &&
              b->damage.m_fHitPoints == 912.5f && edge->hits == 1 && far->hits == 0);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto* e = make(g, {{"WaveFile", std::string("long.wav")}});
        CHECK(e);
        auto h = e->m_hObject;
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(command(g, h, "TRIGGER"));
        CHECK(g.step(.2) && g.objects.get(h)); // Inactive scheduling pauses, direct messages still work.
        auto voice = e->voice;
        ObjectSelection selection{g.objects.get(h)->id};
        std::weak_ptr<const AudioClip> clip = g.server.audio.get(voice)->clip;
        CHECK(g.objects.remove(h));
        CHECK(!g.server.IsObjectAlive(h));
        CHECK(g.step(.1));
        selection.refresh(g.objects);
        CHECK(!selection.id && g.server.audio.get(voice) && g.server.audio.get(voice)->cursor > 0);
        CHECK(!g.server.HandleToObject(h));
        CHECK(g.unload_world() && clip.expired() && !g.server.audio.get(voice) && g.server.string_count() == 0 &&
              CDestructable::m_dwNumDestructables == 0);
        auto* next = make(g, {{"WaveFile", std::string("cue.wav")}});
        CHECK(next && next->m_hObject != h);
        CHECK(command(g, next->m_hObject, "TRIGGER") && next->voice != voice);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        for (const char* key : {"Damage", "DamageRadius"})
            for (float value :
                 {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 1.1e6f})
                CHECK(!make(g, {{key, value}}));
        CHECK(!make(g, {{"Direction", DVector{1.1e7f, 0, 0}}}));
        CHECK(!make(g, {{"WaveFile", std::string(101, 'x')}}));
        auto* e = make(g, {{"WaveFile", std::string("missing.wav")}});
        CHECK(e && command(g, e->m_hObject, "TRIGGER"));
        CHECK(!e->voice && !e->sound_error.empty() && e->triggered &&
              !g.server.scene_explosions.back().client_fx_available);
        CHECK(g.step(.11) && g.server.audio.voices().empty() && !g.server.scene_explosions.back().sound_error.empty());
        auto* empty = make(g, {{"WaveFile", std::string{}}});
        CHECK(empty && command(g, empty->m_hObject, "TRIGGER") && !empty->voice && empty->sound_error.empty());
        return true;
    }
    bool budget(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto* e = make(g, {{"WaveFile", std::string("cue.wav")}});
        CHECK(e);
        for (unsigned i = 0; i < 128; ++i)
            CHECK(command(g, e->m_hObject, "TRIGGER") && e->voice);
        CHECK(g.server.audio.voices().size() == 128 && g.server.scene_explosions.size() == 32);
        CHECK(command(g, e->m_hObject, "TRIGGER") && !e->voice && !e->sound_error.empty());
        CHECK(g.server.advance_audio(.11, {}, {}) && g.server.audio.voices().empty());
        CHECK(command(g, e->m_hObject, "TRIGGER") && e->voice); // Completed one-shots free capacity.
        SoundRequest q;
        q.resource = "cue.wav";
        std::string error;
        auto h = e->m_hObject;
        q.loop = true;
        CHECK(!g.server.PlayDetachedSound(h, q, {}, error));
        q.loop = false;
        q.space = SoundSpace::Ambient;
        CHECK(!g.server.PlayDetachedSound(h, q, {}, error));
        q.space = SoundSpace::Spatial;
        q.speaker = h;
        CHECK(!g.server.PlayDetachedSound(h, q, {}, error));
        q.speaker = nullptr;
        CHECK(g.objects.remove(h) && !g.server.PlayDetachedSound(h, q, {}, error));
        CHECK(g.objects.finish_frame() && !g.server.PlayDetachedSound(h, q, {}, error));
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        unsigned worlds = 0, count = 0, rejected = 0, accepted = 0, sounds = 0, missing = 0;
        std::map<DDWORD, unsigned> types;
        for (const auto& [path, res] : g.resources.index())
        {
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<uint8_t> b;
            World w;
            CHECK(g.resources.read(path, b, error));
            if (!read_world(b, w, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : w.objects)
                if (o.class_name == "CClientExplosionSFX")
                {
                    ++count;
                    Properties p;
                    for (const auto& v : o.properties)
                        p.emplace(v.name, v.value);
                    auto* e = make(g, p);
                    if (!e)
                    {
                        ++rejected;
                        std::cout << path << " rejected " << g.objects.diagnostics().back() << '\n';
                        CHECK(g.objects.finish_frame());
                        continue;
                    }
                    ++accepted;
                    ++types[e->type];
                    CHECK(command(g, e->m_hObject, "TRIGGER"));
                    CHECK(e->targets.null_damager && e->targets.delivered == 0 && !e->targets.cancelled);
                    if (!e->sound.empty())
                    {
                        if (e->voice)
                            ++sounds;
                        else
                            ++missing;
                    }
                    std::cout << path << " name=" << g.server.GetObjectName(e->m_hObject) << " type=" << e->type
                              << " damage=" << e->damage << " radius=" << e->radius << " normal=" << e->normal.x << ','
                              << e->normal.y << ',' << e->normal.z << " sound=" << e->sound
                              << " voice=" << (e->voice ? "decoded" : "absent") << '\n';
                    CHECK(g.objects.remove(e->m_hObject) && g.objects.finish_frame());
                    g.server.audio.clear();
                }
        }
        std::cout << "worlds=" << worlds << " sources=" << count << " accepted=" << accepted << " rejected=" << rejected
                  << " decoded=" << sounds << " missing=" << missing << '\n';
        for (const auto& [id, n] : types)
            std::cout << "type=" << id << " count=" << n << '\n';
        CHECK(worlds == 57 && count == 43 && accepted == 43 && rejected == 0 && sounds == 19 && missing == 24);
        const std::map<DDWORD, unsigned> expected{{1, 3}, {2, 10}, {3, 19}, {6, 7}, {8, 3}, {24, 1}};
        CHECK(types == expected);
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            std::map<EntryState, unsigned> counts;
            unsigned n = 0;
            for (size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& e = g.entries[i];
                ++counts[e.state];
                if (g.world.objects[i].class_name != "CClientExplosionSFX")
                    continue;
                ++n;
                CHECK(e.state == EntryState::Created);
            }
            CHECK(n == 9 && counts[EntryState::Created] == 203 && counts[EntryState::WorldSound] == 4 &&
                  counts[EntryState::EditorOnly] == 372 && counts[EntryState::Unsupported] == 1 &&
                  counts[EntryState::Failed] == 0);
            CHECK(g.objects.snapshot().size() == 225 && g.enter_player(error) && g.objects.snapshot().size() == 228 &&
                  g.step(.02));
            CHECK(g.objects.snapshot().size() == 230 && g.objects.attachments().size() == 11);
            auto* player = g.server_shell.Player(g.local_client);
            CHECK(player);
            auto health = player->m_damage.m_fHitPoints;
            std::vector<HOBJECT> emitted;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* e = dynamic_cast<CClientExplosionSFX*>(r->game.get());
                if (!e)
                    continue;
                emitted.push_back(r->handle);
                CHECK(command(g, r->handle, "TRIGGER") && e->voice && e->targets.delivered == 0);
            }
            CHECK(g.server.scene_explosions.size() == 9 && player->m_damage.m_fHitPoints == health && g.step(.11));
            for (auto h : emitted)
                CHECK(!g.objects.get(h));
            CHECK(!g.run_world(error) && error.find("1 uninstantiated") != std::string::npos);
            CHECK(g.unload_world() && g.server.scene_explosions.empty() && g.server.audio.voices().empty() &&
                  g.server.string_count() == 0 && g.server.message_count() == 0 && g.server.list_count() == 0);
            std::cout << "cycle=" << cycle
                      << " nine original emitters, null-damager suppression, decoded audio, removal and shell unload "
                         "verified\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"properties", properties}, {"commands", commands}, {"damage", damage},   {"lifetime", lifetime},
        {"failures", failures},     {"budget", budget},     {"catalog", catalog}, {"subway", subway}};
    auto i = cases.find(argv[1]);
    return i != cases.end() && i->second(argv[2]) ? 0 : 1;
}
