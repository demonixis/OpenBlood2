#include "game/game_settings.h"
#include "game/original/FlagObjects.h"
#include "server/session.h"
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << ": " << error << '\n';                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Fixture
    {
        ServerSession server;
        std::string error;
        GameSession& game()
        {
            return const_cast<GameSession&>(server.game());
        }
        bool start(const char* data, const char* map = "multi/ctf_cabana", MultiplayerRules rules = {})
        {
            Config config;
            config.data_dir = path_from_utf8(data);
            return server.start(config, map, GAMETYPE_CTF, error, rules);
        }
        CPlayerObj* enter(DBYTE team, Joined& joined, DBYTE character = 0)
        {
            JoinProfile profile;
            profile.team = team;
            profile.character = character;
            profile.name = "CTF " + std::to_string(character);
            return server.join(profile, joined, error)
                       ? dynamic_cast<CPlayerObj*>(game().objects.get(joined.object)->game.get())
                       : nullptr;
        }
        FlagStand* stand(int team)
        {
            for (auto id : game().objects.snapshot())
                if (auto* s = dynamic_cast<FlagStand*>(game().objects.get(id)->game.get()); s && s->team == team)
                    return s;
            return nullptr;
        }
        bool drain(Joined player, std::vector<PrivateEvent>& out)
        {
            return server.take_private_events(player.player, out, error);
        }
    };
    bool profiles(const char* data)
    {
        std::string error;
        const char* maps[] = {"multi/ctf_cabana", "multi/ctf_deadcities", "multi/ctf_dmx", "multi/ctf_jam"};
        for (unsigned i = 0; i < 4; ++i)
        {
            Fixture f;
            CHECK(f.start(data, maps[i]));
            Joined a, b;
            auto* blue = f.enter(1, a, DBYTE(i));
            auto* red = f.enter(2, b, DBYTE(i));
            error = f.error;
            CHECK(blue && red && blue->m_nSkin == 5 && red->m_nSkin == 6 && blue->GetTeamID() == 1 &&
                  red->GetTeamID() == 2);
            CHECK(g_ctf.flags[0] && g_ctf.flags[1] && f.stand(1) && f.stand(2));
            for (auto* p : {blue, red})
            {
                CHECK(p->m_Frags == 0 && p->ctf_score == 0 && p->Respawn() && p->m_nSkin == p->GetTeamID() + 4);
                CHECK(p->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == 50);
            }
            CHECK(f.server.clear_players());
            std::cout << maps[i] << ": two CTF profiles, teams/skins/respawn and external flags passed\n";
        }
        return true;
    }
    bool capture(const char* data)
    {
        std::string error;
        Fixture f;
        MultiplayerRules rules;
        rules.use_team_size = true;
        rules.flag_value = 7;
        rules.frags = 100;
        CHECK(f.start(data, "multi/ctf_cabana", rules));
        Joined a, b, c;
        auto* blue = f.enter(1, a);
        auto* red = f.enter(2, b, 1);
        auto* other = f.enter(2, c, 2);
        CHECK(blue && red && other);
        auto& s = f.game().server;
        auto* flag = g_ctf.flags[1];
        auto* own = g_ctf.flags[0];
        auto* base = f.stand(1);
        CHECK(flag && own && base);
        // TeamMgr counts membership, including inactive or dead members.
        s.SetObjectState(other->m_hObject, OBJSTATE_INACTIVE);
        other->m_damage.m_bDead = true;
        blue->GrabFlag(flag->m_hObject);
        CHECK(blue->m_hFlagAttach && flag->with_player);
        own->GiveToPlayer(nullptr);
        blue->GiveFlag(base->m_hObject);
        CHECK(!blue->m_Frags && blue->m_hFlagAttach);
        CHECK(own->ReturnToFlagStand());
        // Reject a valid configured value whose member multiplier exceeds the wire bound.
        g_ctf.flag_value = 1000000;
        const auto messages = blue->ctf_messages, returns = flag->returns;
        blue->GiveFlag(base->m_hObject);
        CHECK(!blue->ctf_error.empty() && !blue->m_Frags && !s.team_frags[0] && !blue->ctf_score &&
              !g_ctf.team_scores[0]);
        CHECK(blue->m_hFlagAttach && blue->ctf_messages == messages && flag->returns == returns);
        g_ctf.flag_value = 7;
        blue->ctf_error.clear();
        CHECK(f.server.begin_audio(b.player, error));
        const auto clock = s.save_clock();
        std::vector<PrivateEvent> events;
        CHECK(f.drain(a, events) && f.drain(b, events) && f.drain(c, events));
        base->EngineMessageFn(MID_TOUCHNOTIFY, blue->m_hObject, 0);
        CHECK(blue->ctf_error.empty() && blue->m_Frags == 14 && s.team_frags[0] == 14 && blue->ctf_score == 14 &&
              g_ctf.team_scores[0] == 14);
        CHECK(!blue->m_hFlagObject && !blue->m_hFlagAttach && flag->in_stand);
        CHECK(s.save_clock() == clock); // Capture, notification and logical sounds consume no server RNG.
        for (auto joined : {a, b, c})
        {
            CHECK(f.drain(joined, events));
            unsigned captures = 0;
            for (const auto& e : events)
                if (const auto* v = std::get_if<FlagEvent>(&e.value); v && v->kind == CTFEventKind::Captured)
                {
                    CHECK(v->points == 14 && v->capturer == a.object);
                    ++captures;
                }
            CHECK(captures == 1);
        }
        std::vector<AudioEvent> sounds;
        CHECK(f.server.take_audio_events(b.player, sounds, error));
        CHECK(std::any_of(sounds.begin(), sounds.end(),
                          [](const auto& e)
                          {
                              const auto* v = std::get_if<SoundStart>(&e.value);
                              std::string path;
                              return v && resource_path(v->parameters.resource, path) &&
                                     path == "sounds_multipatch/ctfflagcapture.wav" && v->parameters.priority == 1 &&
                                     v->parameters.outer_radius == 2000;
                          }));
        CHECK(f.server.leave(c.player) && f.server.leave(b.player));
        blue->GrabFlag(flag->m_hObject);
        blue->GiveFlag(base->m_hObject);
        CHECK(blue->m_Frags == 21 && s.team_frags[0] == 21); // Source minimum one opponent.
        const auto total = s.team_frags[0];
        CHECK(f.server.leave(a.player) && s.team_frags[0] == total);
        CHECK(f.server.join({}, a, error)); // Automatic assignment still produces a fresh zero-score member.
        const auto* fresh = dynamic_cast<CPlayerObj*>(f.game().objects.get(a.object)->game.get());
        CHECK(fresh && fresh->m_Frags == 0 && s.team_frags[0] == total);
        CHECK(f.server.clear_players());
        std::cout << "CTF captures multiply inactive/dead membership, notify all clients, retain departed totals and "
                     "play logical capture audio\n";
        return true;
    }
    bool scoring(const char* data)
    {
        std::string error;
        Fixture f;
        MultiplayerRules rules;
        rules.only_flag_scores = true;
        rules.frags = 100;
        CHECK(f.start(data, "multi/ctf_cabana", rules));
        Joined a, b, c;
        auto* blue = f.enter(1, a);
        auto* friend_ = f.enter(1, b, 1);
        auto* red = f.enter(2, c, 2);
        CHECK(blue && friend_ && red);
        auto& s = f.game().server;
        const auto frag = [&](CPlayerObj* victim, CPlayerObj* killer, int delta)
        {
            const auto score = killer->m_Frags, total = s.team_frags[killer->GetTeamID() - 1];
            victim->m_damage.last_damage.attacker = killer->m_hObject;
            victim->UpdateFragStatus();
            CHECK(victim->death_error.empty() && killer->m_Frags == score + delta &&
                  s.team_frags[killer->GetTeamID() - 1] == total + delta);
            return true;
        };
        CHECK(frag(red, blue, 0) && frag(friend_, blue, -1) && frag(blue, blue, -1));
        s.negative_team_frags = false;
        CHECK(frag(friend_, blue, 0) && frag(blue, blue, -1));
        s.negative_team_frags = true;
        s.friendly_fire = false;
        CHECK(frag(friend_, blue, 0));
        g_ctf.only_flag_scores = false;
        CHECK(frag(red, blue, 1));
        auto* flag = g_ctf.flags[1];
        blue->GrabFlag(flag->m_hObject);
        blue->GiveFlag(f.stand(1)->m_hObject);
        CHECK(blue->m_Frags == 3 && s.team_frags[0] == 3 && blue->ctf_score == 5);
        // Refusing a score overflow must retain every counter, attachment and publication count.
        blue->GrabFlag(flag->m_hObject);
        CHECK(blue->m_hFlagAttach);
        const auto original = blue->m_Frags, diagnostic = blue->ctf_score;
        const auto messages = blue->ctf_messages, returns = flag->returns;
        for (bool player_bound : {false, true})
        {
            blue->m_Frags = player_bound ? 1000000 : original;
            s.team_frags[0] = player_bound ? 3 : 1000000;
            blue->ctf_error.clear();
            blue->GiveFlag(f.stand(1)->m_hObject);
            CHECK(!blue->ctf_error.empty() && blue->m_Frags == (player_bound ? 1000000 : original) &&
                  s.team_frags[0] == (player_bound ? 3 : 1000000));
            CHECK(blue->ctf_score == diagnostic && blue->ctf_messages == messages && flag->returns == returns &&
                  blue->m_hFlagAttach);
        }
        blue->m_Frags = original;
        s.team_frags[0] = 3;
        blue->ctf_error.clear();
        g_ctf.flag_value = 0;
        blue->GiveFlag(f.stand(1)->m_hObject);
        CHECK(blue->ctf_error.empty() && blue->m_Frags == original && !blue->m_hFlagAttach &&
              flag->returns == returns + 1);
        CHECK(f.server.clear_players());
        std::cout << "OnlyFlagScores enemy suppression, retained penalties, combined capture/kill score, zero-value "
                     "capture and atomic score bounds passed\n";
        return true;
    }
    bool lifetime(const char* data)
    {
        std::string error;
        Fixture f;
        MultiplayerRules rules;
        rules.frags = 5;
        rules.only_flag_scores = true;
        rules.use_team_size = true;
        rules.following_maps = {"multi/ctf_jam"};
        CHECK(f.start(data, "multi/ctf_cabana", rules));
        Joined a, b;
        auto* blue = f.enter(1, a);
        auto* red = f.enter(2, b, 1);
        CHECK(blue && red);
        auto* flag = g_ctf.flags[1];
        blue->GrabFlag(flag->m_hObject);
        CHECK(blue->m_hFlagAttach);
        blue->m_damage.m_bDead = true;
        blue->m_damage.last_damage.attacker = nullptr;
        blue->HandleDeath();
        CHECK(flag->on_ground && !blue->m_hFlagAttach && !blue->m_bDead); // Source drop precedes null-killer guard.
        blue->m_damage.m_bDead = false;
        blue->death_error.clear();
        blue->GrabFlag(flag->m_hObject);
        CHECK(blue->m_hFlagAttach);
        CHECK(f.server.leave(a.player) && flag->on_ground && !flag->carrier &&
              !f.game().objects.get(flag->m_hObject)->attachment);
        const auto flag_id = f.game().objects.get(flag->m_hObject)->id;
        CHECK(f.server.leave(b.player));
        // Original scheduled return remains effective without any connected client.
        for (unsigned i = 0; i < 1802; ++i)
            CHECK(f.server.tick(error));
        CHECK(flag->in_stand && flag->returns == 1 && f.game().objects.get(flag_id));
        blue = f.enter(1, a);
        CHECK(blue);
        blue->GrabFlag(flag->m_hObject);
        blue->GiveFlag(f.stand(1)->m_hObject);
        CHECK(blue->m_Frags == 5 && f.server.tick(error) && f.server.changing_level());
        CHECK(f.server.clear_players());
        while (!f.server.rotation_due())
            CHECK(f.server.tick(error));
        CHECK(f.server.rotate(error) && f.server.game().world_path == "worlds/multi/ctf_jam.dat");
        CHECK((f.game().server.team_frags == std::array<int, 2>{}) && (g_ctf.team_scores == std::array<int, 2>{}));
        CHECK(g_ctf.only_flag_scores && g_ctf.use_team_size && g_ctf.flag_value == 5 && g_ctf.last_need_flag == 0);
        CHECK(g_ctf.flags[0] && g_ctf.flags[1] && g_ctf.flags[0]->in_stand && g_ctf.flags[1]->in_stand);
        std::cout << "Death/disconnect drops, zero-client timed return, capture limit and CTF "
                     "rotation/reset/configuration passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    if (mode == "profiles")
        return profiles(argv[2]) ? 0 : 1;
    if (mode == "capture")
        return capture(argv[2]) ? 0 : 1;
    if (mode == "scoring")
        return scoring(argv[2]) ? 0 : 1;
    if (mode == "lifetime")
        return lifetime(argv[2]) ? 0 : 1;
    return 2;
}
