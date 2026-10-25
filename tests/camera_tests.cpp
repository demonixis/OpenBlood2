#include "app/control_actions.h"
#include "camera_fixture.h"
#include "game/game_settings.h"
#include "game/original/CameraObj.h"
#include "game/original/ClientCrosshair.h"
#include "player_fixture.h"
#include <functional>
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
    bool close(DVector a, DVector b, float e = .001f)
    {
        auto d = vector_subtract(a, b);
        return vector_dot(d, d) < e * e;
    }
    CameraObj* camera(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<CameraObj*>(g.server.HandleToObject(h));
    }
    HOBJECT create(GameSession& g, Properties props = {})
    {
        return g.objects.create(g.objects.find_class("CameraObj"), {}, Creation::World, props);
    }
    bool command(GameSession& g, HOBJECT h, const char* value)
    {
        auto text = g.server.CreateString(value);
        auto m = g.server.StartMessageToObject(nullptr, h, 1000);
        if (!m)
        {
            g.server.FreeString(text);
            return false;
        }
        auto written = g.server.WriteToMessageHString(m, text);
        auto sent = g.server.EndMessage(m);
        g.server.FreeString(text);
        return written == DE_OK && sent == DE_OK;
    }
    bool third_person(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        auto& shell = g.client_shell;
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p && !shell.third_person && close(shell.view.position, shell.camera_position));
        MovementControls controls;
        controls.focused = controls.enabled = controls.mouse_captured = true;
        const auto key = [&](bool down, bool repeat = false, bool blocked = false)
        {
            SDL_Event e{};
            e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            e.key.windowID = 1;
            e.key.scancode = SDL_SCANCODE_C;
            e.key.repeat = repeat;
            controls.event(e, 1);
            apply_control_actions(controls, shell, blocked);
        };
        const auto eye = shell.camera_position;
        const auto aim_rotation = shell.rotation;
        const auto initial_rotation = shell.view.rotation;
        key(true);
        CHECK(shell.third_person && shell.UpdateCameraSelection());
        CHECK(shell.third_person_active && shell.player_visible && !shell.weapons_visible && !shell.view.object);
        CHECK(!p->camera_movement_blocked && shell.player_look_allowed());
        CHECK(close(shell.camera_position, eye) && shell.rotation.yaw == aim_rotation.yaw &&
              shell.rotation.pitch == aim_rotation.pitch);
        CHECK(original_crosshair_visible(shell.m_bInWorld, shell.view.object != nullptr, shell.death_camera.active));
        const auto requested = vector_add(eye, rotate_vector(initial_rotation, {0, 20, -150}));
        CHECK(close(shell.view.position, requested));
        key(true, true);
        CHECK(shell.third_person); // OS repeat cannot toggle twice.
        key(false);
        key(true, false, true);
        CHECK(shell.third_person); // Suppressed input cannot toggle.
        key(false);
        // A solid box behind the player retracts the boom without moving the body.
        ObjectCreateStruct object;
        object.m_Pos = vector_add(eye, rotate_vector(initial_rotation, {0, 10, -75}));
        object.m_Flags = FLAG_SOLID;
        auto obstacle = g.objects.create(g.objects.find_class("BaseClass"), object, Creation::Runtime, {});
        DVector dims{10, 10, 10};
        CHECK(obstacle && g.server.SetObjectDims(obstacle, &dims) == DE_OK && shell.UpdateCameraSelection());
        const auto retracted = vector_subtract(shell.view.position, eye);
        CHECK(vector_dot(retracted, retracted) < 75 * 75 && shell.third_person_active);
        CHECK(close(shell.camera_position, eye));
        auto tight = vector_add(eye, rotate_vector(initial_rotation, {0, 1, -12}));
        CHECK(g.server.TeleportObject(obstacle, &tight) == DE_OK && shell.UpdateCameraSelection());
        CHECK(shell.third_person && !shell.third_person_active && !shell.player_visible && shell.weapons_visible &&
              close(shell.view.position, eye));
        CHECK(g.objects.remove(obstacle) && g.objects.finish_frame() && shell.UpdateCameraSelection());
        CHECK(close(shell.view.position, requested));
        // The screen centre points to the same near target as the eye ray.
        const auto forward = rotate_vector(initial_rotation, {0, 0, 1});
        object.m_Pos = vector_add(eye, vector_scale(forward, 200));
        auto target = g.objects.create(g.objects.find_class("BaseClass"), object, Creation::Runtime, {});
        CHECK(target && g.server.SetObjectDims(target, &dims) == DE_OK && shell.UpdateCameraSelection());
        SegmentHit hit;
        CHECK(g.server.IntersectSegment(eye, vector_add(eye, vector_scale(forward, 10000)), shell.player, hit, false,
                                        SegmentObjects::SolidOrRayHit, SegmentFilter::IgnoreVolumes,
                                        SegmentStart::SkipInterior) == SegmentResult::Hit &&
              hit.object == target);
        auto to_hit = vector_subtract(hit.point, shell.view.position);
        to_hit = vector_scale(to_hit, 1 / std::sqrt(vector_dot(to_hit, to_hit)));
        CHECK(close(rotate_vector(shell.view.rotation, {0, 0, 1}), to_hit));
        CHECK(g.objects.remove(target) && g.objects.finish_frame());
        for (float pitch : {-.6f, .6f})
        {
            shell.rotation.pitch = pitch;
            CHECK(shell.UpdateCameraSelection() && shell.third_person_active);
            const auto boom = vector_subtract(shell.view.position, shell.camera_position);
            CHECK(std::abs(vector_dot(boom, boom) - (150 * 150 + 20 * 20)) < .1f);
            CHECK(shell.rotation.pitch == pitch);
        }
        shell.rotation.pitch = aim_rotation.pitch;
        const auto body_before = shell.position;
        shell.movement_input.forward = true;
        for (int i = 0; i < 10; ++i)
            CHECK(g.step(.01));
        shell.movement_input.forward = false;
        CHECK(!close(shell.position, body_before) && shell.third_person_active && !p->camera_movement_blocked);
        // Hidden first-person weapons still run their real FIRE_KEY path.
        const auto shots = shell.weapons[0].shots;
        shell.weapon_input.primary = true;
        for (int i = 0; i < 100 && shell.weapons[0].shots == shots; ++i)
            CHECK(g.step(.01));
        CHECK(shell.weapons[0].shots > shots && shell.weapons[0].last_result == FireResult::Fired);
        shell.weapon_input.primary = false;
        shell.m_bZoomView = true;
        CHECK(shell.UpdateCameraSelection() && !shell.third_person_active && shell.third_person &&
              shell.weapons_visible);
        shell.m_bZoomView = false;
        CHECK(shell.UpdateCameraSelection() && shell.third_person_active);
        auto scripted = create(g, {{"StartActive", true}, {"HidePlayer", true}});
        CHECK(scripted && shell.UpdateCameraSelection() && !shell.third_person_active && shell.view.object == scripted);
        CHECK(!shell.ToggleThirdPerson() && shell.third_person && !shell.player_look_allowed());
        CHECK(command(g, scripted, "OFF"));
        shell.seeing_eye = scripted;
        CHECK(shell.UpdateCameraSelection() && !shell.third_person_active && shell.player_look_allowed());
        shell.seeing_eye = nullptr;
        shell.orb_view = scripted;
        CHECK(shell.UpdateCameraSelection() && !shell.third_person_active && shell.player_look_allowed());
        shell.orb_view = nullptr;
        shell.death_camera.active = true;
        shell.death_camera.start = g.server.GetTime();
        CHECK(shell.UpdateCameraSelection() && !shell.third_person_active && !shell.ToggleThirdPerson());
        shell.death_camera.active = false;
        CHECK(shell.UpdateCameraSelection() && shell.third_person_active);
        key(true);
        CHECK(!shell.third_person && shell.UpdateCameraSelection() &&
              close(shell.view.position, shell.camera_position));
        CHECK(!shell.player_visible && shell.weapons_visible);
        key(false);
        key(true);
        shell.xr_view = true;
        CHECK(shell.UpdateCameraSelection() && !shell.third_person_active && !shell.third_person);
        CHECK(!shell.ToggleThirdPerson() && close(shell.view.position, shell.camera_position));
        shell.xr_view = false;
        CHECK(shell.ToggleThirdPerson() && shell.third_person);
        CHECK(g.leave_player() && !shell.third_person && !shell.third_person_active);
        CHECK(g.enter_player(error) && !shell.third_person);
        return true;
    }
    bool defaults(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g);
        CHECK(h);
        auto* c = camera(g, h);
        auto* r = g.objects.get(h);
        CHECK(c && !c->IsActive() && c->IsListener() && !c->AllowPlayerMovement() && !c->m_bHidePlayer &&
              c->m_nType == 0);
        CHECK(c->m_fActiveTime == -1 && !r->scheduled && !r->dimensions.x && r->user_flags == 8);
        CHECK(r->state.m_Flags ==
              (FLAG_FORCECLIENTUPDATE | FLAG_FULLPOSITIONRES | FLAG_GOTHRUWORLD | FLAG_DONTFOLLOWSTANDING));
        CHECK(CameraObj::m_dwNumCameras == 1 && g.server.camera_effects.size() == 1 &&
              !g.server.GetCameraView().object);
        CHECK(!create(g, {{"Type", std::int32_t(-1)}}) && !create(g, {{"Type", std::int32_t(3)}}));
        CHECK(!create(g, {{"ActiveTime", std::numeric_limits<float>::infinity()}}));
        // Save/load lot 7: a savegame camera joins the list but publishes nothing until its Load.
        const auto restored = g.objects.create(g.objects.find_class("CameraObj"), {}, Creation::Restore);
        CHECK(restored && g.server.camera_effects.size() == 1 &&
              !(g.server.GetObjectUserFlags(restored) & USRFLG_SAVEABLE));
        CHECK(!c->Setup(3) && c->Setup(1) && c->published.type == 0 && c->m_nType == 1);
        CHECK(g.objects.clear() && CameraObj::m_dwNumCameras == 0 && g.server.camera_effects.empty() &&
              !g_hActiveCamera);
        g.server.set_game_settings(1, GAMETYPE_ACTION);
        auto filtered = create(g);
        CHECK(filtered && g.objects.get(filtered)->life == Life::Pending);
        CHECK(g.objects.finish_frame() && !g.objects.get(filtered) && CameraObj::m_dwNumCameras == 0 &&
              g.server.camera_effects.empty() && !g_hActiveCamera);
        return true;
    }
    bool timing(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g, {{"StartActive", true}, {"ActiveTime", .25f}});
        CHECK(h);
        auto* c = camera(g, h);
        CHECK(c->IsActive() && g_hActiveCamera == h && g.server.GetCameraView().object == h);
        CHECK(g.step(.25) && c->IsActive()); // Source uses strict >, not >=.
        CHECK(g.step(.02) && !c->IsActive() && !g_hActiveCamera && !g.server.GetCameraView().object);
        CHECK(command(g, h, "oN") && c->IsActive());
        auto end = c->m_fDeactivateTime;
        CHECK(g.step(.1) && command(g, h, "ON") && c->m_fDeactivateTime > end);
        CHECK(command(g, h, "LINK") && c->IsActive() && !g.objects.get(h)->scheduled);
        CHECK(g.step(2) && c->IsActive()); // Original unlink stops even an active timer.
        CHECK(command(g, h, "ON") && g.step(.3) && !c->IsActive());
        CHECK(command(g, h, "OFF") && c->m_fDeactivateTime == 0);
        for (float time : {0.0f, -1.0f, -2.0f})
        {
            auto other = create(g, {{"StartActive", true}, {"ActiveTime", time}});
            CHECK(g.step(2) && camera(g, other)->IsActive());
        }
        return true;
    }
    bool links(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto h = create(g, {{"Name", std::string("camera")}, {"StartActive", true}});
        auto* c = camera(g, h);
        auto target = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                       {{"Name", std::string("Target")}, {"Pos", DVector{100, 0, 0}}});
        CHECK(command(g, h, "LINK Target") && c->GetLinkObject() == target && g.objects.link_count(h) == 1);
        CHECK(g.step(.02) && close(rotate_vector({0, 0, 1}, g.objects.get(h)->state.m_Rotation), {1, 0, 0}));
        DVector pos{0, 100, 0};
        CHECK(g.server.TeleportObject(target, &pos) == DE_OK && g.step(.02));
        CHECK(close(rotate_vector({0, 0, 1}, g.objects.get(h)->state.m_Rotation), {0, 1, 0}));
        pos = {};
        CHECK(g.server.TeleportObject(target, &pos) == DE_OK && g.step(.02));
        CHECK(!c->tracking_error.empty() &&
              close(rotate_vector({0, 0, 1}, g.objects.get(h)->state.m_Rotation), {0, 1, 0}));
        CHECK(command(g, h, "LINK target") &&
              c->GetLinkObject() == target); // Exact case-sensitive name lookup, miss keeps link.
        auto second = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                       {{"Name", std::string("Second")}, {"Pos", DVector{0, 0, -20}}});
        CHECK(command(g, h, "LINK Second") && c->GetLinkObject() == second && g.objects.link_count(h) == 1);
        CHECK(!c->SetLinkObject(h) && c->GetLinkObject() == second);
        CHECK(g.objects.remove(target) && g.objects.finish_frame() && c->GetLinkObject() == second);
        CHECK(g.objects.remove(second) && g.objects.finish_frame() && !c->GetLinkObject());
        CHECK(g.step(.02) && c->IsActive() && g.objects.get(h)->scheduled);
        CHECK(g.unload_world() && g.server.list_count() == 0 && g.server.string_count() == 0 && !g_hActiveCamera);
        return true;
    }
    bool selection(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto a = create(g, {{"StartActive", true}}), b = create(g, {{"StartActive", true}});
        CHECK(g_hActiveCamera == b && g.server.GetCameraView().object == a); // Server newest vs client first FX.
        auto nonlistener = create(g, {{"StartActive", true}, {"IsListener", false}});
        CHECK(g_hActiveCamera == b && g.server.GetCameraView().object == a);
        CHECK(g.objects.remove(a) && g.server.GetCameraView().object == b);
        CHECK(g.objects.remove(b) && g.server.GetCameraView().object == nonlistener);
        CHECK(g.objects.finish_frame() && !g_hActiveCamera && CameraObj::m_dwNumCameras == 1);
        CHECK(g.objects.clear() && CameraObj::m_dwNumCameras == 0);
        g.server.SetGameConVar("GAME_WORLDNAME", "WORLDS/01_SubwayA.dat");
        auto old = create(g, {{"StartActive", true}});
        CHECK(camera(g, old)->IsListener() && !camera(g, old)->published.listener);
        CHECK(g_hActiveCamera == old && !g.server.GetCameraView().settings.listener);
        CHECK(g.unload_world() && !g.server.GetGameConVar("GAME_WORLDNAME"));
        auto fresh = create(g, {{"StartActive", true}});
        CHECK(camera(g, fresh)->published.listener);
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.016));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        // An attack started before camera activation still emits its FIRE_KEY.
        // The server must acknowledge the movement gate without consuming ammo.
        g.client_shell.weapon_input.primary = true;
        for (int i = 0; i < 100 && g.client_shell.weapons[0].state != WS_FIRING; ++i)
            CHECK(g.step(.01));
        CHECK(g.client_shell.weapons[0].state == WS_FIRING);
        const auto markers = g.client_shell.weapons[0].fire_keys;
        auto h = create(
            g,
            {{"StartActive", true}, {"Pos", DVector{300, 150, 300}}, {"Type", std::int32_t(1)}, {"HidePlayer", true}});
        auto* c = camera(g, h);
        CHECK(c && c->SetLinkObject(p->m_hObject));
        auto from = g.client_shell.position;
        g.client_shell.movement_input.forward = true;
        g.client_shell.movement_input.jump = true;
        g.client_shell.weapon_input.primary = true;
        g.client_shell.use_down = true;
        const auto uses = p->use_commands;
        const auto ammo = p->m_InventoryMgr.GetAmmoCount(1);
        for (int i = 0; i < 30; ++i)
            CHECK(g.step(.016));
        CHECK(close(from, g.client_shell.position) && p->camera_movement_blocked && p->use_commands == uses);
        CHECK(g.client_shell.weapons[0].fire_keys > markers &&
              g.client_shell.weapons[0].last_result == FireResult::Blocked);
        CHECK(p->m_InventoryMgr.GetAmmoCount(1) == ammo && !g.client_shell.weapons_visible &&
              !g.client_shell.player_visible);
        CHECK(close(g.client_shell.view.position, {300, 150, 300}) &&
              close(g.client_shell.listener_position, {300, 150, 300}));
        CHECK(g.client_shell.view.object == h && g.client_shell.view.settings.type == 1 && c->tracking_updates > 0);
        MovementControls controls;
        controls.focused = controls.enabled = controls.mouse_captured = true;
        auto& shell = g.client_shell;
        const auto rotation = shell.rotation;
        CHECK(!shell.player_look_allowed());
        controls.mouse_x = 50;
        controls.touch_y = .1f;
        SDL_Event turn{};
        turn.type = SDL_EVENT_KEY_DOWN;
        turn.key.windowID = 1;
        turn.key.scancode = SDL_SCANCODE_RIGHT;
        controls.event(turn, 1);
        SDL_Event stick{};
        stick.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        stick.gaxis.which = 5;
        stick.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHTX;
        stick.gaxis.value = 32767;
        controls.event(stick, 1);
        controls.look(shell.rotation, .016, !shell.player_look_allowed());
        CHECK(shell.rotation.yaw == rotation.yaw && shell.rotation.pitch == rotation.pitch && controls.mouse_x == 0 &&
              controls.touch_y == 0);
        turn.type = SDL_EVENT_KEY_UP;
        controls.event(turn, 1);
        stick.gaxis.value = 0;
        controls.event(stick, 1);
        WeaponFire shot;
        CHECK(p->HandleWeaponFireMessage(shot) == FireResult::Blocked);
        CHECK(command(g, h, "OFF") && g.step(.016) && !p->camera_movement_blocked && !g.client_shell.view.object);
        CHECK(shell.player_look_allowed());
        controls.look(shell.rotation, .016, !shell.player_look_allowed());
        CHECK(shell.rotation.yaw == rotation.yaw && shell.rotation.pitch == rotation.pitch);
        controls.mouse_x = 50;
        controls.look(shell.rotation, .016, !shell.player_look_allowed());
        CHECK(shell.rotation.yaw != rotation.yaw);
        for (int i = 0; i < 20; ++i)
            CHECK(g.step(.016));
        CHECK(!close(from, g.client_shell.position, 1) && g.client_shell.weapons_visible);
        auto movable = create(g, {{"StartActive", true}, {"AllowPlayerMovement", true}, {"IsListener", false}});
        from = g.client_shell.position;
        for (int i = 0; i < 20; ++i)
            CHECK(g.step(.016));
        CHECK(g.client_shell.view.object == movable && !p->camera_movement_blocked &&
              !close(from, g.client_shell.position, 1));
        CHECK(!shell.player_look_allowed()); // Movement permission does not grant camera control.
        CHECK(close(g.client_shell.listener_position, g.client_shell.position) && g.client_shell.player_visible);
        CHECK(g.objects.remove(movable) && g.step(.016) && !g.client_shell.view.object);
        CHECK(g.leave_player() && !g.client_shell.view.object && g.client_shell.weapons_visible);
        CHECK(g.enter_player(error) && !g.client_shell.view.object);
        CHECK(g.unload_world() && !g_hActiveCamera && CameraObj::m_dwNumCameras == 0 &&
              g.server.camera_effects.empty());
        return true;
    }
    bool validation(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        DRotation r;
        CHECK(g.server.AlignRotation(r, {0, 0, 0}) == DE_INVALIDPARAMS);
        CHECK(g.server.AlignRotation(r, {0, std::numeric_limits<float>::quiet_NaN(), 0}) == DE_INVALIDPARAMS);
        for (DVector forward : {DVector{0, 0, 1}, DVector{-1, 0, 0}, DVector{0, -1, 0}, DVector{1, 2, 3}})
        {
            CHECK(g.server.AlignRotation(r, forward) == DE_OK);
            CHECK(
                close(rotate_vector({0, 0, 1}, r), vector_scale(forward, 1 / std::sqrt(vector_dot(forward, forward)))));
        }
        auto h = create(g);
        CHECK(!g.server.PublishCameraEffect(h, {}));
        CHECK(!g.server.PublishCameraEffect(nullptr, {}));
        auto unrelated = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(unrelated && !g.server.PublishCameraEffect(unrelated, {}));
        CHECK(g.objects.remove(h) && !g.server.PublishCameraEffect(h, {}));
        CHECK(g.objects.finish_frame() && g.server.camera_effects.empty());
        // Unbound/failed-binding session destructors cannot touch another session's console state.
        g.server.SetGameConVar("GAME_WORLDNAME", "keep");
        {
            GameSession unbound;
        }
        CHECK(std::string(g.server.GetVarValueString(g.server.GetGameConVar("GAME_WORLDNAME"))) == "keep");
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        unsigned worlds = 0, entries = 0, created = 0, rejected = 0;
        std::map<std::string, unsigned> failures;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            CHECK(g.resources.read(path, bytes, error));
            World w;
            if (!read_world(bytes, w, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            g.server.SetGameConVar("GAME_WORLDNAME", path.c_str());
            for (const auto& source : w.objects)
            {
                if (source.class_name != "CameraObj")
                    continue;
                ++entries;
                Properties props;
                for (const auto& prop : source.properties)
                    props.emplace(prop.name, prop.value);
                auto h = create(g, props);
                if (!h)
                {
                    ++rejected;
                    ++failures[g.objects.diagnostics().back()];
                }
                else
                {
                    ++created;
                    auto* c = camera(g, h);
                    CHECK(!c->published.listener);
                    CHECK(command(g, h, "ON") && g.step(.02));
                    CHECK(g.server.GetCameraView().object == h);
                    CHECK(command(g, h, "OFF") && !g.server.GetCameraView().object);
                }
                CHECK(g.objects.clear() && CameraObj::m_dwNumCameras == 0 && g.server.camera_effects.empty() &&
                      !g_hActiveCamera);
            }
        }
        for (const auto& [reason, count] : failures)
            std::cout << count << " rejected: " << reason << '\n';
        std::cout << "worlds=" << worlds << " CameraObj=" << entries << " created=" << created
                  << " rejected=" << rejected << '\n';
        CHECK(worlds == 58 && entries == 62 && created == 62 && rejected == 0);
        CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
        CHECK(CameraObj::m_dwNumCameras == 2 && g.server.camera_effects.size() == 2 &&
              g.objects.snapshot().size() == 230);
        CHECK(test_camera::complete_subway_intro(g));
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && dynamic_cast<CameraObj*>(r->game.get()))
            {
                CHECK(command(g, r->handle, "ON") && g.step(.02) && g.client_shell.view.object);
                break;
            }
        CHECK(g.unload_world() && !g_hActiveCamera && CameraObj::m_dwNumCameras == 0);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"third_person", third_person}, {"defaults", defaults}, {"timing", timing},         {"links", links},
        {"selection", selection},       {"player", player},     {"validation", validation}, {"catalog", catalog}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
