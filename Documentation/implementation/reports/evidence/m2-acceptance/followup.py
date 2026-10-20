from pathlib import Path
for name in ['src/game/original/ParticleExplosionFX.h','src/game/blood_spurts.h']:
 p=Path(name);s=p.read_text().replace('splat_error, sound_error;', 'splat_error, sound_resource, sound_error;');p.write_text(s)
p=Path('src/game/original/ParticleExplosionFX.cpp');s=p.read_text().replace('        sound.outer_radius = 300;', '        sound_resource = sound.resource;\n        sound.outer_radius = 300;');p.write_text(s)
p=Path('src/game/blood_spurts.cpp');s=p.read_text().replace('                r.sound_error = fx.sound_error;', '                r.sound_resource = fx.sound_resource;\n                r.sound_error = fx.sound_error;');p.write_text(s)
p=Path('src/app/inspector_sprites.cpp');s=p.read_text().replace('                ImGui::TextWrapped("%s | %s | %s", fx.error.c_str()', '                ImGui::TextWrapped("Impact sound: %s", fx.sound_resource.c_str());\n                ImGui::TextWrapped("%s | %s | %s", fx.error.c_str()');s=s.replace('                               r.sound_error.c_str());', '                               r.sound_error.c_str());\n            ImGui::TextWrapped("Impact sound: %s", r.sound_resource.c_str());');p.write_text(s)
p=Path('tests/train_session_tests.cpp');s=p.read_text()
s=s.replace('    bool check_effects(const GameSession& g, const char* phase, std::string& error)', '''    struct RouteAudit
    {
        std::set<std::uint64_t> absent_blood_sounds;
    };
    bool check_effects(const GameSession& g, RouteAudit& audit, const char* phase, std::string& error)''')
s=s.replace('''                if constexpr (requires { report.sound_error; })
                    error += report.sound_error;''','''                if constexpr (requires { report.sound_error; })
                {
                    if constexpr (requires { report.sound_resource; })
                    {
                        // Original GetRandom(0,NRES(5)) requests this absent retail
                        // file. Preserve refusal, never count it as decoded audio.
                        if (!report.sound_error.empty() &&
                            report.sound_resource == "sounds/gibs/flesh/gib_impact0.wav" &&
                            !g.resources.find(report.sound_resource) &&
                            report.sound_error == "Missing or oversized sound resource")
                            audit.absent_blood_sounds.insert(report.id);
                        else
                            error += report.sound_error;
                    }
                    else
                        error += report.sound_error;
                }''')
s=s.replace('''        std::cout << "TRAIN_EFFECTS\\t" << phase << '\\t' << reports
                  << " retained FX reports; zero effect/client/audio failures\\n";''','''        if (phase)
            std::cout << "TRAIN_EFFECTS\\t" << phase << '\\t' << reports
                      << " retained FX reports; unexpected failures=0; original absent gib_impact0 requests="
                      << audit.absent_blood_sounds.size() << '\\n';''')
s=s.replace('bool trace_services(const GameSession& g, const char* phase, std::string& error)', 'bool trace_services(const GameSession& g, RouteAudit& audit, const char* phase, std::string& error)')
s=s.replace('CHECK(check_effects(g, phase, error));','CHECK(check_effects(g, audit, phase, error));')
s=s.replace('        const double dt = 1. / hz;', '        RouteAudit audit;\n        const double dt = 1. / hz;')
s=s.replace('            CHECK(check_actor(*p, error));','            CHECK(check_effects(g, audit, nullptr, error));\n            CHECK(check_actor(*p, error));')
s=s.replace('trace_services(g, "SubwayA", error)', 'trace_services(g, audit, "SubwayA", error)')
s=s.replace('trace_services(*game, "CityHub", error)', 'trace_services(*game, audit, "CityHub", error)')
s=s.replace('trace_services(*game, "teardown", error)', 'trace_services(*game, audit, "teardown", error)')
s=s.replace('''        for (unsigned tick = 0; tick < hz; ++tick)
            CHECK(game->step(dt));''','''        audit.absent_blood_sounds.clear();
        CHECK(game->run_world(error));
        for (unsigned tick = 0; tick < hz; ++tick)
        {
            CHECK(game->step(dt));
            CHECK(check_effects(*game, audit, nullptr, error));
        }''')
s=s.replace('''                    CHECK(error.empty() && !weapon->sound_failures && !weapon->sound_alert_failures &&
                          !weapon->impact_sound_failures);''','''                    CHECK(error.empty() && !weapon->sound_failures && !weapon->sound_alert_failures &&
                          !weapon->impact_sound_failures);
                    for (const auto& impact : weapon->impacts)
                    {
                        error = impact.effect.error + impact.effect.sprite_error + impact.effect.blood_error +
                                impact.effect.spark_error;
                        CHECK(error.empty());
                    }''')
p.write_text(s)
