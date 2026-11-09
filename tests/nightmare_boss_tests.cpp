#include "engine/collision.h"
#include "game/original/Nightmare.h"
#include "game/original/Spawner.h"
#include "game/original/Trigger.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << '\n';                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool mount(GameSession& game, const char* path, bool addon)
    {
        Config config;
        config.data_dir = path;
        config.nightmares = addon;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        return true;
    }

    bool floor(GameSession& game)
    {
        WorldModel model;
        model.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        model.surfaces.resize(1);
        model.surfaces[0].flags = 1;
        model.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(game.collision.build(model, error));
        game.server.collision_ready = true;
        return true;
    }

    template <class T> T* named(GameSession& game, std::string_view name)
    {
        for (auto id : game.objects.snapshot())
            if (auto* record = game.objects.get(id); record && record->life == Life::Alive && record->state.m_Name == name)
                if (auto* object = dynamic_cast<T*>(record->game.get()))
                    return object;
        return nullptr;
    }

    void diagnostics(GameSession& game)
    {
        for (const auto& message : game.objects.diagnostics())
            std::cerr << message << '\n';
    }

    bool attack(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path, true) && floor(game));
        game.server.set_game_settings(1, 0);
        auto boss_handle = game.objects.create(game.objects.find_class("Nightmare"), {}, Creation::World,
                                               {{"Pos", DVector{0, 300, 0}}});
        auto target_handle = game.objects.create(game.objects.find_class("GremlinAI"), {}, Creation::World,
                                                 {{"Pos", DVector{0, 50, 550}}, {"Name", std::string("target")}});
        auto* boss = dynamic_cast<Nightmare*>(game.server.HandleToObject(boss_handle));
        if (!boss || !boss->RuntimeReady())
            diagnostics(game);
        CHECK(boss && boss->RuntimeReady() && target_handle && boss->m_MoveObj.Update());
        CHECK(boss->m_damage.m_fHitPoints >= 30001 && boss->m_damage.m_fHitPoints <= 30400 &&
              boss->m_fAIMass == 3750 && boss->m_dwFlags == 0xc02 && !boss->m_damage.apply_damage_physics);
        CHECK(boss->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_NIGHTMARE_BITE);
        CHECK(boss->CanSee(target_handle) && boss->sight.outcome == AISightOutcome::Always && !boss->sight.rays);
        CHECK(boss->SetTarget("target") && boss->SetNewState(AI_Mgr::StateAttackClose));
        boss->Metacmd = 2;
        boss->m_vTargetPos = vector_add(boss->m_MoveObj.GetPos(), {0, 0, 500});
        CHECK(boss->AI_STATE_AttackClose() && boss->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_BEHEMOTH_SHOCKWAVE);
        CHECK(boss->SetNewState(AI_Mgr::StateAttackClose));
        boss->Metacmd = 3;
        boss->m_fStimuli[0] = 150;
        CHECK(boss->AI_STATE_AttackClose() && boss->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_NIGHTMARE_BITE);
        CHECK(boss->SetNewState(AI_Mgr::StateAttackFar) && boss->MC_Fire_Stand());
        CHECK(boss->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_NIGHTMARE_FIREBALLS);
        game.server.SetObjectState(boss_handle, OBJSTATE_INACTIVE);
        game.server.SetObjectState(target_handle, OBJSTATE_INACTIVE);
        CHECK(game.server.GetObjectPos(target_handle, &boss->m_vTargetPos) == DE_OK);
        if (!boss->Fire())
            std::cerr << boss->fire_error << '\n';
        CHECK(boss->fire_result == FireResult::Fired && boss->fire_gun_node);
        const auto projectile = boss->m_InventoryMgr.CurrentWeapon()->projectile;
        auto* ball = dynamic_cast<CNightmareFireball*>(game.server.HandleToObject(projectile));
        CHECK(ball && ball->configured && ball->trail && ball->m_hFiredFrom == boss_handle &&
              !game.server.light_effects.empty() && game.server.light_effects.back().settings.radius.max == 100);
        CHECK(game.objects.advance(.01) && ball->tracked_target == target_handle && ball->flight_sound);
        DVector target, velocity;
        CHECK(game.server.GetObjectPos(target_handle, &target) == DE_OK);
        target.x += 150;
        CHECK(game.server.TeleportObject(target_handle, &target) == DE_OK && game.objects.advance(.01));
        CHECK(game.server.GetVelocity(projectile, &velocity) == DE_OK && velocity.x > 1 &&
              std::abs(std::sqrt(vector_dot(velocity, velocity)) - 1000) < .1f);
        auto* victim = dynamic_cast<AI_Mgr*>(game.server.HandleToObject(target_handle));
        CHECK(victim);
        const auto hp = victim->m_damage.m_fHitPoints;
        for (unsigned frame = 0; frame < 100 && game.server.IsObjectAlive(projectile); ++frame)
            CHECK(game.objects.advance(.01) && game.server.advance_projectiles(.01));
        CHECK(!game.server.IsObjectAlive(projectile) && victim->m_damage.m_fHitPoints < hp);
        CHECK(!game.server.scene_explosions.empty());
        const auto& blast = game.server.scene_explosions.back();
        CHECK(blast.type == 16 && blast.sprite_event && blast.light_event && blast.voice);
        return true;
    }

    bool world(const char* path)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(mount(*game, path, true));
        std::string error;
        if (!GameSession::start_new_game(game, "worlds_ao/endboss_cc_c", 1, 0, error))
        {
            std::cerr << error << '\n';
            diagnostics(*game);
            return false;
        }
        auto* fake = named<Nightmare>(*game, "fakemare");
        auto* spawner = named<Spawner>(*game, "NMspawner");
        auto* end = named<Trigger>(*game, "endtrig");
        CHECK(fake && fake->RuntimeReady() && spawner && end && !end->activations);
        const auto send = [&](HOBJECT target, const char* command)
        {
            auto text = game->server.CreateString(command);
            auto message = game->server.StartMessageToObject(spawner, target, 1000);
            const bool written = text && message && game->server.WriteToMessageHString(message, text) == DE_OK;
            if (text)
                game->server.FreeString(text);
            return written && game->server.EndMessage(message) == DE_OK;
        };
        CHECK(send(spawner->m_hObject, "TRIGGER"));
        auto* boss = dynamic_cast<Nightmare*>(game->server.HandleToObject(spawner->last_child));
        if (!boss || !boss->RuntimeReady())
            diagnostics(*game);
        CHECK(boss && boss->RuntimeReady());
        for (unsigned frame = 0; frame < 60; ++frame)
            if (!game->step(1. / 60))
            {
                for (const auto& reason : game->step_errors())
                    std::cerr << reason << '\n';
                return false;
            }
        const auto health_messages = game->client_shell.boss_health_messages;
        CHECK(boss->RuntimeReady() && health_messages && send(boss->m_hObject, "KILL") && end->activations == 1);
        CHECK(game->pump_player_messages(error) && game->client_shell.boss_health_messages == health_messages + 1 &&
              game->client_shell.boss_health.fraction == 0);
        return true;
    }

    bool edition(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path, false) && floor(game));
        CHECK(!game.server.ValidateSpawnObject("Nightmare"));
        CHECK(!game.objects.create(game.objects.find_class("Nightmare"), {}, Creation::World));
        CHECK(!game.objects.create(game.objects.find_class("CNightmareFireball")));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "attack" ? attack(argv[2]) : mode == "world" ? world(argv[2]) : mode == "edition" && edition(argv[2]))
               ? 0
               : 1;
}
