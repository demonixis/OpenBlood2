#include "../player_fixture.h"
#include "game/game_settings.h"
#include "game/original/CharacterProfiles.h"
#include "game/original/GameStartPoint.h"
#include "game/original/Projectile.h"
#include <iostream>
#include <random>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << "\n";                                               \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool membership(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        game.server.net_game_type = GAMETYPE_TEAMPLAY;
        auto a = game.server.connect_client(), b = game.server.connect_client(), c = game.server.connect_client();
        CHECK(a && b && c);
        const auto initial = game.server.save_clock();
        CHECK(!game.server.AssignClientTeam(nullptr, 0) && !game.server.AssignClientTeam(a, 3));
        CHECK(game.server.AssignClientTeam(a, 1) && game.server.AssignClientTeam(b, 2));
        CHECK(game.server.save_clock() == initial && !game.server.AssignClientTeam(a, 2));
        // Exact original >50 tie branch, including both adjacent boundary rolls.
        for (int wanted : {50, 51})
        {
            for (unsigned i = 0; i < 10000; ++i)
            {
                std::minstd_rand predicted(game.server.save_clock().generator);
                if (std::uniform_int_distribution<int>(0, 99)(predicted) == wanted)
                    break;
                int value;
                CHECK(game.server.RandomInt(0, 99, value));
            }
            std::minstd_rand predicted(game.server.save_clock().generator);
            CHECK(std::uniform_int_distribution<int>(0, 99)(predicted) == wanted);
            CHECK(game.server.AssignClientTeam(c, 0) && game.server.GetClientTeam(c) == (wanted > 50 ? 2 : 1));
            CHECK(std::minstd_rand(game.server.save_clock().generator)() == predicted());
            game.server.ClearClientTeam(c);
        }
        game.server.ClearClientTeam(b);
        const auto before = game.server.save_clock();
        CHECK(game.server.AssignClientTeam(c, 0) && game.server.GetClientTeam(c) == 2 &&
              game.server.save_clock() == before);
        CHECK((game.server.TeamPlayerCounts() == std::array<unsigned, 2>{1, 1}));
        CHECK(!game.server.AddClientTeamFrags(a, 1)); // No original player yet.
        CHECK(game.server.disconnect_client(a) && game.server.GetClientTeam(a) == 0 &&
              !game.server.AssignClientTeam(a, 1));
        CHECK((game.server.TeamPlayerCounts() == std::array<unsigned, 2>{0, 1}));
        game.server.team_frags = {7, -2};
        CHECK(game.unload_world());
        CHECK((game.server.TeamPlayerCounts() == std::array<unsigned, 2>{0, 0}));
        CHECK((game.server.team_frags == std::array<int, 2>{0, 0}));
        CHECK(game.server.disconnect_client(b) && game.server.disconnect_client(c));
        std::cout << "Checked membership, explicit/least-populated teams, exact 50/51 tie draw, departure and world "
                     "reset passed\n";
        return true;
    }
    bool spawn(const std::filesystem::path& root)
    {
        for (bool fallback : {false, true})
        {
            GameSession game;
            CHECK(test_player::setup(game, root, false));
            game.server.net_game_type = GAMETYPE_TEAMPLAY;
            std::array<int, 3> teams{2, 1, 0};
            if (fallback)
                teams = {2, 2, 2};
            std::array<GameStartPoint*, 3> points{};
            for (unsigned i = 0; i < 3; ++i)
            {
                auto id = game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::World,
                                              {{"Name", std::string("Point") + std::to_string(i)},
                                               {"Pos", DVector{float(i * 200), 100, 0}},
                                               {"TeamID", std::int32_t(teams[i])},
                                               {"Multiplayer", true}});
                CHECK(id);
                points[i] = dynamic_cast<GameStartPoint*>(game.objects.get(id)->game.get());
                CHECK(points[i]);
            }
            auto client = game.server.connect_client();
            auto* player = game.server_shell.OnClientEnterWorld(client, nullptr, 1);
            CHECK(player && player->GetTeamID() == 1);
            for (unsigned i = 0; i < 20; ++i)
            {
                auto* list = game.server.FindNamedObjects("Blood2StartPoint");
                CHECK(list && list->m_nInList == 3);
                std::vector<GameStartPoint*> ordered;
                for (auto* p = list->m_pFirstLink; p; p = p->m_pNext)
                    ordered.push_back(dynamic_cast<GameStartPoint*>(game.server.HandleToObject(p->m_hObject)));
                game.server.RelinquishList(list);
                std::minstd_rand random(game.server.save_clock().generator);
                auto ordinal = unsigned(std::uniform_int_distribution<int>(0, 2)(random));
                unsigned chosen = ordinal;
                if (fallback)
                    chosen = 0;
                else
                    while (ordered[chosen]->GetTeamID() != 0 && ordered[chosen]->GetTeamID() != 1)
                        chosen = (chosen + 1) % 3;
                CHECK(player->GoToStartPoint(nullptr));
                CHECK(player->entry_start_name == game.server.GetStringData(ordered[chosen]->GetName()));
                CHECK(player->entry_start_fallback == fallback &&
                      std::minstd_rand(game.server.save_clock().generator)() == random());
            }
            game.server_shell.OnClientExitWorld(client);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        }
        // Failure after assignment must release membership, so auto balance cannot see a ghost.
        GameSession empty;
        CHECK(test_player::setup(empty, root, false));
        empty.server.net_game_type = GAMETYPE_TEAMPLAY;
        auto client = empty.server.connect_client();
        CHECK(client && !empty.server_shell.OnClientEnterWorld(client, nullptr, 2));
        CHECK(empty.server.GetClientTeam(client) == 0 && empty.server.disconnect_client(client));
        std::cout << "Original random ordinal, cyclic same/neutral-team scan, first-marker fallback and failed-entry "
                     "rollback passed\n";
        return true;
    }
    bool voodoo(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root));
        game.server.net_game_type = GAMETYPE_TEAMPLAY;
        auto a = game.server.connect_client(), b = game.server.connect_client();
        auto* owner = game.server_shell.OnClientEnterWorld(a, nullptr, 1);
        auto* teammate = game.server_shell.OnClientEnterWorld(b, nullptr, 1);
        CHECK(owner && teammate);
        DVector origin{0, 41, 0}, target{0, 41, 100};
        CHECK(game.server.TeleportObject(owner->m_hObject, &origin) == DE_OK);
        CHECK(game.server.TeleportObject(teammate->m_hObject, &target) == DE_OK);
        owner->m_fPitch = owner->m_fYaw = 0;
        game.server.friendly_fire = false;
        CVoodooDoll weapon;
        weapon.Init(owner->m_hObject, &owner->m_InventoryMgr, false);
        weapon.last_shot.damage = 10;
        weapon.last_shot.request.voodoo_hit = 4;
        DVector velocity{1, 0, 0};
        CHECK(game.server.SetVelocity(teammate->m_hObject, &velocity) == DE_OK);
        CHECK(weapon.FireVector() == FireResult::Fired && weapon.deliveries == 0 && weapon.targets.empty());
        DVector after;
        CHECK(game.server.GetVelocity(teammate->m_hObject, &after) == DE_OK && after.x == 1);
        CHECK(!owner->m_damage.accepted_damage && !teammate->m_damage.accepted_damage);
        ObjectCreateStruct state;
        state.m_Flags = FLAG_SOLID;
        state.m_Pos = {0, 41, 50};
        auto wall = game.objects.create(game.objects.find_class("BaseClass"), state);
        DVector dimensions{35, 100, 5};
        CHECK(wall && game.server.SetObjectDims(wall, &dimensions) == DE_OK);
        weapon.last_shot.request.alternate = true;
        CHECK(weapon.FireVector() == FireResult::Fired && weapon.deliveries == 0 && weapon.targets.empty());
        CHECK(!owner->m_damage.accepted_damage && !teammate->m_damage.accepted_damage);
        game.server_shell.OnClientExitWorld(b);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(b));
        CHECK(weapon.FireVector() == FireResult::Fired && weapon.deliveries == 1 &&
              owner->m_damage.accepted_damage == 1);
        game.server_shell.OnClientExitWorld(a);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(a));
        std::cout << "Voodoo protected nearest target has no damage/slow FX; protected alternate target suppresses "
                     "self punishment before visibility, true miss still punishes\n";
        return true;
    }
    bool solo(const std::filesystem::path& root)
    {
        // Team options are server state, but outside team games the original
        // rules never consult them: no team, no protection, Voodoo still hits.
        GameSession game;
        CHECK(test_player::setup(game, root));
        const auto solo_type = game.server.net_game_type;
        CHECK(!network_game_type(solo_type));
        game.server.friendly_fire = false;
        game.server.negative_team_frags = false;
        // Solo admits one client; connect the second under network capacity,
        // then enter both through the solo rules.
        game.server.net_game_type = GAMETYPE_BLOODBATH;
        auto a = game.server.connect_client(), b = game.server.connect_client();
        game.server.net_game_type = solo_type;
        CHECK(a && b);
        auto* owner = game.server_shell.OnClientEnterWorld(a, nullptr, 1);
        auto* other = game.server_shell.OnClientEnterWorld(b, nullptr, 1);
        CHECK(owner && other && owner->GetTeamID() == 0 && other->GetTeamID() == 0);
        CHECK(other->m_damage.IsPlayerToPlayerDamageOk(owner->m_hObject, owner->m_hObject));
        DVector origin{0, 41, 0}, target{0, 41, 100};
        CHECK(game.server.TeleportObject(owner->m_hObject, &origin) == DE_OK);
        CHECK(game.server.TeleportObject(other->m_hObject, &target) == DE_OK);
        owner->m_fPitch = owner->m_fYaw = 0;
        CVoodooDoll weapon;
        weapon.Init(owner->m_hObject, &owner->m_InventoryMgr, false);
        weapon.last_shot.damage = 10;
        weapon.last_shot.request.voodoo_hit = 4;
        // Only the delivery matters here: the headless fixture has no FX
        // service for the follow-up particle publication.
        weapon.FireVector();
        CHECK(weapon.deliveries == 1 && weapon.targets.size() == 1 && weapon.targets[0] == other->m_hObject &&
              other->m_damage.accepted_damage == 1 && !owner->m_damage.accepted_damage);
        game.server_shell.OnClientExitWorld(b);
        game.server_shell.OnClientExitWorld(a);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(a) && game.server.disconnect_client(b));
        std::cout << "Solo ignores team options: no team, player damage allowed, Voodoo delivers\n";
        return true;
    }
    bool retail(const std::filesystem::path& data)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = data;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        WorldRequest request;
        request.filename = "worlds/multi/bb_crypt.dat";
        request.game_type = GAMETYPE_TEAMPLAY;
        CHECK(game.load_request(request, error));
        const auto enter = [&](DBYTE team, DBYTE character) -> CPlayerObj*
        {
            auto client = game.server.connect_client();
            if (!client)
                return nullptr;
            auto* p = game.server_shell.OnClientEnterWorld(client, &error, team);
            PlayerInit init;
            init.name = "Team fixture";
            init.character = character;
            init.skin = 3;
            if (!p || !game.server_shell.OnMessage(client, init))
                return nullptr;
            return p;
        };
        for (DBYTE character = 0; character < 4; ++character)
            for (DBYTE team = 1; team <= 2; ++team)
            {
                auto* p = enter(team, character);
                CHECK(p && p->m_nSkin == team + 4 && p->GetTeamID() == team);
                std::string model, skin;
                CHECK(game.server.GetModelFilenames(p->m_hObject, model, skin));
                CHECK(game.resources.find(skin) && p->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == 50);
                CHECK(p->Respawn() && p->m_nSkin == team + 4 && p->GetTeamID() == team);
                auto client = p->GetClient();
                game.server_shell.OnClientExitWorld(client);
                CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
            }
        auto* blue = enter(1, 0);
        auto* friend_ = enter(1, 1);
        auto* red = enter(2, 2);
        CHECK(blue && friend_ && red);
        game.server.friendly_fire = false;
        auto& damage = blue->m_damage;
        damage.m_fResistance = 1;
        damage.m_fHitPoints = 1000;
        damage.m_fArmorPoints = 100;
        const auto hit = [&](LPBASECLASS sender, HOBJECT owner, DamageType type, bool allowed) -> bool
        {
            DVector position;
            CHECK(game.server.GetObjectPos(blue->m_hObject, &position) == DE_OK);
            const auto hp = damage.m_fHitPoints, armor = damage.m_fArmorPoints;
            const auto accepted = damage.accepted_damage, impulses = damage.damage_impulses;
            auto message = game.server.StartMessageToObject(sender, blue->m_hObject, damage_message);
            CHECK(message && game.server.WriteDamage(message, {{0, 0, 1}, position, 5, type, owner}) == DE_OK &&
                  game.server.EndMessage(message) == DE_OK);
            CHECK(damage.accepted_damage == accepted + unsigned(allowed));
            if (!allowed)
                CHECK(damage.m_fHitPoints == hp && damage.m_fArmorPoints == armor &&
                      damage.damage_impulses == impulses);
            return true;
        };
        auto marker = game.objects.create(game.objects.find_class("GameStartPoint"), {}, Creation::Runtime);
        CHECK(marker);
        auto* nonplayer = game.objects.get(marker)->game.get();
        CHECK(hit(friend_, friend_->m_hObject, DamageType::Normal, false));
        CHECK(hit(friend_, friend_->m_hObject, DamageType::Death, false));
        CHECK(hit(red, red->m_hObject, DamageType::Normal, true));
        CHECK(hit(blue, blue->m_hObject, DamageType::Normal, true));
        CHECK(hit(nonplayer, nonplayer->m_hObject, DamageType::Normal, true));
        // The typed damage boundary refuses forged player/owner pairs. Probe
        // the source priority directly, then deliver from a genuinely configured projectile.
        CHECK(!damage.IsPlayerToPlayerDamageOk(friend_->m_hObject, red->m_hObject));
        CHECK(damage.IsPlayerToPlayerDamageOk(red->m_hObject, friend_->m_hObject));
        CHECK(damage.IsPlayerToPlayerDamageOk(friend_->m_hObject, blue->m_hObject));
        CHECK(damage.IsPlayerToPlayerDamageOk(blue->m_hObject, friend_->m_hObject));
        ObjectCreateStruct projectile_state;
        CHECK(game.server.GetObjectPos(friend_->m_hObject, &projectile_state.m_Pos) == DE_OK);
        auto* projectile =
            dynamic_cast<CProjectile*>(game.server.CreateObject(game.server.GetClass("CGrenade"), &projectile_state));
        CHECK(projectile && projectile->Setup({0, 0, 1}, WEAP_HOWITZER, 5, 0, 0, friend_->m_hObject));
        CHECK(hit(projectile, friend_->m_hObject, DamageType::Explode, false));
        game.server.RemoveObject(projectile->m_hObject);
        CHECK(game.objects.finish_frame());
        game.server.friendly_fire = true;
        CHECK(hit(friend_, friend_->m_hObject, DamageType::Normal, true));
        const auto frag = [&](CPlayerObj* victim, CPlayerObj* killer, int delta) -> bool
        {
            const auto player_score = killer->m_Frags, team_score = game.server.team_frags[killer->GetTeamID() - 1];
            victim->m_damage.last_damage.attacker = killer->m_hObject;
            victim->UpdateFragStatus();
            CHECK(victim->death_error.empty());
            CHECK(killer->m_Frags == player_score + delta &&
                  game.server.team_frags[killer->GetTeamID() - 1] == team_score + delta);
            return true;
        };
        CHECK(frag(red, blue, 1) && frag(friend_, blue, -1));
        game.server.negative_team_frags = false;
        CHECK(frag(friend_, blue, 0));
        game.server.friendly_fire = false;
        game.server.negative_team_frags = true;
        CHECK(frag(friend_, blue, 0));
        CHECK(frag(blue, blue, -1)); // Suicide penalty is independent of FF/negative-team-kill settings.
        CHECK(frag(red, blue, 1) && frag(red, blue, 1));
        // Source PlayerObj confirms slow death before CDestructable's FF gate:
        // a teammate can finish an already dying player; the first killer scores.
        blue->m_damage.m_bDead = true;
        blue->m_damage.m_fHitPoints = 0;
        blue->m_damage.last_damage.attacker = red->m_hObject;
        blue->m_bInSlowDeath = true;
        blue->m_fSlowDeathSafeTimer = 0;
        const auto red_frags = red->m_Frags;
        CHECK(hit(friend_, friend_->m_hObject, DamageType::Normal, false));
        CHECK(blue->m_bDead && blue->humiliation_death && red->m_Frags == red_frags + 1);
        const auto score = game.server.team_frags[0];
        auto blue_client = blue->GetClient();
        game.server_shell.OnClientExitWorld(blue_client);
        CHECK(game.objects.finish_frame() && game.server.disconnect_client(blue_client));
        CHECK(game.server.team_frags[0] == score && (game.server.TeamPlayerCounts() == std::array<unsigned, 2>{1, 1}));
        // A team-score overflow cannot partially commit an individual frag.
        game.server.team_frags[0] = 1000000;
        const auto old = friend_->m_Frags;
        red->m_damage.last_damage.attacker = friend_->m_hObject;
        red->UpdateFragStatus();
        CHECK(!red->death_error.empty() && friend_->m_Frags == old && game.server.team_frags[0] == 1000000);
        for (auto* p : {friend_, red})
        {
            auto client = p->GetClient();
            game.server_shell.OnClientExitWorld(client);
            CHECK(game.objects.finish_frame() && game.server.disconnect_client(client));
        }
        CHECK(game.unload_world() && (game.server.team_frags == std::array<int, 2>{0, 0}));
        std::cout << "Eight retail team profiles/respawns, sender-owner/self damage priorities, no blocked "
                     "armor/impulse, team kills/suicide/totals/departure and atomic overflow passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    if (std::string_view(argv[1]) == "membership")
        return membership(path_from_utf8(argv[2])) ? 0 : 1;
    if (std::string_view(argv[1]) == "spawn")
        return spawn(path_from_utf8(argv[2])) ? 0 : 1;
    if (std::string_view(argv[1]) == "voodoo")
        return voodoo(path_from_utf8(argv[2])) ? 0 : 1;
    if (std::string_view(argv[1]) == "solo")
        return solo(path_from_utf8(argv[2])) ? 0 : 1;
    if (std::string_view(argv[1]) == "retail")
        return retail(path_from_utf8(argv[2])) ? 0 : 1;
    return 2;
}
