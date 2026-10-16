#include "game/original/PathPoint.h"
#include "game/server_adapter.h"
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
    HOBJECT point(GameSession& g, const char* name, DVector pos = {}, Properties props = {})
    {
        ObjectCreateStruct s;
        SAFE_STRCPY(s.m_Name, name);
        s.m_Pos = pos;
        props["Name"] = std::string(name);
        props["Pos"] = pos;
        return g.objects.create(g.objects.find_class("PathPoint"), s, Creation::World, props);
    }
    bool copy()
    {
        GameSession g;
        CHECK(g.initialize());
        const auto h = point(g, "route1", {2, 3, 4},
                             {{"ActionTarget", std::string("door")}, {"ActionMessage", std::string("TRIGGER")}});
        CHECK(h);
        PathListData data;
        std::string error;
        const auto strings = g.server.string_count();
        CHECK(data.Copy(g.server, h, error) && error.empty());
        CHECK(data.name == "route1" && data.position.y == 3 && data.source == h);
        CHECK(data.action_target == "door" && data.action_message == "TRIGGER" && g.server.string_count() == strings);
        CHECK(g.objects.remove(h));
        CHECK(!data.Copy(g.server, h, error) && data.name == "route1" && !error.empty());
        CHECK(g.objects.finish_frame() && g.server.string_count() == 0 && data.action_message == "TRIGGER");
        CHECK(!data.Copy(g.server, h, error));
        auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
        CHECK(!data.Copy(g.server, other, error));
        const auto blank = point(g, "", {}, {{"ActionMessage", std::string("ON")}});
        CHECK(blank && data.Copy(g.server, blank, error) && data.name.empty() && !data.action_target &&
              data.action_message == "ON");
        return true;
    }
    bool selection()
    {
        GameSession g;
        CHECK(g.initialize());
        for (auto name :
             {"route1", "ROUTE10", "route2suffix", "route", "routeX1", "route-1", "other1", "route01", "route1"})
            CHECK(point(g, name));
        CHECK(g.server_shell.PostStartWorld());
        PathList out;
        std::string error;
        CHECK(g.server_shell.paths.GetPath("RoUtE", out, error) && error.empty());
        const std::vector<std::string> expected{"route1", "route01", "route2suffix", "ROUTE10", "route1"};
        CHECK(out.size() == expected.size());
        for (size_t i = 0; i < out.size(); ++i)
            CHECK(out[i].name == expected[i]);
        CHECK(out.front().source != out.back().source);
        CHECK(g.server_shell.paths.GetPath("route1", out, error) && out.size() == 1 && out[0].name == "ROUTE10");
        return true;
    }
    bool failure()
    {
        GameSession g;
        CHECK(g.initialize());
        PathList out(1);
        out[0].name = "old";
        std::string error;
        CHECK(!g.server_shell.paths.GetPath("route", out, error) && out[0].name == "old");
        CHECK(point(g, "route1") && g.server_shell.PostStartWorld());
        for (const auto& bad : std::vector<std::string>{"", std::string(101, 'x'), std::string("r\0oute", 6)})
            CHECK(!g.server_shell.paths.GetPath(bad, out, error) && out[0].name == "old" && !error.empty());
        CHECK(!g.server_shell.paths.GetPath("missing", out, error) && out.empty());
        out.resize(1);
        out[0].name = "route";
        CHECK(g.server_shell.paths.GetPath(out[0].name, out, error) && out.size() == 1 && out[0].name == "route1");
        out[0].name = "caller changed";
        CHECK(g.server_shell.paths.entries()[0].name == "route1");
        CHECK(g.server_shell.paths.GetPath("route", out, error) && error.empty());
        return true;
    }
    bool lifetime()
    {
        GameSession g;
        CHECK(g.initialize());
        auto one = point(g, "route1", {1, 2, 3}, {{"ActionMessage", std::string("ON")}});
        auto smell = point(g, "SMELL1");
        CHECK(one && smell);
        CHECK(g.server_shell.PostStartWorld() && g.server_shell.paths.size() == 2);
        PathList saved;
        std::string error;
        CHECK(g.server_shell.paths.GetPath("route", saved, error));
        g.objects.get(one)->state.m_Pos = {9, 9, 9};
        SAFE_STRCPY(g.objects.get(one)->state.m_Name, "changed2");
        CHECK(g.server_shell.paths.entries()[0].position.x == 1 && g.server_shell.paths.entries()[0].name == "route1");
        CHECK(g.objects.remove(one) && g.objects.finish_frame());
        CHECK(g.objects.advance(20) && !g.objects.get(smell));
        CHECK(g.server_shell.paths.size() == 2 && saved[0].action_message == "ON");
        CHECK(g.server_shell.PostStartWorld() && g.server_shell.paths.size() == 0);
        CHECK(!g.server_shell.paths.GetPath("route", saved, error) && saved.size() == 1);
        CHECK(g.unload_world() && g.server.string_count() == 0 && saved[0].name == "route1");
        CHECK(!g.server_shell.world_started);
        CHECK(point(g, "route1") && g.server_shell.PostStartWorld());
        CHECK(g.server_shell.paths.GetPath("route", saved, error) && saved[0].source != one);
        return true;
    }
    bool enumeration()
    {
        GameSession g;
        CHECK(g.initialize());
        auto hidden = point(g, "hidden1"), inactive = point(g, "inactive1"), pending = point(g, "pending1");
        CHECK(hidden && inactive && pending);
        g.objects.get(inactive)->activity = OBJSTATE_INACTIVE;
        CHECK(g.objects.remove(pending));
        CHECK(g.server_shell.PostStartWorld() && g.server_shell.paths.size() == 1);
        CHECK(g.objects.get(hidden) && g.objects.get(inactive)); // Build never consumes PathPoints.
        CHECK(g.objects.finish_frame() && !g.objects.get(pending));
        CHECK(g.server_shell.paths.entries()[0].source == hidden);
        return true;
    }
    bool limits()
    {
        GameSession g;
        CHECK(g.initialize());
        HOBJECT last = nullptr;
        for (size_t i = 0; i < PathMgr::max_points; ++i)
        {
            last = point(g, "p1");
            CHECK(last);
        }
        CHECK(g.server_shell.PostStartWorld() && g.server_shell.paths.size() == PathMgr::max_points);
        last = point(g, "p1");
        CHECK(last);
        CHECK(!g.server_shell.PostStartWorld() && !g.server_shell.world_started && g.server_shell.paths.size() == 0);
        CHECK(g.server_shell.paths.error().find("4096") != std::string::npos);
        CHECK(g.objects.remove(last) && g.objects.finish_frame());
        CHECK(g.server_shell.PostStartWorld() && g.server_shell.paths.size() == PathMgr::max_points);
        PathList out;
        std::string error;
        CHECK(g.server_shell.paths.GetPath("p", out, error) && out.size() == PathMgr::max_points);
        CHECK(g.unload_world() && g.server_shell.paths.size() == 0 && g.server.string_count() == 0);
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.server_shell.paths.size() == 7);
            PathList retained;
            for (const auto& p : g.server_shell.paths.entries())
            {
                std::cout << p.name << " pos=" << p.position.x << ',' << p.position.y << ',' << p.position.z
                          << " target=" << p.action_target.value_or("<absent>")
                          << " message=" << p.action_message.value_or("<absent>") << '\n';
                auto digit = p.name.find_first_of("0123456789");
                CHECK(digit != p.name.npos && digit > 0);
                CHECK(g.server_shell.paths.GetPath(p.name.substr(0, digit), retained, error));
                CHECK(!retained.empty());
                for (size_t j = 1; j < retained.size(); ++j)
                    CHECK(g.objects.get(retained[j - 1].source)->id.slot > g.objects.get(retained[j].source)->id.slot);
            }
            for (const auto& [prefix, expected] : std::map<std::string, std::vector<std::string>>{
                     {"spank", {"spank1", "spank0"}}, {"dog", {"dog3", "dog2", "dog1", "dog0"}}, {"cat", {"cat1"}}})
            {
                PathList selected;
                CHECK(g.server_shell.paths.GetPath(prefix, selected, error) && selected.size() == expected.size());
                std::cout << "Selected " << prefix << ':';
                for (size_t i = 0; i < expected.size(); ++i)
                {
                    CHECK(selected[i].name == expected[i] && !selected[i].action_target && !selected[i].action_message);
                    std::cout << ' ' << selected[i].name;
                }
                std::cout << '\n';
            }
            CHECK(g.enter_player(error));
            CHECK(g.step(.1));
            CHECK(g.server_shell.paths.size() == 7 && g.server_shell.paths.entries()[0].source);
            for (const auto& p : g.server_shell.paths.entries())
                CHECK(g.server.IsObjectAlive(p.source));
            const auto old = retained.front().source;
            const auto name = retained.front().name;
            CHECK(g.unload_world() && g.server_shell.paths.size() == 0 && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
            CHECK(!g.objects.get(old) && retained.front().name == name);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    if (std::string_view(argv[1]) == "catalog")
        return argc == 3 && catalog(argv[2]) ? 0 : 1;
    const std::map<std::string, std::function<bool()>> tests{
        {"copy", copy},         {"selection", selection},     {"failure", failure},
        {"lifetime", lifetime}, {"enumeration", enumeration}, {"limits", limits}};
    auto i = tests.find(argv[1]);
    return i != tests.end() && i->second() ? 0 : 1;
}
