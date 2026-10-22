#pragma once

#include "engine/xr/xr_math.h"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace OpenBlood2
{
    // Session states that matter to the frame loop (XrSessionState without the OpenXR headers).
    enum class XrPhase
    {
        None,
        Idle,
        Ready,
        Synchronized,
        Visible,
        Focused,
        Stopping,
        LossPending,
        Exiting
    };
    const char* xr_phase_name(XrPhase);

    // Raw tracking values in the session's reference space (OpenXR frame, metres).
    struct XrHandState
    {
        bool grip_valid = false, aim_valid = false;
        xr::Pose grip{}, aim{};
    };

    // Gameplay action states (zero/false outside FOCUSED). Sticks: x right, y forward/up.
    struct XrControls
    {
        std::array<float, 2> move{}, right_stick{};
        float fire = 0, alt_fire = 0, use_item = 0; // Triggers and the right grip.
        // jump: A (jump / accept); stick_jump: the left stick click (play only).
        bool use = false, jump = false, stick_jump = false, crouch = false, prev_item = false, next_item = false,
             detonate = false, menu = false;
    };

    // One pure value per frame: everything presentation and input read from OpenXR.
    struct XrFrameInput
    {
        std::int64_t display_time = 0, display_period = 0;
        bool should_render = false, views_valid = false, focused = false;
        std::array<xr::Pose, 2> eye_pose{};
        std::array<xr::Fov, 2> eye_fov{};
        std::array<XrHandState, 2> hands{}; // left, right
        XrControls controls{};
    };

    // Where the UI quad is composited this frame (OpenXR metres): head-locked in VIEW space or
    // world-locked in the session's reference space; the quad's front faces +Z.
    struct XrQuadPlacement
    {
        bool head_locked = true, blend = true;
        xr::Pose pose{};
        float width = 1, height = 1;
    };

    struct XrRuntimeSettings
    {
        bool gpu_debug = false;
    };

    // OpenXR instance (created by SDL_GPU), session, reference space, controller poses and
    // one two-layer colour swapchain (layer = eye). No game knowledge.
    class XrRuntime
    {
      public:
        XrRuntime();
        XrRuntime(const XrRuntime&) = delete;
        XrRuntime& operator=(const XrRuntime&) = delete;
        ~XrRuntime();

        bool create(const XrRuntimeSettings&, std::string& error);
        SDL_GPUDevice* device() const;
        SDL_GPUTextureFormat color_format() const;
        std::uint32_t width() const;
        std::uint32_t height() const;
        // Floor height in the reference space: 0 for LOCAL_FLOOR/STAGE, a nominal -1.6 m for LOCAL.
        float floor_height() const;
        std::string description() const;

        // Drains OpenXR events and drives begin/end session. False on an unrecoverable error.
        bool poll(std::string& error);
        bool session_running() const;
        bool exit_requested() const;
        XrPhase phase() const;
        void request_exit();
        // True once after the runtime recentred the reference space (recalibrate head offsets).
        bool take_recentre();
        // A haptic pulse on one hand (0 left, 1 right); ignored without input focus.
        void vibrate(std::size_t hand, float amplitude, std::int64_t nanoseconds);

        // xrWaitFrame, xrBeginFrame, views and (when focused) controller poses.
        bool begin_frame(XrFrameInput&, std::string& error);
        // The swapchain image for this frame; render eye i into layer i, submit, then end_frame.
        SDL_GPUTexture* acquire_color(std::string& error);
        // One single-layer swapchain for UI (same colour handling as the eyes).
        bool create_quad(std::uint32_t width, std::uint32_t height, std::string& error);
        SDL_GPUTexture* acquire_quad(std::string& error);
        // A second two-layer eye swapchain composited, alpha-blended, over the UI quad (the menu hands
        // and laser): render eye i into layer i with a transparent clear.
        bool create_overlay(std::string& error);
        SDL_GPUTexture* acquire_overlay(std::string& error);
        // Releases the acquired images and ends the frame: the projection layer when rendered (scaled
        // by `fade` where the compositor supports it), the UI quad when it was acquired, then the
        // overlay projection layer when it was acquired.
        bool end_frame(const XrFrameInput&, bool rendered, std::string& error, const XrQuadPlacement* quad = nullptr,
                       float fade = 1);
        // True when end_frame applies `fade` (XR_KHR_composition_layer_color_scale_bias).
        bool fades_in_compositor() const;

      private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace OpenBlood2
