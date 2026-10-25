#include "app/display_options.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace OpenBlood2
{
    namespace
    {
        std::array<int, 2> mode_pixels(const SDL_DisplayMode& mode)
        {
            const double density = mode.pixel_density;
            if (!std::isfinite(density) || density <= 0 || mode.w <= 0 || mode.h <= 0 || mode.w * density > 16384 ||
                mode.h * density > 16384)
                return {};
            return {int(std::lround(mode.w * density)), int(std::lround(mode.h * density))};
        }
        auto window_settings(const MenuSettings& s)
        {
            return std::tuple(s.fullscreen, s.fullscreen_mode, s.resolution_width, s.resolution_height, s.vsync);
        }
        void copy_window(MenuSettings& to, const MenuSettings& from)
        {
            to.fullscreen = from.fullscreen;
            to.fullscreen_mode = from.fullscreen_mode;
            to.resolution_width = from.resolution_width;
            to.resolution_height = from.resolution_height;
            to.vsync = from.vsync;
        }
    } // namespace
    void DisplayOptions::refresh(Menu& menu)
    {
        if (menu.vr)
            return;
        const auto display = SDL_GetDisplayForWindow(runtime_.window);
        if (!display || display == display_)
            return;
        display_ = display;
        auto& caps = menu.video;
#if !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
        caps.window = true;
#endif
        caps.vsync_off = SDL_WindowSupportsGPUPresentMode(runtime_.gpu, runtime_.window, SDL_GPU_PRESENTMODE_IMMEDIATE);
        caps.resolutions = {{0, 0}};
        caps.frame_limits = {30, 60, 120, 240};
        int count = 0;
        auto** modes = SDL_GetFullscreenDisplayModes(display, &count);
        for (int i = 0; modes && i < std::min(count, 4096); ++i)
        {
            const auto& mode = *modes[i];
            const auto size = mode_pixels(mode);
            if (size[0] >= 640 && size[1] >= 480)
                caps.resolutions.push_back(size);
            if (std::isfinite(mode.refresh_rate) && mode.refresh_rate >= 30 && mode.refresh_rate <= 1000)
                caps.frame_limits.push_back(int(std::lround(mode.refresh_rate)));
        }
        SDL_free(modes);
        const auto* current = SDL_GetCurrentDisplayMode(display);
        if (current && std::isfinite(current->refresh_rate) && current->refresh_rate >= 30 &&
            current->refresh_rate <= 1000)
            caps.frame_limits.push_back(int(std::lround(current->refresh_rate)));
        std::sort(caps.resolutions.begin(), caps.resolutions.end());
        caps.resolutions.erase(std::unique(caps.resolutions.begin(), caps.resolutions.end()), caps.resolutions.end());
        std::sort(caps.frame_limits.begin(), caps.frame_limits.end());
        caps.frame_limits.erase(std::unique(caps.frame_limits.begin(), caps.frame_limits.end()),
                                caps.frame_limits.end());
        caps.frame_limits.push_back(0);
        ++menu.revision;
    }
    bool DisplayOptions::set_window(const MenuSettings& settings, std::string& error)
    {
        auto* window = runtime_.window;
        const auto fail = [&]
        {
            error = std::string("Display settings: ") + SDL_GetError();
            return false;
        };
        if (settings.vsync != applied_.vsync &&
            !SDL_SetGPUSwapchainParameters(runtime_.gpu, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                           settings.vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE))
            return fail();
#if !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
        const bool changed = settings.fullscreen != applied_.fullscreen ||
                             settings.fullscreen_mode != applied_.fullscreen_mode ||
                             settings.resolution_width != applied_.resolution_width ||
                             settings.resolution_height != applied_.resolution_height;
        if (!changed)
            return true;
        SDL_DisplayMode selected{};
        const SDL_DisplayMode* mode = nullptr;
        if (settings.fullscreen && settings.fullscreen_mode)
        {
            const auto display = SDL_GetDisplayForWindow(window);
            const auto* desktop = SDL_GetDesktopDisplayMode(display);
            if (!desktop)
                return fail();
            const auto wanted = settings.resolution_width
                                    ? std::array{settings.resolution_width, settings.resolution_height}
                                    : mode_pixels(*desktop);
            {
                bool found = false;
                int count = 0;
                auto** modes = SDL_GetFullscreenDisplayModes(display, &count);
                for (int i = 0; modes && i < std::min(count, 4096); ++i)
                {
                    const auto& candidate = *modes[i];
                    if (mode_pixels(candidate) == wanted && (!found || candidate.refresh_rate > selected.refresh_rate))
                    {
                        selected = candidate;
                        found = true;
                    }
                }
                SDL_free(modes);
                if (!found)
                {
                    error = "This fullscreen resolution is unavailable on the current display.";
                    return false;
                }
            }
            mode = &selected;
        }
        if (!SDL_SetWindowFullscreen(window, false) || !SDL_SyncWindow(window) ||
            !SDL_SetWindowFullscreenMode(window, mode))
            return fail();
        if (settings.fullscreen)
        {
            if (!SDL_SetWindowFullscreen(window, true))
                return fail();
        }
        else
        {
            // SDL sizes windows in logical coordinates; the menu always names drawable pixels.
            const float density = SDL_GetWindowPixelDensity(window);
            if (!std::isfinite(density) || density <= 0)
                return fail();
            const int width = settings.resolution_width ? int(std::lround(settings.resolution_width / density)) : 1200;
            const int height =
                settings.resolution_height ? int(std::lround(settings.resolution_height / density)) : 780;
            if (!SDL_SetWindowSize(window, width, height))
                return fail();
        }
        if (!SDL_SyncWindow(window))
            return fail();
        if (settings.resolution_width && (!settings.fullscreen || settings.fullscreen_mode))
        {
            int width = 0, height = 0;
            if (!SDL_GetWindowSizeInPixels(window, &width, &height))
                return fail();
            if (width != settings.resolution_width || height != settings.resolution_height)
            {
                error = "The window system did not apply the requested pixel resolution.";
                return false;
            }
        }
        if (bool(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != settings.fullscreen)
        {
            error = "The window system did not apply the requested fullscreen state.";
            return false;
        }
#endif
        return true;
    }
    bool DisplayOptions::apply(Menu& menu, std::string& error)
    {
        refresh(menu);
        if (menu.vr || window_settings(menu.settings) == window_settings(applied_))
            return true;
        const auto previous = applied_;
        if (!set_window(menu.settings, error))
        {
            // A platform call may have partially succeeded. Force all requested fields back.
            applied_ = menu.settings;
            std::string rollback;
            if (!set_window(previous, rollback))
                error += " Restore failed: " + rollback;
            applied_ = previous;
            copy_window(menu.settings, previous);
            ++menu.revision;
            return false;
        }
        applied_ = menu.settings;
        return true;
    }
    void DisplayOptions::pace(Uint64 frame_start, int fps)
    {
        if (fps <= 0)
            return;
        const Uint64 duration = 1000000000ULL / Uint64(fps);
        const Uint64 elapsed = SDL_GetTicksNS() - frame_start;
        if (elapsed < duration)
            SDL_DelayPrecise(duration - elapsed);
    }
} // namespace OpenBlood2
