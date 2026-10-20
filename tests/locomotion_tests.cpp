#include "app/movement_controls.h"
#include "camera_fixture.h"
#include "game/original/CharacterProfiles.h"
#include "player_fixture.h"
#include <cmath>
#include <functional>
#include <limits>
#include <random>
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
    bool step(GameSession& game, int frames = 1)
    {
        for (int i = 0; i < frames; ++i)
            if (!game.step(1.0 / 60))
            {
                const auto* p = game.server_shell.Player(game.local_client);
                std::cerr << "Step failed: "
                          << (p                    ? p->locomotion_error
                              : game.trace.empty() ? "no player"
                                                   : game.trace.back())
                          << '\n';
                return false;
            }
        return true;
    }
    bool enter(GameSession& game, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(game, root));
        std::string error;
        CHECK(game.enter_player(error));
        game.client_shell.rotation = {};
        CHECK(step(game, 2));
        return true;
    }
    HOBJECT box(GameSession& game, DVector p, DVector d)
    {
        ObjectCreateStruct state;
        state.m_Pos = p;
        state.m_Flags = FLAG_SOLID;
        auto h = game.objects.create(game.objects.find_class("BaseClass"), state);
        game.server.SetObjectDims(h, &d);
        return h;
    }
    bool stairs(const std::filesystem::path& root, bool world)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto& c = game.client_shell;
        if (world)
        {
            WorldModel mesh;
            mesh.surfaces.resize(1);
            mesh.surfaces[0].flags = 1;
            auto quad = [&](DVector a, DVector b, DVector c, DVector d)
            {
                const auto n = static_cast<std::uint32_t>(mesh.points.size());
                mesh.points.insert(mesh.points.end(), {a, b, c, d});
                mesh.polygons.push_back({0,
                                         {static_cast<std::uint16_t>(n), static_cast<std::uint16_t>(n + 1),
                                          static_cast<std::uint16_t>(n + 2), static_cast<std::uint16_t>(n + 3)}});
            };
            quad({-500, 0, -500}, {500, 0, -500}, {500, 0, 500}, {-500, 0, 500});
            for (int i = 0; i < 3; ++i)
            {
                const float z = 100.0f + i * 50, y = 16.0f * (i + 1);
                quad({-100, y - 16, z}, {100, y - 16, z}, {100, y, z}, {-100, y, z});
                quad({-100, y, z}, {100, y, z}, {100, y, z + 50}, {-100, y, z + 50});
            }
            std::string error;
            CHECK(game.collision.build(mesh, error));
        }
        else
            for (int i = 0; i < 3; ++i)
                CHECK(box(game, {32, 8.0f * (i + 1), 125.0f + i * 50}, {100, 8.0f * (i + 1), 25}));
        c.movement_input.forward = true;
        CHECK(step(game, 39));
        CHECK(c.position.z > 200 && c.position.z < 250);
        CHECK(std::abs(c.position.y - 89.001f) < .02 && c.move_mgr.m_bOnGround);
        CHECK(c.move_mgr.body.stair_steps == 3 && c.move_mgr.body.last_step_height > 15.9f);
        c.movement_input = {};
        CHECK(step(game, 10));
        // Walking off the top regains gravity instead of snapping through a drop.
        c.movement_input.forward = true;
        CHECK(step(game, 40));
        CHECK(c.move_mgr.m_bOnGround && std::abs(c.position.y - 41.001f) < .02);
        return true;
    }
    bool step_limits(const std::filesystem::path& root)
    {
        for (int mode = 0; mode < 4; ++mode)
        {
            GameSession game;
            CHECK(enter(game, root));
            auto& c = game.client_shell;
            const float height = mode == 0 ? 21 : 16;
            CHECK(box(game, {32, height / 2, 125}, {100, height / 2, 25}));
            if (mode == 1)
                CHECK(box(game, {32, 90, 120}, {100, 2, 80})); // only 6 units headroom
            if (mode == 2)
                game.objects.get(c.player)->state.m_Flags &= ~FLAG_STAIRSTEP;
            if (mode == 3)
                c.movement_input.crouch = true; // half of crouched height is 13
            c.movement_input.forward = true;
            CHECK(step(game, 60));
            CHECK(c.position.z < 88.01 && c.move_mgr.body.stair_steps == 0);
        }
        // An unsupported body cannot climb, even when the step is within the limit.
        GameSession game;
        CHECK(enter(game, root));
        auto& c = game.client_shell;
        CHECK(box(game, {32, 8, 125}, {100, 8, 25}));
        DVector p{32, 46, 87}, velocity{0, 10, 100}, end{32, 46, 96};
        CHECK(game.server.TeleportObject(c.player, &p) == DE_OK);
        CHECK(game.server.SetVelocity(c.player, &velocity) == DE_OK);
        CHECK(game.server.MoveObject(c.player, &end) == DE_OK);
        CHECK(game.objects.get(c.player)->state.m_Pos.z < 88.01);
        CHECK(game.objects.get(c.player)->stair_steps == 0);
        return true;
    }
    bool crouch(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto& c = game.client_shell;
        auto* player = game.server_shell.Player(game.local_client);
        const float feet = c.position.y - c.move_mgr.body.dimensions.y;
        c.movement_input.crouch = true;
        CHECK(step(game));
        CHECK(player->crouched && !player->forced_crouch && c.move_mgr.body.dimensions.y == 26);
        CHECK(std::abs(c.position.y - 26 - feet) < .01 && std::abs(c.camera_position.y - c.position.y - 12) < .01);
        auto ceiling = box(game, {32, 65, 64}, {100, 5, 100}); // 60 units clearance: crouch fits, stand cannot.
        CHECK(ceiling);
        c.movement_input = {};
        CHECK(step(game, 20));
        CHECK(player->crouched && player->forced_crouch && player->blocked_resizes >= 20);
        CHECK(c.move_mgr.body.dimensions.y == 26 && c.position.y < 26.01);
        CHECK(player->Respawn()); // Alive entry reset under the same low ceiling.
        CHECK(player->crouched && player->eye_level == 12);
        CHECK(step(game));
        CHECK(player->forced_crouch && player->crouched && c.move_mgr.body.dimensions.y == 26);
        CHECK(game.server.ResizeObject(c.player, {12, 41, 12}) == ResizeResult::Blocked);
        CHECK(game.server.ResizeObject(c.player, {12, std::numeric_limits<float>::infinity(), 12}) ==
              ResizeResult::Invalid);
        CHECK(game.objects.remove(ceiling) && game.objects.finish_frame());
        CHECK(step(game));
        CHECK(!player->crouched && !player->forced_crouch && c.move_mgr.body.dimensions.y == 41);
        CHECK(std::abs(c.position.y - 41 - feet) < .01 && c.camera_position.y - c.position.y == 36);
        c.movement_input = {true, false, false, false, false, false, true};
        CHECK(step(game, 60));
        CHECK(std::abs(c.move_mgr.m_fVelMagnitude - 140) < .1 && player->locomotion_clip == "crawl_1pistol");
        c.movement_input.run = true;
        CHECK(step(game, 30));
        CHECK(std::abs(c.move_mgr.m_fVelMagnitude - 210) < .1);
        c.movement_input = {};
        c.movement_input.crouch = true;
        CHECK(step(game, 60));
        const float base = c.position.y;
        c.movement_input.jump = true;
        float peak = base;
        for (int i = 0; i < 90; ++i)
        {
            CHECK(step(game));
            peak = std::max(peak, c.position.y);
        }
        CHECK(peak - base > 48 && peak - base < 53 && c.move_mgr.jumps == 1);
        return true;
    }
    bool animations(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto& c = game.client_shell;
        auto* p = game.server_shell.Player(game.local_client);
        auto* r = game.objects.get(c.player);
        CHECK(p->locomotion_clip == "idle1" || p->locomotion_clip == "idle2");
        c.movement_input.forward = true;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "walk_pistol" && r->animation.looping);
        const auto revision = r->animation.revision;
        CHECK(step(game, 20));
        CHECK(r->animation.revision == revision && p->pending_model_commands.size() > 0);
        c.movement_input.run = true;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "run_pistol");
        c.movement_input.left = true;
        CHECK(step(game));
        CHECK(p->move_state == CPlayerObj::MoveState::StrafeLeft && p->locomotion_clip == "strafe_right_1pistol");
        c.movement_input.jump = true;
        CHECK(step(game, 20));
        CHECK(p->locomotion_clip == "jmp_pistol" && !r->animation.looping && r->animation.done);
        const auto jump_revision = r->animation.revision;
        CHECK(step(game, 10));
        CHECK(r->animation.revision == jump_revision);
        c.movement_input = {};
        CHECK(step(game, 60));
        CHECK(p->m_InventoryMgr.GetWeapon(0));
        CHECK(p->m_InventoryMgr.ChangeWeaponSlot(0) == InventoryResult::Ok);
        c.movement_input.forward = true;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "walk_knife1");
        c.movement_input.crouch = true;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "crawl_knife1");
        c.weapon_input.primary = true;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "fire_crawl_knife1");
        // A wholly absent chain is an invalid source index: like the engine's
        // ignored SetModelAnimation, the current clip and body are kept, the
        // clip restarts and only looping follows the request.
        auto model = std::make_shared<Model>(*r->model);
        model->animations[animation_index(*model, "run_knife1")].name = "absent";
        model->animations[animation_index(*model, "run_pistol")].name = "absent2";
        r->model = model;
        CHECK(p->m_Anim_Sound.SetAnimationIndexes(p->m_hObject));
        c.weapon_input = {};
        c.movement_input.crouch = false;
        c.movement_input.run = true;
        const auto old = r->animation.animation;
        const auto old_name = r->model->animations[old].name;
        const auto dims = r->dimensions;
        const auto changes = p->animation_changes;
        CHECK(step(game));
        CHECK(p->move_state == CPlayerObj::MoveState::Run && p->locomotion_requested_clip == "run_knife1");
        CHECK(p->locomotion_clip == old_name && p->locomotion_clip_retained && p->locomotion_error.empty());
        CHECK(r->animation.animation == old && r->dimensions.y == dims.y && r->animation.looping);
        CHECK(p->animation_changes == changes + 1 && p->retained_clip_requests == 1 && r->animation.time < 50);
        CHECK(step(game, 5));
        CHECK(p->animation_changes == changes + 1 && p->retained_clip_requests == 1);
        c.movement_input.run = false;
        CHECK(step(game));
        CHECK(p->locomotion_clip == "walk_knife1" && !p->locomotion_clip_retained);
        // Without any valid current clip the request still fails visibly.
        c.movement_input.run = true;
        const auto walk = r->animation.animation;
        r->animation.animation = static_cast<std::uint32_t>(r->model->animations.size());
        // Test the typed branch directly: GameSession intentionally tears down a
        // client on a failed step, so its raw player pointer must not be reused.
        CHECK(!game.server_shell.OnMessage(game.local_client, PlayerControls{c.movement_input, false, 0}));
        CHECK(p->locomotion_error.find("run_knife1") != std::string::npos);
        CHECK(p->locomotion_clip == "walk_knife1");
        r->animation.animation = walk;
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(enter(game, root));
        auto& c = game.client_shell;
        c.movement_input.crouch = true;
        CHECK(step(game));
        auto old = c.player;
        CHECK(game.leave_player());
        CHECK(!c.move_mgr.ready() && !c.movement_input.crouch);
        CHECK(game.server.ResizeObject(old, {12, 41, 12}) == ResizeResult::Invalid);
        std::string error;
        CHECK(game.enter_player(error));
        CHECK(step(game));
        CHECK(!game.server_shell.Player(game.local_client)->crouched && c.move_mgr.body.dimensions.y == 41);
        MovementControls controls;
        controls.enabled = controls.focused = true;
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = 1;
        event.key.scancode = SDL_SCANCODE_LCTRL;
        controls.event(event, 1);
        CHECK(controls.sample(false).crouch && controls.consumes(event, false));
        CHECK(!controls.sample(true).crouch);
        event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.windowID = 1;
        controls.event(event, 1);
        CHECK(!controls.sample(false).crouch);
        return true;
    }
    std::function<void(BaseClass&, DDWORD, void*)> contact_callback;
    struct Contact : BaseClass
    {
        DDWORD EngineMessageFn(DDWORD msg, void* data, float reason) override
        {
            if (contact_callback)
                contact_callback(*this, msg, data);
            return BaseClass::EngineMessageFn(msg, data, reason);
        }
    };
    std::unique_ptr<BaseClass> make_contact()
    {
        return std::make_unique<Contact>();
    }
    ClassInfo contact_class{"StairContact", nullptr, make_contact, {}};
    bool contacts(const std::filesystem::path& root)
    {
        for (bool blocked : {true, false})
        {
            GameSession game;
            CHECK(enter(game, root));
            auto player = game.client_shell.player;
            CHECK(game.objects.register_class(contact_class));
            const float height = blocked ? 21.0f : 16.0f;
            CHECK(box(game, {32, height / 2, 125}, {100, height / 2, 25}));
            ObjectCreateStruct state;
            state.m_Pos = {32, 91, 87};
            state.m_Flags = FLAG_TOUCH_NOTIFY;
            auto sensor = game.objects.create(&contact_class, state);
            DVector dims{1, .1f, 1};
            game.server.SetObjectDims(sensor, &dims);
            bool touched = false, guarded = false;
            contact_callback = [&](BaseClass&, DDWORD msg, void* data)
            {
                if (msg != MID_TOUCHNOTIFY || data != player)
                    return;
                touched = true;
                DVector destination{32, 100, 120};
                const auto dims = game.objects.get(player)->dimensions;
                // Unchanged extents are accepted inside the move; an actual change is refused.
                guarded = game.server.MoveObject(player, &destination) == DE_ERROR &&
                          game.server.ResizeObject(player, dims) == ResizeResult::Resized &&
                          game.server.ResizeObject(player, {dims.x, dims.y + 1, dims.z}) == ResizeResult::Invalid &&
                          !game.unload_world();
                game.objects.remove(player);
            };
            game.client_shell.movement_input.forward = true;
            const bool stepped = step(game, 60);
            contact_callback = {};
            CHECK(stepped);
            if (blocked)
                CHECK(!touched && game.objects.get(player));
            else
                CHECK(touched && guarded && !game.objects.get(player) && !game.local_client);
        }
        return true;
    }
    bool private_world(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
        CHECK(test_camera::complete_subway_intro(game));
        auto& c = game.client_shell;
        auto* r = game.objects.get(c.player);
        for (const auto& clip : r->model->animations)
            std::cout << clip.name << " [" << clip.dimensions.x << ',' << clip.dimensions.y << ',' << clip.dimensions.z
                      << "] " << clip.duration << "ms\n";
        CHECK(step(game, 20));
        auto* p = game.server_shell.Player(game.local_client);
        for (int i = 0; i < 3; ++i)
        {
            c.movement_input = {};
            c.movement_input.crouch = true;
            CHECK(step(game, 10));
            CHECK(p->crouched && p->eye_level == 12 && p->locomotion_clip == "crouch_rifle" && r->dimensions.y == 28);
            const auto low = r->dimensions.y;
            c.movement_input = {};
            CHECK(step(game, 10));
            CHECK(!p->crouched && r->dimensions.y > low);
        }
        c.movement_input.forward = true;
        CHECK(step(game, 10));
        CHECK(p->locomotion_clip == "walk_rifle");
        c.movement_input.run = true;
        CHECK(step(game, 10));
        CHECK(p->locomotion_clip == "run_rifle");
        c.movement_input.jump = true;
        CHECK(step(game, 10));
        CHECK(p->locomotion_clip == "jmp_rifle");
        c.movement_input = {};
        CHECK(step(game, 80));
        CHECK(c.move_mgr.m_bOnGround && c.move_mgr.jumps == 1);
        CHECK(game.unload_world() && game.objects.snapshot().empty() && game.server.string_count() == 0);
        return true;
    }
    // Installed Ishmael/Gabriella name their jumps jump_*, so the source jmp_*
    // chain is invalid for them; every Chosen must still walk, jump and land.
    bool private_chosen(const std::filesystem::path& root)
    {
        for (DBYTE character = 0; character < 4; ++character)
        {
            GameSession game;
            CHECK(game.initialize());
            Config config;
            config.data_dir = root;
            std::string error;
            CHECK(game.resources.mount_game(config, error));
            CHECK(select_character(character, game.client_shell.entry_profile));
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error));
            CHECK(test_camera::complete_subway_intro(game));
            auto& c = game.client_shell;
            auto* r = game.objects.get(c.player);
            CHECK(step(game, 20));
            auto* p = game.server_shell.Player(game.local_client);
            CHECK(p && p->m_nCharacter == character);
            c.movement_input.forward = c.movement_input.run = true;
            CHECK(step(game, 10));
            CHECK(p->move_state == CPlayerObj::MoveState::Run && p->locomotion_clip == "run_rifle");
            const auto run = r->animation.animation;
            c.movement_input.jump = true;
            CHECK(step(game, 10));
            CHECK(p->move_state == CPlayerObj::MoveState::Jump && p->locomotion_requested_clip == "jmp_pistol");
            const bool jump_clip = character < 2;
            CHECK(p->locomotion_clip_retained == !jump_clip && !r->animation.looping);
            CHECK(jump_clip ? p->locomotion_clip == "jmp_rifle"
                            : p->locomotion_clip == "run_rifle" && r->animation.animation == run);
            c.movement_input = {};
            CHECK(step(game, 80));
            CHECK(c.move_mgr.m_bOnGround && c.move_mgr.jumps == 1 && p->locomotion_error.empty());
            CHECK(p->retained_clip_requests == (jump_clip ? 0u : 1u));
            CHECK(game.unload_world());
        }
        return true;
    }
    // Seeded random roaming in CityHub at ~100 Hz: walk/run/strafe/jump/crouch,
    // turning and firing. Float-rounded crease/wedge contacts must not stop play.
    bool roam_cityhub(const std::filesystem::path& root, unsigned seed, DBYTE character, double seconds)
    {
        GameSession game;
        CHECK(game.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(game.resources.mount_game(config, error));
        CHECK(select_character(character, game.client_shell.entry_profile));
        CHECK(game.request_world("02_CityHub", error) && game.enter_player(error));
        auto& c = game.client_shell;
        std::mt19937 random(seed);
        std::uniform_real_distribution<float> unit(0, 1);
        double time = 0, next = 0;
        float yaw_speed = 0;
        DVector last{};
        while (time < seconds)
        {
            if (time >= next)
            {
                next = time + 0.1 + unit(random) * 0.8;
                auto& m = c.movement_input;
                m = {};
                m.forward = unit(random) < .6f;
                m.backward = !m.forward && unit(random) < .3f;
                m.left = unit(random) < .2f;
                m.right = !m.left && unit(random) < .2f;
                m.run = unit(random) < .6f;
                m.jump = unit(random) < .25f;
                m.crouch = unit(random) < .1f;
                c.weapon_input = {};
                c.weapon_input.primary = unit(random) < .15f;
                yaw_speed = (unit(random) - .5f) * 6;
            }
            c.rotation.yaw += yaw_speed * 0.01f;
            const double delta = 0.0095 + unit(random) * 0.001;
            if (auto* r = game.objects.get(c.player))
                last = r->state.m_Pos;
            if (!game.step(delta))
            {
                std::cerr << "seed " << seed << " character " << int(character) << " failed at t=" << time
                          << " last=" << last.x << ',' << last.y << ',' << last.z << '\n';
                for (auto& d : game.objects.diagnostics())
                    std::cerr << "  " << d << '\n';
                for (auto& t : game.trace)
                    std::cerr << "  " << t << '\n';
                return false;
            }
            time += delta;
            CHECK(game.local_client);
        }
        if (auto* r = game.objects.get(c.player))
            std::cout << "seed " << seed << " character " << int(character) << " at " << r->state.m_Pos.x << ','
                      << r->state.m_Pos.y << ',' << r->state.m_Pos.z << ", overlap contacts " << r->overlap_contacts
                      << ", last depth " << r->last_overlap_depth << '\n';
        return true;
    }
    bool cityhub_roam(const std::filesystem::path& root)
    {
        // Each pair stopped with a start penetration before the overlap tolerance.
        for (const auto& [seed, character] : {std::pair{1u, 0}, {4u, 0}, {1u, 1}, {3u, 3}})
            CHECK(roam_cityhub(root, seed, DBYTE(character), 30));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    // Reproduction hook: locomotion_tests roam DATA SEED CHARACTER SECONDS.
    if (argc == 6 && std::string_view(argv[1]) == "roam")
        return roam_cityhub(path_from_utf8(argv[2]), unsigned(std::stoul(argv[3])), DBYTE(std::stoul(argv[4])),
                            std::stod(argv[5]))
                   ? 0
                   : 1;
    if (argc != 3)
        return 2;
    const std::string_view name = argv[1];
    const auto root = path_from_utf8(argv[2]);
    const bool ok = name == "stairs"         ? stairs(root, false)
                    : name == "world_stairs" ? stairs(root, true)
                    : name == "limits"       ? step_limits(root)
                    : name == "crouch"       ? crouch(root)
                    : name == "animations"   ? animations(root)
                    : name == "lifetime"     ? lifetime(root)
                    : name == "contacts"     ? contacts(root)
                    : name == "private"      ? private_world(root)
                    : name == "chosen"       ? private_chosen(root)
                    : name == "cityhub_roam" ? cityhub_roam(root)
                                             : false;
    return ok ? 0 : 1;
}
