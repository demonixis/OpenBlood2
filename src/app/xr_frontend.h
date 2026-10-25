#pragma once

#include "app/scene_presenter.h"
#include "app/xr_lines.h"
#include "app/xr_player.h"
#include "engine/view_arms.h"
#include "engine/xr/xr_runtime.h"
#include "game/original/ViewWeapon.h"
#include <SDL3/SDL_events.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace OpenBlood2
{
    // Virtual gamepad of the XR controllers' menu navigation (beside touch_gamepad_id).
    inline constexpr SDL_JoystickID xr_gamepad_id = 0x7FFFFFF1;

    // The frontend's OpenXR display: session frames, head tracking and calibration, the controller
    // bridge, the stereo scene with the controller overlay, and one UI canvas composited as a quad
    // (the world-locked menu panel, or the head-locked HUD). The frontend keeps its menus, saves,
    // preferences and simulation; it renders ImGui into the canvas instead of the window.
    class XrFrontend
    {
      public:
        // The original 640x480 layout: menu art 1:1, the status bar at its original proportions.
        static constexpr std::uint32_t ui_width = 640, ui_height = 480;
        // Quad size at 0.75 m: about 50 x 39 degrees.
        static constexpr float ui_distance = 0.75f, ui_metres_wide = 0.70f;

        bool create(bool gpu_debug, std::string& error);
        SDL_GPUDevice* device() const
        {
            return xr_.device();
        }
        SDL_GPUTextureFormat ui_format() const
        {
            return xr_.color_format();
        }
        std::vector<int> refresh_rates() const
        {
            return xr_.refresh_rates();
        }
        int refresh_rate() const
        {
            return xr_.refresh_rate();
        }
        bool request_refresh_rate(int rate, std::string& error)
        {
            return xr_.request_refresh_rate(rate, error);
        }
        void set_render_scale(int percent)
        {
            xr_.set_render_scale(percent);
        }
        enum class Begin
        {
            Frame,
            Idle,
            Exit,
            Failed
        };
        // Drains OpenXR events; while the session runs, waits for and begins the next frame.
        Begin begin(std::string& error);
        const XrFrameInput& frame() const
        {
            return frame_;
        }
        // The frame's simulation step: the time between the last two predicted display times (the
        // display period on the first frame), capped like app_frame_delta.
        double delta() const;
        // A session input-focus change since the last call (true: gained).
        std::optional<bool> take_focus_change();
        // Controller menu input while `menu` is shown: the right hand's selection laser on the panel
        // as mouse events in canvas pixels (motion; the right trigger is the left button when it is
        // pressed on the panel), then a virtual gamepad: left stick, A or the right trigger off the
        // panel (accept), B or the left trigger (back), and the menu button in every mode. A button
        // or trigger held across a change of mode waits for its release.
        std::vector<SDL_Event> menu_events(bool menu, SDL_WindowID window);
        // Where the laser meets the canvas (pixels), for the cursor; empty off the panel.
        const std::optional<std::array<float, 2>>& pointer() const
        {
            return pointer_;
        }
        static constexpr std::size_t pointer_hand = 1; // The right hand points.
        bool menu_hands = true; // The panel's Beretta hands; off on the setup screen (no game data).
        // Options > VR, applied every frame.
        struct Options
        {
            float turn_step = xr_snap_angle; // Radians; zero turns smoothly.
            bool laser = true, crosshair = true;
            bool hand_direction = false; // The left stick moves relative to the right hand, not the head.
        };
        void set_options(const Options& options)
        {
            options_ = options;
        }
        void vibrate(std::size_t hand, float amplitude, std::int64_t nanoseconds)
        {
            xr_.vibrate(hand, amplitude, nanoseconds);
        }
        // Gameplay controls into the client shell (replacing the desktop sampling) when `enabled`.
        void apply_controls(GameSession&, bool enabled);
        // After the step: haptic pulses for shots fired; a committed level change recalibrates.
        void after_step(const GameSession&, bool level_changed);
        // Head camera and per-hand view-weapon cameras for the next presenter.update().
        void prepare(GameSession&, ScenePresenter&);
        // Records the eyes (the world when `world`, else a dark clear), then the UI canvas through `ui`
        // (cleared transparent), submits and ends the frame. The canvas is the world-locked menu panel
        // when `panel`, else the head-locked HUD; the panel shows the hands (Beretta view models at
        // the controllers) in place of the view weapons. `fade` darkens the scene.
        bool render(GameSession&, ScenePresenter&, bool world, bool panel, float fade,
                    const std::function<bool(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>& ui, std::string& error);

        ~XrFrontend();

      private:
        XrRuntime xr_;
        std::optional<std::array<float, 2>> pointer_;
        // The right trigger's press in menus: a click on the panel, or accept off it.
        enum class Press
        {
            None,
            Mouse,
            Pad
        };
        Press right_press_ = Press::None;
        xr::TriggerButton left_trigger_, right_trigger_;
        Options options_;
        XrFrameInput frame_{};
        std::int64_t previous_display_time_ = 0;
        XrPlayer player_;
        std::unique_ptr<LineRenderer> lines_;
        std::vector<LineVertex> overlay_;
        bool calibrated_ = false, focused_ = false, panel_placed_ = false;
        // Controls re-enabled (menu closed): buttons and triggers still held wait for release.
        bool controls_enabled_ = false, resume_hold_ = false;
        std::optional<bool> focus_change_;
        xr::Vec3 reference_{};
        float scale_ = 45;
        std::optional<float> body_yaw_; // Arm IK body yaw relative to the tracking yaw.
        xr::Pose panel_pose_{};
        // Virtual pad: last sent axes and buttons; sources (A, B, menu, right and left triggers)
        // blocked until released after a mode change.
        bool menu_mode_ = false;
        std::array<float, 2> sent_axes_{};
        std::array<bool, 3> sent_buttons_{};
        std::array<bool, 5> blocked_{};
        // Menu hands {left, right}: the current character's Beretta view models, animated idle.
        struct MenuHand
        {
            CViewWeapon weapon;
            ViewArmRig rig;
        };
        std::array<MenuHand, 2> hands_;
        int hands_character_ = -1;
        bool hands_failed_ = false;
        void calibrate(GameSession&);
        // The menu laser (panel) or the enabled aim lasers and reticles (play).
        void build_overlay(GameSession&, const XrTracking&, bool panel);
        // Fills presenter.hand_scene with the hands at the tracked controllers; a failure is logged
        // once and leaves the hands out.
        void build_hands(GameSession&, ScenePresenter&, const XrTracking&);
    };
} // namespace OpenBlood2
