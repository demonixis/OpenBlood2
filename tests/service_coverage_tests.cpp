#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    std::string read(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    bool catalog(const std::filesystem::path& root)
    {
        // Deliberately narrow source-format guard, not a C++ parser. A declaration
        // syntax change must update this guard, never silently omit a service.
        const auto header = read(root / "src/game/legacy/server_de.h");
        CHECK(!header.empty());
        std::set<std::string> declared, listed, traced;
        for (const auto& pattern : {R"(virtual\s+[^;{}]*?\b(\w+)\s*\([^;{}]*?\)\s*=\s*0;)", R"(\(\*(\w+)\))"})
        {
            std::regex expression(pattern);
            for (std::sregex_iterator i(header.begin(), header.end(), expression), end; i != end; ++i)
                CHECK(declared.insert((*i)[1]).second);
        }
        CHECK(!declared.empty());
        for (const auto& definition : service_catalog())
        {
            if (definition.boundary == ServiceBoundary::Interface)
                CHECK(listed.insert(std::string(definition.name)).second);
            // SaveObjects/RestoreObjects became bounded in save/load lot 3.
            CHECK(definition.support == ServiceSupport::Bounded);
        }
        CHECK(listed == declared);
        std::string source = read(root / "src/game/server_adapter.h");
        for (const auto& file : std::filesystem::directory_iterator(root / "src/game"))
            if (file.path().extension() == ".cpp" && file.path().filename().string().starts_with("server_"))
                source += read(file.path());
        std::regex expression("trace_service\\(\"([^\"]+)\"\\)");
        for (std::sregex_iterator i(source.begin(), source.end(), expression), end; i != end; ++i)
            CHECK(traced.insert((*i)[1]).second); // Exactly one canonical boundary per entry.
        listed.clear();
        for (const auto& definition : service_catalog())
            CHECK(listed.insert(std::string(definition.name)).second);
        CHECK(traced == listed);
        // Each declared service must trace in its own implementation, not merely
        // exist elsewhere as a matching string. Accept current methods/thunks/inlines.
        std::set<std::string> instrumented;
        const std::regex bodies(
            R"(ServerAdapter::(\w+)\s*\([^;{}]*?\)\s*\{|\b(\w+)\s*=\s*\[\][^{]*\{|\b(\w+)\s*\([^;{}]*?\)\s*override\s*\{)");
        for (std::sregex_iterator i(source.begin(), source.end(), bodies), finish; i != finish; ++i)
        {
            const auto& match = *i;
            const std::string name = match[1].matched   ? match[1].str()
                                     : match[2].matched ? match[2].str()
                                                        : match[3].str();
            if (!declared.contains(name))
                continue;
            CHECK(instrumented.insert(name).second);
            auto begin = static_cast<std::size_t>(match.position() + match.length());
            auto end = begin;
            int depth = 1;
            for (; end < source.size() && depth; ++end)
                depth += (source[end] == '{') - (source[end] == '}');
            CHECK(!depth);
            CHECK(source.substr(begin, end - begin).find("trace_service(\"" + name + "\")") != std::string::npos);
        }
        CHECK(instrumented == declared);
        std::cout << "Catalog: " << declared.size() << " interface services / " << listed.size()
                  << " trace boundaries\n";
        return true;
    }
    const ServiceCoverage* find(const std::vector<ServiceCoverage>& rows, std::string_view name)
    {
        const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.name == name; });
        return found == rows.end() ? nullptr : &*found;
    }
    bool observations()
    {
        GameSession game;
        CHECK(game.initialize());
        auto& s = game.server;
        CHECK(!s.trace_enabled);
        const auto empty = s.service_calls;
        auto rows = service_coverage(empty);
        CHECK(rows.size() == service_catalog().size());
        CHECK(find(rows, "SaveObjects")->support == ServiceSupport::Bounded && !find(rows, "SaveObjects")->calls);
        CHECK(find(rows, "GetPathManager")->support == ServiceSupport::Bounded && !find(rows, "GetPathManager")->calls);
        CHECK(s.GetPathManager() && s.service_calls == empty); // Inspection outside a traced scope.
        {
            ServiceTraceScope trace(s.trace_enabled);
            s.GetPathManager();
            DVector velocity{};
            CHECK(s.SetVelocity(nullptr, &velocity) != DE_OK); // Counted attempt, not success.
            {
                ServiceTraceScope nested(s.trace_enabled);
                // Not an original save file: refused, but not an unavailable service.
                CHECK(s.SaveObjects("unused", nullptr, 0, 0) == DE_ERROR);
                CHECK(s.RestoreObjects("unused", 0, 0) == DE_ERROR);
                // Plain object references outside saves remain unavailable services.
                auto message = s.StartHMessageWrite();
                CHECK(s.WriteToMessageObject(message, nullptr) == DE_ERROR);
                CHECK(!s.ReadFromMessageObject(message));
                s.EndHMessageWrite(message);
                s.EndHMessageRead(nullptr);
            }
            CHECK(s.trace_enabled);
        }
        CHECK(!s.trace_enabled && s.unavailable_history.size() == 2);
        rows = service_coverage(s.service_calls);
        CHECK(find(rows, "GetPathManager")->calls == 1 && find(rows, "SetVelocity")->calls == 1);
        CHECK(find(rows, "SaveObjects")->calls == 1 && find(rows, "RestoreObjects")->calls == 1 &&
              find(rows, "EndHMessageRead")->calls == 1);
        const auto before = s.service_calls;
        CHECK(service_coverage(s.service_calls).size() == rows.size() && s.service_calls == before);
        auto unknown = before;
        unknown["new-unclassified-boundary"] = 4;
        rows = service_coverage(unknown);
        CHECK(rows.size() == service_catalog().size() + 1);
        CHECK(find(rows, "new-unclassified-boundary")->support == ServiceSupport::Unclassified);
        CHECK(find(rows, "new-unclassified-boundary")->calls == 4);
        std::string error;
        CHECK(!game.restore_player({}, error, PlayerRestoreMode::SavedPosition, "invalid-start"));
        CHECK(!s.trace_enabled);
        const auto client_queries = s.service_calls["GetClientUserData"];
        CHECK(!game.command_weapon(0, error) && !game.select_weapon(0, error));
        CHECK(s.service_calls["GetClientUserData"] == client_queries + 2 && !s.trace_enabled);
        CHECK(!game.pump_player_messages(error) && !s.trace_enabled);
        const auto failures = s.unavailable_history;
        CHECK(game.unload_world());
        CHECK(s.unavailable_services.empty() && s.unavailable_history == failures);
        CHECK(s.string_count() == 0 && s.message_count() == 0 && s.client_count() == 0);
        s.service_calls.clear();
        {
            ServiceTraceScope trace(s.trace_enabled);
            std::string name = "temporary-diagnostic-boundary-long-name";
            s.trace_service(name.c_str());
            name.assign("changed caller storage");
            for (int i = 0; i < 1000; ++i)
                s.trace_service("temporary-diagnostic-boundary-long-name");
        }
        CHECK(s.service_calls.size() == 1 && s.service_calls.at("temporary-diagnostic-boundary-long-name") == 1001);
        const auto retained = service_coverage(s.service_calls);
        CHECK(find(retained, "temporary-diagnostic-boundary-long-name")->support == ServiceSupport::Unclassified);
        s.service_calls.clear();
        s.trace_service("disabled-boundary");
        CHECK(s.service_calls.empty());
        {
            ServiceTraceScope trace(s.trace_enabled);
            s.GetPathManager();
        }
        CHECK(s.service_calls.size() == 1 && s.service_calls.at("GetPathManager") == 1);
        CHECK(find(retained, "temporary-diagnostic-boundary-long-name")->calls == 1001);
        return true;
    }
    bool report(const GameSession& game, std::string_view phase)
    {
        unsigned reached = 0, unexercised = 0;
        for (const auto& row : service_coverage(game.server.service_calls))
        {
            CHECK(row.support != ServiceSupport::Unclassified);
            if (row.support == ServiceSupport::Unavailable)
                CHECK(row.calls == 0);
            reached += row.calls > 0;
            unexercised += row.calls == 0;
            std::cout << phase << '\t' << row.name << '\t' << service_boundary_name(row.boundary) << '\t'
                      << service_support_name(row.support) << '\t' << row.calls << '\n';
        }
        CHECK(game.server.unavailable_history.empty());
        unsigned failed_ai = 0;
        for (auto id : game.objects.snapshot())
            if (auto* ai = dynamic_cast<AI_Mgr*>(game.objects.get(id)->game.get()); ai && !ai->runtime_error.empty())
            {
                ++failed_ai;
                std::cout << "AI failure: " << ai->runtime_error << '\n';
            }
        std::cout << "Coverage " << phase << ": reached=" << reached << " unexercised=" << unexercised
                  << " unavailable=0 ai_errors=" << failed_ai << '\n';
        CHECK(failed_ai == 0);
        return true;
    }
    bool private_session(const std::filesystem::path& root)
    {
        std::string error;
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        CHECK(game->initialize() && game->resources.mount_game(config, error));
        CHECK(game->request_world("01_SubwayA", error) && game->enter_player(error));
        CHECK(report(*game, "SubwayA-entry"));
        for (int i = 0; i < 600; ++i)
            CHECK(game->step(1. / 60));
        CHECK(report(*game, "SubwayA-10s"));
        {
            // This diagnostic injects the original typed exit outside a simulation
            // callback, so explicitly include its transport in the source trace.
            ServiceTraceScope trace(game->server.trace_enabled);
            CHECK(game->server.SendWorldExit({"02_CityHub"}, "01_egressa") == DE_OK &&
                  game->pump_player_messages(error));
        }
        CHECK(report(*game, "SubwayA-exit-request"));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
        CHECK(!game->server.trace_enabled && game->server.service_calls.at("ConnectClient") == 1);
        CHECK(game->restoration.restored == 1);
        CHECK(report(*game, "CityHub-restored"));
        for (int i = 0; i < 600; ++i)
            CHECK(game->step(1. / 60));
        CHECK(report(*game, "CityHub-10s"));
        const auto previous = game->server.service_calls;
        CHECK(game->unload_world());
        CHECK(!game->server.trace_enabled && game->server.service_calls.at("DisconnectClient") == 1);
        CHECK(game->server.service_calls.at("RemoveObject") > previous.at("RemoveObject"));
        CHECK(report(*game, "CityHub-teardown"));
        CHECK(game->objects.snapshot().empty() && !game->server.string_count() && !game->server.message_count() &&
              !game->server.list_count() && !game->server.client_count());
        // Deliberate failure probe after the accepted session trace, with no world loaded.
        std::cout << "Reset probe: deliberate plain object write outside a save\n";
        auto probe = game->server.StartHMessageWrite();
        CHECK(game->server.WriteToMessageObject(probe, nullptr) == DE_ERROR);
        game->server.EndHMessageWrite(probe);
        CHECK(!game->server.unavailable_history.empty() && !game->server.message_count());
        CHECK(game->request_world("01_SubwayA", error));
        CHECK(!game->server.service_calls.contains("ConnectClient") &&
              !game->server.service_calls.contains("DisconnectClient"));
        CHECK(game->server.unavailable_history.empty());
        CHECK(game->unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode = argv[1];
    const bool ok = mode == "catalog" && argc == 3   ? catalog(argv[2])
                    : mode == "observations"         ? observations()
                    : mode == "private" && argc == 3 ? private_session(argv[2])
                                                     : false;
    return ok ? 0 : 1;
}
