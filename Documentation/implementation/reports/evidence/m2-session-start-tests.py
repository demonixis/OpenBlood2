from pathlib import Path
p=Path('/home/yann/projects/lithtech/Blood2')
def edit(path,old,new):
 f=p/path;s=f.read_text();assert old in s,(path,old[:80]);f.write_text(s.replace(old,new))
# Existing checks asserted the historical unconditional refusal, not a missing service.
for path in ['tests/sound_tests.cpp','tests/scene_explosion_tests.cpp']:
 edit(path,'CHECK(!g.run_world(error) && error.find("0 uninstantiated") != std::string::npos);','CHECK(g.run_world(error) && error.empty() && g.state == SessionState::Ready);')
edit('tests/sprite_tests.cpp','g.objects.attachments().size() == 11 && !g.run_world(error) &&\n                  error.find("0 uninstantiated") != std::string::npos','g.objects.attachments().size() == 11 && g.run_world(error) &&\n                  error.empty() && g.state == SessionState::Ready')
edit('tests/sprite_tests.cpp','unload verified; M2 blocked (17)','unload verified; full M2 acceptance remains separate')
edit('tests/gideon_runtime_tests.cpp','CHECK(!g.run_world(error));','CHECK(g.run_world(error) && g.state == SessionState::Ready && error.empty());')
edit('tests/player_tests.cpp','CHECK(!game.run_world(error) && error.find("0 uninstantiated") != std::string::npos &&\n                  error.find("acceptance gate not passed") != std::string::npos && game.state == SessionState::Blocked);','CHECK(game.run_world(error) && error.empty() && game.state == SessionState::Ready);')
edit('tests/exit_tests.cpp','CHECK(!g.run_world(error) && error.find("0 uninstantiated") != std::string::npos);','CHECK(!g.run_world(error) && error.find("level change is pending") != std::string::npos);')
edit('tests/world_tests.cpp','CHECK(error.find("player entry: Model resource not mounted") != std::string::npos);','CHECK(error.find("unsupported classes") != std::string::npos && !game.local_client);')
edit('tests/world_tests.cpp','            CHECK(!game.run_world(error));\n            CHECK(game.unload_world() && game.server.client_count()','            CHECK(game.run_world(error) && error.empty());\n            CHECK(game.unload_world() && game.server.client_count()')
edit('tests/world_tests.cpp','        CHECK(!game.run_world(error));\n        std::cout << error','        CHECK(game.run_world(error) && error.empty());\n        std::cout << error')
edit('tests/world_cli_test.cmake','if(NOT status EQUAL 6 OR NOT out MATCHES "RunWorld acceptance gate not passed: 0" OR\n   NOT out MATCHES "remaining required paths must pass M2 acceptance")\n    message(FATAL_ERROR "Missing services must fail startup: ${status}: ${out}${err}")','if(NOT status EQUAL 0 OR NOT out MATCHES "Original session startup validated: player associated before first update" OR\n   NOT out MATCHES "bounded headless check, not full M2 acceptance")\n    message(FATAL_ERROR "Checked startup failed: ${status}: ${out}${err}")')
# Stronger callback check retains startup report/state too.
edit('tests/player_tests.cpp','            rejected_reentry = !game.leave_player()', '            const auto attempts = game.startup.attempts;\n            const auto state = game.state;\n            rejected_reentry = !game.leave_player()')
edit('tests/player_tests.cpp','                               game.local_client;','                               game.local_client && game.startup.attempts == attempts && game.state == state;')
# Synthetic refusal paths share the real DAT/REZ/DTX generation used by NEW_LEVEL tests.
insert='''    bool session_start(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(trial_fixture(game, root));
        std::string error;
        CHECK(game.unload_world());
        CHECK(!game.run_world(error) && game.state == SessionState::Empty &&
              error.find("No collision world") != error.npos);
        CHECK(game.request_world("trial5", error));
        const auto before = game.objects.snapshot();
        CHECK(!game.run_world(error) && game.startup.unknown == 1 && !game.local_client &&
              game.objects.snapshot() == before && game.state == SessionState::Blocked);
        CHECK(game.request_world("trial4", error));
        CHECK(!game.run_world(error) && game.startup.failed == 1 && !game.local_client);
        CHECK(game.request_world("trial1", error));
        game.entries.pop_back(); // Deliberately inconsistent diagnostic state must not index out of bounds.
        CHECK(!game.run_world(error) && error.find("accounting") != error.npos && !game.local_client);
        CHECK(transition_images(game, root));
        CHECK(game.request_world("trial1", error));
        // Entry works, but the generated fixture deliberately has no music assets.
        CHECK(!game.run_world(error) && error.find("music failed") != error.npos &&
              game.startup.player_entered && !game.client_shell.m_bInWorld && !game.local_client &&
              game.server.client_count() == 0 && game.startup.checked_textures > 0);
        const auto report = game.startup;
        {
            GameSession foreign;
            CHECK(!foreign.initialize());
            CHECK(!foreign.run_world(error) && foreign.startup.attempts == 0 &&
                  game.startup.attempts == report.attempts && game.startup.error == report.error);
        }
        CHECK(game.enter_player(error));
        const auto player = game.client_shell.player;
        const auto client = game.local_client;
        const auto ids = game.objects.snapshot();
        CHECK(!game.run_world(error) && game.local_client == client && game.client_shell.player == player &&
              game.objects.snapshot() == ids && !game.startup.player_entered);
        game.server.record_unavailable("synthetic required service failure");
        CHECK(!game.run_world(error) && game.startup.unavailable_services.size() == 1 &&
              error.find("unavailable services") != error.npos);
        CHECK(game.request_world("trial6", error));
        CHECK(!game.run_world(error) && error.find("another level") != error.npos &&
              !game.local_client && !game.pending_level_change);
        CHECK(game.request_world("trial1", error));
        // The indexed-only skin fixture is malformed; strict startup must decode it.
        CHECK(game.resources.mount(root / "synthetic.rez", error));
        CHECK(!game.run_world(error) && error.find("image") != error.npos &&
              !game.local_client && !game.startup.accepted);
        CHECK(game.unload_world() && game.startup.attempts == 0 && game.server.string_count() == 0 &&
              game.server.client_count() == 0 && game.objects.snapshot().empty());
        std::cout << "Startup rejects unknown/failed entries, incomplete accounting, missing music, unavailable "
                     "services, recursive exits and malformed images; foreign/report and player ownership preserved\\n";
        return true;
    }
    bool session_start_private(const std::filesystem::path& root)
    {
        GameSession game;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        for (const char* world : {"01_SubwayA", "02_CityHub", "01_SubwayA"})
        {
            CHECK(game.request_world(world, error));
            CHECK(game.objects.time() == 0 && game.run_world(error));
            CHECK(error.empty() && game.state == SessionState::Ready && game.startup.accepted &&
                  game.startup.player_entered && game.startup.checked_textures > 0 &&
                  !game.startup.failed && !game.startup.unknown && game.startup.unavailable_services.empty());
            const auto client = game.local_client;
            const auto player = game.client_shell.player;
            const auto count = game.objects.snapshot().size();
            CHECK(game.run_world(error) && game.local_client == client && game.client_shell.player == player &&
                  game.objects.snapshot().size() == count && game.objects.time() == 0 &&
                  !game.startup.player_entered && game.startup.attempts == 2);
            for (unsigned frame = 0; frame < 60; ++frame)
                CHECK(game.step(1.0 / 60));
            CHECK(game.run_world(error));
            CHECK(game.server_shell.updates == 60 && game.client_shell.m_bInWorld);
            game.client_shell.player = nullptr; // Controlled association fault; never create a second player.
            CHECK(!game.run_world(error) && error.find("associated") != error.npos && game.local_client == client);
            game.client_shell.player = player;
            AI_Mgr* actor = nullptr;
            for (auto id : game.objects.snapshot())
                if (auto* ai = dynamic_cast<AI_Mgr*>(game.objects.get(id)->game.get()); ai && !ai->runtime_failed)
                {
                    actor = ai;
                    break;
                }
            CHECK(actor);
            actor->runtime_failed = true;
            actor->runtime_error = "controlled startup failure probe";
            CHECK(!game.run_world(error) && error.find("controlled startup failure probe") != error.npos);
            actor->runtime_failed = false;
            actor->runtime_error.clear();
            CHECK(game.run_world(error));
            std::cout << world << " startup: " << game.startup.checked_textures << " decoded images, "
                      << game.startup.omitted << " original missing-BSP omissions; 60 original updates\\n";
            CHECK(game.leave_player() && game.state == SessionState::Inspection && !game.local_client);
            CHECK(game.unload_world() && game.startup.attempts == 0 && game.state == SessionState::Empty &&
                  !game.server.string_count() && !game.server.client_count() && game.objects.snapshot().empty());
        }
        return true;
    }
'''
edit('tests/world_tests.cpp','    bool level_change(const std::filesystem::path& root)', insert+'    bool level_change(const std::filesystem::path& root)')
edit('tests/world_tests.cpp','    else if (name == "level_change_private")','    else if (name == "session_start_private")\n        ok = session_start_private(root);\n    else if (name == "level_change_private")')
edit('tests/world_tests.cpp','        else if (name == "level_change")','        else if (name == "session_start")\n            ok = session_start(root);\n        else if (name == "level_change")')
edit('tests/CMakeLists.txt','foreach(case IN ITEMS level_change surface_effects','foreach(case IN ITEMS session_start level_change surface_effects')
edit('tests/CMakeLists.txt','    add_test(NAME world.level_change_private','    add_test(NAME world.session_start_private COMMAND world_tests session_start_private "${BLOOD2_TEST_DATA_DIR}")\n    set_tests_properties(world.session_start_private PROPERTIES LABELS "private;headless;world" TIMEOUT 90)\n    add_test(NAME world.level_change_private')
