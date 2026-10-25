#include "app/control_actions.h"
#include "app/menu_input.h"
#include "game/original/HandWeaponModel.h"
#include "player_fixture.h"
#include <functional>
#include <numbers>
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
    constexpr SDL_WindowID window = 1;
    SDL_Event key(SDL_Scancode code, bool down, bool repeat = false)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = window;
        e.key.scancode = code;
        e.key.repeat = repeat;
        return e;
    }
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        return true;
    }
    bool movement(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        MovementControls c;
        c.focused = c.enabled = c.mouse_captured = true;
        MenuSettings settings;
        auto& shell = g.client_shell;
        CHECK(assign_control(c.bindings, InputAction::Strafe, InputSource::Key, SDL_SCANCODE_LALT));
        c.event(key(SDL_SCANCODE_LALT, true), window);
        c.event(key(SDL_SCANCODE_RIGHT, true), window);
        c.mouse_x = 10;
        CHECK(c.sample(false).right && c.sample(false).mouse_strafe > 0);
        PlayerRotation rotation;
        c.look(rotation, .02, false);
        CHECK(rotation.yaw == 0);
        c.event(key(SDL_SCANCODE_LALT, false), window);
        c.look(rotation, .02, false);
        CHECK(rotation.yaw > 0);
        c.event(key(SDL_SCANCODE_RIGHT, false), window);
        CHECK(assign_control(c.bindings, InputAction::RunLock, InputSource::Key, SDL_SCANCODE_CAPSLOCK));
        c.event(key(SDL_SCANCODE_CAPSLOCK, true), window);
        apply_control_actions(c, shell, false, &settings);
        CHECK(c.sample(false).run && settings.run_lock);
        c.event(key(SDL_SCANCODE_CAPSLOCK, true, true), window);
        apply_control_actions(c, shell, false, &settings);
        CHECK(c.sample(false).run);
        c.player_changed(true);
        CHECK(c.sample(false).run); // Unlike RunLatch, survives releasing movement / opening menus.
        c.event(key(SDL_SCANCODE_CAPSLOCK, true), window);
        apply_control_actions(c, shell, false, &settings);
        CHECK(!settings.run_lock);
        CHECK(assign_control(c.bindings, InputAction::TurnAround, InputSource::Key, SDL_SCANCODE_BACKSPACE));
        rotation = {};
        c.event(key(SDL_SCANCODE_BACKSPACE, true), window);
        for (int i = 0; i < 30; ++i)
            c.look(rotation, .01, false);
        CHECK(std::abs(std::abs(rotation.yaw) - std::numbers::pi_v<float>) < .0001f);
        const auto turned = rotation.yaw;
        c.event(key(SDL_SCANCODE_BACKSPACE, true, true), window);
        c.look(rotation, .1, false);
        CHECK(rotation.yaw == turned);
        c.event(key(SDL_SCANCODE_BACKSPACE, false), window);
        c.event(key(SDL_SCANCODE_BACKSPACE, true), window);
        c.look(rotation, .01, true);
        c.look(rotation, .01, false);
        CHECK(rotation.yaw == turned && c.turn_remaining == 0);
        c.mouse_look = false;
        c.mouse_captured = true;
        c.mouse_y = -10;
        CHECK(c.sample(false).mouse_move && c.sample(false).mouse_forward > 0);
        c.look(rotation, .01, false);
        CHECK(rotation.pitch == 0);
        CHECK(assign_control(c.bindings, InputAction::MouseAim, InputSource::Key, SDL_SCANCODE_M));
        c.event(key(SDL_SCANCODE_M, true), window);
        c.mouse_y = -10;
        CHECK(!c.sample(false).mouse_move && c.sample(false).mouse_forward == 0);
        c.look(rotation, .01, false);
        CHECK(rotation.pitch < 0);
        // The mouse movement reaches the ordinary physics path, not a second integration.
        shell.movement_input.mouse_move = true;
        shell.movement_input.mouse_forward = .5f;
        auto start = shell.position;
        for (int i = 0; i < 10; ++i)
            CHECK(g.step(.01));
        CHECK(shell.position.z > start.z);
        return true;
    }
    bool crosshair(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        MovementControls c;
        c.focused = true;
        MenuSettings settings;
        auto& shell = g.client_shell;
        CHECK(assign_control(c.bindings, InputAction::Crosshair, InputSource::Key, SDL_SCANCODE_H));
        const auto tap = [&]
        {
            c.event(key(SDL_SCANCODE_H, true), window);
            c.event(key(SDL_SCANCODE_H, false), window);
        };
        tap();
        apply_control_actions(c, shell, false, &settings);
        CHECK(!shell.m_bShowCrosshair && !settings.crosshair);
        settings.crosshair_number = 41;
        c.event(key(SDL_SCANCODE_LSHIFT, true), window);
        tap();
        apply_control_actions(c, shell, false, &settings);
        CHECK(shell.m_nCrosshair == 0 && settings.crosshair_number == 0 && !settings.crosshair);
        c.event(key(SDL_SCANCODE_LSHIFT, false), window);
        shell.m_bZoomView = true;
        shell.m_nCrosshair = 99;
        tap();
        apply_control_actions(c, shell, false, &settings);
        CHECK(shell.m_nCrosshair == 99 && !settings.crosshair);
        shell.m_bZoomView = false;
        tap();
        apply_control_actions(c, shell, false, &settings);
        CHECK(shell.m_bShowCrosshair && settings.crosshair);
        return true;
    }
    bool message(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        MenuInput input;
        input.controls.focused = true;
        Menu menu;
        CHECK(assign_control(input.controls.bindings, InputAction::Message, InputSource::Key, SDL_SCANCODE_T));
        auto send = [&](SDL_Event e) { input.event(e, window, menu, true, true, false, 640, 480); };
        send(key(SDL_SCANCODE_T, true));
        CHECK(input.message.active && !menu.active);
        send(key(SDL_SCANCODE_W, true));
        CHECK(!input.controls.sample(input.message.active).forward);
        SDL_Event text{};
        text.type = SDL_EVENT_TEXT_INPUT;
        text.text.windowID = window;
        text.text.text = "hello";
        send(text);
        CHECK(input.message.text == "hello");
        send(key(SDL_SCANCODE_BACKSPACE, true));
        send(key(SDL_SCANCODE_RETURN, true));
        CHECK(!input.message.active && input.message.submitted == "hell" && !menu.active);
        CHECK(!input.controls.sample(false).forward);
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerText{*input.message.submitted}));
        std::string error;
        CHECK(g.pump_player_messages(error) && g.client_shell.chat_lines.back() == ":hell");
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerText{std::string(80, 'x')}));
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerText{"a\nb"}));
        send(key(SDL_SCANCODE_T, true));
        std::string long_line(90, 'a');
        text.text.text = long_line.c_str();
        send(text);
        CHECK(input.message.text.size() == 79);
        send(key(SDL_SCANCODE_ESCAPE, true));
        CHECK(!input.message.active && !input.message.submitted && !menu.active);
        send(key(SDL_SCANCODE_T, true));
        SDL_Event lost{};
        lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        lost.window.windowID = window;
        send(lost);
        CHECK(!input.message.active && !input.message.submitted && menu.active);
        return true;
    }
    bool gameplay(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        auto& shell = g.client_shell;
        auto& inv = p->m_InventoryMgr;
        CHECK(inv.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok);
        CHECK(g.step(.01));
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(.01));
        CHECK(inv.CurrentWeapon(true));
        const auto right = inv.CurrentWeapon()->m_hHandModel;
        const auto left = inv.CurrentWeapon(true)->m_hHandModel;
        shell.weapons[0].state = WS_FIRING;
        shell.drop_weapon_requested = true;
        CHECK(g.step(.01) && inv.CurrentWeapon(true) && inv.drops == 0);
        shell.weapons[0].state = shell.weapons[1].state = WS_REST;
        shell.drop_weapon_requested = true;
        CHECK(g.step(.01) && inv.CurrentWeapon() && !inv.CurrentWeapon(true) && inv.last_dropped == left);
        CHECK(shell.weapons[0].model && !shell.weapons[1].model && g.server.IsObjectAlive(left));
        shell.drop_weapon_requested = true;
        CHECK(g.step(.01) && !inv.CurrentWeapon() && inv.last_dropped == right && !shell.weapons[0].model);
        CHECK(g.server.IsObjectAlive(left) && g.server.IsObjectAlive(right));
        // Normal refusals do not fail the frame or recreate a dropped view weapon.
        shell.drop_weapon_requested = true;
        CHECK(g.step(.01) && !shell.control_notice.empty() && inv.drops == 2);
        const auto voices = p->voice_messages;
        shell.taunt_down = true;
        CHECK(g.step(.01) && p->taunt_commands == 1 && p->voice_messages == voices + 1);
        shell.taunt_down = false;
        // A movable object's accepted translation follows the player displacement; ordinary objects do not.
        DVector position{0, 100, 0};
        CHECK(g.server.TeleportObject(p->m_hObject, &position) == DE_OK);
        p->m_fPitch = p->m_fYaw = 0;
        auto target =
            g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                             {{"Pos", DVector{0, 100 + p->eye_level, 55}}, {"Flags", std::int32_t(FLAG_SOLID)}});
        CHECK(target);
        DVector dims{4, 4, 4};
        CHECK(g.server.SetObjectDims(target, &dims) == DE_OK);
        const PlayerControlAction grab{PlayerControlAction::Grab, {5, 0, 0}};
        CHECK(g.server_shell.OnMessage(g.local_client, grab) && g.objects.get(target)->state.m_Pos.x == 0);
        g.server.SetObjectUserFlags(target, 1u << 4);
        CHECK(g.server_shell.OnMessage(g.local_client, grab) && p->grab_target == target && p->grab_moves == 1);
        CHECK(g.objects.get(target)->state.m_Pos.x > 4.9f);
        p->camera_movement_blocked = true;
        CHECK(!g.server_shell.OnMessage(g.local_client, grab));
        CHECK(!g.server_shell.OnMessage(g.local_client, PlayerControlAction{PlayerControlAction::Taunt}));
        CHECK(g.leave_player() && !shell.grab_down && !shell.taunt_down && !shell.drop_weapon_requested);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"movement", movement}, {"crosshair", crosshair}, {"message", message}, {"gameplay", gameplay}};
    const auto it = cases.find(argv[1]);
    return it == cases.end() ? 2 : it->second(argv[2]) ? 0 : 1;
}
