#include "audio_fixture.h"
#include "game/original/ConversationTrigger.h"
#include <algorithm>
#include <functional>
#include <iostream>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_audio::setup(g, root));
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    ConversationTrigger* create(GameSession& g, Properties props = {})
    {
        auto h = g.objects.create(g.objects.find_class("ConversationTrigger"), {}, Creation::World, props);
        return dynamic_cast<ConversationTrigger*>(g.server.HandleToObject(h));
    }
    bool send(GameSession& g, ConversationTrigger* c)
    {
        auto text = g.server.CreateString("TRIGGER");
        auto message =
            g.server.StartMessageToObject(g.server.HandleToObject(g.client_shell.player), c->m_hObject, 1000);
        bool ok =
            message && g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    bool mapping(const std::filesystem::path&)
    {
        CHECK(std::string(conversation_text("SOUNDS\\VOICES\\1acaleb1.WAV")) == "Bring it on!");
        CHECK(!conversation_text("sounds/1ACaleb1.wav") && !conversation_text("default.wav") && !conversation_text(""));
        CHECK(std::string(conversation_text("2Caleb2.wav")) == "Letting me live.");
        return true;
    }
    bool sequence(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (unsigned flags = 0; flags < 4; ++flags)
        {
            auto* c = create(g, {{"WaveFile", std::string("1ACaleb1.wav")},
                                 {"WaveFile2", std::string("2Caleb2.wav")},
                                 {"WaveFile4", std::string("ignored.wav")},
                                 {"RepeatLastMessage", bool(flags & 1)},
                                 {"LoopToFirstMessage", bool(flags & 2)},
                                 {"CharacterType", std::int32_t(-1)}});
            CHECK(c && c->count == 2 && c->waves[3].empty() && c->character == 255);
            CHECK(!c->touch_activate && !c->ai_activate && !c->relay_activate && c->player_activate);
            for (unsigned i = 0; i < 4; ++i)
            {
                c->SetActive(true);
                CHECK(send(g, c));
            }
            CHECK(c->queued == (flags ? 4u : 2u));
            CHECK(c->current == (flags & 1 ? 1u : flags & 2 ? 0u : 2u));
            std::vector<PlayerEvent> events;
            CHECK(g.server.take_client_events(g.local_client, events) && events.size() == c->queued);
            CHECK(std::get<Conversation>(events[0]).text == "Bring it on!");
            CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame());
        }
        auto* c = create(g, {{"WaveFile", std::string("missing.wav")}});
        CHECK(c && send(g, c));
        CHECK(c->current == 0 && c->unmapped == 1 && !c->error.empty() && !c->queued);
        CHECK(!create(g, {{"WaveFile", std::string(200, 'x')}}));
        CHECK(g.unload_world() && !g.server.message_count() && !g.server.string_count());
        return true;
    }
    bool delayed(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* c = create(g, {{"WaveFile", std::string("1ACaleb1.wav")},
                             {"LoopToFirstMessage", true},
                             {"MessageDelay", .1f},
                             {"MessageDelay2", .2f}});
        CHECK(c && send(g, c) && !c->queued);
        CHECK(g.objects.advance(.11) && c->queued == 1); // Delayed virtual SendMessage ignores empty target slots.
        CHECK(g.objects.advance(.11) && c->queued == 2);
        CHECK(g.unload_world());
        return true;
    }
    bool transport(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        Conversation v{"cue.wav", "short text", 0};
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendConversation(v) == DE_OK);
        auto* c = create(g, {{"WaveFile", std::string("1ACaleb1.wav")}});
        CHECK(c && send(g, c) && c->current == 1 && !c->queued && c->last_delivery == DE_ERROR);
        std::vector<PlayerEvent> events;
        CHECK(g.server.take_client_events(g.local_client, events) && events.size() == 256);
        v.file = std::string(200, 'x');
        CHECK(g.server.SendConversation(v) == DE_INVALIDPARAMS);
        v.file = "cue.wav";
        v.text = std::string(500, 'x');
        CHECK(g.server.SendConversation(v) == DE_INVALIDPARAMS);
        v.text = std::string("x\0y", 3);
        CHECK(g.server.SendConversation(v) == DE_INVALIDPARAMS);
        v.text = "valid";
        CHECK(g.server.SendConversation(v) == DE_OK);
        CHECK(g.objects.remove(g.client_shell.player) && g.objects.finish_frame());
        CHECK(g.server.SendConversation(v) == DE_NOTFOUND);
        CHECK(g.unload_world() && !g.server.message_count());
        return true;
    }
    bool timing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& c = g.client_shell.communication;
        CHECK(c.Start(g.server, {"cue.wav", "short", 2}) && c.portrait == "interface/commlink/i_comm.pcx");
        CHECK(!c.Start(g.server, {"other.wav", "replacement", 0}) && c.busy == 1 && c.message.file == "cue.wav");
        CHECK(g.objects.advance(.2f));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(c.level == 1 && !c.voice);
        CHECK(g.objects.advance(.25 - g.objects.time()));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(c.level == 2 && c.text_level == 1 && c.voice && c.voice_manager_requests == 3);
        const auto id = c.voice;
        const auto* voice = g.server.audio.get(id);
        CHECK(voice && voice->request.space == SoundSpace::Local && !voice->request.streaming &&
              !voice->request.reverb_requested && voice->request.priority == 2);
        CHECK(g.server.advance_audio(.1, {}, {}));
        CHECK(g.server.audio.get(id)->done);
        CHECK(g.objects.advance(2.5));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(c.level == 2 && c.voice == id);
        CHECK(g.objects.advance(.01));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(c.level == 3 && !c.voice && !g.server.audio.get(id));
        CHECK(g.objects.advance(.21));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(!c.level);
        CHECK(c.Start(g.server, {"default.wav", "no voice", 99}));
        CHECK(g.objects.advance(.21));
        c.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
        CHECK(!c.voice && !c.play_voice && c.portrait == "interface/commlink/u_comm.pcx");
        CHECK(g.unload_world() && !c.level && !c.received);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        auto* c = create(g, {{"WaveFile", std::string("1ACaleb1.wav")}});
        CHECK(c && send(g, c));
        CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame());
        CHECK(g.pump_player_messages(error) && g.client_shell.communication.received == 1);
        CHECK(g.step(.25));
        auto& comm = g.client_shell.communication;
        CHECK(comm.level == 2 && !comm.voice && comm.sound_failures == 1 && !comm.error.empty());
        CHECK(g.step(.25) && comm.sound_attempts == 1); // No fabricated retries after missing file.
        comm.Reset(g.server);
        CHECK(g.server.SendConversation({"long.wav", "voice", 0}) == DE_OK && g.pump_player_messages(error));
        CHECK(g.step(.25) && comm.voice);
        auto id = comm.voice;
        CHECK(g.objects.remove(g.client_shell.player) && g.objects.finish_frame());
        CHECK(g.step(.01) && !g.server.audio.get(id) && !comm.level);
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config cfg;
        cfg.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(cfg, error));
        unsigned worlds = 0, entries = 0, rejected = 0, cues = 0, mapped = 0, decoded = 0;
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
            for (const auto& source : world.objects)
            {
                if (source.class_name != "ConversationTrigger")
                    continue;
                ++entries;
                Properties props;
                for (const auto& property : source.properties)
                    props.emplace(property.name, property.value);
                auto* c = create(g, props);
                if (!c)
                {
                    ++rejected;
                    std::cout << "reject " << path << " " << g.objects.diagnostics().back() << '\n';
                    continue;
                }
                for (unsigned i = 0; i < c->count; ++i)
                {
                    ++cues;
                    auto* text = conversation_text(c->waves[i]);
                    mapped += bool(text);
                    CHECK(!g.resources.find(c->waves[i])); // Observed absent authored resources, not decoder failures.
                    std::shared_ptr<const AudioClip> clip;
                    bool valid = g.server.audio.load(g.resources, c->waves[i], clip, error);
                    decoded += valid;
                    std::cout << path << " " << source.class_name << " " << c->waves[i] << " mapped=" << bool(text)
                              << " decoded=" << valid << '\n';
                }
                CHECK(g.objects.remove(c->m_hObject) && g.objects.finish_frame());
            }
        }
        std::cout << "Conversation catalog worlds=" << worlds << " entries=" << entries << " rejected=" << rejected
                  << " cues=" << cues << " mapped=" << mapped << " decoded=" << decoded << '\n';
        CHECK(worlds == 57 && entries == 7 && rejected == 0 && cues == 7 && mapped == 0 && decoded == 0);
        CHECK(g.unload_world());
        return true;
    }
    bool private_game(const std::filesystem::path& root)
    {
        GameSession g;
        g.server.audio.inspect_pcm = true;
        CHECK(g.initialize());
        std::string error;
        Config cfg;
        cfg.data_dir = root;
        CHECK(g.resources.mount_game(cfg, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            unsigned created = 0, unknown = 0, failed = 0;
            unsigned omitted = 0;
            ConversationTrigger* conversation = nullptr;
            for (const auto& e : g.entries)
            {
                created += e.state == EntryState::Created;
                unknown += e.state == EntryState::Unsupported;
                failed += e.state == EntryState::Failed;
                omitted += e.state == EntryState::MissingWorldModel;
            }
            for (auto id : g.objects.snapshot())
                if (auto* c = dynamic_cast<ConversationTrigger*>(g.objects.get(id)->game.get()))
                    conversation = c;
            CHECK(created == 453 && unknown == 0 && failed == 0 && omitted == 1 && conversation);
            std::cout << "CityHub conversation count=" << conversation->count << " file=" << conversation->waves[0]
                      << " mapped=" << bool(conversation_text(conversation->waves[0])) << '\n';
            g.server_shell.start_point_name = "01_egressa";
            CHECK(g.enter_player(error));
            conversation->SetActive(true);
            CHECK(send(g, conversation));
            CHECK(g.pump_player_messages(error));
            std::cout << "queued=" << conversation->queued << " unmapped=" << conversation->unmapped
                      << " error=" << conversation->error << '\n';
            CHECK(conversation->count == 1 && conversation->waves[0] == "default.wav" && conversation->queued == 0 &&
                  conversation->unmapped == 1 && conversation->current == 0 &&
                  g.client_shell.communication.received == 0);
            // Diagnostic original callback using a real installed voice; authored placeholder stays unchanged.
            std::string file;
            for (const auto& [path, resource] : g.resources.index())
            {
                (void)resource;
                if (path.ends_with("/1acaleb1.wav"))
                {
                    file = path;
                    break;
                }
            }
            CHECK(!file.empty());
            std::replace(file.begin(), file.end(), '/', '\\');
            auto* probe = create(g, {{"WaveFile", file}});
            CHECK(probe && send(g, probe) && probe->queued == 1);
            CHECK(g.pump_player_messages(error) && g.objects.advance(.25));
            g.client_shell.communication.Update(g.server, g.client_shell.player, g.client_shell.voice_manager);
            auto id = g.client_shell.communication.voice;
            CHECK(id && g.server.audio.get(id)->clip->frames() == 25930);
            const auto& samples = g.server.audio.get(id)->clip->samples;
            CHECK(std::any_of(samples.begin(), samples.end(), [](float v) { return std::abs(v) > .1f; }));
            CHECK(g.server.audio.advance(.25, {}, {}) && g.server.audio.peak > 0);
            CHECK(g.objects.remove(probe->m_hObject) && g.objects.finish_frame() && g.server.audio.get(id));
            std::cout << "Diagnostic real voice " << file << " frames=" << g.server.audio.get(id)->clip->frames()
                      << " peak=" << g.server.audio.peak << '\n';
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.message_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"mapping", mapping}, {"sequence", sequence}, {"delayed", delayed},      {"transport", transport},
        {"clock", timing},    {"lifetime", lifetime}, {"private", private_game}, {"catalog", catalog}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
