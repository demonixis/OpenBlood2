#include "audio_fixture.h"
#include "game/original/RandomSoundFX.h"
#include <cmath>
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
        return g.objects.create(g.objects.find_class("RandomSoundFX"), {}, Creation::World, p);
    }
    RandomSoundFX* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<RandomSoundFX*>(g.server.HandleToObject(h));
    }
    bool trigger(GameSession& g, HOBJECT h, const char* cmd)
    {
        auto text = g.server.CreateString(cmd);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = msg && g.server.WriteToMessageHString(msg, text) == DE_OK && g.server.EndMessage(msg) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    Properties sequence_props()
    {
        return {{"InitiallyOn", false},
                {"SequentialPlay", true},
                {"SoundFile1", std::string("long.wav")},
                {"SoundFile2", std::string("cue.wav")},
                {"MinWaitTime", .2f},
                {"MaxWaitTime", .2f}};
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g);
        CHECK(h);
        auto* c = get(g, h);
        CHECK(c && c->on && c->positional && !c->sequential && !c->loop_sequence && !c->count && c->volume == 100);
        CHECK(g.objects.get(h)->scheduled && g.step(59) && g.objects.get(h));
        CHECK(g.step(2) && !g.objects.get(h)); // Empty list removes on first update, not precreate.
        h = g.objects.create(g.objects.find_class("RandomSoundFX"), {}, Creation::Runtime);
        CHECK(h && !get(g, h)->positional); // Source constructor differs from world property default.
        auto gap = make(g, {{"SoundFile1", std::string("cue.wav")}, {"SoundFile3", std::string("long.wav")}});
        CHECK(gap && get(g, gap)->count == 1 && get(g, gap)->sounds[2].empty());
        auto wrapped = make(g, {{"Volume", std::int32_t(256)}});
        CHECK(wrapped && get(g, wrapped)->volume == 0);
        wrapped = make(g, {{"Volume", std::int32_t(-1)}});
        CHECK(wrapped && get(g, wrapped)->volume == 100);
        auto zero = make(g, {{"MinWaitTime", 0.0f}, {"MaxWaitTime", 0.0f}, {"SoundFile1", std::string("cue.wav")}});
        CHECK(zero && !g.objects.get(zero)->scheduled);
        return true;
    }
    bool commands(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, sequence_props());
        CHECK(h);
        auto* c = get(g, h);
        CHECK(trigger(g, h, "on") && !c->on && !g.objects.get(h)->scheduled); // Case-sensitive source prefixes.
        CHECK(trigger(g, h, "ONWARD") && c->on && g.step(.101) && c->attempts == 1 && c->current == 1);
        auto voice = c->sound;
        CHECK(voice);
        CHECK(trigger(g, h, "OFF anything") && !c->on && !g.objects.get(h)->scheduled && c->sound == voice);
        CHECK(g.step(.3) && c->attempts == 1 && !g.server.audio.get(voice)->done);
        CHECK(g.step(1) && c->sound == voice && g.server.audio.get(voice)->done);
        CHECK(trigger(g, h, "TOGGLE suffix") && c->on && c->current == 0);
        CHECK(g.step(.101) && c->attempts == 2 && c->current == 1 && c->sound != voice && !g.server.audio.get(voice));
        CHECK(trigger(g, h, "unrecognized") && c->current == 0 && g.step(.101) && c->last_choice == 0);
        CHECK(trigger(g, h, "OFF") && !c->on && trigger(g, h, "TOGGLE") && c->on);
        CHECK(!g.server.string_count() && !g.server.message_count() && !g.server.list_count());
        return true;
    }
    bool sequence(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, sequence_props());
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        auto* c = get(g, h);
        auto first = c->sound;
        CHECK(first && c->current == 1 && c->last_choice == 0 &&
              g.server.audio.get(first)->request.resource == "long.wav");
        CHECK(g.step(.201) && c->attempts == 2 && c->current == 0 && !c->on && !g.objects.get(h)->scheduled);
        CHECK(!g.server.audio.get(first) && c->sound && g.server.audio.get(c->sound)->request.resource == "cue.wav");
        CHECK(g.step(1) && c->attempts == 2 && g.server.audio.get(c->sound)->done);
        auto props = sequence_props();
        props["LoopSequence"] = true;
        h = make(g, props);
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        c = get(g, h);
        for (unsigned i = 0; i < 6; ++i)
            CHECK(g.step(.201) && c->on && c->last_choice == ((i + 1) % 2));
        CHECK(c->attempts == 7 && g.server.audio.voices().size() == 2);
        return true;
    }
    bool random_sequence(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        std::vector<unsigned> previous;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            // Same session reset restores the portable seed, without claiming the retail RNG sequence.
            CHECK(g.unload_world());
            auto h = make(g, {{"SoundFile1", std::string("cue.wav")},
                              {"SoundFile2", std::string("long.wav")},
                              {"SoundFile3", std::string("stereo.wav")},
                              {"MinWaitTime", .1f},
                              {"MaxWaitTime", .2f},
                              {"PosRangeRadius", 10.0f},
                              {"Pos", DVector{20, 30, 40}}});
            CHECK(h);
            auto* c = get(g, h);
            std::vector<unsigned> choices;
            unsigned counts[3]{};
            for (unsigned i = 0; i < 120; ++i)
            {
                CHECK(g.step(.201) && c->attempts == i + 1 && c->sound && !c->failures);
                CHECK(c->last_choice < 3 && c->last_position.x >= 10 && c->last_position.x <= 30 &&
                      c->last_position.y == 30 && c->last_position.z >= 30 && c->last_position.z <= 50);
                auto* r = g.objects.get(h);
                CHECK(r->scheduled && r->due - g.objects.time() >= .0999 && r->due - g.objects.time() <= .2001);
                choices.push_back(c->last_choice);
                ++counts[c->last_choice];
            }
            CHECK(counts[0] && counts[1] && counts[2]);
            if (cycle)
                CHECK(choices == previous);
            else
                previous = choices;
        }
        int value = 9;
        CHECK(!g.server.RandomInt(2, 1, value) && value == 9);
        CHECK(g.server.RandomInt(-3, -3, value) && value == -3);
        return true;
    }
    bool spatial(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, {{"InitiallyOn", false},
                          {"SequentialPlay", true},
                          {"PositionalSound", false},
                          {"SoundFile1", std::string("long.wav")},
                          {"SoundRadius", 100.0f},
                          {"Volume", std::int32_t(50)},
                          {"Pos", DVector{75, 0, 0}}});
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        auto* c = get(g, h);
        auto id = c->sound;
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.advance_audio(.01, {}, {}));
        auto* v = g.server.audio.get(id);
        CHECK(v && v->request.space == SoundSpace::Ambient && !v->request.reverb_requested && !v->request.speaker);
        CHECK(std::abs(v->left - .25f) < .0001 && v->left == v->right && std::abs(g.server.audio.peak - .125f) < .001);
        CHECK(g.server.advance_audio(.01, {}, {{0, 1, 0}, 0}) && v->left == v->right &&
              std::abs(v->left - .25f) < .0001);
        DVector moved{};
        CHECK(g.server.TeleportObject(h, &moved) == DE_OK && g.server.advance_audio(.01, {}, {}));
        CHECK(v->position.x == 75); // Fixed random position does not follow emitter after start.
        CHECK(g.server.advance_audio(.01, {-30, 0, 0}, {}) && v->left == 0 && v->right == 0);
        CHECK(g.server.KillSound(id));
        SoundRequest r;
        r.resource = "long.wav";
        r.inner_radius = 50;
        r.outer_radius = 100;
        std::string error;
        id = g.server.PlaySoundAt(h, r, {75, 0, 0}, error);
        CHECK(id);
        CHECK(g.server.advance_audio(.01, {}, {}));
        v = g.server.audio.get(id);
        CHECK(v && v->left == 0 && std::abs(v->right - .5f) < .0001);
        CHECK(g.server.advance_audio(.01, {}, {{0, 1, 0}, 0}) && std::abs(v->left - .5f) < .0001 && v->right == 0);
        r.speaker = h;
        CHECK(!g.server.PlaySoundAt(h, r, {}, error));
        r.speaker = nullptr;
        r.space = SoundSpace::Local;
        CHECK(!g.server.PlaySoundAt(h, r, {}, error));
        r.space = SoundSpace(99);
        CHECK(!g.server.PlaySoundAt(h, r, {}, error));
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, sequence_props());
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        auto id = get(g, h)->sound;
        CHECK(id);
        CHECK(g.objects.remove(h) && g.server.advance_audio(.01, {}, {}));
        CHECK(!g.server.audio.get(id) && g.objects.finish_frame() && !g.objects.get(h));
        h = make(g, sequence_props());
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        auto next = get(g, h)->sound;
        CHECK(next > id);
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        CHECK(!trigger(g, h, "ON") && !g.server.KillSound(next) && !g.server.string_count() &&
              !g.server.message_count());
        h = make(g, sequence_props());
        CHECK(h && trigger(g, h, "ON") && g.step(.101) && get(g, h)->sound > next);
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto props = sequence_props();
        props["SoundFile1"] = std::string("missing.wav");
        auto h = make(g, props);
        CHECK(h && trigger(g, h, "ON") && g.step(.101));
        auto* c = get(g, h);
        CHECK(c->attempts == 1 && c->failures == 1 && !c->sound && c->current == 1 && !c->error.empty());
        CHECK(g.step(.201) && c->attempts == 2 && c->failures == 1 && c->sound && !c->on && c->error.empty());
        CHECK(!make(g, {{"MinWaitTime", -1.0f}}));
        CHECK(!make(g, {{"MinWaitTime", 61.0f}, {"MaxWaitTime", 60.0f}}));
        CHECK(!make(g, {{"PosRangeRadius", -1.0f}}));
        CHECK(!make(g, {{"SoundRadius", 0.0f}}));
        CHECK(!g.objects.create(g.objects.find_class("RandomSoundFX"), {}, Creation::Restore));
        CHECK(g.objects.finish_frame());
        CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.audio.voices().empty());
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
        unsigned worlds = 0, controllers = 0, references = 0, missing = 0, invalid = 0, rejected = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World w;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, w, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : w.objects)
                if (o.class_name == "RandomSoundFX")
                {
                    ++controllers;
                    Properties p;
                    for (const auto& prop : o.properties)
                        p.emplace(prop.name, prop.value);
                    auto h = make(g, p);
                    if (!h)
                    {
                        ++rejected;
                        std::cout << path << " rejected controller\n";
                        continue;
                    }
                    auto* c = get(g, h);
                    CHECK(c);
                    if (path == "worlds/01_subwaya.dat")
                        std::cout << "SUBWAY " << g.objects.get(h)->state.m_Name << " on=" << c->on
                                  << " positional=" << c->positional << " sequential=" << c->sequential
                                  << " loop=" << c->loop_sequence << " waits=" << c->min_wait << ',' << c->max_wait
                                  << " range=" << c->position_radius << " radius=" << c->sound_radius
                                  << " volume=" << unsigned(c->volume) << '\n';
                    for (unsigned i = 0; i < c->count; ++i)
                    {
                        ++references;
                        std::shared_ptr<const AudioClip> clip;
                        if (!g.server.audio.load(g.resources, c->sounds[i], clip, error))
                        {
                            if (!g.resources.find(c->sounds[i]))
                                ++missing;
                            else
                                ++invalid;
                            std::cout << path << ' ' << c->sounds[i] << " failure=" << error << '\n';
                        }
                        else if (path == "worlds/01_subwaya.dat")
                            std::cout << "  " << c->sounds[i] << " rate=" << clip->rate << " frames=" << clip->frames()
                                      << " duration=" << clip->duration() << '\n';
                    }
                }
            CHECK(g.objects.clear());
            g.server.audio.clear();
        }
        std::cout << "worlds=" << worlds << " controllers=" << controllers << " refs=" << references
                  << " missing=" << missing << " invalid=" << invalid << " rejected=" << rejected << '\n';
        CHECK(worlds == 57 && controllers == 54 && references == 223 && missing == 35 && !invalid && !rejected);
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        SoundId last = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            std::map<EntryState, unsigned> counts;
            for (const auto& e : g.entries)
                ++counts[e.state];
            CHECK(counts[EntryState::Created] == 187 && counts[EntryState::Unsupported] == 17 &&
                  counts[EntryState::EditorOnly] == 372 && !counts[EntryState::Failed]);
            CHECK(g.objects.snapshot().size() == 194 && g.enter_player(error) && g.objects.snapshot().size() == 197);
            std::vector<HOBJECT> targets;
            for (auto id : g.objects.snapshot())
                if (dynamic_cast<RandomSoundFX*>(g.objects.get(id)->game.get()))
                    targets.push_back(g.objects.get(id)->handle);
            CHECK(targets.size() == 2);
            for (auto h : targets)
            {
                const auto* c = get(g, h);
                const auto* r = g.objects.get(h);
                CHECK(c->on && r->scheduled && r->due - g.objects.time() >= c->min_wait - .001 &&
                      r->due - g.objects.time() <= c->max_wait + .001);
                CHECK(trigger(g, h, "ON"));
            }
            CHECK(g.step(.101));
            g.server.audio.inspect_pcm = true;
            for (auto h : targets)
            {
                auto* c = get(g, h);
                CHECK(c->sound && !c->failures && c->sound > last && c->attempts == 1);
                last = c->sound;
                const auto* v = g.server.audio.get(c->sound);
                CHECK(v && v->clip->duration() > 0 && !v->request.speaker);
                std::cout << "cycle=" << cycle << " name=" << g.objects.get(h)->state.m_Name
                          << " sound=" << v->request.resource << '\n';
            }
            CHECK(g.step(.05) && g.server.audio.mixed_frames == 2400 && g.objects.snapshot().size() == 189 &&
                  g.objects.attachments().size() == 1);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips() &&
                  !g.server.audio.get(last) && g.objects.snapshot().empty() && !g.server.message_count() &&
                  !g.server.string_count() && !g.server.list_count());
            g.server.audio.inspect_pcm = false;
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"defaults", defaults},      {"commands", commands}, {"sequence", sequence},
        {"random", random_sequence}, {"spatial", spatial},   {"lifetime", lifetime},
        {"failures", failures},      {"catalog", catalog},   {"subway", subway}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
