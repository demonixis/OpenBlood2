#include "app/model_scene.h"
#include "app/movement_controls.h"
#include "camera_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Door.h"
#include "game/original/Trigger.h"
#include <cmath>
#include <map>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool capture_scene(GameSession& g, Runtime& runtime, const std::filesystem::path& path, std::string& error)
    {
        const auto time = g.objects.time();
        WorldMesh mesh;
        ModelView scene;
        WorldRenderer world(runtime.gpu), models(runtime.gpu);
        CHECK(mesh.build(g.world, error, &g.resources) && update_model_scene(g, scene, error));
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        Capture capture;
        CHECK(capture.initialize(runtime.gpu, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420));
        CHECK(world.prepare(mesh, g.world_revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420, error));
        CHECK(models.prepare(scene.mesh, scene.revision, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 640, 420, error, true));
        const auto view = g.client_shell.view;
        auto* cmd = SDL_AcquireGPUCommandBuffer(runtime.gpu);
        CHECK(cmd);
        CHECK(world.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error, DrawLoad::Clear,
                         nullptr, WorldPass::Opaque, g.server.world_time()));
        CHECK(models.draw(cmd, capture.texture, scene.mesh, scene.world_draws, view.position, view.rotation, true,
                          error, DrawLoad::Load, world.depth_target()));
        CHECK(world.draw(cmd, capture.texture, mesh, draws, view.position, view.rotation, true, error, DrawLoad::Load,
                         nullptr, WorldPass::Translucent, g.server.world_time()));
        auto* copy = SDL_BeginGPUCopyPass(cmd);
        CHECK(copy);
        SDL_GPUTextureRegion region{};
        region.texture = capture.texture;
        region.w = 640;
        region.h = 420;
        region.d = 1;
        SDL_GPUTextureTransferInfo target{};
        target.transfer_buffer = capture.transfer;
        target.pixels_per_row = 640;
        target.rows_per_layer = 420;
        SDL_DownloadFromGPUTexture(copy, &region, &target);
        SDL_EndGPUCopyPass(copy);
        CHECK(SDL_SubmitGPUCommandBuffer(cmd) && SDL_WaitForGPUIdle(runtime.gpu));
        const auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(runtime.gpu, capture.transfer, false));
        CHECK(bytes);
        std::size_t filled = 0, missing = 0;
        for (std::size_t i = 0; i < 640 * 420 * 4; i += 4)
        {
            filled += bytes[i] != 6 || bytes[i + 1] != 9 || bytes[i + 2] != 13;
            missing += bytes[i] > 240 && bytes[i + 1] < 10 && bytes[i + 2] > 240;
        }
        SDL_UnmapGPUTransferBuffer(runtime.gpu, capture.transfer);
        CHECK(filled > 10000 && missing == 0 && g.objects.time() == time);
        CHECK(capture.save(path.string().c_str()));
        std::cout << "GPU checkpoint " << path.filename() << ": " << filled << " textured pixels, no fallback\n";
        return true;
    }
    bool trace_services(const GameSession& g, const char* phase, std::string& error)
    {
        for (const auto& row : service_coverage(g.server.service_calls))
        {
            CHECK(row.support != ServiceSupport::Unclassified &&
                  (row.support != ServiceSupport::Unavailable || row.calls == 0));
            std::cout << "TRAIN_TRACE\t" << phase << '\t' << row.name << '\t' << service_support_name(row.support)
                      << '\t' << row.calls << '\n';
        }
        CHECK(g.server.unavailable_history.empty());
        CHECK(g.server.message_stats.refused == 0);
        std::uint64_t door_sounds = 0;
        for (auto id : g.objects.snapshot())
            if (const auto* door = dynamic_cast<const Door*>(g.objects.get(id)->game.get()))
            {
                CHECK(door->sound_failures == 0 && door->sound_requests == door->sound_starts);
                door_sounds += door->sound_starts;
            }
        if (std::string_view(phase) == "SubwayA")
            CHECK(door_sounds > 0);
        std::cout << "TRAIN_DOOR_AUDIO\t" << phase << '\t' << door_sounds << " decoded starts, zero failures\n";
        return true;
    }

    bool traverse(const std::filesystem::path& root, unsigned hz, const std::filesystem::path& captures)
    {
        std::string error;
        Runtime runtime;
        const bool gpu = !captures.empty();
        if (gpu)
        {
            CHECK(runtime.initialize(true, error));
            std::cout << "GPU device: "
                      << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                               "unreported")
                      << '\n';
            std::error_code ec;
            std::filesystem::create_directories(captures, ec);
            CHECK(!ec);
        }
        const double dt = 1. / hz;
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        auto& g = *game;
        if (!g.initialize() || !g.resources.mount_game(config, error) || !g.request_world("01_SubwayA", error) ||
            !g.enter_player(error) || !test_camera::complete_subway_intro(g))
            return false;
        for (const auto& entry : g.entries)
            CHECK(entry.state != EntryState::Unsupported && entry.state != EntryState::Failed);
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        const auto initial_position = g.objects.get(p->m_hObject)->state.m_Pos;
        CHECK(initial_position.z < -3000);
        HOBJECT relay = nullptr;
        for (auto id : g.objects.snapshot())
            if (std::string_view(g.objects.get(id)->state.m_Name) == "Trigger2")
                relay = g.objects.get(id)->handle;
        auto* trigger = dynamic_cast<Trigger*>(g.server.HandleToObject(relay));
        CHECK(trigger && trigger->activations == 0);
        std::set<HOBJECT> damaged, dead;
        bool ammo_pickup = false, health_pickup = false, black = false, middle_captured = false;
        if (gpu)
        {
            CHECK(g.server.audio.open_output(error));
            CHECK(capture_scene(g, runtime, captures / "start.png", error));
        }
        const auto id = gpu ? SDL_GetWindowID(runtime.window) : 8;
        MovementControls controls;
        controls.player_changed(true);
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = id;
        if (gpu)
        {
            CHECK(SDL_PushEvent(&focus));
            SDL_Event event{};
            while (SDL_PollEvent(&event))
                controls.event(event, id);
            CHECK(controls.sync_mouse(runtime.window, true, false, error));
            CHECK(controls.mouse_captured && SDL_GetWindowRelativeMouseMode(runtime.window));
        }
        else
        {
            controls.event(focus, id);
            controls.mouse_captured = true; // Pure mapping; native capture is verified separately.
        }
        auto input = [&](SDL_Event event)
        {
            if (gpu)
                return SDL_PushEvent(&event);
            controls.event(event, id);
            return true;
        };
        auto key = [&](SDL_Scancode code, bool down)
        {
            SDL_Event event{};
            event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.windowID = id;
            event.key.scancode = code;
            event.key.down = down;
            return input(event);
        };
        for (unsigned tick = 0; tick < 90 * hz && !g.pending_level_change; ++tick)
        {
            const float old_ammo = p->m_InventoryMgr.GetAmmoCount(1), old_health = p->m_damage.m_fHitPoints;
            std::map<HOBJECT, float> health;
            const auto position = g.objects.get(p->m_hObject)->state.m_Pos;
            HOBJECT target = nullptr;
            float closest = 1e20f;
            DVector aim{};
            for (auto id : g.objects.snapshot())
            {
                const auto* record = g.objects.get(id);
                const auto* ai = record ? dynamic_cast<const AI_Mgr*>(record->game.get()) : nullptr;
                if (ai && ai->runtime_failed)
                {
                    std::cerr << "AI failure " << record->state.m_Name << ": " << ai->runtime_error << '\n';
                    return false;
                }
                if (ai && ai->npc_type == AI_Mgr::NPCType::Cultist)
                {
                    health[record->handle] = ai->m_damage.m_fHitPoints;
                    if (ai->m_damage.IsDead() && damaged.contains(record->handle))
                        dead.insert(record->handle);
                }
                if (!ai || ai->npc_type != AI_Mgr::NPCType::Cultist || ai->m_damage.IsDead())
                    continue;
                DVector head;
                DRotation rotation;
                if (!g.server.GetModelNodeTransform(record->handle, "head", &head, &rotation))
                    return false;
                SegmentHit hit;
                const auto delta = vector_subtract(head, g.client_shell.view.position);
                const auto distance = vector_dot(delta, delta);
                if (distance < closest &&
                    g.server.IntersectSegment(g.client_shell.view.position, head, p->m_hObject, hit, false,
                                              SegmentObjects::SolidOrRayHit, SegmentFilter::IgnoreVolumes,
                                              SegmentStart::SkipInterior) == SegmentResult::Hit &&
                    hit.object == record->handle)
                {
                    target = record->handle;
                    closest = distance;
                    aim = delta;
                }
            }
            const bool melee = p->m_InventoryMgr.GetAmmoCount(1) == 0;
            const bool approach = !target || closest > (melee ? 55.f * 55.f : 220.f * 220.f);
            const bool use = tick % hz == 0;
            const float yaw = target && !use ? std::atan2(aim.x, aim.z) : 0;
            const float pitch = target && !use ? -std::atan2(aim.y, std::hypot(aim.x, aim.z)) : 0;
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = id;
            motion.motion.xrel = std::remainder(yaw - g.client_shell.rotation.yaw, 6.28318530718f) / .0025f;
            motion.motion.yrel = (pitch - g.client_shell.rotation.pitch) / .0025f;
            CHECK(input(motion));
            SDL_Event fire{};
            fire.type = target ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            fire.button.windowID = id;
            fire.button.button = SDL_BUTTON_LEFT;
            fire.button.down = target != nullptr;
            CHECK(input(fire));
            const float center = position.z < 2000 ? -32.f : -80.f;
            CHECK(key(SDL_SCANCODE_W, approach));
            CHECK(key(SDL_SCANCODE_A, !target && position.x > center + 4));
            CHECK(key(SDL_SCANCODE_D, !target && position.x < center - 4));
            CHECK(key(SDL_SCANCODE_E, use));
            CHECK(key(melee ? SDL_SCANCODE_1 : SDL_SCANCODE_2, true));
            CHECK(key(melee ? SDL_SCANCODE_1 : SDL_SCANCODE_2, false));
            if (gpu)
            {
                SDL_Event event{};
                while (SDL_PollEvent(&event))
                    controls.event(event, id);
                CHECK(controls.sync_mouse(runtime.window, true, false, error) && controls.mouse_captured);
            }
            const auto slot = controls.sample_weapon_slot(false);
            if (slot >= 0 && !g.command_weapon(static_cast<DBYTE>(slot), error))
                return false;
            controls.look(g.client_shell.rotation, dt, false);
            const auto buttons = controls.sample_fire(false);
            g.client_shell.weapon_input = {buttons.primary, buttons.alternate, false};
            g.client_shell.movement_input = controls.sample(false);
            g.client_shell.use_down = controls.sample_use(false);
            if (tick % (2 * hz) == 0)
                std::cout << "Walk t=" << float(tick) / hz << " pos=" << position.x << ',' << position.y << ','
                          << position.z << " HP=" << p->m_damage.m_fHitPoints
                          << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
                          << " target=" << (target ? g.objects.get(target)->state.m_Name : "none") << std::endl;
            if (!g.step(dt) || p->m_damage.IsDead())
            {
                for (const auto& line : g.trace)
                    std::cerr << line << '\n';
                std::cerr << "Step/death failure HP=" << p->m_damage.m_fHitPoints << '\n';
                return false;
            }
            for (const auto& [handle, hp] : health)
            {
                auto* record = g.objects.get(handle);
                auto* ai = record ? dynamic_cast<AI_Mgr*>(record->game.get()) : nullptr;
                if (ai && ai->m_damage.m_fHitPoints < hp && ai->m_damage.GetLastDamager() == p->m_hObject)
                    damaged.insert(handle);
                if (damaged.contains(handle) && (!ai || ai->m_damage.IsDead()))
                    dead.insert(handle);
            }
            ammo_pickup |= p->m_InventoryMgr.GetAmmoCount(1) > old_ammo;
            health_pickup |= p->m_damage.m_fHitPoints > old_health;
            black |= g.client_shell.fade.Scale() == 0;
            if (gpu && !middle_captured && position.z > 0)
            {
                CHECK(capture_scene(g, runtime, captures / "middle.png", error));
                middle_captured = true;
            }
        }
        std::cout << "Route summary: damaged=" << damaged.size() << " dead=" << dead.size()
                  << " ammoPickup=" << ammo_pickup << " healthPickup=" << health_pickup
                  << " relay=" << trigger->activations << " relayStart=" << trigger->delay_start
                  << " fade=" << p->fade_messages << " time=" << g.server.GetTime() << '\n';
        for (const auto& slot : trigger->slots)
            std::cout << "Relay slot " << slot.target << " sent=" << slot.sent << " pending=" << slot.pending << '\n';
        for (const auto& line : g.objects.diagnostics())
            std::cout << "Object diagnostic: " << line << '\n';
        if (!g.pending_level_change)
            for (const auto& [name, calls] : g.server.service_calls)
                std::cout << "Failed route calls " << name << '=' << calls << '\n';
        const auto knife_shots = p->m_InventoryMgr.GetWeapon(0)->shots;
        // Source frame histories leave different dropped pickups on this fixed
        // route. Require real dry-ammo knife firing at 30 Hz and ammunition
        // collection at 60 Hz, without diagnostic ammo/stat grants.
        CHECK(damaged.size() >= 4 && dead.size() >= 4 && health_pickup);
        CHECK(hz == 30 ? knife_shots > 0 : ammo_pickup);
        CHECK(trigger->activations == 1 && trigger->last_sender == p->m_hObject && black);
        for (const auto& slot : trigger->slots)
            CHECK(slot.sent == 1);
        CHECK(g.client_shell.weapons.front().fire_keys > 0 && p->fade_messages == 1);
        CHECK(p->unsupported_triggers == 1 && p->last_unsupported_player_trigger == "MUSIC LEVEL 3");
        CHECK(trace_services(g, "SubwayA", error));
        const float health = p->m_damage.m_fHitPoints, ammo = p->m_InventoryMgr.GetAmmoCount(1);
        const auto revision = g.world_revision;
        std::cout << "Completed train at " << hz << " Hz: " << damaged.size() << " damaged / " << dead.size()
                  << " dead authored Cultists, HP=" << health << " ammo=" << ammo << "\n";
        std::cout << "Message lifetime: " << g.server.message_stats.created << " created, "
                  << g.server.message_stats.released << " released, peak=" << g.server.message_stats.peak_live << '\n';
        std::cout << "Knife FIRE_KEY shots=" << knife_shots << '\n';
        if (!g.pending_level_change)
            return false;
        std::cout << "Reached authored NEW_LEVEL from spawn\n";
        if (GameSession::process_level_change(game, error) != LevelChangeResult::Committed)
        {
            std::cerr << error << '\n';
            return false;
        }
        CHECK(game->world_path == "worlds/02_cityhub.dat" && game->world_revision > revision);
        CHECK(game->restoration.restored == 1 && game->level_change.cleanup_complete);
        CHECK(game->level_change.selected_start == "01_egressa" && !game->level_change.start_fallback);
        p = game->server_shell.Player(game->local_client);
        CHECK(p && p->m_damage.m_fHitPoints == health && p->m_InventoryMgr.GetAmmoCount(1) == ammo);
        CHECK(game->client_shell.fade.Scale() == 1 && !game->client_shell.fade.messages);
        controls.player_changed();
        if (gpu)
            CHECK(controls.sync_mouse(runtime.window, true, false, error) &&
                  !SDL_GetWindowRelativeMouseMode(runtime.window));
        CHECK(!controls.sample_fire(false).primary && !controls.sample_use(false));
        for (unsigned tick = 0; tick < hz; ++tick)
            CHECK(game->step(dt));
        if (gpu)
        {
            CHECK(game->server.audio.has_output() && game->server.audio.submitted_frames > 0);
            CHECK(capture_scene(*game, runtime, captures / "cityhub.png", error));
        }
        CHECK(trace_services(*game, "CityHub", error));
        CHECK(game->unload_world() && game->objects.snapshot().empty());
        CHECK(!game->server.string_count() && !game->server.list_count() && !game->server.message_count() &&
              !game->server.client_count() && game->server.audio.voices().empty());
        CHECK(trace_services(*game, "teardown", error));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 3 || argc > 4 || (std::string_view(argv[2]) != "30" && std::string_view(argv[2]) != "60"))
        return 2;
    return traverse(argv[1], std::string_view(argv[2]) == "30" ? 30 : 60, argc == 4 ? argv[3] : "") ? 0 : 1;
}
