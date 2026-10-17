#include "audio_fixture.h"
#include "game/original/KeyFramer.h"
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
    HOBJECT create(GameSession& g, const char* type, Properties p = {})
    {
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, p);
    }
    KeyFramer* framer(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<KeyFramer*>(g.server.HandleToObject(h));
    }
    bool command(GameSession& g, HOBJECT h, const char* value)
    {
        auto text = g.server.CreateString(value);
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        const auto written = g.server.WriteToMessageHString(m, text);
        const auto sent = g.server.EndMessage(m);
        g.server.FreeString(text);
        return written == DE_OK && sent == DE_OK;
    }
    HOBJECT target(GameSession& g)
    {
        return create(g, "BaseClass", {{"Name", std::string("speaker")}, {"Pos", DVector{0, 100, 0}}});
    }
    HOBJECT key(GameSession& g, unsigned i, float delay, const char* sound, bool loop = false, Properties props = {})
    {
        props.emplace("Name", "k" + std::to_string(i));
        props.emplace("Pos", DVector{float(i) * 100, 100, 0});
        props.emplace("TimeStamp", delay);
        props.emplace("SoundName", std::string(sound));
        props.emplace("SoundRadius", 200.f);
        props.emplace("LoopSound", loop);
        return create(g, "Key", props);
    }
    HOBJECT path(GameSession& g)
    {
        return create(g, "KeyFramer",
                      {{"Name", std::string("controller")},
                       {"ObjectName", std::string("speaker")},
                       {"BaseKeyName", std::string("k")},
                       {"StartActive", true}});
    }
    bool voice(GameSession& g, const KeyFramer& f, HOBJECT speaker, bool loop)
    {
        const auto* v = g.server.audio.get(f.sound_voice);
        CHECK(v && v->clip && v->clip->frames() && f.sound_error.empty());
        CHECK(v->owner == speaker && v->request.speaker == speaker && !v->world_owned && !v->auto_remove);
        CHECK(v->request.loop == loop && !v->request.streaming && v->request.space == SoundSpace::Spatial);
        CHECK(v->request.outer_radius == 200 && v->request.inner_radius == 20 && v->request.priority == 1 &&
              v->request.volume == 1 && v->request.reverb_requested);
        float duration = 0;
        CHECK(g.server.GetSoundDuration(f.sound_voice, duration) && duration > 0);
        return true;
    }
    bool timeline(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        const auto speaker = target(g);
        CHECK(speaker && key(g, 0, 0, "cue.wav", true) && key(g, 1, .2f, "") && key(g, 2, .2f, "cue.wav"));
        const auto h = path(g);
        auto* f = framer(g, h);
        CHECK(f && g.step(.01) && !f->sound_requests && g.step(.1) && voice(g, *f, speaker, true));
        const auto first = f->sound_voice;
        CHECK(g.step(.1) && f->sound_voice == first && f->processed == 2); // Empty key does not stop sound.
        CHECK(command(g, h, "OFF") && g.step(.1) && g.server.audio.get(first)->loops > 0);
        f->Resume();
        CHECK(g.step(.1) && f->sound_voice == first && !g.objects.get(h)->scheduled); // Resume does not rearm.
        CHECK(command(g, h, "OFF") && command(g, h, "ON") && g.step(.1));
        CHECK(f->sound_voice > first && !g.server.audio.get(first) && voice(g, *f, speaker, true));
        const auto replacement = f->sound_voice;
        CHECK(g.step(.31) && !f->active && !g.server.audio.get(replacement) && voice(g, *f, speaker, false));
        const auto last = f->sound_voice;
        CHECK(g.step(.2) && g.server.audio.get(last)->done && f->sound_voice == last);
        CHECK(f->sound_requests == 3 && f->sound_starts == 3 && !f->sound_failures);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.server.audio.get(last));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        return true;
    }
    bool replacement(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(target(g) && target(g) && key(g, 0, 0, "cue.wav", true) && key(g, 1, 1, "missing.wav"));
        const auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && f->targets.size() == 2 && g.step(.1));
        CHECK(f->sound_requests == 2 && f->sound_starts == 2 && g.server.audio.voices().size() == 1);
        CHECK(voice(g, *f, f->targets.back().object, true)); // One source handle, not a voice per object.
        const auto old = f->sound_voice;
        CHECK(g.step(1) && !f->active && !g.server.audio.get(old) && !f->sound_voice);
        CHECK(f->sound_requests == 4 && f->sound_failures == 2 && !f->sound_error.empty());
        CHECK(g.server.audio.voices().empty() && g.unload_world());
        // An event without controlled objects does not manufacture a voice.
        CHECK(key(g, 0, 0, "cue.wav") && path(g) && g.step(.01) && g.step(.1));
        CHECK(g.server.audio.voices().empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        g.server.audio.inspect_pcm = true;
        auto speaker = target(g);
        CHECK(key(g, 0, 0, "cue.wav", true) && key(g, 1, 10, ""));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.step(.1));
        const auto first = f->sound_voice;
        g.objects.get(speaker)->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(command(g, h, "OFF"));
        DVector moved{40, 100, 0};
        CHECK(g.server.MoveObject(speaker, &moved) == DE_OK);
        CHECK(g.server.advance_audio(.02, moved, {}) && g.server.audio.peak > 0);
        CHECK(g.server.audio.get(first)->position.x == 40 && g.server.audio.get(first)->cursor > 0);
        CHECK(g.objects.remove(speaker) && g.objects.finish_frame() && f->targets.empty());
        CHECK(g.server.advance_audio(.02, {}, {}) && !g.server.audio.get(first));
        CHECK(g.objects.remove(h) && g.objects.finish_frame()); // Stale retained ID safe at destruction.
        CHECK(g.unload_world());
        speaker = target(g);
        CHECK(key(g, 0, 0, "long.wav", false,
                  {{"MessageTarget", std::string("controller")}, {"MessageName", std::string("OFF")}}));
        h = path(g);
        f = framer(g, h);
        CHECK(g.step(.01) && g.step(.1) && !f->active && f->sound_voice > first && voice(g, *f, speaker, false));
        const auto next = f->sound_voice;
        CHECK(!g.server.KillSound(first) && g.server.audio.get(next));
        f->keys[0].message = "REMOVE";
        CHECK(command(g, h, "ON") && g.step(.1) && !g.objects.get(h) && g.server.audio.voices().empty());
        CHECK(g.unload_world() && !g.server.audio.cached_clips() && !g.server.string_count() &&
              !g.server.message_count());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(target(g) && key(g, 0, 0, "cue.wav", false,
                               {{"SoundRadius", 0.f},
                                {"MessageTarget", std::string("controller")},
                                {"MessageName", std::string("OFF")}}));
        auto h = path(g);
        auto* f = framer(g, h);
        CHECK(g.step(.01) && g.step(.1) && f->sound_requests == 1 && f->sound_failures == 1 && !f->active);
        CHECK(!f->sound_voice && !f->sound_error.empty() && g.server.audio.voices().empty());
        f->keys[0].sound_radius = 200;
        SoundRequest request;
        request.resource = "long.wav";
        request.speaker = f->targets.front().object;
        request.space = SoundSpace::Spatial;
        request.outer_radius = 200;
        request.inner_radius = 20;
        std::string error;
        for (unsigned i = 0; i < 128; ++i)
            CHECK(g.server.PlayCharacterSound(request.speaker, request, true, error));
        CHECK(command(g, h, "ON") && g.step(.1) && !f->active && f->sound_failures == 2 && !f->sound_voice);
        CHECK(g.server.audio.voices().size() == 128 && g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(Config{root, "private"}, error));
        unsigned keys = 0, decoded = 0, missing = 0;
        for (const auto& [name, entry] : g.resources.index())
        {
            if (!name.starts_with("worlds/") || !name.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World world;
            CHECK(g.resources.read(name, bytes, error));
            if (!read_world(bytes, world, error))
            {
                CHECK(name == "worlds/16_bridge.dat");
                continue;
            }
            for (const auto& o : world.objects)
                if (o.class_name == "Key")
                    for (const auto& p : o.properties)
                        if (p.name == "SoundName")
                            if (const auto* sound = std::get_if<std::string>(&p.value); sound && !sound->empty())
                            {
                                ++keys;
                                AudioClip clip;
                                if (g.resources.read(*sound, bytes, error))
                                {
                                    CHECK(read_wave(bytes, clip, error) && clip.frames());
                                    ++decoded;
                                }
                                else
                                {
                                    std::cout << "Missing " << name << " " << *sound << " error=" << error << std::endl;
                                    CHECK((name == "worlds/09_abandoned_buildings.dat" &&
                                           *sound == "pickuptruckloop1.wav") ||
                                          (name == "worlds/11_slaughter_house.dat" &&
                                           *sound == "sounds\\enemies\\civilian\\scream2.wav"));
                                    ++missing;
                                }
                            }
        }
        CHECK(keys == 16 && decoded == 13 && missing == 3);
        std::cout << "Private sound keys=" << keys << " decoded=" << decoded << " unresolved exact paths=" << missing
                  << '\n';
        SoundId previous = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("12_Train_Station", error) && g.step(1. / 60));
            HOBJECT h = nullptr;
            for (auto id : g.objects.snapshot())
                if (auto* f = dynamic_cast<KeyFramer*>(g.objects.get(id)->game.get()); f && f->base_key_name == "tr")
                    h = f->m_hObject;
            auto* f = framer(g, h);
            CHECK(f && f->keys.size() == 2 && f->targets.size() == 1 && f->keys[0].sound_radius == 6000);
            CHECK(command(g, h, "ON")); // Diagnostic activation, unmodified authored keys/target.
            for (unsigned step = 0; step < 1200 && !f->sound_requests; ++step)
                CHECK(g.step(1. / 60));
            CHECK(f->sound_requests == 1 && f->sound_starts == 1 && !f->sound_failures && f->sound_voice > previous);
            previous = f->sound_voice;
            const auto* v = g.server.audio.get(previous);
            CHECK(v && v->clip && v->clip->frames() && v->request.speaker == f->targets[0].object &&
                  v->owner == f->targets[0].object && !v->auto_remove && v->request.inner_radius == 600 &&
                  v->request.outer_radius == 6000 && v->request.priority == 1 &&
                  v->request.loop == f->keys[0].loop_sound);
            CHECK(command(g, h, "OFF") && g.step(.1) && g.server.audio.get(previous)->cursor > 0);
            std::cout << "Authored " << g.objects.get(h)->state.m_Name << " sound=" << f->last_sound
                      << " key_time=" << f->keys[0].time << " loop=" << f->sound_loop << " frames=" << v->clip->frames()
                      << " cursor=" << v->cursor << '\n';
            CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.server.audio.get(previous));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
            CHECK(!g.server.string_count() && !g.server.list_count() && !g.server.message_count());
        }
        return true;
    }

} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests{{"timeline", timeline},
                                                                                         {"replacement", replacement},
                                                                                         {"lifetime", lifetime},
                                                                                         {"failures", failures},
                                                                                         {"private", private_data}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
