#include "engine/collision.h"
#include "engine/container.h"
#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/original/BoneLeech.h"
#include "game/original/DrudgeLord.h"
#include "game/original/NapalmProjectile.h"
#include "game/original/PickupObject.h"
#include "game/original/RotatingDoor.h"
#include "game/original/Spawner.h"
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
    ObjectRecord* named(GameSession& g, std::string_view name)
    {
        for (const auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->state.m_Name == name)
                return r;
        return nullptr;
    }
    bool trigger(GameSession& g, HOBJECT target, const char* text)
    {
        auto string = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, target, 1000);
        CHECK(string && msg);
        const auto wrote = g.server.WriteToMessageHString(msg, string);
        g.server.FreeString(string);
        CHECK(wrote == DE_OK && g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    bool city_spawn6(const char* root)
    {
        auto game = std::make_unique<GameSession>();
        CHECK(mount(*game, root));
        std::string error;
        CHECK(GameSession::start_new_game(game, "02_cityhubc", 1, 0, error));
        auto* r = named(*game, "spawn6");
        auto* spawn = r ? dynamic_cast<Spawner*>(r->game.get()) : nullptr;
        CHECK(spawn && trigger(*game, r->handle, "TRIGGER"));
        auto* actor = game->objects.get(spawn->last_child);
        CHECK(actor && actor->type->name == "ShikariAI" && actor->placement_nudges > 0);
        auto* shikari = dynamic_cast<AI_Mgr*>(actor->game.get());
        CHECK(shikari && shikari->RuntimeReady() && actor->state.m_Pos.z < -817);
        CHECK(game->step(1. / 60));

        // Reproduce the older save's recorded pose, then use the ordinary restore
        // path. A prior save can already contain the actor inside the crate.
        DVector embedded{2624, 880, -816};
        CHECK(game->server.TeleportObject(actor->handle, &embedded) == DE_OK);
        SAFE_STRCPY(actor->state.m_Name, "spawn6 regression");
        GameSave save;
        if (!game->save_game(save, error) || !GameSession::load_game(game, save, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        actor = named(*game, "spawn6 regression");
        CHECK(actor && actor->state.m_Pos.z == embedded.z);
        // The player starts on the other side of the map. Activate this loaded
        // actor as the visibility ping would when the player returns.
        game->server.SetObjectState(actor->handle, OBJSTATE_ACTIVE);
        CHECK(actor->activity == OBJSTATE_ACTIVE);
        DVector falling{0, -10, 0};
        CHECK(game->server.SetVelocity(actor->handle, &falling) == DE_OK);
        if (!game->server.advance_physics(1. / 60))
        {
            for (const auto& message : game->objects.diagnostics())
                std::cerr << message << '\n';
            return false;
        }
        CHECK(actor->state.m_Pos.z < -817 && actor->overlap_contacts > 0);
        CHECK(game->step(1. / 60));
        return true;
    }
    bool steam_swing(const char* root)
    {
        GameSession game;
        CHECK(mount(game, root));
        std::string error;
        CHECK(game.request_world("04_steamtunnels", error) && game.enter_player(error));
        for (const auto* name : {"endventmaster1", "endventmaster2", "endventmaster3"})
        {
            auto* valve = named(game, name);
            CHECK(valve && trigger(game, valve->handle, "TRIGGER"));
        }
        auto* door_record = named(game, "rotdoorgubgub99");
        auto* door = door_record ? dynamic_cast<RotatingDoor*>(door_record->game.get()) : nullptr;
        CHECK(door && door->state == Door::Closed);
        ObjectRecord* shikari = nullptr;
        for (const auto id : game.objects.snapshot())
            if (auto* r = game.objects.get(id); r && r->type->name == "ShikariAI")
            {
                shikari = r;
                break;
            }
        CHECK(shikari);
        auto beside_door = vector_add(door_record->state.m_Pos, {0, -32, 27});
        CHECK(game.server.TeleportObject(shikari->handle, &beside_door) == DE_OK);
        game.server.SetNextUpdate(shikari->handle, 0);
        game.server.SetObjectFlags(shikari->handle, shikari->state.m_Flags & ~FLAG_GRAVITY);
        CHECK(trigger(game, door_record->handle, "TRIGGER"));
        for (int i = 0; i < 180 && door->state != Door::Open; ++i)
            CHECK(game.step(1. / 60));
        CHECK(door->state == Door::Open && door_record->mover_pushes > 0 && game.server.IsObjectAlive(shikari->handle));
        const auto at = shikari->state.m_Pos;
        CollisionHit hit;
        CHECK(game.collision.sweep(at, at, shikari->dimensions, hit) == CollisionQuery::Miss);
        CHECK(sweep_object(*door_record, at, at, shikari->dimensions, hit) == CollisionQuery::Miss);
        return true;
    }
    bool abandoned_chopper(const char* root)
    {
        GameSession game;
        CHECK(mount(game, root));
        std::string error;
        CHECK(game.request_world("09_abandoned_buildings", error) && game.enter_player(error));
        PathList path;
        CHECK(game.server_shell.paths.GetPath("GideonPathA", path, error) && path.size() == 9 &&
              path.front().name == "GideonPathA0" && path.back().name == "GideonPathA8");
        CHECK(game.server_shell.paths.GetPath("MadScientistPathC", path, error) && path.size() == 2 &&
              path.front().name == "MadScientistPathC0" && path.back().name == "MadScientistPathC1");
        auto* record = named(game, "MadScientist");
        auto* actor = record ? dynamic_cast<AI_Mgr*>(record->game.get()) : nullptr;
        const auto* scene = named(game, "Trigger-Remove");
        CHECK(actor && scene);
        const auto& movement = std::get<std::string>(scene->properties.at("MessageName4"));
        auto start = path.front().position;
        start.y = record->state.m_Pos.y;
        const auto handle = record->handle;
        CHECK(game.server.TeleportObject(handle, &start) == DE_OK && trigger(game, handle, movement.c_str()));
        // This isolated sequence starts at the boarding marker, without the
        // scripted camera that normally keeps its actors in the visible set.
        CHECK(game.server.SetDeactivationTime(handle, 60));
        for (int i = 0; i < 600 && game.server.IsObjectAlive(handle); ++i)
        {
            CHECK(game.step(1. / 60));
            if (game.server.IsObjectAlive(handle) && actor->runtime_failed)
            {
                std::cerr << actor->runtime_error << '\n';
                return false;
            }
        }
        if (game.server.IsObjectAlive(handle))
        {
            const auto at = record->state.m_Pos;
            std::cerr << "Scientist stopped at " << at.x << ',' << at.y << ',' << at.z << ", arrivals "
                      << actor->path_arrivals << ", activity " << record->activity << ", script " << actor->script_error
                      << '\n';
            return false;
        }
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
        // These offers now enter inventory and acknowledge their pickup.
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
            CHECK(!g.server.IsObjectAlive(h));
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
    if (std::string_view(argv[1]) == "city_spawn6")
        return city_spawn6(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "steam_swing")
        return steam_swing(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "abandoned_chopper")
        return abandoned_chopper(argv[2]) ? 0 : 1;
    return entry(argv[2], argv[1]) ? 0 : 1;
}
