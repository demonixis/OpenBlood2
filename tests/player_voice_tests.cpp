#include "audio_fixture.h"
#include "camera_fixture.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/WeaponPickups.h"
#include <algorithm>
#include <cmath>
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
    bool mount(GameSession& g, const std::filesystem::path& root)
    {
        test_player::Directory d;
        auto& caleb = d.children["sounds"].children["chosen"].children["caleb"];
        caleb.children["weapon"].files["a.WAV"] = test_audio::wave();
        caleb.children["weapon"].files["b.wav"] = test_audio::wave();
        caleb.children["pain"].files["a.wav"] = test_audio::wave();
        caleb.children["burning"].files["a.wav"] = test_audio::wave();
        caleb.children["jump"].files["a.wav"] = test_audio::wave();
        caleb.children["jump"].files["ignored.txt"] = {1};
        caleb.children["jump"].children["nested"].files["ignored.wav"] = test_audio::wave();
        caleb.children["idle"].files["standard.wav"] = test_audio::wave();
        d.children["caleb"].children["idle"].files["custom.wav"] = test_audio::wave();
        for (int n = 1; n <= 9; ++n)
            if (n != 2)
                caleb.children["unique"].files["msc" + std::to_string(n) + "_cal.wav"] = test_audio::wave();
        for (int n = 0; n < 66; ++n)
            caleb.children["taunt"].files["a" + std::to_string(n) + ".wav"] = test_audio::wave();
        caleb.children["death"].files[std::string(120, 'a') + ".wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "voices.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        CHECK(g.resources.mount(root / "voices.rez", error));
        return true;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_audio::setup(g, root) && mount(g, root));
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    bool send(GameSession& g, const char* command)
    {
        auto text = g.server.CreateString(command);
        auto message = g.server.StartMessageToObject(nullptr, g.client_shell.player, 1000);
        const bool ok =
            message && g.server.WriteToMessageHString(message, text) == DE_OK && g.server.EndMessage(message) == DE_OK;
        g.server.FreeString(text);
        return ok;
    }
    bool groups(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& vm = g.client_shell.voice_manager;
        CHECK(vm.groups[0][0].custom && vm.groups[0][0].sounds == std::vector<std::string>{"caleb/idle/custom.wav"});
        CHECK(vm.groups[0][11].sounds.size() == 1 && !vm.groups[0][11].custom);
        CHECK(vm.groups[0][10].sounds.size() == 64 && vm.groups[0][10].rejected == 2);
        CHECK(vm.groups[0][4].sounds.empty() && vm.groups[0][4].rejected == 1);
        std::mt19937 rng(1), untouched = rng;
        CHECK(vm.groups[0][11].GetRandomSoundString(rng) && rng == untouched && vm.groups[0][11].last == -1);
        std::string previous;
        for (int i = 0; i < 100; ++i)
        {
            const auto* next = vm.groups[0][9].GetRandomSoundString(rng);
            CHECK(next && *next != previous);
            previous = *next;
        }
        const auto last = vm.groups[0][9].last;
        vm.Init(g.resources);
        vm.Reset();
        CHECK(vm.groups[0][9].last == last);
        CHECK(g.unload_world());
        return true;
    }
    bool timing(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& vm = g.client_shell.voice_manager;
        const auto player = g.client_shell.player;
        CHECK(!vm.PlayEventSound(player, 0, 9, false) && vm.reason == "cooldown");
        CHECK(g.objects.advance(.01));
        vm.on = false;
        CHECK(!vm.PlayEventSound(player, 0, 9, false) && vm.reason == "disabled");
        vm.on = true;
        CHECK(!vm.PlayEventSound(player, 0, 9, true) && vm.reason == "external camera");
        CHECK(vm.PlayEventSound(player, 0, 9, false));
        auto id = vm.voice;
        const auto* v = g.server.audio.get(id);
        CHECK(v && v->request.streaming && v->clip->stream && v->request.priority == 5 && !v->request.reverb_requested);
        CHECK(!vm.PlayEventSound(player, 0, 9, true) && vm.reason == "busy");
        CHECK(g.server.advance_audio(.1, {}, {}) && g.server.audio.get(id)->done);
        CHECK(g.objects.advance(vm.next_play_time - g.objects.time()));
        CHECK(!vm.PlayEventSound(player, 0, 9, false) && vm.reason == "cooldown");
        CHECK(g.objects.advance(.01) && vm.PlayEventSound(player, 0, 9, false));
        CHECK(!g.server.audio.get(id));
        id = vm.voice;
        CHECK(!vm.PlaySound(player, "missing.wav", true) && !vm.voice && !g.server.audio.get(id) && !vm.error.empty());
        CHECK(vm.PlayEventSound(player, 0, 11, false));
        CHECK(!g.server.audio.get(vm.voice)->request.streaming && !g.server.audio.get(vm.voice)->clip->stream);
        vm.StopAll();
        CHECK(!vm.PlayUniqueSound(player, 0, 1, false) && !vm.played[0][1] && !vm.error.empty());
        CHECK(vm.PlayUniqueSound(player, 0, 0, false) && vm.played[0][0]);
        vm.on = false;
        CHECK(!vm.PlayUniqueSound(player, 0, 0, false) && vm.reason == "unique already played");
        vm.on = true;
        id = vm.voice;
        CHECK(g.objects.remove(player) && g.objects.finish_frame());
        CHECK(g.server.advance_audio(.01, {}, {}) && !g.server.audio.get(id));
        CHECK(!vm.PlayUniqueSound(player, 0, 2, false) && !vm.played[0][2] && !vm.error.empty());
        CHECK(g.unload_world() && !vm.voice && g.server.audio.voices().empty());
        return true;
    }
    bool messages(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CHECK(g.objects.advance(.01));
        auto& vm = g.client_shell.voice_manager;
        auto* p = g.server_shell.Player(g.local_client);
        std::string error;
        CHECK(g.server.SendToClient(g.local_client, VoiceEvent{true, 4, 9}) == DE_INVALIDPARAMS);
        CHECK(g.server.SendToClient(g.local_client, VoiceEvent{false, 0, 9}) == DE_INVALIDPARAMS);
        CHECK(p->PlayVoiceUniqueEventOnClient(0) && g.pump_player_messages(error) && vm.voice);
        const auto first = vm.voice;
        const auto strings = g.server.string_count();
        CHECK(send(g, "sOuNd cue.wav") && p->trigger_sound_starts == 1 && p->trigger_voice_stops == 1);
        const auto local = p->trigger_sound;
        const auto* v = g.server.audio.get(local);
        CHECK(v && v->auto_remove && v->world_owned && v->request.priority == 6 && !v->request.streaming &&
              !v->request.reverb_requested);
        CHECK(g.pump_player_messages(error) && !vm.voice && !g.server.audio.get(first) && g.server.audio.get(local));
        CHECK(send(g, "sound  cue.wav") && p->trigger_sound_resource == " cue.wav" && !p->trigger_sound &&
              !p->trigger_sound_error.empty());
        CHECK(g.pump_player_messages(error));
        const auto requests = p->trigger_sound_requests;
        CHECK(send(g, "sound") && send(g, "sound ") && p->trigger_sound_requests == requests);
        CHECK(send(g, "sound\tcue.wav") && p->ignored_triggers == 1);
        g.server.set_game_settings(1, 1);
        CHECK(send(g, "sound cue.wav") && p->trigger_sound_requests == requests);
        CHECK(!p->PlayVoiceUniqueEventOnClient(0) && !p->PlayVoiceGroupEventOnClient(9, true));
        g.server.set_game_settings(1, GAMETYPE_SINGLE);
        p->m_bDead = true;
        CHECK(send(g, "sound cue.wav") && p->trigger_sound_requests == requests + 1);
        p->m_bDead = false;
        CHECK(g.pump_player_messages(error) && g.server.string_count() == strings);
        CHECK(vm.PlayEventSound(p->m_hObject, 0, 9, false));
        const auto interrupted = vm.voice;
        CHECK(g.server.SendToClient(g.local_client, Conversation{"cue.wav", "short", 0}) == DE_OK);
        CHECK(g.pump_player_messages(error) && !vm.voice && !g.server.audio.get(interrupted));
        CHECK(g.objects.advance(.25));
        g.client_shell.communication.Update(g.server, p->m_hObject, vm);
        CHECK(g.client_shell.communication.voice && vm.next_play_time > g.server.GetTime());
        CHECK(!vm.PlayEventSound(p->m_hObject, 0, 9, false) && vm.reason == "cooldown");
        // A second conversation is busy, but still stops VoiceMgr before that refusal.
        CHECK(vm.PlaySound(p->m_hObject, "long.wav", true));
        const auto busy_voice = vm.voice;
        CHECK(g.server.SendToClient(g.local_client, Conversation{"cue.wav", "second", 0}) == DE_OK);
        CHECK(g.pump_player_messages(error) && g.client_shell.communication.busy == 1 &&
              !g.server.audio.get(busy_voice));
        CHECK(g.server.SendToClient(g.local_client, VoiceEvent{true, 0, 9}) == DE_OK);
        const auto client = g.local_client;
        CHECK(g.leave_player() && !vm.voice && g.server.SendToClient(client, VoiceStop{}) == DE_INVALIDPARAMS);
        CHECK(g.enter_player(error) && !vm.voice && !vm.played[0][0]);
        CHECK(g.unload_world());
        return true;
    }
    bool pickup(const std::filesystem::path& root)
    {
        unsigned branches = 0, replies = 0;
        // Scan the seeded stream until all three pickup outcomes are covered.
        // A fixed short prefix need not contain two successive successful chance draws.
        unsigned silent = 0;
        for (int offset = 0; offset < 4096 && !(silent && replies && branches > replies); ++offset)
        {
            GameSession g;
            CHECK(setup(g, root));
            CHECK(g.objects.advance(.01));
            int draw;
            for (int i = 0; i < offset; ++i)
                CHECK(g.server.RandomInt(0, 99, draw));
            auto* p = g.server_shell.Player(g.local_client);
            const auto pickup = SpawnWeaponPickup(WEAP_BERETTA, p->m_hObject, {400, 100, 400}, {}, false);
            CHECK(pickup && g.objects.engine_message(pickup, MID_TOUCHNOTIFY, p->m_hObject));
            CHECK(!g.server.IsObjectAlive(pickup) && p->m_InventoryMgr.CurrentWeapon(true));
            branches += p->weapon_pickup_voice_requests;
            replies += p->voice_messages;
            silent += p->weapon_pickup_voice_requests == 0;
            CHECK(p->weapon_pickup_voice_requests <= 1 && p->voice_messages <= p->weapon_pickup_voice_requests);
            CHECK(!p->voice_delivery_failures);
            std::string error;
            CHECK(g.pump_player_messages(error));
            CHECK(g.client_shell.voice_manager.starts == p->voice_messages);
            // Explicit original mapped helper: unique precedes optional group, whose busy gate preserves the unique.
            g.client_shell.voice_manager.StopAll();
            p->PlayWeaponVoiceSoundOnClient(9);
            CHECK(g.pump_player_messages(error) && g.client_shell.voice_manager.played[0][5]);
            CHECK(g.client_shell.voice_manager.last_resource == "sounds/chosen/caleb/unique/msc6_cal.wav");
            CHECK(g.unload_world());
        }
        std::cout << "accepted pickup branches=" << branches << " group replies=" << replies << '\n';
        CHECK(silent && branches && replies && replies < branches);
        return true;
    }
    bool damage(GameSession& g, HOBJECT owner, float amount, DamageType type, HOBJECT attacker)
    {
        DVector position;
        CHECK(g.server.GetObjectPos(owner, &position) == DE_OK);
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(attacker), owner, damage_message);
        CHECK(m && g.server.WriteDamage(m, {{0, 0, 1}, position, amount, type, attacker, true}) == DE_OK &&
              g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool damage_voices(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root) && g.objects.advance(.01));
        auto* p = g.server_shell.Player(g.local_client);
        const auto h = p->m_hObject;
        auto& vm = g.client_shell.voice_manager;
        std::string error;
        CHECK(CPlayerObj::LocalVoicePlayer() == p && p->m_nLastHitPoints == 100 && !p->damage_voice_checks);
        p->m_damage.m_fResistance = 1;
        CHECK(damage(g, h, 15, DamageType::Normal, h) && damage(g, h, 16, DamageType::Normal, h));
        CHECK(p->UpdateStats() && g.pump_player_messages(error));
        CHECK(p->m_nLastHitPoints == 69 && p->m_nLastDamage == 31 && p->damage_voice_checks == 1 &&
              p->damage_voice_requests == 1 && p->damage_voice_event == 2 && vm.starts == 1);
        CHECK(vm.last_resource == "sounds/chosen/caleb/pain/a.wav");
        CHECK(g.server.advance_audio(.01, {}, {}) && g.server.audio.get(vm.voice)->cursor > 0);
        CHECK(p->UpdateStats(true) && g.pump_player_messages(error) && p->damage_voice_checks == 1);
        // Same-frame healing back to the previous integer health cancels the net-loss branch.
        CHECK(damage(g, h, 1, DamageType::Normal, h));
        CHECK(p->m_damage.Heal(1) && p->UpdateStats() && p->damage_voice_checks == 1);
        p->m_damage.SetHitPoints(69.9f);
        CHECK(p->UpdateStats());
        CHECK(damage(g, h, .4f, DamageType::Fire, h) && p->UpdateStats() && p->damage_voice_checks == 1);
        CHECK(damage(g, h, 31, DamageType::Fire, h) && p->UpdateStats() && g.pump_player_messages(error));
        CHECK(p->m_nLastDamage == 31 && p->damage_voice_event == 5 && vm.starts == 1 && vm.reason == "busy");
        vm.StopAll();
        p->m_damage.Heal(100);
        CHECK(p->UpdateStats());
        CHECK(damage(g, h, 31, DamageType::Fire, h) && p->UpdateStats() && g.pump_player_messages(error));
        CHECK(vm.starts == 2 && vm.last_resource == "sounds/chosen/caleb/burning/a.wav");
        vm.StopAll();
        g.server.set_game_settings(1, 1);
        const auto messages = p->voice_messages, checks = p->damage_voice_checks;
        CHECK(damage(g, h, 31, DamageType::Normal, h) && p->UpdateStats() && g.pump_player_messages(error));
        CHECK(p->damage_voice_checks == checks + 1 && p->voice_messages == messages && !vm.voice);
        g.server.set_game_settings(0, 1);
        PlayerState saved;
        CHECK(p->SaveFields(saved, error));
        CHECK(g.server.SetClientUserData(g.local_client, nullptr) == DE_OK);
        p->SetClient(nullptr);
        CHECK(!CPlayerObj::LocalVoicePlayer() && p->LoadFields(saved, error));
        p->SetClient(g.local_client);
        CHECK(g.server.SetClientUserData(g.local_client, h) == DE_OK);
        CHECK(!p->m_nLastHitPoints && !p->m_nLastDamage && p->UpdateStats());
        CHECK(p->damage_voice_checks == checks + 1); // Restore never invents damage feedback.
        CHECK(g.objects.remove(h) && !CPlayerObj::LocalVoicePlayer() && g.objects.finish_frame());
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool combat_private(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "diagnostic-combat-voices.dat";
        g.state = SessionState::Inspection;
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{-500, 100, 0}}}));
        CHECK(g.enter_player(error) && g.objects.advance(.01));
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(CPlayerObj::LocalVoicePlayer() == player);
        auto other = g.objects.create(g.objects.find_class("BaseClass"), {});
        const auto keep = g.objects.snapshot();
        auto& vm = g.client_shell.voice_manager;
        unsigned kills = 0, gibs = 0, corpses = 0, both = 0, scripted = 0, no_hit_node = 0;
        for (int mode = 0; mode < 4; ++mode)
            for (int trial = 0; trial < (mode == 3 ? 128 : 64); ++trial)
            {
                vm.StopAll();
                const auto starts = vm.starts, requests = vm.requests;
                auto h = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                          {{"Pos", DVector{0, 100, 0}},
                                           {"AIState", std::string("IDLE")},
                                           {"AIWeapon1", std::string("BERETTA")}});
                auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
                CHECK(a && a->RuntimeReady() && a->m_MoveObj.Update());
                if (mode != 1)
                    a->m_dwFlags |= 0x800u; // Isolate ordinary death from the source gib branch.
                a->m_damage.SetArmorPoints(0);
                const bool by_player = mode != 2 && (mode == 3 ? trial < 64 : trial % 2 == 0);
                if (mode == 3)
                {
                    // Isolate instant-death attribution from random head localization.
                    a->m_damage.ai_hit_localization = false;
                    a->m_damage.node_hit = 2;
                    CHECK(damage(g, h, 1, DamageType::Normal, by_player ? other : player->m_hObject));
                    CHECK(!a->m_damage.IsDead());
                    a->m_damage.ai_hit_localization = true; // Restore ordinary accepted-damage dispatch.
                }
                CHECK(damage(g, h, 500,
                             mode == 1   ? DamageType::Explode
                             : mode == 3 ? DamageType::Death
                                         : DamageType::Normal,
                             by_player ? player->m_hObject : other));
                CHECK(a->m_damage.IsDead() && !a->runtime_failed);
                if (a->kill_voice_roll < 0)
                {
                    CHECK(a->m_damage.node_hit == -1 && mode != 3); // Original HandleDamage early return.
                    ++no_hit_node;
                }
                CHECK(a->kill_voice_requests ==
                      unsigned(by_player && a->kill_voice_roll >= 0 && a->kill_voice_roll < 6));
                CHECK(a->big_gib_voice_requests == unsigned(mode == 1 && by_player && a->big_gib_voice_roll < 7));
                auto expected = a->kill_voice_requests + a->big_gib_voice_requests;
                CHECK(a->combat_voice_deliveries == expected);
                kills += a->kill_voice_requests;
                if (mode == 3)
                {
                    scripted += a->kill_voice_requests;
                    CHECK(a->m_damage.GetLastDamager() == (by_player ? other : player->m_hObject));
                    CHECK(a->m_damage.last_damage.attacker == (by_player ? player->m_hObject : other));
                }
                gibs += a->big_gib_voice_requests;
                both += expected == 2;
                if (mode == 2)
                {
                    CHECK(a->AI_Update() && a->AI_Update() && a->last_corpse);
                    auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(a->last_corpse));
                    CHECK(c);
                    CHECK(damage(g, c->m_hObject, 500, DamageType::Explode, other));
                    CHECK(c->dead && c->big_gib_voice_roll >= 0);
                    expected = unsigned(c->big_gib_voice_roll < 7);
                    CHECK(c->big_gib_voice_requests == expected && c->combat_voice_deliveries == expected);
                    corpses += expected; // No player-killer requirement for the original corpse reply.
                }
                const bool gib_reply = mode == 2 || a->big_gib_voice_requests;
                CHECK(g.objects.remove(h) &&
                      g.objects.finish_frame()); // Message owns its values after actor destruction.
                CHECK(g.pump_player_messages(error) && vm.requests == requests + expected);
                CHECK(vm.starts ==
                      starts + unsigned(expected != 0)); // Big-gib precedes kill; busy suppresses the latter.
                if (expected)
                {
                    CHECK(g.server.advance_audio(.01, {}, {}));
                    const auto* voice = g.server.audio.get(vm.voice);
                    CHECK(voice && voice->cursor > 0 && voice->request.streaming && voice->clip->stream);
                    CHECK(vm.last_resource.find(gib_reply ? "/biggib/" : "/kill/") != std::string::npos);
                }
                for (const auto handle : g.objects.snapshot())
                    if (std::find(keep.begin(), keep.end(), handle) == keep.end())
                        CHECK(g.objects.remove(g.objects.get(handle)->handle));
                CHECK(g.objects.finish_frame());
                g.server.flesh_gibs.clear();
                g.server.blood_spurts.clear();
                g.server.blood_splats.clear();
            }
        CHECK(kills && gibs && corpses && scripted);
        std::cout << "Isolated original deaths: kill replies=" << kills << " gib replies=" << gibs
                  << " corpse replies=" << corpses << " scripted kills=" << scripted << " simultaneous=" << both
                  << " missing-node early returns=" << no_hit_node << '\n';
        CHECK(g.unload_world() && !CPlayerObj::LocalVoicePlayer() && g.server.audio.voices().empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        auto game = std::make_unique<GameSession>();
        auto& g = *game;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error) && g.request_world("01_SubwayA", error) &&
              g.enter_player(error));
        CHECK(test_camera::complete_subway_intro(g));
        auto& vm = g.client_shell.voice_manager;
        unsigned groups = 0, files = 0;
        for (const auto& character : vm.groups)
            for (const auto& group : character)
            {
                groups += !group.sounds.empty();
                files += group.sounds.size();
            }
        std::cout << "retail groups=" << groups << " WAV entries=" << files << '\n';
        for (int unique = 0; unique < 9; ++unique)
        {
            vm.StopAll();
            const bool started = vm.PlayUniqueSound(g.client_shell.player, 0, unique, false);
            if (!started)
                std::cerr << "unique " << unique << ": " << vm.error << " / " << vm.reason << '\n';
            if (unique == 1 || unique == 8)
            {
                CHECK(!started && vm.error == "Missing streaming sound resource" && !vm.played[0][unique]);
                CHECK(!g.resources.index().contains(vm.last_resource));
                continue; // Two confirmed absent authored references, never marked played.
            }
            CHECK(started);
            CHECK(g.server.advance_audio(.01, {}, {}));
            const auto* v = g.server.audio.get(vm.voice);
            CHECK(v && v->cursor > 0 && v->clip->stream && v->request.streaming);
        }
        vm.StopAll();
        CHECK(vm.PlayEventSound(g.client_shell.player, 0, 9, false));
        const auto voice = vm.voice;
        const auto last = vm.groups[0][9].last;
        WorldMessage message;
        CHECK(original_world_request("missing", 0, LOADTYPE_NEW_LEVEL, 0, message) &&
              g.queue_level_change(message, "", error));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Rejected);
        CHECK(game.get() == &g && vm.voice == voice && g.server.audio.get(voice) && vm.played[0][0]);
        CHECK(original_world_request("02_CityHub", 0, LOADTYPE_NEW_LEVEL, 0, message) &&
              g.queue_level_change(message, "01_egressa", error));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
        auto& next = game->client_shell.voice_manager;
        CHECK(!next.voice && !game->server.audio.get(voice) && next.groups[0][9].last == last &&
              next.next_play_time == 0);
        for (int unique = 0; unique < 9; ++unique)
            CHECK(next.played[0][unique] == (unique != 1 && unique != 8));
        CHECK(game->objects.advance(.01));
        CHECK(!next.PlayUniqueSound(game->client_shell.player, 0, 0, false) && next.reason == "unique already played");
        CHECK(next.PlayEventSound(game->client_shell.player, 0, 9, false) && next.groups[0][9].last != last);
        CHECK(send(*game, "SOUND sounds/chosen/caleb/unique/msc1_cal.wav") && game->pump_player_messages(error));
        const auto* p = game->server_shell.Player(game->local_client);
        CHECK(!next.voice && p->trigger_sound_starts == 1 && game->server.audio.get(p->trigger_sound));
        CHECK(game->unload_world() && game->server.audio.voices().empty() && !game->server.client_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    const std::filesystem::path root = argv[2];
    const bool ok = mode == "groups"           ? groups(root)
                    : mode == "timing"         ? timing(root)
                    : mode == "messages"       ? messages(root)
                    : mode == "pickup"         ? pickup(root)
                    : mode == "damage"         ? damage_voices(root)
                    : mode == "combat_private" ? combat_private(root)
                                               : mode == "private" && private_data(root);
    return ok ? 0 : 1;
}
