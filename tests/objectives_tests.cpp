#include "game/game_settings.h"
#include "game/original/ObjectivesTrigger.h"
#include "string_fixture.h"
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
    bool send(GameSession& g, HOBJECT target, HOBJECT sender)
    {
        auto text = g.server.CreateString("TRIGGER");
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(sender), target, 1000);
        if (!message)
        {
            g.server.FreeString(text);
            return false;
        }
        const bool ok = g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    HOBJECT create(GameSession& g, Properties properties = {})
    {
        return g.objects.create(g.objects.find_class("ObjectivesTrigger"), {}, Creation::World, properties);
    }
    bool strings(const std::filesystem::path&)
    {
        for (bool wide : {false, true})
        {
            auto pe = test_strings::pe({{5000, u"Title \u00e9"}, {6000, u"Text|\U0001f600"}}, 1036, wide);
            StringResources result;
            std::string error;
            CHECK(read_string_resources(pe.b, result, error));
            CHECK(result.language == 1036 && result.strings.size() == 2);
            CHECK(result.strings.at(5000) == "Title \xc3\xa9" && result.strings.at(6000) == "Text|\xf0\x9f\x98\x80");
            CHECK(!read_string_resources(pe.b, result, error, 1033) && result.strings.empty());
            CHECK(read_string_resources(pe.b, result, error, 1036));
        }
        return true;
    }
    bool malformed(const std::filesystem::path&)
    {
        auto good = test_strings::pe({{5000, u"Title"}, {6000, u"Text"}});
        StringResources result;
        std::string error;
        for (std::size_t i = 0; i < good.b.size(); ++i)
            CHECK(!read_string_resources(std::span(good.b).first(i), result, error) && result.strings.empty());
        for (const auto& [offset, value] : std::vector<std::pair<unsigned, unsigned>>{{60, 0xffffffff},
                                                                                      {64, 0},
                                                                                      {84, 1},
                                                                                      {88, 0},
                                                                                      {180, 2},
                                                                                      {200, 0xfffffff0},
                                                                                      {204, 0xffffffff},
                                                                                      {324, 0xfffffff0},
                                                                                      {328, 0xffffffff},
                                                                                      {332, 0xffffffff},
                                                                                      {524, 0xffff0000},
                                                                                      {532, 0x80000000},
                                                                                      {552, 0},
                                                                                      {556, 0x80000000},
                                                                                      {580, 0x80000000},
                                                                                      {584, 0xffffffff}})
        {
            auto bad = good;
            bad.patch(offset, value);
            CHECK(!read_string_resources(bad.b, result, error) && result.strings.empty());
        }
        for (auto text :
             {std::u16string(1, 0xd800), std::u16string(1, 0xdc00), std::u16string(1, 0), std::u16string(4097, 'a')})
            CHECK(!read_string_resources(test_strings::pe({{5000, text}}).b, result, error));
        // Different languages in different blocks must not silently mix.
        auto multilingual = good;
        CHECK(multilingual.b.size() >= 588);
        multilingual.patch(0x200 + 56 + 16, 1036);
        CHECK(!read_string_resources(multilingual.b, result, error));
        CHECK(read_string_resources(multilingual.b, result, error, 1033));
        CHECK(result.strings.size() == 1 && result.strings.contains(6000));
        auto duplicate = good;
        CHECK(duplicate.b.size() >= 564);
        duplicate.patch(0x200 + 48, 313); // Duplicate block ID.
        CHECK(!read_string_resources(duplicate.b, result, error) && result.strings.empty());
        return true;
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = create(g);
        CHECK(h);
        auto* t = dynamic_cast<ObjectivesTrigger*>(g.server.HandleToObject(h));
        CHECK(t && !t->touch_activate && !t->ai_activate && !t->object_activate && !t->relay_activate &&
              t->player_activate);
        CHECK(t->objectives.resource == 0 && t->objectives.title == "Objective #1:" &&
              t->sound == "Sounds\\Thunder.wav");
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player) && t->activations == 0);
        auto relay = g.objects.create(g.objects.find_class("Trigger"), {}, Creation::World);
        CHECK(send(g, h, relay) && t->rejected == 1 && t->active);
        CHECK(send(g, h, g.client_shell.player) && t->deliveries == 1 && t->objective_sound_requests == 1 &&
              !t->active);
        CHECK(g.client_shell.objective_messages == 0); // Owned queued delivery, not a callback into client.
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.objectives_resolved && g.client_shell.objectives.title == "Objective #1:");
        CHECK(send(g, h, g.client_shell.player) && t->deliveries == 1);
        CHECK(g.unload_world() && g.client_shell.objective_messages == 0 && g.client_shell.objectives.title.empty());
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool localized(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CHECK(test_strings::mount(g, root, test_strings::pe({{5000, u"Resource title"}, {6000, u"Resource|text"}}).b));
        auto h = create(g, {{"ResourceNum", std::int32_t(1)},
                            {"TitleToDisplay", std::string("ignored")},
                            {"TextToDisplay", std::string("ignored")}});
        CHECK(send(g, h, g.client_shell.player));
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.objectives_resolved && g.client_shell.objectives.title == "Resource title" &&
              g.client_shell.objectives.text == "Resource|text");
        CHECK(g.server.client_strings.language == 1033 && g.server.client_strings.strings.size() == 2);
        auto empty = create(g, {{"ResourceNum", std::int32_t(1)}, {"TitleToDisplay", std::string{}}});
        CHECK(send(g, empty, g.client_shell.player) && g.pump_player_messages(error));
        CHECK(g.client_shell.objective_messages ==
              1); // Source suppresses even localized objectives without both strings.
        CHECK(g.leave_player() && g.client_shell.objectives.title.empty());
        CHECK(g.enter_player(error) && g.client_shell.objective_messages == 0);
        CHECK(g.unload_world() && g.server.client_strings.strings.empty() && !g.server.client_strings_attempted);
        return true;
    }
    bool missing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = create(g, {{"ResourceNum", std::int32_t(1)}});
        CHECK(send(g, h, g.client_shell.player));
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(!g.client_shell.objectives_resolved && !g.client_shell.objectives_error.empty() &&
              !g.server.client_strings_error.empty());
        CHECK(g.client_shell.objectives.title.empty() && g.client_shell.objectives.text.empty() &&
              g.client_shell.m_bInWorld);
        CHECK(!create(g, {{"ResourceNum", std::int32_t(-1)}}) && !create(g, {{"ResourceNum", std::int32_t(65535)}}));
        CHECK(g.server.SendObjectives({UINT32_MAX, "title", "text"}) == DE_INVALIDPARAMS);
        CHECK(g.server.SendObjectives({0, std::string(MAX_GP_STRING_LEN + 1, 'a'), "text"}) == DE_INVALIDPARAMS);
        CHECK(g.server.SendObjectives({0, std::string("a\0b", 3), "text"}) == DE_INVALIDPARAMS);
        CHECK(g.leave_player() && g.server.SendObjectives({0, "title", "text"}) == DE_NOTFOUND);
        CHECK(g.unload_world());
        return true;
    }
    bool queue(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        MissionObjectives value{0, "title", "text"};
        for (unsigned i = 0; i < 256; ++i)
            CHECK(g.server.SendObjectives(value) == DE_OK);
        CHECK(g.server.SendObjectives(value) == DE_ERROR);
        auto h = create(g);
        CHECK(send(g, h, g.client_shell.player));
        const auto* trigger = dynamic_cast<ObjectivesTrigger*>(g.server.HandleToObject(h));
        CHECK(trigger && trigger->last_delivery == DE_ERROR && trigger->deliveries == 0 && !trigger->active);
        value.title = "changed";
        std::string error;
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.objective_messages == 256 && g.client_shell.objectives.title == "title");
        for (auto mode : {GAMETYPE_CUSTOM, GAMETYPE_BLOODBATH, GAMETYPE_ACTION})
        {
            g.server.set_game_settings(1, mode);
            CHECK(g.server.SendObjectives(value) == DE_OK);
            CHECK(g.pump_player_messages(error));
            CHECK(g.client_shell.objective_messages == 257);
        }
        CHECK(g.server.SendObjectives(value) == DE_OK);
        CHECK(g.objects.remove(g.client_shell.player) && g.objects.finish_frame());
        std::vector<PlayerEvent> events;
        CHECK(!g.server.take_client_events(g.local_client, events) && events.empty());
        CHECK(g.unload_world());
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
        std::string title;
        CHECK(g.server.GetClientResourceString(5000, title));
        CHECK(g.server.client_strings.language == 1033 && g.server.client_strings.strings.size() == 790);
        std::cout << "cres.dll strings=" << g.server.client_strings.strings.size()
                  << " language=" << g.server.client_strings.language << '\n';
        unsigned worlds = 0, entries = 0, created = 0, rejected = 0, missing = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.ends_with(".dat") || !path.starts_with("worlds/"))
                continue;
            std::vector<std::uint8_t> bytes;
            CHECK(g.resources.read(path, bytes, error));
            World world;
            if (!read_world(bytes, world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& source : world.objects)
            {
                if (source.class_name != "ObjectivesTrigger")
                    continue;
                ++entries;
                Properties props;
                for (const auto& property : source.properties)
                    props.emplace(property.name, property.value);
                auto h = create(g, props);
                if (!h)
                {
                    ++rejected;
                    std::cout << "reject " << path << ": " << g.objects.diagnostics().back() << '\n';
                }
                else
                {
                    ++created;
                    auto* t = dynamic_cast<ObjectivesTrigger*>(g.server.HandleToObject(h));
                    CHECK(t);
                    if (t->objectives.resource)
                    {
                        std::string a, b;
                        if (!g.server.GetClientResourceString(5000 + t->objectives.resource - 1, a) ||
                            !g.server.GetClientResourceString(6000 + t->objectives.resource - 1, b))
                            ++missing;
                    }
                }
                CHECK(g.objects.clear());
            }
        }
        std::cout << "worlds=" << worlds << " objectives=" << entries << " created=" << created
                  << " rejected=" << rejected << " missing_resource_pairs=" << missing << '\n';
        CHECK(worlds == 58 && entries == 55 && created == 55 && rejected == 0 && missing == 0);
        CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
        unsigned sent = 0;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && dynamic_cast<ObjectivesTrigger*>(r->game.get()))
            {
                CHECK(send(g, r->handle, g.client_shell.player));
                ++sent;
            }
        CHECK(sent == 1 && g.pump_player_messages(error));
        CHECK(g.client_shell.objective_messages >= 1 && g.client_shell.objectives_resolved &&
              g.client_shell.objectives.resource == 1);
        std::cout << "SubwayA objective delivered and resolved; live=" << g.objects.snapshot().size() << '\n';
        CHECK(g.unload_world() && g.server.string_count() == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"strings", strings}, {"malformed", malformed}, {"defaults", defaults}, {"localized", localized},
        {"missing", missing}, {"queue", queue},         {"catalog", catalog}};
    const auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
