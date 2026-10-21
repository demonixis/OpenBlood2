#pragma once

#include "app/scene_presenter.h"
#include "app/xr_lines.h"
#include "app/xr_player.h"
#include "engine/xr/xr_runtime.h"
#include <SDL3/SDL_events.h>
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
        // Gameplay controls into the client shell (replacing the desktop sampling) when `enabled`.
        void apply_controls(GameSession&, bool enabled);
        // After the step: haptic pulses for shots fired; a committed level change recalibrates.
        void after_step(const GameSession&, bool level_changed);
        // Head camera, view weapon and controller overlay for the next presenter.update().
        void prepare(GameSession&, ScenePresenter&);
        // Records the eyes (the world when `world`, else a dark clear), then the UI canvas through `ui`
        // (cleared transparent), submits and ends the frame. The canvas is the world-locked menu panel
        // when `panel`, else the head-locked HUD. `fade` darkens the scene.
        bool render(GameSession&, ScenePresenter&, bool world, bool panel, float fade,
                    const std::function<bool(SDL_GPUCommandBuffer*, SDL_GPUTexture*)>& ui, std::string& error);

      private:
        XrRuntime xr_;
        XrFrameInput frame_{};
        XrPlayer player_;
        std::unique_ptr<LineRenderer> lines_;
        std::vector<LineVertex> overlay_;
        bool calibrated_ = false, focused_ = false, panel_placed_ = false;
        std::optional<bool> focus_change_;
        xr::Vec3 reference_{};
        float scale_ = 45;
        xr::Pose panel_pose_{};
        // Virtual pad: last sent axes and buttons; buttons blocked until released after a mode change.
        bool menu_mode_ = false;
        std::array<float, 2> sent_axes_{};
        std::array<bool, 3> sent_buttons_{}, blocked_{};
        void calibrate(GameSession&);
    };
} // namespace OpenBlood2
