#include "app/demo_flow.h"
#include "engine/collision.h"
#include "engine/hud_surface.h"
#include "game/original/Door.h"
#include "game/original/ExitTrigger.h"
#include "game/original/Fanatic.h"
#include "game/original/NewStatusBar.h"
#include "game/original/WeaponPickups.h"
#include "game/server_adapter.h"
#include "player_fixture.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << " | " << error << '\n';                             \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool flow()
    {
        std::string error;
        DemoFlow demo;
        MovementControls controls;
        controls.focused = true;
        bool inspector = false;
        demo.reset(controls, inspector, true);
        auto key = [&](SDL_Scancode code, bool repeat = false, unsigned window = 1)
        {
            SDL_Event e{};
            e.type = SDL_EVENT_KEY_DOWN;
            e.key.windowID = window;
            e.key.scancode = code;
            e.key.repeat = repeat;
            return demo.event(e, 1, controls, inspector, true, true);
        };
        key(SDL_SCANCODE_W);
        CHECK(controls.keys[SDL_SCANCODE_W]);
        key(SDL_SCANCODE_ESCAPE, false, 2);
        CHECK(!demo.paused());
        key(SDL_SCANCODE_ESCAPE);
        CHECK(demo.paused() && !controls.keys[SDL_SCANCODE_W] && !controls.mouse_requested);
        key(SDL_SCANCODE_ESCAPE, true);
        CHECK(demo.paused());
        key(SDL_SCANCODE_W);
        CHECK(!controls.keys[SDL_SCANCODE_W]);
        key(SDL_SCANCODE_ESCAPE);
        CHECK(!demo.paused() && controls.mouse_requested);
        key(SDL_SCANCODE_F10);
        CHECK(inspector && !controls.mouse_requested);
        key(SDL_SCANCODE_F10);
        CHECK(!inspector && controls.mouse_requested);
        SDL_Event retry{};
        retry.type = SDL_EVENT_KEY_DOWN;
        retry.key.windowID = 1;
        retry.key.scancode = SDL_SCANCODE_RETURN;
        demo.event(retry, 1, controls, inspector, true, true, false);
        CHECK(demo.restart);
        demo.restart = false;
        SDL_Event e{};
        e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = 1;
        demo.event(e, 1, controls, inspector, true, true);
        CHECK(demo.paused() && !controls.focused);
        e.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        demo.event(e, 1, controls, inspector, true, true);
        CHECK(demo.paused() && controls.focused);
        demo.state = DemoFlow::State::Finished;
        demo.reset(controls, inspector, false);
        key(SDL_SCANCODE_F10);
        CHECK(!inspector && !demo.quit);
        key(SDL_SCANCODE_RETURN, true);
        CHECK(!demo.quit);
        key(SDL_SCANCODE_RETURN);
        key(SDL_SCANCODE_W);
        CHECK(demo.quit && !controls.keys[SDL_SCANCODE_W]);
        return true;
    }
    bool rules(const std::filesystem::path& scratch)
    {
        std::string error;
        GameSession g;
        CHECK(test_player::setup(g, scratch));
        g.resources.set_demo(true);
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        p->m_InventoryMgr.SetMagic(1);
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 0);
        CHECK(g.step(.1));
        CHECK(std::abs(p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) - .2f) < .0001f);
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 99.9f);
        CHECK(g.step(.1) && p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 100);
        p->m_InventoryMgr.SetMagic(6);
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 0);
        CHECK(g.step(.1) && std::abs(p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) - 1.2f) < .0001f);
        p->m_damage.m_bDead = DTRUE;
        p->m_bDead = DTRUE;
        const auto focus = p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS);
        CHECK(g.step(.1));
        CHECK(p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == focus);
        CHECK(g.server.SendWorldExit({"EndGame", WorldExitType::World}, "") == DE_OK && g.pump_player_messages(error));
        CHECK(!g.client_shell.world_exit.demo_end_pending && g.client_shell.world_exit.ignored_dead == 1);
        p->m_damage.m_bDead = DFALSE;
        p->m_bDead = DFALSE;
        g.resources.set_demo(false);
        CHECK(g.server.SendWorldExit({"EndGame", WorldExitType::World}, "") == DE_OK && g.pump_player_messages(error));
        CHECK(!g.client_shell.world_exit.demo_end_pending && g.pending_level_change);
        g.resources.set_demo(true);
        CHECK(g.server.SendWorldExit({"eNdGaMe", WorldExitType::World}, "") == DE_OK);
        CHECK(g.pump_player_messages(error));
        CHECK(g.client_shell.world_exit.demo_end_pending && !g.pending_level_change &&
              !g.client_shell.world_exit.pending);
        CHECK(g.client_shell.m_bInWorld && g.client_shell.world_exit.queued == 1);
        HudCanvas missing(g.resources);
        CHECK(!missing.CreateSurfaceFromBitmap("screens/info.pcx") && !missing.error.empty());
        HudSurface corrupt;
        CHECK(!read_pcx(std::vector<std::uint8_t>(128), corrupt, error) && !error.empty());
        CHECK(g.unload_world());
        return true;
    }
    bool load(GameSession& g, const char* root, const char* world)
    {
        std::string error;
        Config cfg;
        cfg.demo = true;
        cfg.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(cfg, error));
        CHECK(g.resources.mounts().size() == 1 && g.resources.demo());
        CHECK(g.request_world(world, error) && g.run_world(error));
        for (std::size_t i = 0; i < g.entries.size(); ++i)
            if (g.entries[i].state == EntryState::MissingWorldModel)
            {
                std::cout << "Existing source omission " << world << " #" << i << " " << g.world.objects[i].class_name;
                for (const auto& property : g.world.objects[i].properties)
                    if (property.name == "Name")
                        if (const auto* name = std::get_if<std::string>(&property.value))
                            std::cout << " / " << *name;
                std::cout << ": " << g.entries[i].detail << '\n';
            }
        return true;
    }
    bool advance(GameSession& g, unsigned frames)
    {
        for (unsigned i = 0; i < frames; ++i)
            if (!g.step(1. / 60))
            {
                for (const auto& message : g.step_errors())
                    std::cerr << message << '\n';
                return false;
            }
        return true;
    }
    bool send_exit(GameSession& g)
    {
        std::string error;
        HOBJECT exit = nullptr;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && dynamic_cast<ExitTrigger*>(r->game.get()))
            {
                exit = r->game->m_hObject;
                break;
            }
        CHECK(exit);
        auto text = g.server.CreateString("TRIGGER");
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(g.client_shell.player), exit, 1000);
        CHECK(message && g.server.WriteToMessageHString(message, text) == DE_OK &&
              g.server.EndMessage(message) == DE_OK);
        g.server.FreeString(text);
        CHECK(g.pump_player_messages(error));
        return true;
    }
    bool worlds(const char* root)
    {
        std::string error;
        auto g = std::make_unique<GameSession>();
        CHECK(load(*g, root, "Demo_01"));
        CHECK(g->step(1e-7)); // Real-clock startup regression: authored HealthPU0 wall overlap.
        CHECK(advance(*g, 180));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p);
        p->m_damage.SetHitPoints(73);
        HOBJECT authored_voodoo = nullptr;
        for (auto id : g->objects.snapshot())
            if (auto* r = g->objects.get(id); r && dynamic_cast<VoodooDollPU*>(r->game.get()))
                authored_voodoo = r->handle;
        // Labelled contact delivery to the authored pickup, not an extra/fake pickup.
        CHECK(authored_voodoo && g->objects.engine_message(authored_voodoo, MID_TOUCHNOTIFY, p->m_hObject));
        CHECK(p->m_InventoryMgr.GetWeapon(2) && p->m_InventoryMgr.GetWeapon(2)->data.m_nType == WEAP_VOODOO);
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 57);
        CHECK(g->command_weapon(2, error));
        const auto revision = g->world_revision;
        const auto player = p->m_hObject;
        WorldMessage bad;
        CHECK(original_world_request("missing_demo_world", 0, LOADTYPE_NEW_LEVEL, 0, bad));
        CHECK(g->queue_level_change(bad, "", error));
        CHECK(GameSession::process_level_change(g, error) == LevelChangeResult::Rejected);
        CHECK(g->client_shell.player == player && g->world_revision == revision && p->m_damage.m_fHitPoints == 73);
        CHECK(send_exit(*g) && g->pending_level_change);
        CHECK(GameSession::process_level_change(g, error) == LevelChangeResult::Committed);
        CHECK(g->world_path == "worlds/demo_02.dat" && g->level_change.selected_start == "05_egress");
        p = g->server_shell.Player(g->local_client);
        CHECK(p);
        CHECK(g->resources.demo() && p->m_damage.m_fHitPoints == 73);
        CHECK(g->server.GetVarValueFloat(g->server.GetGameConVar("GAME_DIFFICULTY")) == 1);
        const auto* standing = g->objects.get(p->m_hObject);
        CHECK(standing && standing->start_departures > 0 &&
              std::abs(standing->state.m_Pos.y - standing->dimensions.y + 5568) < 5);
        const float entry_y = standing->state.m_Pos.y;
        CHECK(p->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_VOODOO &&
              p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 57);
        CHECK(advance(*g, 180));
        Door* elevator = nullptr;
        for (auto id : g->objects.snapshot())
            if (auto* r = g->objects.get(id); r && std::string_view(r->state.m_Name) == "endelevator")
                elevator = dynamic_cast<Door*>(r->game.get());
        CHECK(elevator && elevator->state == Door::Opening);
        for (unsigned frame = 0; frame < 6000 && elevator->state == Door::Opening; ++frame)
            CHECK(advance(*g, 1));
        CHECK(elevator->state == Door::Open);
        const auto rise = g->objects.get(p->m_hObject)->state.m_Pos.y - entry_y;
        std::cout << "Demo_02 authored elevator: state=" << elevator->state << ", rider rise=" << rise
                  << ", requested travel=" << elevator->move_dist << '\n';
        CHECK(std::abs(rise - elevator->move_dist) < 5 && !p->m_damage.IsDead());
        CHECK(send_exit(*g));
        CHECK(g->client_shell.world_exit.demo_end_pending && !g->pending_level_change);
        HudCanvas canvas(g->resources);
        const auto* end = canvas.CreateSurfaceFromBitmap("screens/info.pcx");
        CHECK(end && end->width == 640 && end->height == 480);
        CHECK(g->unload_world() && !g->client_shell.m_bInWorld);
        // Standalone second-level entry must work independently of player transfer.
        CHECK(g->request_world("Demo_02", error) && g->run_world(error) && advance(*g, 60));
        std::cout << "Demo worlds: start, 3s first world/full second-world elevator, authored exits, rollback, "
                     "Voodoo/Focus transfer, end PCX, "
                     "direct Demo_02\n";
        return true;
    }
    CBaseCharacter* target(GameSession& g, DVector at)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        state.m_Pos = at;
        state.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("CBaseCharacter"), state);
        auto* p = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        DVector dims{10, 20, 10};
        if (!p || !p->m_damage.Init(h) || g.server.SetObjectDims(h, &dims) != DE_OK)
            return nullptr;
        p->m_damage.SetHitPoints(1000);
        p->m_damage.m_fMaxHitPoints = 1000;
        return p;
    }
    bool weapon(const char* root)
    {
        std::string error;
        GameSession g;
        Config cfg;
        cfg.demo = true;
        cfg.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(cfg, error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic.dat";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Pos", DVector{0, 100, 0}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        CNewStatusBar hud(g.resources);
        CHECK(hud.Init());
        PlayerStats sample;
        sample.weapon_slot = 2;
        sample.ammo = sample.health = sample.max_health = 100;
        sample.max_armor = 100;
        for (auto type :
             {WEAP_MELEE, WEAP_BERETTA, WEAP_SUBMACHINEGUN, WEAP_ASSAULTRIFLE, WEAP_TESLACANNON, WEAP_VOODOO})
        {
            sample.weapon = type;
            CHECK(hud.Update(sample, 640, 480));
        }
        sample.alternate_ammo_type = AMMO_DIEBUGDIE;
        sample.alternate_ammo = 5;
        CHECK(hud.Update(sample, 640, 480) && hud.tabs()[3]->value == 5 && !hud.tabs()[3]->hIcon);
        p->m_fPitch = p->m_fYaw = 0;
        DVector origin;
        CHECK(g.server.GetObjectPos(p->m_hObject, &origin) == DE_OK);
        auto pickup = g.objects.create(g.objects.find_class("VoodooDollPU"), {}, Creation::World,
                                       {{"Pos", vector_add(origin, {100, 0, 0})}});
        CHECK(pickup);
        CHECK(g.objects.engine_message(pickup, MID_TOUCHNOTIFY, p->m_hObject));
        CHECK(p->m_InventoryMgr.GetWeapon(2) && p->m_InventoryMgr.GetWeapon(2)->data.m_nType == WEAP_VOODOO);
        CHECK(g.command_weapon(2, error));
        auto* w = dynamic_cast<CVoodooDoll*>(p->m_InventoryMgr.CurrentWeapon());
        CHECK(w);
        auto* near = target(g, vector_add(origin, {0, 0, 100}));
        auto* far = target(g, vector_add(origin, {40, 0, 250}));
        CHECK(near && far);
        WeaponFire fire;
        fire.weapon = WEAP_VOODOO;
        fire.position = origin;
        for (DBYTE variant = 0; variant < 5; ++variant)
        {
            fire.voodoo_hit = variant;
            p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
            const auto before = near->m_damage.m_fHitPoints;
            CHECK(w->Fire(fire) == FireResult::Fired && w->targets.size() == 1 && w->targets[0] == near->m_hObject);
            CHECK(near->m_damage.m_fHitPoints < before && p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 95);
            CHECK(w->hit_effect == variant && g.server.object_particles.active.back().effect.recipe == 12u + variant);
        }
        fire.voodoo_hit = 5;
        const auto shots = w->shots;
        CHECK(w->Fire(fire) == FireResult::Invalid && w->shots == shots);
        fire.voodoo_hit = 0;
        fire.alternate = true;
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        CHECK(w->Fire(fire) == FireResult::Fired && w->targets.size() == 2 &&
              p->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 0);
        CHECK(w->Fire(fire) == FireResult::Dry);
        CHECK(w->sounds_played == 6 && w->fire_sound_error.empty());
        // Effect cylinder count, duration and shared-color behavior use real indexed demo texture.
        for (auto& instance : g.server.object_particles.active)
        {
            auto fx = instance.effect; // Probe a copy; never advance the session owner's clock here.
            NodePose pose;
            pose.position = origin;
            std::minstd_rand random{1};
            CHECK(fx.Update(.001, &pose, random) && fx.Update(.11, &pose, random));
            CHECK(fx.emitted == (fx.recipe == 12 ? 12u : 32u));
            CHECK(fx.Update(.6, &pose, random) && fx.ramping && fx.alpha < 1);
            CHECK(!fx.Update(.76, &pose, random) && fx.error.empty());
        }
        ObjectCreateStruct wall;
        wall.m_Flags = FLAG_SOLID;
        wall.m_Pos = vector_add(origin, {0, 0, 50});
        auto obstruction = g.objects.create(g.objects.find_class("BaseClass"), wall);
        DVector wall_dims{35, 100, 5};
        CHECK(obstruction && g.server.SetObjectDims(obstruction, &wall_dims) == DE_OK);
        fire.alternate = false;
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        CHECK(w->Fire(fire) == FireResult::Fired && w->targets.empty());
        g.server.RemoveObject(obstruction);
        g.server.RemoveObject(near->m_hObject);
        g.server.RemoveObject(far->m_hObject);
        CHECK(g.objects.finish_frame());
        auto enemy_handle =
            g.objects.create(g.objects.find_class("Fanatic"), {}, Creation::World,
                             {{"Pos", vector_add(origin, {0, 0, 150})}, {"AIState", std::string("IDLE")}});
        auto* enemy = dynamic_cast<Fanatic*>(g.server.HandleToObject(enemy_handle));
        CHECK(enemy && enemy->RuntimeReady());
        const auto enemy_hp = enemy->m_damage.m_fHitPoints;
        // The real client animation must deliver its selected variant on FIRE_KEY.
        auto& view_weapon = g.client_shell.weapons[0];
        const auto fired = view_weapon.shots;
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        g.client_shell.rotation = {};
        g.client_shell.weapon_input.primary = true;
        CHECK(advance(g, 150));
        CHECK(view_weapon.shots > fired && w->last_shot.request.voodoo_hit < 5);
        CHECK(w->sounds_played > 6 && w->fire_sound_error.empty());
        // Damage may remove the enemy through its original death/corpse path.
        CHECK(!g.server.IsObjectAlive(enemy_handle) ||
              dynamic_cast<Fanatic*>(g.server.HandleToObject(enemy_handle))->m_damage.m_fHitPoints < enemy_hp);
        g.client_shell.weapon_input = {};
        p->m_fYaw = 3.14159265f;
        fire.alternate = false;
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        const auto hp = p->m_damage.m_fHitPoints;
        CHECK(w->Fire(fire) == FireResult::Fired && w->targets.empty() && p->m_damage.m_fHitPoints < hp);
        p->m_damage.SetHitPoints(200);
        fire.alternate = true;
        p->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 100);
        CHECK(w->Fire(fire) == FireResult::Fired && w->targets.empty() && w->deliveries == 1);
        CHECK(p->m_damage.m_fHitPoints <= 100);
        // Actual five view animation resources exist; no substituted clips.
        const auto& view = g.client_shell.weapons[0];
        for (auto name : {"fire", "fire_lleg", "fire_larm", "fire_rarm", "fire_rleg"})
            CHECK(animation_index(*view.model, name) != no_animation);
        std::cout << "Voodoo: actual pickup/view assets, five hit variants, closest/all targets, Focus, self damage, "
                     "six cylinder effects\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "flow")
        return flow() ? 0 : 1;
    if (argc != 3)
        return 2;
    if (mode == "rules")
        return rules(argv[2]) ? 0 : 1;
    if (mode == "worlds")
        return worlds(argv[2]) ? 0 : 1;
    if (mode == "weapon")
        return weapon(argv[2]) ? 0 : 1;
    return 2;
}
