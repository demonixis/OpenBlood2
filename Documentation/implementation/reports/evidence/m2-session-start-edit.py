from pathlib import Path
p=Path('/home/yann/projects/lithtech/Blood2')
def edit(path,old,new):
 f=p/path;s=f.read_text();assert old in s,(path,old[:80]);f.write_text(s.replace(old,new))
f=p/'src/game/level_change.cpp';s=f.read_text();a=s.index('    namespace\n    {\n');b=s.index('    bool GameSession::queue_level_change',a)
helpers=s[a:b].replace('    namespace\n    {\n','',1).replace('    } // namespace\n\n','')
helpers='\n'.join(line[4:] if line.startswith('    ') else line for line in helpers.splitlines())+'\n'
helpers=helpers.replace('check_destination(GameSession& game, LevelChangeReport& report','check_world_initialization(GameSession& game, WorldValidation& report').replace('check_presentation(GameSession& game, LevelChangeReport& report','check_world_presentation(GameSession& game, WorldValidation& report')
helpers=helpers.replace('        report.entries.clear();','''        report.entries.clear();
        if (game.entries.size() != game.world.objects.size())
        {
            error = "World source-entry accounting is incomplete";
            return false;
        }''')
(p/'src/game/world_validation.cpp').write_text('#include "game/world_validation.h"\n#include "engine/world_view.h"\n#include "game/original/SkyObjects.h"\n#include "game/server_adapter.h"\n#include <set>\n\nnamespace OpenBlood2\n{\n'+helpers+'} // namespace OpenBlood2\n')
s=s[:a]+s[b:];s=s.replace('#include "engine/world_view.h"\n#include "game/original/SkyObjects.h"','#include "game/world_validation.h"').replace('#include <set>\n','').replace('check_destination(', 'check_world_initialization(').replace('check_presentation(', 'check_world_presentation(');f.write_text(s)
(p/'src/game/world_validation.h').write_text('''#pragma once
#include "game/world_preparation.h"
namespace OpenBlood2
{
    class GameSession;
    // Explicit CPU checks shared by fresh startup and NEW_LEVEL; never advance simulation.
    bool check_world_initialization(GameSession&, WorldValidation&, std::string&);
    bool check_world_presentation(GameSession&, WorldValidation&, std::string&);
} // namespace OpenBlood2
''')
edit('src/game/world_preparation.h','    struct LevelChangeReport\n','''    struct WorldValidation
    {
        std::size_t unknown = 0, failed = 0, omitted = 0, checked_textures = 0;
        std::vector<DestinationTrialEntry> entries;
        std::vector<std::string> unavailable_services;
    };
    struct SessionStartReport : WorldValidation
    {
        std::uint64_t attempts = 0, revision = 0;
        double time = 0;
        bool accepted = false, player_entered = false;
        std::string error;
    };
    struct LevelChangeReport : WorldValidation
''')
# Remove duplicate fields from LevelChangeReport, keeping shared base.
f=p/'src/game/world_preparation.h';s=f.read_text();a=s.index('    struct LevelChangeReport');s=s[:a]+s[a:].replace('        std::size_t unknown = 0, failed = 0, omitted = 0, checked_textures = 0;\n        std::vector<DestinationTrialEntry> entries;\n        std::vector<std::string> unavailable_services;\n','');f.write_text(s)
edit('src/game/server_adapter.h','        LevelChangeReport level_change;','        LevelChangeReport level_change;\n        SessionStartReport startup;')
edit('src/game/session.h','        Inspection,\n        Blocked','        Inspection,\n        Ready, // Explicit startup checkpoint passed; not a promise about future requests.\n        Blocked')
edit('CMakeLists.txt','    src/game/session.cpp','    src/game/session.cpp\n    src/game/world_validation.cpp')
edit('src/game/session.cpp','#include "game/server_adapter.h"','#include "game/server_adapter.h"\n#include "game/world_validation.h"')
f=p/'src/game/session.cpp';s=f.read_text();a=s.index('    bool GameSession::run_world(');b=s.index('    bool GameSession::unload_world()',a)
s=s[:a]+'''    bool GameSession::run_world(std::string& error)
    {
        error.clear();
        if (objects.busy() || ServerAdapter::active_ != &server || g_pServerDE != &server || !server.bound_)
        {
            error = "RunWorld requires the active session outside callbacks";
            return false; // Reentrant/foreign calls must not overwrite the last report.
        }
        const auto attempts = startup.attempts + 1;
        startup = {};
        startup.attempts = attempts;
        startup.revision = world_revision;
        startup.time = objects.time();
        bool entered = false;
        const auto reject = [&]
        {
            startup.error = error;
            // Original start triggers are not transactional. Remove only a player
            // entered by this attempt; retain world mutations and the explicit report.
            if (entered && !leave_player())
                startup.error += "; failed player cleanup";
            state = world_path.empty() ? SessionState::Empty : SessionState::Blocked;
            if (trace.empty() || trace.back() != startup.error)
                trace.push_back(startup.error);
            error = startup.error;
            return false;
        };
        if (world_path.empty() || !server.collision_ready)
        {
            error = "No collision world loaded";
            return reject();
        }
        if (pending_level_change)
        {
            error = "RunWorld refused while a level change is pending";
            return reject();
        }
        if (!check_world_initialization(*this, startup, error))
            return reject();
        for (auto id : objects.snapshot())
            if (const auto* record = objects.get(id); record->life == Life::Alive)
                if (const auto* ai = dynamic_cast<const AI_Mgr*>(record->game.get()); ai && ai->runtime_failed)
                {
                    error = "Original NPC failure: " + std::string(record->state.m_Name) + ": " + ai->runtime_error;
                    return reject();
                }
        if (!local_client)
        {
            if (!enter_player(error))
            {
                error = "Player entry: " + error;
                return reject();
            }
            entered = startup.player_entered = true;
        }
        const auto* player = server_shell.Player(local_client);
        if (!client_shell.m_bInWorld || !player || client_shell.player != player->m_hObject ||
            !server.IsObjectAlive(player->m_hObject) || player->m_bDead || player->m_damage.IsDead())
        {
            error = "RunWorld requires a live player associated with the original client";
            return reject();
        }
        if (pending_level_change)
        {
            error = "Start trigger requested another level during entry";
            return reject();
        }
        if (!check_world_initialization(*this, startup, error) || !check_world_presentation(*this, startup, error))
            return reject();
        if (client_shell.music.failures || !server.audio.music_error.empty())
        {
            error = "Original client music failed: " + client_shell.music.error + " " + server.audio.music_error;
            return reject();
        }
        state = SessionState::Ready;
        startup.accepted = true;
        trace.push_back("Original session startup validated; future simulation/resource requests remain checked");
        return true;
    }
''' + s[b:]
s=s.replace('        client_shell.OnExitWorld();','        client_shell.OnExitWorld();\n        if (state == SessionState::Ready)\n            state = SessionState::Inspection;')
s=s.replace('        state = SessionState::Empty;\n        return true;','        state = SessionState::Empty;\n        startup = {};\n        return true;')
f.write_text(s)
# CLI: enter before first simulation, and validate the resulting frame as well.
edit('src/app/main.cpp','        if (!game.step(1.0 / 60.0))\n            return 5;\n        if (options.start_game && !game.run_world(error))','        if (options.start_game && !game.run_world(error))')
edit('src/app/main.cpp','        if (options.inspect_player)\n            spdlog::info("Original shell/player entry inspection succeeded:', '''        if (!game.step(1.0 / 60.0))
            return 5;
        if (options.start_game && !game.run_world(error))
        {
            spdlog::error("Startup update: {}", error);
            return 6;
        }
        if (options.start_game)
            spdlog::info("Original session startup validated: player associated before first update; {} decoded images; "
                         "bounded headless check, not full M2 acceptance", game.startup.checked_textures);
        else if (options.inspect_player)
            spdlog::info("Original shell/player entry inspection succeeded:''')
edit('src/app/main.cpp','  --start-game        Check session startup; fails explicitly on missing M2 services','  --start-game        Validate original entry and one headless update; exits after checks')
edit('src/app/inspector.cpp','        ImGui::TextColored(ImVec4(1, .65f, .3f, 1), "Inspection / full gameplay blocked");','''        ImGui::TextUnformatted(game.state == SessionState::Ready ? "Startup checkpoint passed" :
                               game.state == SessionState::Blocked ? "Startup check rejected" : "Inspection");
        if (game.startup.attempts)
        {
            const auto& report = game.startup;
            ImGui::Text("Last startup check: %s at %.3f s | %zu decoded images | %zu unknown / %zu failed / %zu omitted",
                        report.accepted ? "passed" : "rejected", report.time, report.checked_textures,
                        report.unknown, report.failed, report.omitted);
            if (!report.error.empty())
                ImGui::TextWrapped("%s", report.error.c_str());
            ImGui::TextDisabled("Snapshot only; future requests and full M2 acceptance are separate.");
        }''')
