from pathlib import Path
p=Path('src/app/inspector_sprites.cpp');s=p.read_text().replace('''        for (const auto& r : effects.reports)
            ImGui::TextWrapped("Event %llu %s -> #%llu''','''        for (const auto& r : effects.reports)
        {
            ImGui::TextWrapped("Event %llu %s -> #%llu''');s=s.replace('''            ImGui::TextWrapped("Impact sound: %s", r.sound_resource.c_str());
    }''','''            ImGui::TextWrapped("Impact sound: %s", r.sound_resource.c_str());
        }
    }''');p.write_text(s)
p=Path('tests/blood_spurt_fixture.h');s=p.read_text().replace('const std::filesystem::path& root, std::string& error)', 'const std::filesystem::path& root, std::string& error, int missing_variant = -1)');s=s.replace('''        for (int i = 0; i <= 3; ++i)
            dir.children''', '''        for (int i = 0; i <= 3; ++i)
            if (i != missing_variant)
                dir.children''');p.write_text(s)
p=Path('tests/blood_spurt_tests.cpp');s=p.read_text().replace('const std::filesystem::path& root)\n    {\n        std::string error;', 'const std::filesystem::path& root, int missing_variant = -1)\n    {\n        std::string error;',1);s=s.replace('test_blood_spurt::mount(g.resources, root, error)', 'test_blood_spurt::mount(g.resources, root, error, missing_variant)',1)
needle='    bool impact(const std::filesystem::path& root)\n'
s=s.replace(needle,'''    bool missing_sound(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, 0));
        std::string error;
        CHECK(!g.resources.find("sounds/gibs/flesh/gib_impact0.wav"));
        BloodSpurtSettings s;
        s.position = {0, 1, 0};
        s.min_velocity = s.max_velocity = {0, -20, 0};
        for (unsigned i = 0; i < 32; ++i)
            CHECK(g.server.PublishClientBloodSpurt(s, "absent original zero", false, error) && error.empty());
        auto& effects = g.server.blood_spurts;
        CHECK(effects.advance(.1, g.server));
        unsigned absent = 0, played = 0;
        for (const auto& report : effects.reports)
        {
            CHECK(report.impacts == 1 && report.splat_event && report.error.empty() && report.splat_error.empty());
            CHECK(!report.sound_resource.empty());
            if (report.sound_resource == "sounds/gibs/flesh/gib_impact0.wav")
            {
                CHECK(report.sound_error == "Missing or oversized sound resource");
                ++absent;
            }
            else
            {
                CHECK(report.sound_error.empty());
                ++played;
            }
        }
        CHECK(absent > 0 && played > 0 && g.server.audio.voices().size() == played);
        CHECK(effects.advance(2.101, g.server) && effects.active.empty());
        unsigned retained = 0;
        for (const auto& report : effects.reports)
        {
            CHECK(report.ended && report.error.empty() && !report.sound_resource.empty());
            retained += !report.sound_error.empty();
        }
        CHECK(retained == absent);
        effects.clear();
        CHECK(effects.reports.empty());
        std::cout << "Original absent-zero requests=" << absent << ", decoded starts=" << played
                  << "; splats survive audio refusal, history survives emitter expiry\\n";
        return true;
    }
'''+needle)
s=s.replace(': mode == "impact"   ? impact(argv[2])', ': mode == "missing_sound" ? missing_sound(argv[2])\n            : mode == "impact"   ? impact(argv[2])');p.write_text(s)
p=Path('tests/train_session_tests.cpp');s=p.read_text();needle='    bool trace_services(const GameSession& g, RouteAudit& audit, const char* phase, std::string& error)\n';s=s.replace(needle,'''    bool audit_rejections()
    {
        GameSession g;
        RouteAudit audit;
        std::string error;
        CHECK(check_effects(g, audit, nullptr, error));
        BloodSpurtReport report;
        report.id = 1;
        report.sound_resource = "sounds/gibs/flesh/gib_impact0.wav";
        report.sound_error = "Missing or oversized sound resource";
        g.server.blood_spurts.reports.push_back(report);
        CHECK(check_effects(g, audit, nullptr, error) && audit.absent_blood_sounds.size() == 1);
        g.server.blood_spurts.reports.back().sound_resource = "sounds/gibs/flesh/gib_impact1.wav";
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.blood_spurts.reports.back().sound_resource = report.sound_resource;
        g.server.blood_spurts.reports.back().sound_error = "Audio decode failed";
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.blood_spurts.clear();
        g.server.weapon_sprites.failed = 1;
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.weapon_sprites.clear();
        g.client_shell.voice_manager.failures = 1;
        CHECK(!check_effects(g, audit, nullptr, error));
        std::cout << "Acceptance negative controls: unrelated absent sound, decode, FX and voice failures rejected\\n";
        return true;
    }
'''+needle);s=s.replace('''    if (argc < 3 || argc > 4''','''    if (argc == 2 && std::string_view(argv[1]) == "audit")
        return audit_rejections() ? 0 : 1;
    if (argc < 3 || argc > 4''');p.write_text(s)
p=Path('tests/CMakeLists.txt');s=p.read_text().replace('foreach(case IN ITEMS formulas impact weapon failures)','foreach(case IN ITEMS formulas impact weapon failures missing_sound)');s=s.replace('blood2_project_options(train_session_tests)\n','blood2_project_options(train_session_tests)\nadd_test(NAME train_session.audit COMMAND train_session_tests audit)\nset_tests_properties(train_session.audit PROPERTIES LABELS "headless;train_session" TIMEOUT 10)\n');p.write_text(s)
