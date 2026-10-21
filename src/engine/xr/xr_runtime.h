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

    // One pure value per frame: everything presentation and input read from OpenXR.
    struct XrFrameInput
    {
        std::int64_t display_time = 0, display_period = 0;
        bool should_render = false, views_valid = false, focused = false;
        std::array<xr::Pose, 2> eye_pose{};
        std::array<xr::Fov, 2> eye_fov{};
        std::array<XrHandState, 2> hands{}; // left, right
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

        // xrWaitFrame, xrBeginFrame, views and (when focused) controller poses.
        bool begin_frame(XrFrameInput&, std::string& error);
        // The swapchain image for this frame; render eye i into layer i, submit, then end_frame.
        SDL_GPUTexture* acquire_color(std::string& error);
        // Releases the acquired image and ends the frame, with a projection layer when rendered.
        bool end_frame(const XrFrameInput&, bool rendered, std::string& error);

      private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace OpenBlood2
