#include "app/xr_frontend.h"
#include "app/session_playback.h"
#include <algorithm>
#include <cmath>
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
        bool clear_eyes(SDL_GPUCommandBuffer* command, SDL_GPUTexture* color)
        {
            for (Uint32 eye = 0; eye < 2; ++eye)
            {
                SDL_GPUColorTargetInfo target{};
                target.texture = color;
                target.layer_or_depth_plane = eye;
                target.load_op = SDL_GPU_LOADOP_CLEAR;
                target.store_op = SDL_GPU_STOREOP_STORE;
                target.clear_color = {0.02f, 0.02f, 0.03f, 1};
                SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
                if (!pass)
                    return false;
                SDL_EndGPURenderPass(pass);
            }
            return true;
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
    } // namespace

    bool XrFrontend::create(bool gpu_debug, std::string& error)
    {
        if (!xr_.create({gpu_debug}, error) || !xr_.create_quad(ui_width, ui_height, error))
            return false;
        lines_ = std::make_unique<LineRenderer>(xr_.device());
        return lines_->prepare(xr_.color_format(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT, error);
    }

    XrFrontend::Begin XrFrontend::begin(std::string& error)
    {
        if (!xr_.poll(error))
            return Begin::Failed;
        if (xr_.take_recentre())
            calibrated_ = false;
        if (xr_.exit_requested())
            return Begin::Exit;
        if (!xr_.session_running())
            return Begin::Idle;
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
        return app_frame_delta(double(frame_.display_period) / 1e9);
    }

    std::optional<bool> XrFrontend::take_focus_change()
    {
        return std::exchange(focus_change_, std::nullopt);
    }

    std::vector<SDL_Event> XrFrontend::menu_events(bool menu)
    {
        const auto& c = frame_.controls;
        if (menu != menu_mode_)
        {
            menu_mode_ = menu;
            blocked_ = {c.jump, c.crouch, c.menu}; // A, B and the menu button held right now.
        }
        // SDL axes: y grows downward. The menu button reaches the frontend in every mode.
        const std::array<float, 2> axes =
            menu ? std::array<float, 2>{c.move[0], -c.move[1]} : std::array<float, 2>{0, 0};
        const std::array<bool, 3> held{menu && c.jump, menu && c.crouch, c.menu};
        std::vector<SDL_Event> events;
        constexpr std::array<int, 2> axis_codes{SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY};
        for (std::size_t i = 0; i < 2; ++i)
            if (axes[i] != sent_axes_[i])
            {
                events.push_back(pad_axis(axis_codes[i], axes[i]));
                sent_axes_[i] = axes[i];
            }
        constexpr std::array<int, 3> button_codes{SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                                                  SDL_GAMEPAD_BUTTON_START};
        for (std::size_t i = 0; i < 3; ++i)
        {
            const bool raw = i == 0 ? c.jump : i == 1 ? c.crouch : c.menu;
            if (!raw)
                blocked_[i] = false;
            const bool down = held[i] && !blocked_[i];
            if (down != sent_buttons_[i])
            {
                events.push_back(pad_button(button_codes[i], down));
                sent_buttons_[i] = down;
            }
        }
        return events;
    }

    void XrFrontend::calibrate(GameSession& game)
    {
        for (std::size_t i = 0; i < 3; ++i)
            reference_[i] = (frame_.eye_pose[0].position[i] + frame_.eye_pose[1].position[i]) / 2;
        calibrated_ = true;
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
        if (!enabled || !frame_.focused || !calibrated_ || !frame_.views_valid || !shell.m_bInWorld)
            return;
        xr_apply_controls(game, player_, frame_, xr_tracking(game, player_, frame_, reference_, scale_), reference_,
                          scale_);
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
        overlay_.clear();
        if (!frame_.views_valid)
            return;
        if (!calibrated_ && game.client_shell.m_bInWorld)
            calibrate(game);
        const auto t = xr_tracking(game, player_, frame_, reference_, scale_);
        CameraView head = game.client_shell.view;
        head.position = dvec(t.stereo.head.position);
        head.rotation = engine_rotation(t.stereo.head.orientation);
        presenter.camera_override = head;
        // View weapons follow the right controller's aim until the arm IK lot.
        const auto& right = frame_.hands[1];
        const xr::Pose weapon = right.aim_valid ? xr_world_pose(t, reference_, scale_, right.aim) : t.stereo.head;
        const auto look = xr::rotate(weapon.orientation, {0, 0, 1});
        presenter.weapon_camera = ViewWeaponCamera{-std::asin(std::clamp(look[1], -1.f, 1.f)),
                                                   std::atan2(look[0], look[2]), dvec(weapon.position)};
        // Controller boxes, the aim laser and its reticle, in world units.
        for (std::size_t hand = 0; hand < 2; ++hand)
            if (frame_.hands[hand].grip_valid)
                add_box(overlay_, xr_world_pose(t, reference_, scale_, frame_.hands[hand].grip),
                        {0.03f * scale_, 0.035f * scale_, 0.065f * scale_},
                        hand ? std::array<float, 4>{1, 0.45f, 0.05f, 1} : std::array<float, 4>{0.1f, 0.8f, 1, 1});
        if (right.aim_valid && game.client_shell.m_bInWorld)
        {
            const auto aim = xr_world_pose(t, reference_, scale_, right.aim);
            const auto target = xr_aim_target(game, aim);
            add_line(overlay_, aim.position, target, {0.9f, 0.15f, 0.1f, 1});
            const auto& eye = t.stereo.head.position;
            const float dx = target[0] - eye[0], dy = target[1] - eye[1], dz = target[2] - eye[2];
            const float size = std::max(0.5f, 0.012f * std::sqrt(dx * dx + dy * dy + dz * dz));
            const std::array<float, 4> white{1, 1, 1, 1};
            add_line(overlay_, {target[0] - size, target[1], target[2]}, {target[0] + size, target[1], target[2]},
                     white);
            add_line(overlay_, {target[0], target[1] - size, target[2]}, {target[0], target[1] + size, target[2]},
                     white);
            add_line(overlay_, {target[0], target[1], target[2] - size}, {target[0], target[1], target[2] + size},
                     white);
        }
    }

    bool XrFrontend::render(GameSession& game, ScenePresenter& presenter, bool world, bool panel, float fade,
                            const std::function<bool(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>& ui, std::string& error)
    {
        bool drawn = false;
        XrQuadPlacement placement;
        if (frame_.should_render && frame_.views_valid)
        {
            // The menu panel stays where it opened: 0.75 m ahead of the head, level, facing it.
            if (panel && !panel_placed_)
            {
                xr::Vec3 head{};
                for (std::size_t i = 0; i < 3; ++i)
                    head[i] = (frame_.eye_pose[0].position[i] + frame_.eye_pose[1].position[i]) / 2;
                const auto forward = xr::rotate(frame_.eye_pose[0].orientation, {0, 0, -1});
                const float yaw = std::atan2(-forward[0], -forward[2]); // OpenXR yaw about +Y
                panel_pose_ = {
                    {head[0] - std::sin(yaw) * ui_distance, head[1] - 0.05f, head[2] - std::cos(yaw) * ui_distance},
                    {0, std::sin(yaw / 2), 0, std::cos(yaw / 2)}};
                panel_placed_ = true;
            }
            panel_placed_ = panel_placed_ && panel;
            placement.head_locked = !panel;
            placement.blend = !panel;
            placement.pose = panel ? panel_pose_ : xr::Pose{{0, 0, -ui_distance}, {0, 0, 0, 1}};
            placement.width = ui_metres_wide;
            placement.height = ui_metres_wide * float(ui_height) / float(ui_width);
            const bool compositor_fade = xr_.fades_in_compositor();
            presenter.scene_fade = compositor_fade ? 1 : fade;
            SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(xr_.device());
            SDL_GPUTexture* color = command ? xr_.acquire_color(error) : nullptr;
            SDL_GPUTexture* canvas = color ? xr_.acquire_quad(error) : nullptr;
            if (!canvas)
            {
                if (command)
                    SDL_CancelGPUCommandBuffer(command);
                if (error.empty())
                    error = SDL_GetError();
                std::string ignored;
                xr_.end_frame(frame_, false, ignored);
                return false;
            }
            const auto t = xr_tracking(game, player_, frame_, reference_, scale_);
            std::array<StereoEye, 2> views;
            for (std::size_t i = 0; i < 2; ++i)
                views[i] = {dvec(t.stereo.eyes[i].position), engine_rotation(t.stereo.eyes[i].orientation),
                            eye_projection(frame_.eye_fov[i])};
            const auto count = static_cast<std::uint32_t>(overlay_.size());
            presenter.eye_overlay = [&](SDL_GPURenderPass* pass, SDL_GPUCommandBuffer* commands, std::size_t eye)
            {
                lines_->draw(pass, commands, xr::line_uniform(t.stereo.eyes[eye], frame_.eye_fov[eye], 1, 65536),
                             xr_.width(), xr_.height(), ScenePresenter::view_model_depth, 1, 0, count);
            };
            bool ok = lines_->upload(command, overlay_, error) &&
                      (world && presenter.render_world ? presenter.record_eyes(game, command, color, xr_.color_format(),
                                                                               xr_.width(), xr_.height(), views, error)
                                                       : clear_eyes(command, color)) &&
                      ui(command, canvas);
            presenter.eye_overlay = nullptr;
            if (!ok)
            {
                SDL_CancelGPUCommandBuffer(command);
                if (error.empty())
                    error = SDL_GetError();
                std::string ignored;
                xr_.end_frame(frame_, false, ignored);
                return false;
            }
            if (!SDL_SubmitGPUCommandBuffer(command))
            {
                error = SDL_GetError();
                std::string ignored;
                xr_.end_frame(frame_, false, ignored);
                return false;
            }
            drawn = true;
        }
        return xr_.end_frame(frame_, drawn, error, drawn ? &placement : nullptr, xr_.fades_in_compositor() ? fade : 1);
    }
} // namespace OpenBlood2
