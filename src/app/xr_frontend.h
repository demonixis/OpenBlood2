#pragma once

#include "app/scene_presenter.h"
#include "app/xr_lines.h"
#include "app/xr_player.h"
#include "engine/xr/xr_runtime.h"
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
        // The frame's simulation step: the predicted display period, capped like app_frame_delta.
        double delta() const;
        // A session input-focus change since the last call (true: gained).
        std::optional<bool> take_focus_change();
        // Controller menu navigation as virtual gamepad events: left stick, A (accept) and B (back)
        // while `menu` is shown, the menu button always. A button held across a change of mode waits
        // for its release.
        std::vector<SDL_Event> menu_events(bool menu);
        // The selection laser on the menu panel as mouse events in canvas pixels (motion, left
        // button on the pointing hand's trigger), from the hand whose trigger was pressed last.
        std::vector<SDL_Event> pointer_events(bool panel, SDL_WindowID window);
        // Where the laser meets the canvas (pixels), for the cursor; empty off the panel.
        const std::optional<std::array<float, 2>>& pointer() const
        {
            return pointer_;
        }
        std::size_t pointer_hand() const
        {
            return pointer_hand_;
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
        // when `panel`, else the head-locked HUD. `fade` darkens the scene.
        bool render(GameSession&, ScenePresenter&, bool world, bool panel, float fade,
                    const std::function<bool(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>& ui, std::string& error);

        ~XrFrontend();

      private:
        XrRuntime xr_;
        SDL_GPUTexture* depth_ = nullptr; // Eye depth for the overlay when no world is drawn.
        std::optional<std::array<float, 2>> pointer_;
        std::size_t pointer_hand_ = 1;
        bool pointer_down_ = false;
        xr::PressEdge left_trigger_, right_trigger_;
        XrFrameInput frame_{};
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
        // Virtual pad: last sent axes and buttons; buttons blocked until released after a mode change.
        bool menu_mode_ = false;
        std::array<float, 2> sent_axes_{};
        std::array<bool, 3> sent_buttons_{}, blocked_{};
        void calibrate(GameSession&);
        // Controller boxes plus the menu laser (panel) or the aim lasers and reticles (play).
        void build_overlay(GameSession&, const XrTracking&, bool panel);
    };
} // namespace OpenBlood2
