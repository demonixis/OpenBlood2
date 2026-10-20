from pathlib import Path
p=Path('tests/train_session_tests.cpp')
s=p.read_text()
needle='    bool trace_services(const GameSession& g, const char* phase, std::string& error)\n'
addition='''    // Inspect failures independently of attempted-service counts. Kept in the
    // route harness so acceptance cannot turn a positive counter into success.
    bool check_inventory(const CInventoryMgr& inventory, std::string& error)
    {
        CHECK(inventory.drop_error.empty());
        for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
            for (bool left : {false, true})
                if (const auto* weapon = inventory.GetWeapon(slot, left))
                {
                    error = weapon->fire_sound_error + weapon->hand_error + weapon->projectile_error +
                            weapon->impact_error;
                    CHECK(error.empty() && !weapon->sound_failures && !weapon->sound_alert_failures &&
                          !weapon->impact_sound_failures);
                }
        return true;
    }
    bool check_actor(const CPlayerObj& actor, std::string& error)
    {
        CHECK(check_inventory(actor.m_InventoryMgr, error));
        error = actor.motion_error + actor.m_Anim_Sound.sound_error;
        CHECK(error.empty() && !actor.motion_failures && !actor.m_Anim_Sound.sound_failures);
        return true;
    }
    bool check_effects(const GameSession& g, const char* phase, std::string& error)
    {
        std::size_t reports = 0;
        auto check = [&](const auto& effects)
        {
            if constexpr (requires { effects.failed; })
                CHECK(effects.failed == 0);
            if constexpr (requires { effects.update_error; })
            {
                error = effects.update_error;
                CHECK(error.empty());
            }
            for (const auto& report : effects.reports)
            {
                ++reports;
                error = report.error;
                if constexpr (requires { report.splat_error; })
                    error += report.splat_error;
                if constexpr (requires { report.sound_error; })
                    error += report.sound_error;
                CHECK(error.empty());
            }
            return true;
        };
        const auto& s = g.server;
        CHECK(check(s.explosion_sprites) && check(s.sparks) && check(s.blood_spurts) && check(s.blood_splats) &&
              check(s.weapon_sprites) && check(s.explosion_models) && check(s.laser_beams) && check(s.object_particles) &&
              check(s.polygrids) && check(s.particle_streams) && check(s.explosion_lights) && check(s.explosion_rings) &&
              check(s.explosion_flames) && check(s.flesh_gibs) && check(s.explosion_fragments));
        for (const auto& report : s.scene_explosions)
        {
            error = report.sound_error + report.fragment_error + report.flame_error + report.ring_error +
                    report.light_error + report.model_error + report.sprite_error;
            CHECK(error.empty());
        }
        for (const auto& voice : s.audio.voices())
        {
            error = voice.error;
            CHECK(error.empty());
        }
        const auto& client = g.client_shell;
        error = client.blocker + client.objectives_error + client.item_audio_error + client.world_exit.error +
                s.audio.music_error + s.audio.output_error;
        CHECK(error.empty() && !client.voice_manager.failures && !client.communication.sound_failures &&
              !client.music.failures);
        std::cout << "TRAIN_EFFECTS\\t" << phase << '\\t' << reports
                  << " retained FX reports; zero effect/client/audio failures\\n";
        return true;
    }
'''
s=s.replace(needle,addition+needle)
s=s.replace('        for (const auto& row : service_coverage(g.server.service_calls))','        CHECK(check_effects(g, phase, error));\n        for (const auto& row : service_coverage(g.server.service_calls))')
s=s.replace('!g.enter_player(error) || !test_camera::complete_subway_intro(g)', '!g.run_world(error) || !test_camera::complete_subway_intro(g)')
s=s.replace('                if (ai && ai->npc_type == AI_Mgr::NPCType::Cultist)\n', '''                if (ai)
                {
                    CHECK(check_inventory(ai->m_InventoryMgr, error));
                    error = ai->ai_key_error + ai->gib_error + ai->death_error + ai->damage_reaction_error;
                    CHECK(error.empty() && !ai->key_failures && !ai->unavailable_keys);
                }
                if (ai && ai->npc_type == AI_Mgr::NPCType::Cultist)
''',1)
s=s.replace('            ammo_pickup |=', '''            CHECK(check_actor(*p, error));
            error = p->entry_error + p->locomotion_error + p->death_error + p->death_effect_error +
                    p->player_trigger_error + p->sound_error + p->smell_error;
            CHECK(error.empty() && !p->unsupported_triggers && !p->sound_failures && !p->voice_delivery_failures &&
                  !p->activation_ping_failures && !p->unavailable_sound_alerts);
            ammo_pickup |=''')
p.write_text(s)
