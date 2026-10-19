#include "engine/collision.h"
#include "engine/container.h"
#include "game/original/BoneLeech.h"
#include "game/original/DrudgeLord.h"
#include "game/original/NapalmProjectile.h"
#include "game/original/PickupObject.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
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
    bool mount(GameSession& g, const char* root)
    {
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        return true;
    }
    bool entry(const char* root, const char* world)
    {
        auto g = std::make_unique<GameSession>();
        CHECK(mount(*g, root));
        std::string error;
        if (!GameSession::start_new_game(g, world, 1, 0, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        WorldValidation report;
        CHECK(check_world_initialization(*g, report, error));
        CHECK(report.unknown == 0 && report.failed == 0 && report.unavailable_services.empty());
        const bool city = std::string_view(world) == "02_cityhubc";
        unsigned absent = 0, missing = 0, leeches = 0, lords = 0, tables = 0;
        for (std::size_t i = 0; i < g->entries.size(); ++i)
        {
            const auto& e = g->entries[i];
            const auto& source = g->world.objects[i];
            if (e.state == EntryState::AbsentOriginalClass)
            {
                ++absent;
                CHECK(!e.object && !e.detail.empty());
                CHECK(source.class_name == "Gremlin" || source.class_name == "RocketAmmoPU");
            }
            if (e.state == EntryState::MissingResource)
            {
                ++missing;
                CHECK(!e.object && !e.detail.empty());
                CHECK(source.class_name == "CDestructableModel" || source.class_name == "DetailSprite");
            }
            if (const auto* r = g->objects.get(e.object))
            {
                if (const auto* a = dynamic_cast<const BoneLeech*>(r->game.get()))
                {
                    ++leeches;
                    CHECK(a->RuntimeReady());
                }
                if (const auto* a = dynamic_cast<const DrudgeLord*>(r->game.get()))
                {
                    ++lords;
                    CHECK(a->RuntimeReady());
                }
                if (r->body_properties.polygon_bounds)
                {
                    ++tables;
                    CHECK(r->world_body && r->world_model && !(r->state.m_Flags & FLAG_BOXPHYSICS));
                    if (std::string_view(r->state.m_Name) == "table2")
                        CHECK(r->world_model->boundary_only && r->world_model->off_plane_faces == 10 &&
                              r->world_model->open_edges > 0 && !r->world_model->closed_components);
                }
            }
        }
        CHECK(city ? (leeches == 17 && absent == 0 && missing == 1)
                   : (lords == 1 && absent == 8 && missing == 3 && tables == 2));
        CHECK(g->local_client && g->client_shell.m_bInWorld);
        WorldMessage request;
        CHECK(g->client_shell.StartNewWorld(world, request) && g->inspect_destination(request, "", error));
        CHECK(g->destination_inspection.unknown == 0 && g->destination_inspection.invalid == 0 &&
              g->destination_inspection.omitted == absent);
        if (!city)
        {
            const auto& assets = g->music_catalog.assets;
            const auto silence =
                std::find_if(assets.begin(), assets.end(), [](const auto& a) { return a.filename == "s.sec"; });
            CHECK(silence != assets.end() && silence->style_reference == "Track1-a" &&
                  silence->style_filename == "track1a.sty" && silence->section_style && silence->timeline);
        }
        // Exercise post-entry clocks and pings, without forcing AI targets or changing the world.
        for (int i = 0; i < 60; ++i)
            CHECK(g->step(1. / 60));
        std::cout << world << ": started, omitted=" << report.omitted << " runtime=" << g->objects.snapshot().size()
                  << '\n';
        CHECK(g->unload_world());
        CHECK(g->objects.snapshot().empty() && !g->server.string_count() && !g->server.message_count() &&
              !g->server.list_count());
        return true;
    }
    bool actors(const char* root)
    {
        GameSession g;
        CHECK(mount(g, root));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        auto create = [&](const char* name, DVector at)
        {
            return g.objects.create(g.objects.find_class(name), {}, Creation::World,
                                    {{"Name", std::string(name)}, {"Pos", at}});
        };
        auto leech = create("BoneLeech", {0, 50, 0});
        auto lord = create("DrudgeLord", {500, 80, 0});
        auto* b = leech ? dynamic_cast<BoneLeech*>(g.server.HandleToObject(leech)) : nullptr;
        auto* d = lord ? dynamic_cast<DrudgeLord*>(g.server.HandleToObject(lord)) : nullptr;
        if (!b || !d || !b->RuntimeReady() || !d->RuntimeReady())
            for (const auto& msg : g.objects.diagnostics())
                std::cerr << msg << '\n';
        CHECK(b && d && b->RuntimeReady() && d->RuntimeReady());
        CHECK(b->m_damage.m_fHitPoints >= 10 && b->m_damage.m_fHitPoints <= 11 && b->m_fAIMass == 125);
        CHECK(b->m_vScale.x > 1.8f && b->m_vScale.x < 2.1f && b->m_dwFlags == 0x40 && !b->m_bCabal);
        CHECK(d->m_damage.m_fHitPoints >= 500 && d->m_damage.m_fHitPoints <= 501 && d->m_damage.m_fArmorPoints == 100);
        CHECK(b->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_BONELEECH_SUCK);
        CHECK(b->m_damage.ai_body_hits && !b->AnimSound()->hit_spheres_valid);
        const float hp = b->m_damage.m_fHitPoints;
        auto message = g.server.StartMessageToObject(d, leech, damage_message);
        CHECK(message && g.server.WriteDamage(message, {{0, 0, 1}, {0, 50, 0}, 1, DamageType::Normal, lord}) == DE_OK &&
              g.server.EndMessage(message) == DE_OK);
        CHECK(b->m_damage.accepted_damage == 1 && b->m_damage.m_fHitPoints == hp - 1 && b->m_damage.node_hit == 2 &&
              b->m_damage.hit_queries == 0);
        CHECK(d->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_SOUL_HOOK);
        CHECK(b->ComputeState(3) && b->m_nState == AI_Mgr::StateSearchSmell);
        CHECK(b->ComputeState(1) && b->m_nState == AI_Mgr::StateAttackClose);
        CHECK(b->m_MoveObj.Update() && b->FireJump());
        CHECK(b->jumps == 1 && b->m_nCurMetacmd == 10);
        DVector v;
        CHECK(g.server.GetVelocity(leech, &v) == DE_OK && v.y == 600 && std::abs(v.z - 160) < .01f);
        d->m_fStimuli[0] = .9f;
        d->m_fStimuli[5] = 1;
        CHECK(d->ComputeState(1) && d->m_nState == AI_Mgr::StateAttackClose);
        d->m_fStimuli[5] = .2f;
        CHECK(d->ComputeState(1) && d->m_nState == AI_Mgr::StateAttackFar);
        CHECK(d->m_MoveObj.Update());
        d->m_vTargetPos = {500, 80, 500};
        CHECK(d->MC_Fire_Stand() && d->fireball_clips == 1);
        CHECK(d->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_DRUDGE_FIREBALL);
        CHECK(d->Fire(false));
        const auto projectile = d->m_InventoryMgr.CurrentWeapon()->projectile;
        auto* napalm = projectile ? dynamic_cast<CNapalmProjectile*>(g.server.HandleToObject(projectile)) : nullptr;
        CHECK(napalm && napalm->configured && napalm->m_nRadius == 300 && napalm->m_hFiredFrom == lord);
        CHECK(g.objects.remove(lord));
        g.objects.finish_frame();
        CHECK(g.server.IsObjectAlive(projectile) && !napalm->m_hFiredFrom);
        CHECK(!(g.server.GetObjectFlags(projectile) & FLAG_GRAVITY));
        CHECK(g.objects.get(projectile)->state.m_Scale.x == 0);
        CHECK(g.objects.advance(.02));
        CHECK(g.server.IsObjectAlive(projectile) && g.objects.get(projectile)->state.m_Scale.x == .25f);
        CHECK(napalm->flight_sound || !napalm->sound_error.empty());
        const auto sound = napalm->flight_sound;
        napalm->m_bExplode = true;
        CHECK(g.objects.advance(.01)); // Original update explodes, then defers removal.
        CHECK(!g.server.IsObjectAlive(projectile) && g.server.scene_explosions.back().type == 4);
        CHECK(!sound || !g.server.audio.get(sound));
        CHECK(g.objects.finish_frame());
        // Registered offers remain visible when the corresponding inventory action is unavailable.
        CHECK(g.request_world("02_cityhubc", error) && g.enter_player(error));
        for (const char* name : {"TheEyePU", "NightGogglesPU", "BugSprayPU"})
        {
            auto h = g.objects.create(g.objects.find_class(name), {}, Creation::String,
                                      {{"Name", std::string(name)}, {"Pos", DVector{0, 100, 0}}});
            CHECK(h);
            auto* pickup = dynamic_cast<PickupObject*>(g.server.HandleToObject(h));
            if (pickup && !pickup->error.empty())
                std::cerr << name << ": " << pickup->error << '\n';
            CHECK(pickup && pickup->error.empty());
            CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player));
            CHECK(g.server.IsObjectAlive(h));
        }
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    if (std::string_view(argv[1]) == "actors")
        return actors(argv[2]) ? 0 : 1;
    return entry(argv[2], argv[1]) ? 0 : 1;
}
