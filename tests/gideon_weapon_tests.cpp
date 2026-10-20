#include "app/sprite_scene.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/CampaignWeapons.h"
#include "player_fixture.h"
#include <cmath>
#include <limits>
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
    bool setup(GameSession& g, const char* data, const char* fixture)
    {
        CHECK(test_player::setup(g, fixture));
        std::string error;
        CHECK(g.enter_player(error));
        Config c;
        c.data_dir = data;
        CHECK(g.resources.mount_game(c, error));
        return true;
    }
    bool send(GameSession& g, CPlayerObj* p, float duration)
    {
        auto m = g.server.StartMessageToObject(p, p->m_hObject, immobilize_message);
        CHECK(m && g.server.WriteImmobilize(m, duration) == DE_OK);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    CBaseCharacter* owner(GameSession& g, DVector pos)
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), s);
        auto* a = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        return a && a->m_InventoryMgr.Init(h, nullptr) ? a : nullptr;
    }
    bool definitions(const char*, const char*)
    {
        const auto* v = weapon_definition(43);
        const auto* s = weapon_definition(45);
        CHECK(v && s);
        CHECK(v->m_nAmmoType == AMMO_NONE && v->m_nFireType == TYPE_RIFLE && v->m_fProjVelocity == 2000 &&
              v->m_nDamageRadius == 0);
        CHECK(v->m_fMinDamage == 15 && v->m_fMaxDamage == 40 && v->m_fRange == 2000);
        CHECK(s->m_nAmmoType == AMMO_NONE && s->m_nFireType == TYPE_MELEE && s->m_fRange == 200 &&
              !s->m_szProjectileClass);
        DBYTE id = 0;
        CHECK(AI_Mgr::WeaponStrToInt("GIDEON_SPEAR", id) && id == 45);
        CHECK(AI_Mgr::WeaponStrToInt("GIDEON_VOMIT", id) && id == 43);
        return true;
    }
    bool timer(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(send(g, p, 4) && p->m_bImprisoned && p->MovementBlocked() && p->imprison_accepts == 1 &&
              !p->imprison_notice.empty());
        const float start = p->m_fImprisonStart;
        CHECK(g.objects.advance(1));
        CHECK(send(g, p, 50) && p->m_fImprisonStart == start && p->m_fImprisonLength == 4 && p->imprison_accepts == 1);
        CHECK(g.objects.advance(3) && p->CheckImprisonment() && p->m_bImprisoned);
        CHECK(g.objects.advance(.002) && p->CheckImprisonment() && !p->m_bImprisoned && !p->MovementBlocked() &&
              p->imprison_expiries == 1);
        p->m_bDead = true;
        CHECK(send(g, p, 4) && !p->m_bImprisoned);
        p->m_bDead = false;
        CHECK(send(g, p, 0) && p->m_bImprisoned && p->CheckImprisonment() && p->m_bImprisoned);
        CHECK(g.objects.advance(.002) && !p->m_bImprisoned);
        CHECK(g.unload_world() && !g.server.message_count());
        return true;
    }
    bool messages(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        for (float value : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            auto m = g.server.StartMessageToObject(p, p->m_hObject, immobilize_message);
            CHECK(m);
            CHECK(g.server.WriteImmobilize(m, value) == DE_INVALIDPARAMS && g.server.EndMessage(m) == DE_ERROR &&
                  !p->m_bImprisoned);
        }
        auto m = g.server.StartMessageToObject(p, p->m_hObject, immobilize_message);
        CHECK(m && g.server.WriteImmobilize(m, 4) == DE_OK);
        CHECK(g.server.WriteToMessageDWord(m, 1) == DE_INVALIDPARAMS && g.server.EndMessage(m) == DE_ERROR);
        m = g.server.StartMessageToObject(p, p->m_hObject, immobilize_message);
        CHECK(m && g.server.EndMessage(m) == DE_ERROR);
        CHECK(!g.server.message_count() && !p->m_bImprisoned);
        CHECK(send(g, p, 4));
        m = g.server.StartMessageToObject(p, p->m_hObject, immobilize_message);
        CHECK(m && g.server.EndMessage(m) == DE_ERROR && p->m_fImprisonLength == 4);
        return true;
    }
    bool movement(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        std::string error;
        CHECK(send(g, p, 4));
        g.client_shell.movement_input.forward = true;
        g.client_shell.movement_input.jump = true;
        const auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        CHECK(g.client_shell.UpdateMovement(g.server_shell, .05, error));
        CHECK(g.client_shell.move_mgr.m_dwControlFlags == 0);
        const auto moved = g.objects.get(p->m_hObject)->state.m_Pos;
        CHECK(moved.x == pos.x && moved.z == pos.z && moved.y <= pos.y);
        WeaponFire shot;
        shot.weapon = 1;
        shot.direction = {0, 0, 1};
        CHECK(p->HandleWeaponFireMessage(shot) == FireResult::Blocked);
        CHECK(g.objects.advance(4.01) && !p->m_bImprisoned);
        CHECK(g.pump_player_messages(error)); // First server update sends the checked client move code.
        CHECK(g.client_shell.UpdateMovement(g.server_shell, .05, error) &&
              g.client_shell.move_mgr.m_dwControlFlags != 0);
        CHECK(g.unload_world());
        return true;
    }
    bool saved(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        std::string error;
        CHECK(send(g, p, 4) && g.objects.advance(1));
        PlayerState state;
        CHECK(p->SaveFields(state, error));
        CHECK(state.imprisoned && state.imprison_start_offset == -1 && state.imprison_length == 4);
        auto h = g.objects.create(g.objects.find_class("CPlayerObj"), {}, Creation::Runtime);
        auto* copy = dynamic_cast<CPlayerObj*>(g.server.HandleToObject(h));
        CHECK(copy);
        CHECK(g.objects.advance(1) && copy->LoadFields(state, error) && copy->MovementBlocked());
        CHECK(copy->m_fImprisonStart == g.server.GetTime() - 1 && copy->m_fImprisonLength == 4);
        PlayerState round;
        CHECK(copy->SaveFields(round, error) && round == state);
        auto bad = state;
        bad.imprison_length = -1;
        CHECK(!copy->LoadFields(bad, error) && copy->m_fImprisonLength == 4);
        CHECK(g.objects.advance(3) && copy->CheckImprisonment() && copy->m_bImprisoned);
        CHECK(g.objects.advance(.002) && copy->CheckImprisonment() && !copy->m_bImprisoned);
        CHECK(g.unload_world());
        return true;
    }
    bool spear(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        const auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        const auto from = vector_add(pos, {-100, 0, 0});
        auto* a = owner(g, from);
        CHECK(a);
        CHECK(a->m_InventoryMgr.ObtainWeapon(45, 0) == InventoryResult::Ok);
        auto* weapon = dynamic_cast<CGideonSpear*>(a->m_InventoryMgr.CurrentWeapon());
        CHECK(weapon);
        WeaponFire fire;
        fire.weapon = 45;
        fire.position = from;
        fire.direction = {1, 0, 0};
        fire.seed = 11;
        const float hp = p->m_damage.m_fHitPoints;
        CHECK(a->m_InventoryMgr.FireCurrentWeapon(fire) == FireResult::Fired);
        CHECK(weapon->immobilize_target == p->m_hObject && weapon->immobilize_deliveries == 1 && p->m_bImprisoned &&
              p->m_damage.m_fHitPoints < hp);
        CHECK(g.objects.advance(4.01) && !p->m_bImprisoned);
        ObjectCreateStruct obstacle;
        obstacle.m_Pos = vector_add(from, {50, 0, 0});
        obstacle.m_Flags = FLAG_SOLID;
        const auto wall = g.objects.create(g.objects.find_class("BaseClass"), obstacle);
        DVector extents{5, 30, 30};
        CHECK(wall && g.server.SetObjectDims(wall, &extents) == DE_OK);
        const float blocked_hp = p->m_damage.m_fHitPoints;
        CHECK(a->m_InventoryMgr.FireCurrentWeapon(fire) == FireResult::Fired);
        CHECK(!weapon->immobilize_target && weapon->immobilize_deliveries == 1 &&
              !p->m_bImprisoned && p->m_damage.m_fHitPoints == blocked_hp);
        CHECK(!g.server.message_count());
        CHECK(g.unload_world());
        return true;
    }
    bool lifetime(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        ObjectCreateStruct state;
        state.m_Pos = {100, 100, 100};
        auto h = g.objects.create(g.objects.find_class("CVomitProjectile"), state);
        auto* projectile = dynamic_cast<CVomitProjectile*>(g.server.HandleToObject(h));
        CHECK(projectile && projectile->Setup({1, 0, 0}, 43, 25, 2000, 0, p->m_hObject));
        CHECK(g.objects.advance(10) && g.server.IsObjectAlive(h));
        CHECK(g.objects.advance(.002) && !g.server.IsObjectAlive(h));
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server) &&
              g.server.object_particles.active.empty());
        CHECK(g.server.explosion_models.active.empty() && g.server.explosion_sprites.active.empty());
        auto* resources = g.server.client_resources;
        g.server.client_resources = nullptr;
        h = g.objects.create(g.objects.find_class("CVomitProjectile"), state);
        CHECK(!g.server.IsObjectAlive(h));
        g.server.client_resources = resources;
        CHECK(g.objects.advance(0) && g.server.object_particles.failed == 1);
        CHECK(send(g, p, 1));
        p->movement_blocked = true;
        CHECK(g.objects.advance(1.002) && !p->m_bImprisoned && p->MovementBlocked());
        p->movement_blocked = false;
        CHECK(!p->MovementBlocked());
        CHECK(g.unload_world());
        return true;
    }
    bool vomit(const char* data, const char* fixture)
    {
        GameSession g;
        CHECK(setup(g, data, fixture));
        auto* p = g.server_shell.Player(g.local_client);
        const auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        const auto from = vector_add(pos, {-100, 0, 0});
        auto* a = owner(g, from);
        CHECK(a);
        CHECK(a->m_InventoryMgr.ObtainWeapon(43, 0) == InventoryResult::Ok);
        auto* weapon = a->m_InventoryMgr.CurrentWeapon();
        CHECK(weapon);
        WeaponFire fire;
        fire.weapon = 43;
        fire.position = from;
        fire.direction = {1, 0, 0};
        fire.seed = 3;
        CHECK(a->m_InventoryMgr.FireCurrentWeapon(fire) == FireResult::Fired);
        const auto h = weapon->projectile;
        auto* projectile = dynamic_cast<CVomitProjectile*>(g.server.HandleToObject(h));
        CHECK(projectile && projectile->trail && !projectile->failed);
        CHECK(projectile->damage_type == DamageType::Normal && !(g.server.GetObjectFlags(h) & FLAG_GRAVITY));
        CHECK(g.objects.advance(.002));
        CHECK(g.objects.get(h)->state.m_Scale.x == 0);
        CHECK(g.objects.advance(.01));
        CHECK(g.objects.get(h)->state.m_Scale.x == .25f);
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
        CHECK(projectile->AdvanceFlight(.02));
        CHECK(g.objects.advance(.03));
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server) &&
              g.server.object_particles.particle_count() > 0);
        SpriteView scene;
        std::string error;
        CHECK(update_sprite_scene(g, scene, error) && !scene.draws.empty());
        const auto& trail = g.server.object_particles.active.front().effect;
        CHECK(trail.recipe == 3 && trail.min_life > 0 && trail.max_life <= .25f);
        const float hp = p->m_damage.m_fHitPoints;
        CHECK(projectile->AdvanceFlight(.1) && projectile->m_bExplode);
        CHECK(g.objects.advance(.002) && !g.objects.get(h) && p->m_damage.m_fHitPoints < hp);
        CHECK(g.server.explosion_models.active.empty() && g.server.explosion_rings.active.empty());
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server) &&
              g.server.object_particles.active.empty());
        CHECK(g.unload_world() && !g.server.message_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "definitions" ? definitions(argv[2], argv[3])
            : mode == "timer"     ? timer(argv[2], argv[3])
            : mode == "messages"  ? messages(argv[2], argv[3])
            : mode == "movement"  ? movement(argv[2], argv[3])
            : mode == "saved"     ? saved(argv[2], argv[3])
            : mode == "spear"     ? spear(argv[2], argv[3])
            : mode == "lifetime"  ? lifetime(argv[2], argv[3])
                                  : vomit(argv[2], argv[3]))
               ? 0
               : 1;
}
