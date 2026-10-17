#include "audio_fixture.h"
#include "game/game_settings.h"
#include "game/original/Trigger.h"
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
    HOBJECT create(GameSession& g, Properties properties = {}, const char* type = "Trigger")
    {
        properties.try_emplace("Name", std::string("audio-trigger"));
        properties.try_emplace("ActivationSound", std::string("long.wav"));
        properties.try_emplace("SoundRadius", 750.f);
        properties.try_emplace("ObjectActivate", true);
        properties.try_emplace("Pos", DVector{80, 100, 0});
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, properties);
    }
    Trigger* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<Trigger*>(g.server.HandleToObject(h));
    }
    bool send(GameSession& g, HOBJECT h, const char* command, HOBJECT sender)
    {
        auto text = g.server.CreateString(command);
        auto message = g.server.StartMessageToObject(sender ? g.server.HandleToObject(sender) : nullptr, h, 1000);
        CHECK(text && message && g.server.WriteToMessageHString(message, text) == DE_OK);
        const auto result = g.server.EndMessage(message);
        g.server.FreeString(text);
        CHECK(result == DE_OK);
        return true;
    }
    HOBJECT body(GameSession& g)
    {
        return g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                {{"Name", std::string("sender")}, {"Pos", DVector{10, 100, 0}}});
    }
    bool enter(GameSession& g)
    {
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Pos", DVector{400, 100, 0}}}));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    bool voice(GameSession& g, const Trigger& t, HOBJECT owner, float radius, DBYTE priority)
    {
        const auto* v = g.server.audio.get(t.last_voice);
        CHECK(v && v->clip && v->clip->frames() && t.sound_error.empty());
        CHECK(v->owner == owner && v->request.speaker == owner && !v->world_owned && v->auto_remove);
        CHECK(v->request.space == SoundSpace::Spatial && !v->request.loop && !v->request.streaming &&
              v->request.inner_radius == radius * .1f && v->request.outer_radius == radius &&
              v->request.priority == priority && v->request.volume == 1 && v->request.reverb_requested);
        return true;
    }
    bool activation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        g.server.audio.inspect_pcm = true;
        const auto sender = body(g);
        for (const auto* type : {"Trigger", "BigTrigger", "ToggleTrigger"})
        {
            auto h = create(g, {{"ActivationCount", std::int32_t(2)}, {"ResetTime", .05f}}, type);
            auto* t = get(g, h);
            CHECK(t && send(g, h, "TRIGGER", sender) && !t->sound_requests);
            CHECK(send(g, h, "TRIGGER", sender) && t->activations == 1 && voice(g, *t, h, 750, 1));
            const auto first = t->last_voice;
            CHECK(send(g, h, "TRIGGER", sender) && t->sound_starts == 1); // Original inactive guard.
            CHECK(g.step(.06) && t->active);
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, sender) && t->sound_starts == 1);
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, sender) && t->sound_starts == 2);
            CHECK(g.server.audio.get(first)); // Unretained requests overlap, even for the same clip.
            g.objects.get(h)->state.m_Pos = {200, 100, 0};
            CHECK(g.server.advance_audio(.01, {200, 100, 0}, {}) && g.server.audio.peak > 0);
            CHECK(g.server.audio.get(first)->position.x == 200 && g.server.audio.get(t->last_voice)->cursor > 0);
            CHECK(g.server.advance_audio(1.1, {}, {}) && !g.server.audio.get(first) &&
                  !g.server.audio.get(t->last_voice));
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
        }
        return true;
    }
    bool feedback(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root) && enter(g));
        auto ph = g.client_shell.player;
        auto* p = g.server_shell.Player(g.local_client);
        auto h = create(
            g, {{"Locked", true}, {"LockedSound", std::string("long.wav")}, {"UnlockedSound", std::string("cue.wav")}});
        auto* t = get(g, h);
        CHECK(t && send(g, h, "TRIGGER", ph) && t->locked && !t->activations && voice(g, *t, ph, 200, 2));
        const auto denied = t->last_voice;
        CHECK(send(g, h, "TRIGGER", ph) && t->sound_starts == 2 && g.server.audio.get(denied));
        auto sender = body(g);
        CHECK(send(g, h, "UNLOCK", sender) && !t->locked && voice(g, *t, sender, 200, 2));
        const auto unlocked = t->last_voice;
        CHECK(send(g, h, "UNLOCK", sender) && t->sound_starts == 3 && t->last_voice == unlocked);
        CHECK(send(g, h, "LOCK", sender));
        CHECK(p->m_InventoryMgr.AddKey({"Key", "Key", "k", "kh", 1}) == InventoryResult::Ok);
        CHECK(send(g, h, "TRIGGER", ph) && !t->locked && p->m_InventoryMgr.keys.empty());
        CHECK(t->sound_starts == 4 && voice(g, *t, h, 750, 1)); // Key response itself has no unlock cue.
        CHECK(g.server.audio.get(denied) && g.server.audio.get(unlocked));
        const auto starts = t->sound_starts;
        CHECK(send(g, h, "LOCK", ph) && !t->locked && send(g, h, "UNLOCK", sender));
        CHECK(t->sound_starts == starts); // Inactive commands do not produce feedback.
        t->SetActive(true);
        t->player_activate = false;
        CHECK(send(g, h, "LOCK", sender) && send(g, h, "TRIGGER", ph) && t->sound_starts == starts);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        const auto sender = body(g);
        auto h = create(g, {{"Locked", true}, {"UnlockedSound", std::string("long.wav")}});
        auto* t = get(g, h);
        CHECK(t && send(g, h, "UNLOCK", sender) && voice(g, *t, sender, 200, 2));
        const auto unlocked = t->last_voice;
        CHECK(send(g, h, "TRIGGER", sender) && voice(g, *t, h, 750, 1));
        const auto activation_voice = t->last_voice;
        g.objects.get(h)->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.server.advance_audio(.02, {}, {}) && g.server.audio.get(activation_voice)->cursor > 0);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        CHECK(g.server.advance_audio(.01, {}, {}) && !g.server.audio.get(activation_voice));
        CHECK(g.server.audio.get(unlocked)); // Speaker is the sender, not the deleted trigger.
        CHECK(g.objects.remove(sender) && g.objects.finish_frame());
        CHECK(g.server.advance_audio(.01, {}, {}) && !g.server.audio.get(unlocked));
        auto next = create(g);
        CHECK(send(g, next, "TRIGGER", next) && get(g, next)->last_voice > activation_voice);
        CHECK(!g.server.KillSound(activation_voice) && g.server.audio.get(get(g, next)->last_voice));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        CHECK(!g.server.string_count() && !g.server.message_count());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto sender = body(g);
        auto h = create(g, {{"ActivationSound", std::string("missing.wav")}});
        auto* t = get(g, h);
        CHECK(t && send(g, h, "TRIGGER", sender) && t->activations == 1 && !t->active && !t->last_voice);
        CHECK(t->sound_failures == 1 && !t->sound_error.empty() && g.server.audio.voices().empty());
        t->SetActive(true);
        t->activation_sound.clear();
        CHECK(send(g, h, "TRIGGER", sender) && t->sound_requests == 1 && t->sound_error.empty());
        t->SetActive(true);
        t->activation_sound = "cue.wav";
        t->sound_radius = 0; // Authored zero is retained; mixer refuses invalid attenuation.
        CHECK(send(g, h, "TRIGGER", sender) && t->activations == 3 && t->sound_failures == 2);
        t->SetActive(true);
        t->locked = true;
        t->unlocked_sound = "cue.wav";
        CHECK(send(g, h, "UNLOCK", nullptr) && !t->locked && t->sound_failures == 3 && !t->last_voice);
        g.server.set_game_settings(1, GAMETYPE_ACTION);
        CHECK(send(g, h, "LOCK", sender) && send(g, h, "UNLOCK", sender) && !t->locked);
        CHECK(t->sound_failures == 4 && t->sound_error.find("Action-mode") != std::string::npos);
        CHECK(g.server.audio.voices().empty() && !t->sound_starts);
        g.server.set_game_settings(1, GAMETYPE_SINGLE);
        t->sound_radius = 100;
        for (unsigned i = 0; i < 128; ++i)
        {
            t->SetActive(true);
            CHECK(send(g, h, "TRIGGER", sender) && t->last_voice);
        }
        t->SetActive(true);
        CHECK(send(g, h, "TRIGGER", sender) && !t->active && !t->last_voice);
        CHECK(t->sound_starts == 128 && t->sound_failures == 5 && g.server.audio.voices().size() == 128);
        CHECK(g.unload_world() && g.server.audio.voices().empty());
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
            CHECK(g.request_world("05_CDM", error) && g.enter_player(error));
            unsigned checked = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* t = r ? dynamic_cast<Trigger*>(r->game.get()) : nullptr;
                if (!t || !t->active || t->locked_sound.empty() || !t->player_activate || t->named_activate)
                    continue;
                // Labelled diagnostic denial on the authored sensor, not a traversal.
                CHECK(send(g, r->handle, "LOCK", g.client_shell.player));
                CHECK(send(g, r->handle, "TRIGGER", g.client_shell.player) && t->locked);
                CHECK(voice(g, *t, g.client_shell.player, 200, 2) && !t->sound_failures && t->last_voice > previous);
                previous = t->last_voice;
                std::cout << "Authored trigger " << r->state.m_Name << ": " << t->last_sound
                          << " frames=" << g.server.audio.get(previous)->clip->frames() << '\n';
                CHECK(g.server.advance_audio(.01, g.objects.get(g.client_shell.player)->state.m_Pos, {}));
                CHECK(g.server.audio.get(previous)->cursor > 0);
                ++checked;
            }
            CHECK(checked > 0);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips());
        }
        CHECK(!g.server.string_count() && !g.server.message_count() && !g.server.client_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests{{"activation", activation},
                                                                                         {"feedback", feedback},
                                                                                         {"lifetime", lifetime},
                                                                                         {"failures", failures},
                                                                                         {"private", private_data}};
    const auto test = tests.find(argv[1]);
    return test != tests.end() && test->second(argv[2]) ? 0 : 1;
}
