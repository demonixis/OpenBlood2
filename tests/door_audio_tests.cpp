#include "audio_fixture.h"
#include "brush_fixture.h"
#include "game/original/Door.h"
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
    HOBJECT create(GameSession& g, const char* type = "Door", Properties properties = {})
    {
        const auto name = "audio-door-" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, {-10, 100, -10}, {10, 120, 10});
        model.translation = {0, 110, 0};
        g.world.models.push_back(std::move(model));
        properties["Name"] = name;
        properties.try_emplace("Speed", 100.f);
        properties.try_emplace("MoveDist", 50.f);
        properties.try_emplace("MoveDir", DVector{1, 0, 0});
        properties.try_emplace("TriggerClosed", true);
        properties.try_emplace("OpenBusySound", std::string("cue.wav"));
        properties.try_emplace("CloseBusySound", std::string("long.wav"));
        properties.try_emplace("OpenStopSound", std::string("cue.wav"));
        properties.try_emplace("CloseStopSound", std::string("long.wav"));
        if (std::string_view(type) == "RotatingDoor")
            properties.try_emplace("RotationAngles", DVector{0, 90, 0});
        properties.try_emplace("SoundRadius", 7.f); // Not StartSound's actual radii.
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, properties);
    }
    Door* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<Door*>(g.server.HandleToObject(h));
    }
    bool send(GameSession& g, HOBJECT h, const char* command)
    {
        auto text = g.server.CreateString(command);
        auto message = g.server.StartMessageToObject(nullptr, h, 1000);
        CHECK(text && message && g.server.WriteToMessageHString(message, text) == DE_OK);
        const auto result = g.server.EndMessage(message);
        g.server.FreeString(text);
        CHECK(result == DE_OK);
        return true;
    }
    bool step(GameSession& g, unsigned frames)
    {
        for (unsigned i = 0; i < frames; ++i)
            CHECK(g.step(.02));
        return true;
    }
    bool voice(GameSession& g, Door& d, bool loop)
    {
        const auto* v = g.server.audio.get(d.last_voice);
        CHECK(v && v->clip && v->clip->frames() && d.sound_error.empty());
        CHECK(v->owner == d.m_hObject && v->request.speaker == d.m_hObject && !v->world_owned);
        CHECK(v->request.loop == loop && v->auto_remove == !loop && !v->request.streaming);
        CHECK(v->request.space == SoundSpace::Spatial && v->request.inner_radius == 500 &&
              v->request.outer_radius == 1500 && v->request.volume == 1 && v->request.priority == 1 &&
              v->request.reverb_requested && d.loop_voice == (loop ? d.last_voice : 0));
        return true;
    }
    bool states(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        g.server.audio.inspect_pcm = true;
        for (const auto* type : {"Door", "RotatingDoor"})
        {
            auto h = create(g, type);
            auto* d = get(g, h);
            CHECK(d && !d->sound_requests && !d->last_voice);
            CHECK(send(g, h, "TRIGGER") && !d->sound_requests && step(g, 2));
            CHECK(d->state == Door::Opening && d->sound_requests == 1 && voice(g, *d, true));
            const auto opening = d->loop_voice;
            CHECK(step(g, 4) && g.server.audio.get(opening)->loops > 0);
            CHECK(g.server.advance_audio(.01, g.objects.get(h)->state.m_Pos, {}) && g.server.audio.peak > 0);
            const auto* attached = g.server.audio.get(opening);
            CHECK(attached->position.x == g.objects.get(h)->state.m_Pos.x);
            CHECK(step(g, 100) && d->state == Door::Open && !d->loop_voice && !g.server.audio.get(opening));
            CHECK(d->sound_starts == 2 && !d->sound_failures && !g.server.audio.get(d->last_voice));
            CHECK(send(g, h, "TRIGGERCLOSE") && voice(g, *d, true));
            const auto closing = d->loop_voice;
            CHECK(step(g, 100) && d->state == Door::Closed && !g.server.audio.get(closing));
            CHECK(d->sound_starts == 4 && !d->sound_failures);
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
        }
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool replacement(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = create(g, "Door", {{"OpenStopSound", std::string("long.wav")}});
        auto* d = get(g, h);
        CHECK(d && send(g, h, "TRIGGER") && step(g, 2) && voice(g, *d, true));
        auto previous = d->loop_voice;
        d->close_busy.clear();
        CHECK(send(g, h, "TRIGGERCLOSE") && !g.server.audio.get(previous) && !d->loop_voice && !d->last_voice);
        CHECK(d->sound_requests == 1 && step(g, 10) && d->state == Door::Closed && voice(g, *d, false));
        const auto stop = d->last_voice;
        CHECK(send(g, h, "TRIGGER") && step(g, 2) && voice(g, *d, true));
        CHECK(g.server.audio.get(stop)); // No retained stop handle to interrupt.
        previous = d->loop_voice;
        d->close_busy = "missing.wav";
        CHECK(send(g, h, "TRIGGERCLOSE") && d->state == Door::Closing && !g.server.audio.get(previous));
        CHECK(!d->loop_voice && !d->last_voice && d->sound_failures == 1 && !d->sound_error.empty());
        CHECK(g.server.audio.get(stop) && step(g, 10) && d->state == Door::Closed);
        CHECK(d->sound_starts == 4 && d->sound_requests == 5 && d->sound_failures == 1);
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = create(g);
        auto* d = get(g, h);
        CHECK(d && send(g, h, "TRIGGER") && step(g, 2));
        const auto id = d->loop_voice;
        CHECK(id && g.server.audio.get(id));
        auto* record = g.objects.get(h);
        record->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(step(g, 2) && g.server.audio.get(id)->cursor > 0); // No rendering/UI dependency.
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.server.audio.get(id));
        h = create(g, "Door", {{"OpenStopSound", std::string("long.wav")}});
        d = get(g, h);
        CHECK(send(g, h, "TRIGGER") && step(g, 2) && d->loop_voice > id);
        const auto next = d->loop_voice;
        CHECK(!g.server.KillSound(id) && g.server.audio.get(next));
        for (unsigned i = 0; i < 100 && d->state != Door::Open; ++i)
            CHECK(step(g, 1));
        CHECK(d->state == Door::Open && voice(g, *d, false));
        const auto one_shot = d->last_voice;
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        CHECK(g.server.advance_audio(.01, {}, {}) && !g.server.audio.get(one_shot));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        CHECK(!g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(Config{root, "private"}, error));
        SoundId previous = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            std::vector<HOBJECT> doors;
            for (auto id : g.objects.snapshot())
            {
                const auto* r = g.objects.get(id);
                const auto* d = dynamic_cast<const Door*>(r->game.get());
                if (d && !d->open_busy.empty() && d->move_dist > 0 && d->speed > 0 && d->open_delay < .05f)
                    doors.push_back(r->handle);
            }
            CHECK(!doors.empty());
            for (auto h : doors)
            {
                auto* d = get(g, h);
                CHECK(send(g, h, "TRIGGER") && step(g, 2));
                CHECK(voice(g, *d, true) && d->sound_starts == 1 && !d->sound_failures && d->loop_voice > previous);
                previous = d->loop_voice;
                std::cout << "Authored door " << g.objects.get(h)->state.m_Name << ": " << d->last_sound
                          << " frames=" << g.server.audio.get(d->last_voice)->clip->frames() << '\n';
                CHECK(step(g, 4) && g.server.audio.get(previous)->cursor > 0);
                CHECK(send(g, h, "TRIGGERCLOSE") && !g.server.audio.get(previous));
                CHECK(step(g, 50) && d->state == Door::Closed && !d->sound_failures);
            }
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests{
        {"states", states}, {"replacement", replacement}, {"lifetime", lifetime}, {"private", private_data}};
    const auto test = tests.find(argv[1]);
    return test != tests.end() && test->second(argv[2]) ? 0 : 1;
}
