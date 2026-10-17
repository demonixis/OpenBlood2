#include "audio_fixture.h"
#include "game/original/SoundFX.h"
#include <functional>
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
    HOBJECT make(GameSession& g, Properties p = {})
    {
        return g.objects.create(g.objects.find_class("SoundFX"), {}, Creation::World, p);
    }
    SoundFX* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<SoundFX*>(g.server.HandleToObject(h));
    }
    bool trigger(GameSession& g, HOBJECT h, const char* text)
    {
        auto s = g.server.CreateString(text);
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = m && g.server.WriteToMessageHString(m, s) == DE_OK && g.server.EndMessage(m) == DE_OK;
        g.server.FreeString(s);
        return ok;
    }
    Properties props(bool ambient = true)
    {
        return {{"StartOn", false},
                {"Sound", std::string("long.wav")},
                {"Ambient", ambient},
                {"RampUpSound", std::string("cue.wav")},
                {"RampDownSound", std::string("stereo.wav")}};
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g);
        CHECK(h);
        auto* s = get(g, h);
        CHECK(s && s->state == SoundFX::On && s->ambient && s->start_on && !s->file_stream && !s->sound);
        CHECK(s->outer_radius == 500 && s->inner_radius == 100 && s->volume == 100 && s->priority == 0);
        CHECK(!g.objects.get(h)->scheduled && (g.server.GetObjectUserFlags(h) & (1u << 3)));
        h = g.objects.create(g.objects.find_class("SoundFX"), {}, Creation::Runtime);
        CHECK(h);
        s = get(g, h);
        CHECK(!s->ambient && s->outer_radius == 0 && s->volume == 0 && s->state == SoundFX::On);
        auto p = props(false);
        p["StartOn"] = true;
        p["Volume"] = std::int32_t(-1);
        p["Priority"] = std::int32_t(258);
        h = g.objects.create(g.objects.find_class("SoundFX"), {}, Creation::String, p);
        CHECK(h);
        s = get(g, h);
        CHECK(s->state == SoundFX::On && s->volume == 255 && s->priority == 2 && s->sound);
        CHECK(g.server.audio.get(s->sound)->request.volume == 1 && !g.server.audio.get(s->sound)->request.loop);
        CHECK(!g.objects.get(h)->scheduled); // InitialUpdate cancels duration even for a spatial ON one-shot.
        CHECK(g.step(2) && s->state == SoundFX::On && g.server.audio.get(s->sound)->done);
        return true;
    }
    bool ramps(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, props());
        CHECK(h);
        auto* s = get(g, h);
        CHECK(s->state == SoundFX::Off && !s->sound && !g.objects.get(h)->scheduled);
        CHECK(trigger(g, h, "oN") && s->state == SoundFX::RampUp && s->sound);
        auto first = s->sound;
        CHECK(g.server.audio.get(first)->request.resource == "cue.wav" && !g.server.audio.get(first)->request.loop);
        CHECK(g.step(.05) && s->state == SoundFX::RampUp && g.step(.051) && s->state == SoundFX::On);
        CHECK(!g.server.audio.get(first) && s->sound != first && !g.objects.get(h)->scheduled);
        auto loop = s->sound;
        CHECK(g.server.audio.get(loop)->request.loop && g.step(3.1) && g.server.audio.get(loop)->loops >= 3);
        CHECK(trigger(g, h, "TRIGGER") && s->sound != loop && !g.server.audio.get(loop)); // Restart, not idempotent.
        CHECK(trigger(g, h, "OFF") && s->state == SoundFX::RampDown && g.objects.get(h)->scheduled);
        CHECK(g.step(.101) && s->state == SoundFX::Off && !s->sound && !g.objects.get(h)->scheduled);
        CHECK(trigger(g, h, "TOGGLE") && s->state == SoundFX::RampUp);
        CHECK(trigger(g, h, "toggle") && s->state == SoundFX::RampDown); // Any non-OFF counts as on.
        CHECK(trigger(g, h, "ON") && s->state == SoundFX::RampDown);     // ON doesn't reverse ramp down.
        CHECK(g.step(.101) && s->state == SoundFX::Off);
        auto attempts = s->attempts;
        CHECK(trigger(g, h, "ONWARD") && trigger(g, h, " ON") && trigger(g, h, "OFF;") && trigger(g, h, nullptr));
        CHECK(s->attempts == attempts && s->state == SoundFX::Off && !g.server.string_count() &&
              !g.server.message_count());
        return true;
    }
    bool spatial(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto p = props(false);
        p["RampUpSound"] = std::string{};
        p["Pos"] = DVector{300, 0, 0};
        p["Volume"] = std::int32_t(50);
        auto h = make(g, p);
        CHECK(h);
        auto* s = get(g, h);
        CHECK(trigger(g, h, "ON") && s->state == SoundFX::On && s->sound);
        auto first = s->sound;
        auto* v = g.server.audio.get(first);
        CHECK(v && !v->request.loop && v->request.reverb_requested && !v->request.speaker && !v->world_owned);
        CHECK(g.server.advance_audio(.05, {}, {}) && v->right > v->left && v->request.volume == .5f);
        DVector moved{0, 0, 300};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK);
        CHECK(g.server.advance_audio(.05, {}, {}) && v->position.x == 300 &&
              v->position.z == 0); // Sampled, not attached.
        CHECK(g.step(1.001) && s->state == SoundFX::RampDown && s->sound != first && !g.server.audio.get(first));
        CHECK(g.step(.101) && s->state == SoundFX::Off && !s->sound);
        p = props();
        p["RampUpSound"] = p["RampDownSound"] = std::string{};
        h = make(g, p);
        CHECK(h);
        s = get(g, h);
        CHECK(trigger(g, h, "ON") && s->state == SoundFX::On && s->sound);
        CHECK(trigger(g, h, "OFF") && s->state == SoundFX::Off && !s->sound && !g.objects.get(h)->scheduled);
        return true;
    }
    bool deadlines(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto p = props();
        p["RampDownSound"] = std::string{};
        auto h = make(g, p);
        CHECK(h);
        auto* s = get(g, h);
        CHECK(trigger(g, h, "ON") && g.objects.get(h)->scheduled);
        const auto due = g.objects.get(h)->due;
        CHECK(trigger(g, h, "OFF") && s->state == SoundFX::Off && !s->sound && g.objects.get(h)->due == due);
        CHECK(g.objects.get(h)->scheduled && g.step(.101) && !g.objects.get(h)->scheduled);
        // Failed replacement preserves the old ramp deadline, which later advances the new state.
        h = make(g, props());
        CHECK(h);
        s = get(g, h);
        CHECK(trigger(g, h, "ON"));
        s->ramp_down = "missing.wav";
        CHECK(trigger(g, h, "OFF") && !s->sound && s->failures == 1 && g.objects.get(h)->scheduled);
        CHECK(g.step(.101) && s->state == SoundFX::Off && !g.objects.get(h)->scheduled);
        h = make(g, {{"StartOn", false}, {"RampUpSound", std::string("cue.wav")}});
        CHECK(h);
        s = get(g, h);
        CHECK(trigger(g, h, "ON") && g.step(.101) && s->state == SoundFX::On && !s->sound &&
              !g.objects.get(h)->scheduled);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(!g.objects.create(g.objects.find_class("SoundFX"), {}, Creation::Restore));
        CHECK(!make(g, {{"VoiceType", std::int32_t(1)}})); // Nonzero historical behavior is not guessed.
        auto p = props();
        p["RampUpSound"] = std::string("missing.wav");
        auto h = make(g, p);
        CHECK(h);
        auto* s = get(g, h);
        CHECK(trigger(g, h, "ON") && !s->sound && s->state == SoundFX::RampUp && s->failures == 1 && !s->error.empty());
        CHECK(!g.objects.get(h)->scheduled && g.step(5) && s->state == SoundFX::RampUp && s->attempts == 1);
        p = props(false);
        p["RampUpSound"] = std::string{};
        p["FileStream"] = true;
        h = make(g, p);
        CHECK(h);
        s = get(g, h);
        CHECK(trigger(g, h, "ON") && s->state == SoundFX::On && s->sound && s->error.empty());
        CHECK(g.server.audio.get(s->sound)->request.streaming && g.server.audio.get(s->sound)->clip->samples.empty());
        CHECK(g.objects.get(h)->scheduled && s->failures == 0);
        CHECK(trigger(g, h, "OFF")); // Ramp-down stream still owns a retained voice.
        CHECK(g.step(.101) && !s->sound);
        p = props();
        p["StartOn"] = true;
        p["OuterRadius"] = -1.0f;
        h = make(g, p);
        CHECK(h);
        s = get(g, h);
        CHECK(s->failures == 1 && !s->sound && !s->error.empty()); // Typed mixer refuses invalid radii.
        CHECK(g.server.audio.voices().empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        SoundId previous = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            auto p = props();
            p["StartOn"] = true;
            p["DifficultyMed"] = false;
            auto filtered = make(g, p);
            CHECK(filtered && g.objects.get(filtered)->life == Life::Pending);
            auto id = get(g, filtered)->sound;
            CHECK(id && g.server.audio.get(id)); // Own startup precedes base filter.
            g.objects.finish_frame();
            CHECK(!g.objects.get(filtered) && !g.server.audio.get(id));
            auto h = make(g, props());
            CHECK(h && trigger(g, h, "ON"));
            auto* s = get(g, h);
            auto voice = s->sound;
            CHECK(voice > previous);
            previous = voice;
            g.server.SetObjectState(h, OBJSTATE_INACTIVE);
            CHECK(g.server.GetObjectState(h) == OBJSTATE_INACTIVE);
            CHECK(g.step(.2) && s->state == SoundFX::RampUp && g.server.audio.get(voice)->done);
            g.server.SetObjectState(h, OBJSTATE_ACTIVE);
            CHECK(g.server.GetObjectState(h) == OBJSTATE_ACTIVE && g.step(.001) && s->state == SoundFX::On);
            voice = s->sound;
            CHECK(voice && g.objects.remove(h));
            g.objects.finish_frame();
            CHECK(!g.server.audio.get(voice) && !g.objects.get(h) && !trigger(g, h, "ON"));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_bytes());
            CHECK(!g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        }
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        unsigned worlds = 0, entries = 0, rejected = 0, created = 0, attempts = 0, failures = 0, references = 0,
                 missing = 0, city = 0;
        unsigned voice_schema = 0, action_schema = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : world.objects)
                if (o.class_name == "SoundFX")
                {
                    ++entries;
                    Properties p;
                    for (const auto& prop : o.properties)
                        p.emplace(prop.name, prop.value);
                    if (path == "worlds/02_cityhub.dat")
                        ++city;
                    auto h = make(g, p);
                    if (!h)
                    {
                        ++rejected;
                        const auto& detail = g.objects.diagnostics().back();
                        if (detail == "SoundFX: invalid property VoiceType")
                            ++voice_schema;
                        else if (detail == "SoundFX: invalid property TypeAction")
                            ++action_schema;
                        else
                            CHECK(false);
                        std::cout << path << " rejected: " << g.objects.diagnostics().back() << '\n';
                    }
                    else
                    {
                        ++created;
                        auto* s = get(g, h);
                        CHECK(s);
                        attempts += s->attempts;
                        failures += s->failures;
                        std::cout << path << " created: " << g.objects.get(h)->state.m_Name
                                  << " state=" << s->state_name() << " stream=" << s->file_stream
                                  << " error=" << s->error << '\n';
                    }
                    for (const auto* key : {"Sound", "RampUpSound", "RampDownSound"})
                        if (p.contains(key))
                        {
                            const auto* value = std::get_if<std::string>(&p.at(key));
                            CHECK(value);
                            if (value->empty())
                                continue;
                            ++references;
                            std::shared_ptr<const AudioClip> clip;
                            if (!g.server.audio.load(g.resources, *value, clip, error))
                            {
                                ++missing;
                                std::cout << path << " resource " << *value << ": " << error << '\n';
                            }
                        }
                    CHECK(g.objects.clear());
                    g.server.audio.clear();
                }
        }
        std::cout << "SoundFX catalog worlds=" << worlds << " entries=" << entries << " created=" << created
                  << " rejected=" << rejected << " starts=" << attempts << " failures=" << failures
                  << " references=" << references << " unreadable=" << missing << " city=" << city
                  << " VoiceType=" << voice_schema << " TypeAction=" << action_schema << '\n';
        CHECK(worlds == 57 && entries == 124 && rejected == 3 && created == 121 && attempts == 100 && failures == 0 &&
              references == 124 && missing == 1 && voice_schema == 0 && action_schema == 3 && city == 5 &&
              !g.server.string_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"defaults", defaults}, {"ramps", ramps},       {"spatial", spatial}, {"deadlines", deadlines},
        {"failures", failures}, {"lifetime", lifetime}, {"catalog", catalog}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
