#include "app/model_scene.h"
#include "app/movement_controls.h"
#include "camera_fixture.h"
#include "engine/capture.h"
#include "engine/runtime.h"
#include "engine/world_renderer.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Door.h"
#include "game/original/Trigger.h"
#include "game/save_slots.h"
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
    // Inspect failures independently of attempted-service counts. Kept in the
    // route harness so acceptance cannot turn a positive counter into success.
    // `restored`: the owner came from a mid-route load. The original creates the held model only in
    // CInventoryMgr's MID_UPDATE, and an inactive NPC gets none, so one killed before it woke has no
    // weapon in hand and DropCurrentWeapon refuses silently (CHWEAP_NOTAVAIL), as in the original.
    bool check_inventory(const CInventoryMgr& inventory, std::string& error, bool restored = false)
    {
        error = restored && inventory.drop_error == "No live current held weapon" ? "" : inventory.drop_error;
        CHECK(error.empty());
        for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
            for (bool left : {false, true})
                if (const auto* weapon = inventory.GetWeapon(slot, left))
                {
                    error =
                        weapon->fire_sound_error + weapon->hand_error + weapon->projectile_error + weapon->impact_error;
                    CHECK(error.empty() && !weapon->sound_failures && !weapon->sound_alert_failures &&
                          !weapon->impact_sound_failures);
                    for (const auto& impact : weapon->impacts)
                    {
                        error = impact.effect.error + impact.effect.sprite_error + impact.effect.blood_error +
                                impact.effect.spark_error;
                        CHECK(error.empty());
                    }
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
    struct RouteAudit
    {
        std::set<std::uint64_t> absent_blood_sounds;
    };
    bool check_effects(const GameSession& g, RouteAudit& audit, const char* phase, std::string& error)
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
                {
                    if constexpr (requires { report.sound_resource; })
                    {
                        // Original GetRandom(0,NRES(5)) requests this absent retail
                        // file. Preserve refusal, never count it as decoded audio.
                        if (!report.sound_error.empty() &&
                            report.sound_resource == "sounds/gibs/flesh/gib_impact0.wav" &&
                            !g.resources.find(report.sound_resource) &&
                            report.sound_error == "Missing or oversized sound resource")
                            audit.absent_blood_sounds.insert(report.id);
                        else
                            error += report.sound_error;
                    }
                    else
                        error += report.sound_error;
                }
                CHECK(error.empty());
            }
            return true;
        };
        const auto& s = g.server;
        CHECK(check(s.explosion_sprites) && check(s.sparks) && check(s.blood_spurts) && check(s.blood_splats) &&
              check(s.weapon_sprites) && check(s.explosion_models) && check(s.laser_beams) &&
              check(s.object_particles) && check(s.polygrids) && check(s.particle_streams) &&
              check(s.explosion_lights) && check(s.explosion_rings) && check(s.explosion_flames) &&
              check(s.flesh_gibs) && check(s.explosion_fragments));
        for (const auto& report : s.scene_explosions)
        {
            error = report.sound_error + report.fragment_error + report.flame_error + report.ring_error +
                    report.light_error + report.model_error + report.sprite_error;
            CHECK(error.empty());
        }
        for (const auto& [id, voice] : s.audio.voices())
        {
            error = voice.error;
            CHECK(error.empty());
        }
        const auto& client = g.client_shell;
        // client.blocker is a historical scope note, not a failure result.
        error = client.objectives_error + client.item_audio_error + client.world_exit.error + s.audio.music_error +
                s.audio.output_error;
        CHECK(error.empty() && !client.voice_manager.failures && !client.communication.sound_failures &&
              !client.music.failures);
        if (phase)
            std::cout << "TRAIN_EFFECTS\t" << phase << '\t' << reports
                      << " retained FX reports; unexpected failures=0; original absent gib_impact0 requests="
                      << audit.absent_blood_sounds.size() << '\n';
        return true;
    }
    bool audit_rejections()
    {
        GameSession g;
        RouteAudit audit;
        std::string error;
        CHECK(check_effects(g, audit, nullptr, error));
        BloodSpurtReport report;
        report.id = 1;
        report.sound_resource = "sounds/gibs/flesh/gib_impact0.wav";
        report.sound_error = "Missing or oversized sound resource";
        g.server.blood_spurts.reports.push_back(report);
        CHECK(check_effects(g, audit, nullptr, error) && audit.absent_blood_sounds.size() == 1);
        g.server.blood_spurts.reports.back().sound_resource = "sounds/gibs/flesh/gib_impact1.wav";
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.blood_spurts.reports.back().sound_resource = report.sound_resource;
        g.server.blood_spurts.reports.back().sound_error = "Audio decode failed";
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.blood_spurts.clear();
        g.server.weapon_sprites.failed = 1;
        CHECK(!check_effects(g, audit, nullptr, error));
        g.server.weapon_sprites.clear();
        g.client_shell.voice_manager.failures = 1;
        CHECK(!check_effects(g, audit, nullptr, error));
        std::cout << "Acceptance negative controls: unrelated absent sound, decode, FX and voice failures rejected\n";
        return true;
    }
    bool trace_services(const GameSession& g, RouteAudit& audit, const char* phase, std::string& error)
    {
        CHECK(check_effects(g, audit, phase, error));
        for (const auto& row : service_coverage(g.server.service_calls))
        {
            error = std::string(row.name);
            CHECK(row.support != ServiceSupport::Unclassified &&
                  (row.support != ServiceSupport::Unavailable || row.calls == 0));
            error.clear();
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

    bool traverse(const std::filesystem::path& root, unsigned hz, const std::filesystem::path& captures,
                  bool save_midway)
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
        RouteAudit audit;
        const double dt = 1. / hz;
        auto game = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        GameSession* session = game.get(); // Replaced by a mid-route load.
        if (!session->initialize() || !session->resources.mount_game(config, error) ||
            !session->request_world("01_SubwayA", error) || !session->run_world(error) ||
            !test_camera::complete_subway_intro(*session))
            return false;
        for (const auto& entry : session->entries)
            CHECK(entry.state != EntryState::Unsupported && entry.state != EntryState::Failed);
        auto* p = session->server_shell.Player(session->local_client);
        CHECK(p);
        const auto initial_position = session->objects.get(p->m_hObject)->state.m_Pos;
        CHECK(initial_position.z < -3000);
        HOBJECT relay = nullptr;
        for (auto id : session->objects.snapshot())
            if (std::string_view(session->objects.get(id)->state.m_Name) == "Trigger2")
                relay = session->objects.get(id)->handle;
        auto* trigger = dynamic_cast<Trigger*>(session->server.HandleToObject(relay));
        CHECK(trigger && trigger->activations == 0);
        std::set<HOBJECT> damaged, dead;
        bool ammo_pickup = false, health_pickup = false, black = false, middle_captured = false, saved = false;
        std::size_t dead_before_save = 0;
        // Inspection counters are per session: the restored session starts them again.
        std::uint64_t music_messages_before = 0, music_received_before = 0, fire_keys_before = 0;
        if (gpu)
        {
            CHECK(session->server.audio.open_output(error));
            CHECK(capture_scene(*session, runtime, captures / "start.png", error));
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
        for (unsigned tick = 0; tick < 90 * hz && !session->pending_level_change; ++tick)
        {
            if (save_midway && !saved && session->objects.get(p->m_hObject)->state.m_Pos.z > 0)
            {
                // Save/load lot 14: save mid-level, round-trip the container bytes, load, and continue
                // the same route in the restored session. Handles are new after the load, so the
                // wounded Cultists are carried by name and the dead ones by count.
                saved = true;
                CHECK(trigger->activations == 0);
                std::set<std::string> wounded;
                for (auto h : damaged)
                    if (!dead.contains(h) && session->objects.get(h))
                        wounded.insert(session->objects.get(h)->state.m_Name);
                dead_before_save = dead.size();
                music_messages_before = p->music_messages;
                music_received_before = session->client_shell.music.received;
                fire_keys_before = session->client_shell.weapons.front().fire_keys;
                GameSave save, loaded;
                CHECK(session->save_game(save, error));
                SaveMeta meta;
                meta.kind = SaveKind::Quick;
                meta.display_name = "route";
                meta.world_path = save.world_path;
                meta.world_title = "route";
                meta.version = "route";
                SaveContainer container, decoded;
                std::vector<std::uint8_t> bytes;
                SaveError fault;
                CHECK(save_container(save, meta, container, error) && encode_save(container, bytes, fault));
                CHECK(decode_save(bytes, decoded, fault) && game_save_from(decoded, loaded, error));
                if (!GameSession::load_game(game, loaded, error))
                {
                    std::cerr << "Mid-route load refused at " << game->game_load.stage << ": " << error << '\n';
                    return false;
                }
                session = game.get();
                p = session->server_shell.Player(session->local_client);
                CHECK(p);
                trigger = nullptr;
                damaged.clear();
                dead.clear();
                for (auto object : session->objects.snapshot())
                {
                    const auto* record = session->objects.get(object);
                    if (std::string_view(record->state.m_Name) == "Trigger2")
                        trigger = dynamic_cast<Trigger*>(record->game.get());
                    if (wounded.contains(record->state.m_Name) && dynamic_cast<const AI_Mgr*>(record->game.get()))
                        damaged.insert(record->handle);
                }
                CHECK(trigger && trigger->activations == 0);
                std::cout << "Saved and loaded mid-route at t=" << float(tick) / hz << " (" << bytes.size()
                          << " bytes, " << session->game_load.records << " records); " << dead_before_save
                          << " dead and " << damaged.size() << " wounded Cultists carried" << std::endl;
            }
            const float old_ammo = p->m_InventoryMgr.GetAmmoCount(1), old_health = p->m_damage.m_fHitPoints;
            std::map<HOBJECT, float> health;
            const auto position = session->objects.get(p->m_hObject)->state.m_Pos;
            HOBJECT target = nullptr;
            float closest = 1e20f;
            DVector aim{};
            for (auto id : session->objects.snapshot())
            {
                const auto* record = session->objects.get(id);
                const auto* ai = record ? dynamic_cast<const AI_Mgr*>(record->game.get()) : nullptr;
                if (ai && ai->runtime_failed)
                {
                    std::cerr << "AI failure " << record->state.m_Name << ": " << ai->runtime_error << '\n';
                    return false;
                }
                if (ai)
                {
                    CHECK(check_inventory(ai->m_InventoryMgr, error, ai->restored_loads > 0));
                    error = ai->ai_key_error + ai->gib_error + ai->death_error + ai->damage_reaction_error;
                    CHECK(error.empty() && !ai->key_failures && !ai->unavailable_keys);
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
                if (!session->server.GetModelNodeTransform(record->handle, "head", &head, &rotation))
                    return false;
                SegmentHit hit;
                const auto delta = vector_subtract(head, session->client_shell.view.position);
                const auto distance = vector_dot(delta, delta);
                if (distance < closest &&
                    session->server.IntersectSegment(session->client_shell.view.position, head, p->m_hObject, hit,
                                                     false, SegmentObjects::SolidOrRayHit, SegmentFilter::IgnoreVolumes,
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
            motion.motion.xrel = std::remainder(yaw - session->client_shell.rotation.yaw, 6.28318530718f) / .0025f;
            motion.motion.yrel = (pitch - session->client_shell.rotation.pitch) / .0025f;
            CHECK(input(motion));
            SDL_Event fire{};
            // Finish the 30 Hz dry-ammo probe through normal firing after the relay.
            // Authored pickups may now leave bullets after the last enemy dies.
            const bool dry_ammo_probe = hz == 30 && trigger->activations && p->m_InventoryMgr.GetWeapon(0)->shots == 0;
            // Let an alerted, wounded Cultist answer before finishing the first fight.
            // This exercises a real incoming impulse independently of our kill cadence.
            const bool receive_fire = hz == 30 && target && !damaged.empty() && p->m_damage.damage_impulses == 0;
            const bool firing = !receive_fire && (target || dry_ammo_probe);
            fire.type = firing ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            fire.button.windowID = id;
            fire.button.button = SDL_BUTTON_LEFT;
            fire.button.down = firing;
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
            if (slot >= 0 && !session->command_weapon(static_cast<DBYTE>(slot), error))
                return false;
            controls.look(session->client_shell.rotation, dt, false);
            const auto buttons = controls.sample_fire(false);
            session->client_shell.weapon_input = {buttons.primary, buttons.alternate, false};
            session->client_shell.movement_input = controls.sample(false);
            session->client_shell.use_down = controls.sample_use(false);
            if (tick % (2 * hz) == 0)
                std::cout << "Walk t=" << float(tick) / hz << " pos=" << position.x << ',' << position.y << ','
                          << position.z << " HP=" << p->m_damage.m_fHitPoints
                          << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
                          << " target=" << (target ? session->objects.get(target)->state.m_Name : "none") << std::endl;
            if (!session->step(dt) || p->m_damage.IsDead())
            {
                for (const auto& line : session->trace)
                    std::cerr << line << '\n';
                std::cerr << "Step/death failure HP=" << p->m_damage.m_fHitPoints << '\n';
                return false;
            }
            for (const auto& [handle, hp] : health)
            {
                auto* record = session->objects.get(handle);
                auto* ai = record ? dynamic_cast<AI_Mgr*>(record->game.get()) : nullptr;
                if (ai && ai->m_damage.m_fHitPoints < hp && ai->m_damage.GetLastDamager() == p->m_hObject)
                    damaged.insert(handle);
                if (damaged.contains(handle) && (!ai || ai->m_damage.IsDead()))
                    dead.insert(handle);
            }
            CHECK(check_effects(*session, audit, nullptr, error));
            CHECK(check_actor(*p, error));
            error = p->entry_error + p->locomotion_error + p->death_error + p->death_effect_error +
                    p->player_trigger_error + p->sound_error + p->smell_error;
            CHECK(error.empty() && !p->unsupported_triggers && !p->sound_failures && !p->voice_delivery_failures &&
                  !p->activation_ping_failures && !p->unavailable_sound_alerts);
            ammo_pickup |= p->m_InventoryMgr.GetAmmoCount(1) > old_ammo;
            health_pickup |= p->m_damage.m_fHitPoints > old_health;
            black |= session->client_shell.fade.Scale() == 0;
            if (gpu && !middle_captured && position.z > 0)
            {
                CHECK(capture_scene(*session, runtime, captures / "middle.png", error));
                middle_captured = true;
            }
        }
        std::cout << "Route summary: damaged=" << damaged.size() << " dead=" << dead.size()
                  << " ammoPickup=" << ammo_pickup << " healthPickup=" << health_pickup
                  << " relay=" << trigger->activations << " relayStart=" << trigger->delay_start
                  << " fade=" << p->fade_messages << " time=" << session->server.GetTime() << '\n';
        for (const auto& slot : trigger->slots)
            std::cout << "Relay slot " << slot.target << " sent=" << slot.sent << " pending=" << slot.pending << '\n';
        for (const auto& line : session->objects.diagnostics())
            std::cout << "Object diagnostic: " << line << '\n';
        if (!session->pending_level_change)
            for (const auto& [name, calls] : session->server.service_calls)
                std::cout << "Failed route calls " << name << '=' << calls << '\n';
        const auto knife_shots = p->m_InventoryMgr.GetWeapon(0)->shots;
        // Source frame histories leave different dropped pickups on this fixed
        // route. Require real dry-ammo knife firing at 30 Hz and ammunition
        // collection at 60 Hz, without diagnostic ammo/stat grants. 120 Hz guards
        // high-refresh per-update emission (blood spurts); its pickups are not fixed. Neither are
        // those of the mid-route save variant, whose leaf-list activation wakes a different set of
        // Cultists before the save than the plain 60 Hz run.
        CHECK(!save_midway || saved);
        CHECK(damaged.size() + dead_before_save >= 4 && dead.size() + dead_before_save >= 4 && health_pickup);
        CHECK(hz == 30 ? knife_shots > 0 : hz == 120 || save_midway || ammo_pickup);
        CHECK(trigger->activations == 1 && trigger->last_sender == p->m_hObject && black);
        for (const auto& slot : trigger->slots)
            CHECK(slot.sent == 1);
        CHECK(session->client_shell.weapons.front().fire_keys + fire_keys_before > 0 && p->fade_messages == 1);
        std::cout << "Music: player messages " << p->music_messages << ", received "
                  << session->client_shell.music.received
                  << ", level " << int(session->client_shell.music.logical_level) << ", loop "
                  << session->server.audio.music_loop.active() << '/' << int(session->server.audio.music_loop.level())
                  << '\n';
        CHECK(p->unsupported_triggers == 0 && p->music_messages + music_messages_before == 1 &&
              session->client_shell.music.received + music_received_before == 1 &&
              session->client_shell.music.logical_level == 3 && session->client_shell.music.failures == 0 &&
              session->server.audio.music_loop.active() && session->server.audio.music_loop.level() == 3 &&
              session->server.audio.music_error.empty());
        CHECK(p->m_damage.apply_damage_physics && p->m_damage.last_impulse_ok);
        // The 30 Hz driver takes enemy hits; the 60 Hz route can finish unhurt.
        if (hz == 30)
            CHECK(p->m_damage.damage_impulses > 0);
        std::cout << "Player damage impulses=" << p->m_damage.damage_impulses << '\n';
        CHECK(trace_services(*session, audit, "SubwayA", error));
        const float health = p->m_damage.m_fHitPoints, ammo = p->m_InventoryMgr.GetAmmoCount(1);
        const auto revision = session->world_revision;
        std::cout << "Completed train at " << hz << " Hz: " << damaged.size() << " damaged / " << dead.size()
                  << " dead authored Cultists, HP=" << health << " ammo=" << ammo << "\n";
        std::cout << "Message lifetime: " << session->server.message_stats.created << " created, "
                  << session->server.message_stats.released
                  << " released, peak=" << session->server.message_stats.peak_live << '\n';
        std::cout << "Knife FIRE_KEY shots=" << knife_shots << '\n';
        if (!session->pending_level_change)
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
        CHECK(p->m_damage.apply_damage_physics && p->m_damage.damage_impulses == 0);
        CHECK(game->client_shell.fade.Scale() == 1 && !game->client_shell.fade.messages);
        controls.player_changed();
        if (gpu)
            CHECK(controls.sync_mouse(runtime.window, true, false, error) &&
                  !SDL_GetWindowRelativeMouseMode(runtime.window));
        CHECK(!controls.sample_fire(false).primary && !controls.sample_use(false));
        audit.absent_blood_sounds.clear();
        CHECK(game->run_world(error));
        for (unsigned tick = 0; tick < hz; ++tick)
        {
            CHECK(game->step(dt));
            CHECK(check_effects(*game, audit, nullptr, error));
        }
        if (gpu)
        {
            CHECK(game->server.audio.has_output() && game->server.audio.submitted_frames > 0);
            CHECK(capture_scene(*game, runtime, captures / "cityhub.png", error));
        }
        CHECK(trace_services(*game, audit, "CityHub", error));
        CHECK(game->unload_world() && game->objects.snapshot().empty());
        CHECK(!game->server.string_count() && !game->server.list_count() && !game->server.message_count() &&
              !game->server.client_count() && game->server.audio.voices().empty());
        CHECK(trace_services(*game, audit, "teardown", error));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "audit")
        return audit_rejections() ? 0 : 1;
    const std::string_view rate = argc >= 3 ? argv[2] : "";
    if (argc < 3 || argc > 5 || (rate != "30" && rate != "60" && rate != "120"))
        return 2;
    // [--save] [captures]: the save variant saves and loads mid-route (save/load lot 14).
    const bool save = argc >= 4 && std::string_view(argv[3]) == "--save";
    const std::filesystem::path captures = argc == 5 ? argv[4] : argc == 4 && !save ? argv[3] : "";
    return traverse(argv[1], rate == "30" ? 30 : rate == "60" ? 60 : 120, captures, save) ? 0 : 1;
}
