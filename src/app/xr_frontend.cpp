#include "app/xr_frontend.h"
#include "app/session_playback.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    namespace
    {
        // Nominal user eye height: the original player's standing eye height maps to it.
        constexpr float nominal_eye_height = 1.6f, fallback_units_per_meter = 45;

        DRotation engine_rotation(const xr::Quat& q)
        {
            return {DVector{q[0], q[1], q[2]}, q[3]};
        }
        DVector dvec(const xr::Vec3& v)
        {
            return {v[0], v[1], v[2]};
        }
        EyeProjection eye_projection(const xr::Fov& fov)
        {
            const auto p = xr::projection(fov);
            return {p.scale_x,
                    p.scale_y,
                    p.offset_x,
                    p.offset_y,
                    std::tan(double(fov.left)),
                    std::tan(double(fov.right)),
                    std::tan(double(fov.down)),
                    std::tan(double(fov.up))};
        }
        // World units per metre from the player's standing eye height above its feet.
        float units_per_meter(GameSession& game)
        {
            DVector dims{};
            const auto* player = game.server_shell.Player(game.local_client);
            if (!player || !game.client_shell.player ||
                game.server.GetObjectDims(game.client_shell.player, &dims) != DE_OK || dims.y <= 0)
                return fallback_units_per_meter;
            const float eye = dims.y + player->eye_level;
            return eye > 0 && std::isfinite(eye) ? eye / nominal_eye_height : fallback_units_per_meter;
        }
        SDL_Event pad_button(int button, bool down)
        {
            SDL_Event e{};
            e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            e.gbutton.which = xr_gamepad_id;
            e.gbutton.button = Uint8(button);
            e.gbutton.down = down;
            return e;
        }
        SDL_Event pad_axis(int axis, float value)
        {
            SDL_Event e{};
            e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            e.gaxis.which = xr_gamepad_id;
            e.gaxis.axis = Uint8(axis);
            e.gaxis.value = Sint16(std::lround(std::clamp(value, -1.f, 1.f) * 32767.f));
            return e;
        }
        bool left_weapon(const GameSession& game)
        {
            return std::any_of(game.client_shell.weapons.begin(), game.client_shell.weapons.end(),
                               [](const CViewWeapon& w) { return w.left && w.model; });
        }
    } // namespace

    bool XrFrontend::create(bool gpu_debug, std::string& error)
    {
        if (!xr_.create({gpu_debug}, error) || !xr_.create_quad(ui_width, ui_height, error) ||
            !xr_.create_overlay(error))
            return false;
        lines_ = std::make_unique<LineRenderer>(xr_.device());
        return lines_->prepare(xr_.color_format(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT, error);
    }

    XrFrontend::~XrFrontend()
    {
        lines_.reset();
    }

    XrFrontend::Begin XrFrontend::begin(std::string& error)
    {
        if (!xr_.poll(error))
            return Begin::Failed;
        if (xr_.take_recentre())
            calibrated_ = panel_placed_ = false; // Poses in the old space are stale.
        if (xr_.exit_requested())
            return Begin::Exit;
        if (!xr_.session_running())
            return Begin::Idle;
        previous_display_time_ = frame_.display_time;
        if (!xr_.begin_frame(frame_, error))
            return Begin::Failed;
        if (frame_.focused != focused_)
        {
            focused_ = frame_.focused;
            focus_change_ = focused_;
        }
        return Begin::Frame;
    }

    double XrFrontend::delta() const
    {
        // A frame that misses its display time advances by the frames it missed, so the simulation
        // and the audio keep real time (a fixed display period ran them slow and starved audio).
        const auto elapsed = frame_.display_time - previous_display_time_;
        const bool measured = previous_display_time_ > 0 && elapsed > 0;
        return app_frame_delta(double(measured ? elapsed : frame_.display_period) / 1e9);
    }

    std::optional<bool> XrFrontend::take_focus_change()
    {
        return std::exchange(focus_change_, std::nullopt);
    }

    std::vector<SDL_Event> XrFrontend::menu_events(bool menu, SDL_WindowID window)
    {
        const auto& c = frame_.controls;
        // Sources: A, B, the menu button, the right and left triggers.
        const std::array<bool, 5> raw{c.jump, c.use, c.menu, right_trigger_.update(c.fire),
                                      left_trigger_.update(c.alt_fire)};
        if (menu != menu_mode_)
        {
            menu_mode_ = menu;
            blocked_ = raw;
        }
        std::array<bool, 5> live{};
        for (std::size_t i = 0; i < raw.size(); ++i)
        {
            if (!raw[i])
                blocked_[i] = false;
            live[i] = raw[i] && !blocked_[i] && (menu || i == 2); // The menu button works in play too.
        }
        std::vector<SDL_Event> events;
        // The right hand's laser on the panel.
        std::optional<std::array<float, 2>> point;
        const auto& hand = frame_.hands[pointer_hand];
        if (menu && panel_placed_ && frame_.focused && hand.aim_valid)
        {
            const auto direction = xr::rotate(hand.aim.orientation, {0, 0, -1});
            std::array<float, 2> uv{};
            float distance = 0;
            if (xr::panel_hit(panel_pose_, ui_metres_wide, ui_metres_wide * float(ui_height) / float(ui_width),
                              hand.aim.position, direction, uv, distance))
                point = std::array<float, 2>{uv[0] * float(ui_width), uv[1] * float(ui_height)};
        }
        if (point && (!pointer_ || *pointer_ != *point))
        {
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = window;
            motion.motion.x = (*point)[0];
            motion.motion.y = (*point)[1];
            events.push_back(motion);
        }
        // The right trigger clicks where the laser meets the panel, else accepts; its release always
        // follows its press, even off the panel.
        const auto mouse_button = [&](bool down)
        {
            SDL_Event button{};
            button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            button.button.windowID = window;
            button.button.button = SDL_BUTTON_LEFT;
            button.button.down = down;
            button.button.clicks = 1;
            const auto at = point ? *point : pointer_.value_or(std::array<float, 2>{-1, -1});
            button.button.x = at[0];
            button.button.y = at[1];
            events.push_back(button);
        };
        if (live[3] && right_press_ == Press::None)
        {
            right_press_ = point ? Press::Mouse : Press::Pad;
            if (point)
                mouse_button(true);
        }
        else if (!live[3] && right_press_ != Press::None)
        {
            if (right_press_ == Press::Mouse)
                mouse_button(false);
            right_press_ = Press::None;
        }
        pointer_ = point;
        // SDL axes: y grows downward.
        const std::array<float, 2> axes =
            menu ? std::array<float, 2>{c.move[0], -c.move[1]} : std::array<float, 2>{0, 0};
        constexpr std::array<int, 2> axis_codes{SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY};
        for (std::size_t i = 0; i < 2; ++i)
            if (axes[i] != sent_axes_[i])
            {
                events.push_back(pad_axis(axis_codes[i], axes[i]));
                sent_axes_[i] = axes[i];
            }
        const std::array<bool, 3> buttons{live[0] || right_press_ == Press::Pad, live[1] || live[4], live[2]};
        constexpr std::array<int, 3> button_codes{SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                                                  SDL_GAMEPAD_BUTTON_START};
        for (std::size_t i = 0; i < 3; ++i)
            if (buttons[i] != sent_buttons_[i])
            {
                events.push_back(pad_button(button_codes[i], buttons[i]));
                sent_buttons_[i] = buttons[i];
            }
        return events;
    }

    void XrFrontend::calibrate(GameSession& game)
    {
        for (std::size_t i = 0; i < 3; ++i)
            reference_[i] = (frame_.eye_pose[0].position[i] + frame_.eye_pose[1].position[i]) / 2;
        calibrated_ = true;
        body_yaw_.reset();
        scale_ = units_per_meter(game);
        spdlog::info("XR calibration: {:.2f} units/m (player eye over a {:.2f} m user eye)", scale_,
                     nominal_eye_height);
    }

    void XrFrontend::apply_controls(GameSession& game, bool enabled)
    {
        if (frame_.views_valid && !calibrated_ && game.client_shell.m_bInWorld)
            calibrate(game);
        auto& shell = game.client_shell;
        shell.locomotion_yaw.reset();
        shell.left_hand_forward.reset();
        const bool active = enabled && frame_.focused && calibrated_ && frame_.views_valid && shell.m_bInWorld;
        if (active && !controls_enabled_)
            resume_hold_ = true; // The press that closed the menu must not also fire or jump.
        controls_enabled_ = active;
        if (!active)
            return;
        auto input = frame_;
        auto& c = input.controls;
        if (resume_hold_)
        {
            resume_hold_ = c.fire > .35f || c.alt_fire > .35f || c.use_item > .35f || c.use || c.jump || c.stick_jump ||
                           c.crouch || c.prev_item || c.next_item || c.detonate || c.menu;
            if (resume_hold_)
            {
                const auto move = c.move, right_stick = c.right_stick;
                c = {};
                c.move = move;
                c.right_stick = right_stick;
            }
        }
        player_.turn_step = options_.turn_step;
        player_.hand_direction = options_.hand_direction;
        xr_apply_controls(game, player_, input, xr_tracking(game, player_, input, reference_, scale_), reference_,
                          scale_, delta());
    }

    void XrFrontend::after_step(const GameSession& game, bool level_changed)
    {
        if (level_changed)
        {
            calibrated_ = false;
            player_ = {};
        }
        // A short pulse on the hand whose weapon actually fired.
        for (std::size_t i = 0; i < game.client_shell.weapons.size(); ++i)
        {
            const auto& weapon = game.client_shell.weapons[i];
            if (weapon.shots > player_.shots[i])
                xr_.vibrate(weapon.left ? 0 : 1, .6f, 30'000'000);
            player_.shots[i] = weapon.shots;
        }
    }

    void XrFrontend::prepare(GameSession& game, ScenePresenter& presenter)
    {
        if (!frame_.views_valid)
            return;
        if (!calibrated_ && game.client_shell.m_bInWorld)
            calibrate(game);
        const auto t = xr_tracking(game, player_, frame_, reference_, scale_);
        CameraView head = game.client_shell.view;
        head.position = dvec(t.stereo.head.position);
        head.rotation = engine_rotation(t.stereo.head.orientation);
        presenter.camera_override = head;
        // Each hand's view weapon at its controller: aimed along the aim pose, held at the grip,
        // arms solved from the virtual shoulders; the head's original placement when a controller
        // is not tracked.
        const auto shoulders = xr_shoulders(t, scale_, body_yaw_);
        std::array<ViewWeaponCamera, 2> cameras{};
        for (std::size_t hand = 0; hand < 2; ++hand)
        {
            const auto& h = frame_.hands[hand];
            const bool tracked = h.aim_valid && h.grip_valid;
            const auto aim = tracked ? xr_world_pose(t, reference_, scale_, h.aim) : t.stereo.head;
            const auto look = xr::rotate(aim.orientation, {0, 0, 1});
            auto& camera = cameras[hand];
            camera.pitch = -std::asin(std::clamp(look[1], -1.f, 1.f));
            camera.yaw = std::atan2(look[0], look[2]);
            camera.eye = dvec(t.stereo.head.position);
            camera.scale = view_model_meters * scale_;
            if (!tracked)
                continue;
            ViewArmPlacement arms;
            arms.grip = {dvec(xr_world_pose(t, reference_, scale_, h.grip).position), engine_rotation(aim.orientation)};
            arms.scale = view_model_meters * scale_;
            for (std::size_t side = 0; side < 2; ++side)
            {
                arms.shoulders[side] = dvec(shoulders.shoulders[side]);
                arms.poles[side] = dvec(shoulders.poles[side]);
            }
            camera.arms = arms;
        }
        presenter.weapon_cameras = cameras;
    }

    void XrFrontend::build_overlay(GameSession& game, const XrTracking& t, bool panel)
    {
        overlay_.clear();
        const std::array<float, 4> red{0.9f, 0.15f, 0.1f, 1}, white{1, 1, 1, 1};
        if (panel)
        {
            // The menu laser stops at the panel: the compositor draws the panel over its last part
            // and the canvas shows the cursor.
            const auto& hand = frame_.hands[pointer_hand];
            if (!hand.aim_valid || !panel_placed_)
                return;
            const auto direction = xr::rotate(hand.aim.orientation, {0, 0, -1});
            std::array<float, 2> uv{};
            float distance = 2;
            if (!xr::panel_hit(panel_pose_, ui_metres_wide, ui_metres_wide * float(ui_height) / float(ui_width),
                               hand.aim.position, direction, uv, distance))
                distance = 2;
            const xr::Pose end{{hand.aim.position[0] + direction[0] * distance,
                                hand.aim.position[1] + direction[1] * distance,
                                hand.aim.position[2] + direction[2] * distance},
                               hand.aim.orientation};
            add_line(overlay_, xr_world_pose(t, reference_, scale_, hand.aim).position,
                     xr_world_pose(t, reference_, scale_, end).position, red);
            return;
        }
        // Nothing to aim with during scripted cameras and the death camera.
        const auto& client = game.client_shell;
        if (!client.m_bInWorld || client.view.object || client.death_camera.active ||
            (!options_.laser && !options_.crosshair))
            return;
        // Play: the right laser always, the left one with a left-hand weapon (dual weapons).
        const bool dual = left_weapon(game);
        for (std::size_t hand = 0; hand < 2; ++hand)
        {
            if (!frame_.hands[hand].aim_valid || (hand == 0 && !dual))
                continue;
            const auto aim = xr_world_pose(t, reference_, scale_, frame_.hands[hand].aim);
            const auto target = xr_aim_target(game, aim);
            if (options_.laser)
                add_line(overlay_, aim.position, target, red);
            if (!options_.crosshair)
                continue;
            const auto& eye = t.stereo.head.position;
            const float dx = target[0] - eye[0], dy = target[1] - eye[1], dz = target[2] - eye[2];
            const float size = std::max(0.5f, 0.012f * std::sqrt(dx * dx + dy * dy + dz * dz));
            add_line(overlay_, {target[0] - size, target[1], target[2]}, {target[0] + size, target[1], target[2]},
                     white);
            add_line(overlay_, {target[0], target[1] - size, target[2]}, {target[0], target[1] + size, target[2]},
                     white);
            add_line(overlay_, {target[0], target[1], target[2] - size}, {target[0], target[1], target[2] + size},
                     white);
        }
    }

    void XrFrontend::build_hands(GameSession& game, ScenePresenter& presenter, const XrTracking& t)
    {
        auto& scene = presenter.hand_scene;
        scene.begin_frame();
        presenter.draw_hands = true; // Even empty: the frozen view weapons stay hidden behind the menu.
        if (hands_failed_)
            return;
        std::string error;
        const auto fail = [&]
        {
            spdlog::warn("XR menu hands unavailable: {}", error);
            hands_failed_ = true;
            scene.begin_frame();
        };
        // The current character's hands (Caleb until a game gives another).
        int character = 0;
        for (const auto& weapon : game.client_shell.weapons)
            if (weapon.model)
            {
                character = weapon.character;
                break;
            }
        if (character != hands_character_)
        {
            for (std::size_t side = 0; side < 2; ++side)
            {
                auto& hand = hands_[side];
                if (!hand.weapon.Create(WEAP_BERETTA, side == 0, game.models, game.resources, error,
                                        DBYTE(character)) ||
                    !rig_view_arms(*hand.weapon.model, hand.weapon.model_path, side == 0, hand.rig))
                {
                    if (error.empty())
                        error = "Beretta view-model arms not found";
                    return fail();
                }
                // Original SetupViewModel: idle, else the static pose.
                const auto idle = animation_index(*hand.weapon.model, "idle");
                hand.weapon.animation = {};
                hand.weapon.animation.animation =
                    idle != no_animation ? idle : animation_index(*hand.weapon.model, "static_model");
                hand.weapon.animation.looping = true;
            }
            hands_character_ = character;
        }
        const auto* definition = weapon_definition(WEAP_BERETTA);
        if (!definition)
        {
            error = "Beretta definition missing";
            return fail();
        }
        const auto shoulders = xr_shoulders(t, scale_, body_yaw_);
        std::vector<std::uint32_t> events;
        for (std::size_t side = 0; side < 2; ++side)
        {
            auto& hand = hands_[side];
            const auto& h = frame_.hands[side];
            if (hand.weapon.animation.animation != no_animation &&
                !advance_animation(*hand.weapon.model, hand.weapon.animation, delta(), events, error))
                return fail();
            if (!h.aim_valid || !h.grip_valid)
                continue;
            ViewArmPlacement arms;
            arms.grip = {dvec(xr_world_pose(t, reference_, scale_, h.grip).position),
                         engine_rotation(xr_world_pose(t, reference_, scale_, h.aim).orientation)};
            arms.scale = view_model_meters * scale_;
            for (std::size_t i = 0; i < 2; ++i)
            {
                arms.shoulders[i] = dvec(shoulders.shoulders[i]);
                arms.poles[i] = dvec(shoulders.poles[i]);
            }
            const auto& offset = definition->m_vViewModelOffset;
            ViewArmPose held;
            if (!pose_view_arms(*hand.weapon.model, hand.weapon.animation, hand.rig, arms,
                                {side == 0 ? -offset.x : offset.x, offset.y, offset.z}, held))
            {
                error = "Menu hand placement failed";
                return fail();
            }
            const auto s = arms.scale;
            if (!scene.add(hand.weapon.model, hand.weapon.animation, hand.weapon.skin_path, held.model, {s, s, s}, true,
                           true, {}, side == 0 ? "left menu hand" : "right menu hand", game.resources, error,
                           held.hidden, 1, false, held.targets))
                return fail();
        }
    }

    bool XrFrontend::render(GameSession& game, ScenePresenter& presenter, bool world, bool panel, float fade,
                            const std::function<bool(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>& ui, std::string& error)
    {
        bool drawn = false;
        XrQuadPlacement placement;
        if (frame_.should_render && frame_.views_valid)
        {
            // The menu panel stays where it opened (re-placed after a recentre): 0.75 m ahead of
            // the head, its centre 10 cm below the eyes, facing the user. Placement waits for focus
            // so a headset being put on does not fix it.
            if (panel && !panel_placed_ && frame_.focused)
            {
                xr::Vec3 head{};
                for (std::size_t i = 0; i < 3; ++i)
                    head[i] = (frame_.eye_pose[0].position[i] + frame_.eye_pose[1].position[i]) / 2;
                const auto forward = xr::rotate(frame_.eye_pose[0].orientation, {0, 0, -1});
                const float yaw = std::atan2(-forward[0], -forward[2]); // OpenXR yaw about +Y
                panel_pose_ = {
                    {head[0] - std::sin(yaw) * ui_distance, head[1] - 0.10f, head[2] - std::cos(yaw) * ui_distance},
                    {0, std::sin(yaw / 2), 0, std::cos(yaw / 2)}};
                panel_placed_ = true;
            }
            panel_placed_ = panel_placed_ && panel;
            const bool show_panel = panel && panel_placed_;
            placement.head_locked = !show_panel;
            placement.blend = !show_panel;
            placement.pose = show_panel ? panel_pose_ : xr::Pose{{0, 0, -ui_distance}, {0, 0, 0, 1}};
            placement.width = ui_metres_wide;
            placement.height = ui_metres_wide * float(ui_height) / float(ui_width);
            presenter.scene_fade = xr_.fades_in_compositor() ? 1 : fade;
            SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(xr_.device());
            SDL_GPUTexture* color = command ? xr_.acquire_color(error) : nullptr;
            SDL_GPUTexture* canvas = color ? xr_.acquire_quad(error) : nullptr;
            // The menu hands and laser go to the overlay layer, composited over the panel.
            SDL_GPUTexture* overlay = canvas && show_panel ? xr_.acquire_overlay(error) : nullptr;
            const auto abandon = [&]
            {
                if (error.empty())
                    error = SDL_GetError();
                std::string ignored;
                xr_.end_frame(frame_, false, ignored);
                return false;
            };
            if (!canvas || (show_panel && !overlay))
            {
                if (command)
                    SDL_CancelGPUCommandBuffer(command);
                return abandon();
            }
            const auto t = xr_tracking(game, player_, frame_, reference_, scale_);
            build_overlay(game, t, show_panel);
            presenter.draw_hands = false;
            if (show_panel && menu_hands)
                build_hands(game, presenter, t);
            std::array<StereoEye, 2> views;
            for (std::size_t i = 0; i < 2; ++i)
                views[i] = {dvec(t.stereo.eyes[i].position), engine_rotation(t.stereo.eyes[i].orientation),
                            eye_projection(frame_.eye_fov[i])};
            const auto count = static_cast<std::uint32_t>(overlay_.size());
            const auto lines = [&](SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* commands, std::size_t eye)
            {
                lines_->draw(pass, commands, xr::line_uniform(t.stereo.eyes[eye], frame_.eye_fov[eye], 1, 65536),
                             xr_.width(), xr_.height(), ScenePresenter::view_model_depth, 1, 0, count);
            };
            // Play: the aim laser in the scene. Menus: the scene (or a dark clear without a world),
            // then the hands and the menu laser on the overlay layer above the panel.
            presenter.eye_overlay = show_panel ? nullptr : std::function(lines);
            const bool scene = world && presenter.render_world;
            bool ok = lines_->upload(command, overlay_, error) &&
                      (scene ? presenter.record_eyes(game, command, color, xr_.color_format(), xr_.width(),
                                                     xr_.height(), views, error)
                             : presenter.record_empty_eyes(command, color, xr_.color_format(), xr_.width(),
                                                           xr_.height(), views, error));
            if (ok && show_panel)
            {
                presenter.eye_overlay = lines;
                ok = presenter.record_overlay_eyes(command, overlay, xr_.width(), xr_.height(), views, error);
            }
            ok = ok && ui(command, canvas);
            presenter.eye_overlay = nullptr;
            if (!ok)
            {
                SDL_CancelGPUCommandBuffer(command);
                return abandon();
            }
            if (!SDL_SubmitGPUCommandBuffer(command))
                return abandon();
            drawn = true;
        }
        // In panel mode the canvas waits until the panel has a place.
        return xr_.end_frame(frame_, drawn, error, drawn && (!panel || panel_placed_) ? &placement : nullptr,
                             xr_.fades_in_compositor() ? fade : 1);
    }
} // namespace OpenBlood2
