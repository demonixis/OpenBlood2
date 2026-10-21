#include "engine/xr/xr_runtime.h"
#include "build_info.h"
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

// SDL's vendored Vulkan and OpenXR headers; OpenXR must precede SDL_openxr.h (separate block).
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_openxr.h>

namespace OpenBlood2
{
    const char* xr_phase_name(XrPhase phase)
    {
        switch (phase)
        {
            case XrPhase::None:
                return "none";
            case XrPhase::Idle:
                return "idle";
            case XrPhase::Ready:
                return "ready";
            case XrPhase::Synchronized:
                return "synchronized";
            case XrPhase::Visible:
                return "visible";
            case XrPhase::Focused:
                return "focused";
            case XrPhase::Stopping:
                return "stopping";
            case XrPhase::LossPending:
                return "loss pending";
            case XrPhase::Exiting:
                return "exiting";
        }
        return "unknown";
    }

    namespace
    {
        // Zero-initialised OpenXR structure with its type tag.
        template <typename T> T make(XrStructureType type)
        {
            T value{};
            value.type = type;
            return value;
        }
        XrPhase phase_from(XrSessionState state)
        {
            switch (state)
            {
                case XR_SESSION_STATE_IDLE:
                    return XrPhase::Idle;
                case XR_SESSION_STATE_READY:
                    return XrPhase::Ready;
                case XR_SESSION_STATE_SYNCHRONIZED:
                    return XrPhase::Synchronized;
                case XR_SESSION_STATE_VISIBLE:
                    return XrPhase::Visible;
                case XR_SESSION_STATE_FOCUSED:
                    return XrPhase::Focused;
                case XR_SESSION_STATE_STOPPING:
                    return XrPhase::Stopping;
                case XR_SESSION_STATE_LOSS_PENDING:
                    return XrPhase::LossPending;
                case XR_SESSION_STATE_EXITING:
                    return XrPhase::Exiting;
                default:
                    return XrPhase::None;
            }
        }
        xr::Pose pose_from(const XrPosef& p)
        {
            return {{p.position.x, p.position.y, p.position.z},
                    {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w}};
        }
        xr::Fov fov_from(const XrFovf& f)
        {
            return {f.angleLeft, f.angleRight, f.angleUp, f.angleDown};
        }
        XrPosef xr_pose(const xr::Pose& p)
        {
            XrPosef out{};
            out.orientation = {p.orientation[0], p.orientation[1], p.orientation[2], p.orientation[3]};
            out.position = {p.position[0], p.position[1], p.position[2]};
            return out;
        }
        XrFovf xr_fov(const xr::Fov& f)
        {
            return {f.left, f.right, f.up, f.down};
        }
        constexpr XrSpaceLocationFlags pose_valid =
            XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    } // namespace

    struct XrRuntime::State
    {
        // Instance-level entry points, loaded through SDL's loader.
        PFN_xrGetInstanceProcAddr get_proc = nullptr;
        PFN_xrResultToString result_to_string = nullptr;
        PFN_xrGetSystemProperties get_system_properties = nullptr;
        PFN_xrEnumerateViewConfigurationViews enumerate_views = nullptr;
        PFN_xrEnumerateReferenceSpaces enumerate_spaces = nullptr;
        PFN_xrCreateReferenceSpace create_reference_space = nullptr;
        PFN_xrDestroySpace destroy_space = nullptr;
        PFN_xrDestroySession destroy_session = nullptr;
        PFN_xrPollEvent poll_event = nullptr;
        PFN_xrBeginSession begin_session = nullptr;
        PFN_xrEndSession end_session = nullptr;
        PFN_xrRequestExitSession request_exit_session = nullptr;
        PFN_xrWaitFrame wait_frame = nullptr;
        PFN_xrBeginFrame begin_frame = nullptr;
        PFN_xrEndFrame end_frame = nullptr;
        PFN_xrLocateViews locate_views = nullptr;
        PFN_xrLocateSpace locate_space = nullptr;
        PFN_xrAcquireSwapchainImage acquire_image = nullptr;
        PFN_xrWaitSwapchainImage wait_image = nullptr;
        PFN_xrReleaseSwapchainImage release_image = nullptr;
        PFN_xrStringToPath string_to_path = nullptr;
        PFN_xrPathToString path_to_string = nullptr;
        PFN_xrCreateActionSet create_action_set = nullptr;
        PFN_xrDestroyActionSet destroy_action_set = nullptr;
        PFN_xrCreateAction create_action = nullptr;
        PFN_xrSuggestInteractionProfileBindings suggest_bindings = nullptr;
        PFN_xrAttachSessionActionSets attach_action_sets = nullptr;
        PFN_xrCreateActionSpace create_action_space = nullptr;
        PFN_xrSyncActions sync_actions = nullptr;
        PFN_xrGetCurrentInteractionProfile current_profile = nullptr;
        PFN_xrGetDisplayRefreshRateFB get_refresh_rate = nullptr;

        bool loader_loaded = false;
        bool local_floor_extension = false, refresh_rate_extension = false, format_list_extension = false;
        bool gamma_views = false; // sRGB images written through UNORM views (gamma-space output).
        bool color_scale_extension = false;
        SDL_GPUTextureFormat srgb = SDL_GPU_TEXTUREFORMAT_INVALID, unorm = SDL_GPU_TEXTUREFORMAT_INVALID;
        std::array<VkFormat, 2> view_formats{};
        PFN_xrEnumerateSwapchainImages enumerate_images = nullptr;
        XrSpace view_space = XR_NULL_HANDLE;
        // The UI quad: one single-layer swapchain placed per frame.
        XrSwapchain quad_swapchain = XR_NULL_HANDLE;
        SDL_GPUTexture** quad_images = nullptr;
        std::uint32_t quad_count = 0, quad_width = 0, quad_height = 0;
        bool quad_acquired = false;

        XrResult create_swapchain(std::uint32_t width, std::uint32_t height, std::uint32_t layers,
                                  XrSwapchain& swapchain_out, SDL_GPUTexture**& images_out)
        {
            auto format_list =
                make<XrVulkanSwapchainFormatListCreateInfoKHR>(XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR);
            format_list.viewFormatCount = static_cast<std::uint32_t>(view_formats.size());
            format_list.viewFormats = view_formats.data();
            auto info = make<XrSwapchainCreateInfo>(XR_TYPE_SWAPCHAIN_CREATE_INFO);
            info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            if (gamma_views)
            {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT;
                info.format = view_formats[0];
                if (format_list_extension)
                    info.next = &format_list;
            }
            info.sampleCount = 1;
            info.width = width;
            info.height = height;
            info.faceCount = 1;
            info.arraySize = layers;
            info.mipCount = 1;
            return SDL_CreateGPUXRSwapchain(device, session, &info, gamma_views ? unorm : srgb, &swapchain_out,
                                            &images_out);
        }
        SDL_GPUTexture* acquire(XrSwapchain chain, SDL_GPUTexture** textures, std::uint32_t count, bool& acquired,
                                std::string& error)
        {
            if (acquired)
            {
                error = "OpenXR swapchain image already acquired";
                return nullptr;
            }
            std::uint32_t index = 0;
            auto acquire_info = make<XrSwapchainImageAcquireInfo>(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO);
            if (!check(acquire_image(chain, &acquire_info, &index), "xrAcquireSwapchainImage", error))
                return nullptr;
            acquired = true;
            auto wait = make<XrSwapchainImageWaitInfo>(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
            wait.timeout = XR_INFINITE_DURATION;
            if (!check(wait_image(chain, &wait), "xrWaitSwapchainImage", error) || index >= count)
            {
                if (error.empty())
                    error = "OpenXR swapchain image index out of range";
                return nullptr;
            }
            return textures[index];
        }
        bool release(XrSwapchain chain, bool& acquired, std::string& error)
        {
            if (!acquired)
                return true;
            // The frame's GPU work was submitted before this release.
            auto release_info = make<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
            acquired = false;
            return check(release_image(chain, &release_info), "xrReleaseSwapchainImage", error);
        }
        SDL_GPUDevice* device = nullptr;
        XrInstance instance = XR_NULL_HANDLE;
        XrSystemId system = XR_NULL_SYSTEM_ID;
        XrSession session = XR_NULL_HANDLE;
        XrSpace space = XR_NULL_HANDLE;
        XrReferenceSpaceType space_type = XR_REFERENCE_SPACE_TYPE_LOCAL;
        XrActionSet action_set = XR_NULL_HANDLE;
        XrAction grip_action = XR_NULL_HANDLE, aim_action = XR_NULL_HANDLE;
        struct
        {
            XrAction haptic = XR_NULL_HANDLE, move = XR_NULL_HANDLE, right_stick = XR_NULL_HANDLE,
                     fire = XR_NULL_HANDLE, alt_fire = XR_NULL_HANDLE, use = XR_NULL_HANDLE, use_item = XR_NULL_HANDLE,
                     jump = XR_NULL_HANDLE, crouch = XR_NULL_HANDLE, next_item = XR_NULL_HANDLE,
                     detonate = XR_NULL_HANDLE, run = XR_NULL_HANDLE, menu = XR_NULL_HANDLE;
        } actions;
        PFN_xrGetActionStateBoolean get_boolean = nullptr;
        PFN_xrGetActionStateFloat get_float = nullptr;
        PFN_xrGetActionStateVector2f get_vector = nullptr;
        PFN_xrApplyHapticFeedback apply_haptic = nullptr;
        std::array<XrPath, 2> hand_paths{};
        std::array<XrSpace, 2> grip_spaces{}, aim_spaces{};
        XrSwapchain swapchain = XR_NULL_HANDLE;
        SDL_GPUTexture** images = nullptr;
        std::uint32_t image_count = 0;
        SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
        std::uint32_t width = 0, height = 0;
        bool image_acquired = false;
        XrPhase phase = XrPhase::None;
        bool running = false, exit = false, recentred = false;
        std::string system_name;

        std::string result(XrResult code) const
        {
            char text[XR_MAX_RESULT_STRING_SIZE] = {};
            if (result_to_string && instance && XR_SUCCEEDED(result_to_string(instance, code, text)))
                return text;
            return "XrResult " + std::to_string(static_cast<int>(code));
        }
        bool check(XrResult code, const char* what, std::string& error) const
        {
            if (XR_SUCCEEDED(code))
                return true;
            error = std::string(what) + ": " + result(code);
            return false;
        }
        template <typename T> bool load(const char* name, T& out, std::string& error)
        {
            if (XR_FAILED(get_proc(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&out))) || !out)
            {
                error = std::string("OpenXR entry point unavailable: ") + name;
                return false;
            }
            return true;
        }
        XrPath path(const char* text)
        {
            XrPath out = XR_NULL_PATH;
            if (XR_FAILED(string_to_path(instance, text, &out)))
                return XR_NULL_PATH;
            return out;
        }
    };

    XrRuntime::XrRuntime() : state_(std::make_unique<State>()) {}

    XrRuntime::~XrRuntime()
    {
        auto& s = *state_;
        if (s.device)
            SDL_WaitForGPUIdle(s.device);
        if (s.quad_swapchain && s.device)
            SDL_DestroyGPUXRSwapchain(s.device, s.quad_swapchain, s.quad_images);
        if (s.swapchain && s.device)
            SDL_DestroyGPUXRSwapchain(s.device, s.swapchain, s.images);
        if (s.view_space)
            s.destroy_space(s.view_space);
        for (auto space : s.grip_spaces)
            if (space)
                s.destroy_space(space);
        for (auto space : s.aim_spaces)
            if (space)
                s.destroy_space(space);
        if (s.action_set)
            s.destroy_action_set(s.action_set);
        if (s.space)
            s.destroy_space(s.space);
        if (s.session)
            s.destroy_session(s.session);
        // SDL created the instance with the device and destroys it with the device.
        if (s.device)
            SDL_DestroyGPUDevice(s.device);
        if (s.loader_loaded)
            SDL_OpenXR_UnloadLibrary();
    }

    SDL_GPUDevice* XrRuntime::device() const
    {
        return state_->device;
    }
    SDL_GPUTextureFormat XrRuntime::color_format() const
    {
        return state_->format;
    }
    std::uint32_t XrRuntime::width() const
    {
        return state_->width;
    }
    std::uint32_t XrRuntime::height() const
    {
        return state_->height;
    }
    float XrRuntime::floor_height() const
    {
        return state_->space_type == XR_REFERENCE_SPACE_TYPE_LOCAL ? -1.6f : 0.0f;
    }
    bool XrRuntime::session_running() const
    {
        return state_->running;
    }
    bool XrRuntime::exit_requested() const
    {
        return state_->exit;
    }
    XrPhase XrRuntime::phase() const
    {
        return state_->phase;
    }

    std::string XrRuntime::description() const
    {
        const auto& s = *state_;
        const char* space = s.space_type == XR_REFERENCE_SPACE_TYPE_STAGE   ? "STAGE"
                            : s.space_type == XR_REFERENCE_SPACE_TYPE_LOCAL ? "LOCAL"
                                                                            : "LOCAL_FLOOR";
        return s.system_name + ", " + std::to_string(s.width) + "x" + std::to_string(s.height) + " per eye, " +
               std::to_string(s.image_count) + " images, format " + std::to_string(static_cast<int>(s.format)) +
               (s.gamma_views ? " (UNORM views of sRGB images)" : "") + ", space " + space;
    }

    bool XrRuntime::create(const XrRuntimeSettings& settings, std::string& error)
    {
        auto& s = *state_;
        if (s.device)
        {
            error = "OpenXR runtime already created";
            return false;
        }
        // Optional extensions are enabled only when the runtime lists them.
        if (!SDL_OpenXR_LoadLibrary())
        {
            error = std::string("OpenXR loader unavailable: ") + SDL_GetError();
            return false;
        }
        s.loader_loaded = true;
        s.get_proc = reinterpret_cast<PFN_xrGetInstanceProcAddr>(SDL_OpenXR_GetXrGetInstanceProcAddr());
        PFN_xrEnumerateInstanceExtensionProperties enumerate_extensions = nullptr;
        if (!s.get_proc ||
            XR_FAILED(s.get_proc(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                                 reinterpret_cast<PFN_xrVoidFunction*>(&enumerate_extensions))) ||
            !enumerate_extensions)
        {
            error = "OpenXR loader has no xrEnumerateInstanceExtensionProperties";
            return false;
        }
        std::uint32_t count = 0;
        std::vector<XrExtensionProperties> available;
        if (XR_SUCCEEDED(enumerate_extensions(nullptr, 0, &count, nullptr)) && count)
        {
            available.resize(count, make<XrExtensionProperties>(XR_TYPE_EXTENSION_PROPERTIES));
            if (XR_FAILED(enumerate_extensions(nullptr, count, &count, available.data())))
                available.clear();
        }
        const auto listed = [&available](const char* name)
        {
            return std::any_of(available.begin(), available.end(),
                               [name](const XrExtensionProperties& e)
                               { return std::strcmp(e.extensionName, name) == 0; });
        };
        std::vector<const char*> extensions;
        if (listed(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME))
        {
            extensions.push_back(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME);
            s.local_floor_extension = true;
        }
        if (listed(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
        {
            extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
            s.refresh_rate_extension = true;
        }
        if (listed(XR_KHR_COMPOSITION_LAYER_COLOR_SCALE_BIAS_EXTENSION_NAME))
        {
            extensions.push_back(XR_KHR_COMPOSITION_LAYER_COLOR_SCALE_BIAS_EXTENSION_NAME);
            s.color_scale_extension = true;
        }
        if (listed(XR_KHR_VULKAN_SWAPCHAIN_FORMAT_LIST_EXTENSION_NAME))
        {
            extensions.push_back(XR_KHR_VULKAN_SWAPCHAIN_FORMAT_LIST_EXTENSION_NAME);
            s.format_list_extension = true;
        }
        spdlog::info(
            "OpenXR: {} instance extensions listed; local floor {}, refresh rate {}, swapchain format list {}, "
            "colour scale {}",
            available.size(), s.local_floor_extension, s.refresh_rate_extension, s.format_list_extension,
            s.color_scale_extension);

        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, settings.gpu_debug);
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENABLE_BOOLEAN, true);
        SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_INSTANCE_POINTER, &s.instance);
        SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_SYSTEM_ID_POINTER, &s.system);
        SDL_SetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_VERSION_NUMBER, XR_API_VERSION_1_0);
        SDL_SetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_COUNT_NUMBER,
                              static_cast<Sint64>(extensions.size()));
        SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_NAMES_POINTER, extensions.data());
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME_STRING, "OpenBlood 2");
        SDL_SetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_VERSION_NUMBER, build::version_code);
        SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENGINE_NAME_STRING, "OpenBlood2");
        s.device = SDL_CreateGPUDeviceWithProperties(props);
        SDL_DestroyProperties(props);
        if (!s.device || !s.instance)
        {
            error = std::string("SDL_GPU OpenXR device: ") + SDL_GetError();
            return false;
        }

        if (!s.load("xrResultToString", s.result_to_string, error) ||
            !s.load("xrGetSystemProperties", s.get_system_properties, error) ||
            !s.load("xrEnumerateViewConfigurationViews", s.enumerate_views, error) ||
            !s.load("xrEnumerateReferenceSpaces", s.enumerate_spaces, error) ||
            !s.load("xrCreateReferenceSpace", s.create_reference_space, error) ||
            !s.load("xrDestroySpace", s.destroy_space, error) ||
            !s.load("xrDestroySession", s.destroy_session, error) || !s.load("xrPollEvent", s.poll_event, error) ||
            !s.load("xrBeginSession", s.begin_session, error) || !s.load("xrEndSession", s.end_session, error) ||
            !s.load("xrRequestExitSession", s.request_exit_session, error) ||
            !s.load("xrWaitFrame", s.wait_frame, error) || !s.load("xrBeginFrame", s.begin_frame, error) ||
            !s.load("xrEndFrame", s.end_frame, error) || !s.load("xrLocateViews", s.locate_views, error) ||
            !s.load("xrLocateSpace", s.locate_space, error) ||
            !s.load("xrAcquireSwapchainImage", s.acquire_image, error) ||
            !s.load("xrWaitSwapchainImage", s.wait_image, error) ||
            !s.load("xrReleaseSwapchainImage", s.release_image, error) ||
            !s.load("xrStringToPath", s.string_to_path, error) || !s.load("xrPathToString", s.path_to_string, error) ||
            !s.load("xrCreateActionSet", s.create_action_set, error) ||
            !s.load("xrDestroyActionSet", s.destroy_action_set, error) ||
            !s.load("xrCreateAction", s.create_action, error) ||
            !s.load("xrSuggestInteractionProfileBindings", s.suggest_bindings, error) ||
            !s.load("xrAttachSessionActionSets", s.attach_action_sets, error) ||
            !s.load("xrCreateActionSpace", s.create_action_space, error) ||
            !s.load("xrSyncActions", s.sync_actions, error) ||
            !s.load("xrGetCurrentInteractionProfile", s.current_profile, error) ||
            !s.load("xrGetActionStateBoolean", s.get_boolean, error) ||
            !s.load("xrGetActionStateFloat", s.get_float, error) ||
            !s.load("xrGetActionStateVector2f", s.get_vector, error) ||
            !s.load("xrApplyHapticFeedback", s.apply_haptic, error))
            return false;
        if (s.refresh_rate_extension && !s.load("xrGetDisplayRefreshRateFB", s.get_refresh_rate, error))
            return false;

        auto system = make<XrSystemProperties>(XR_TYPE_SYSTEM_PROPERTIES);
        if (!s.check(s.get_system_properties(s.instance, s.system, &system), "xrGetSystemProperties", error))
            return false;
        s.system_name = system.systemName;
        spdlog::info("OpenXR system: {} (vendor {}), max swapchain {}x{}, orientation tracking {}, position "
                     "tracking {}",
                     system.systemName, system.vendorId, system.graphicsProperties.maxSwapchainImageWidth,
                     system.graphicsProperties.maxSwapchainImageHeight,
                     bool(system.trackingProperties.orientationTracking),
                     bool(system.trackingProperties.positionTracking));

        auto session_info = make<XrSessionCreateInfo>(XR_TYPE_SESSION_CREATE_INFO);
        session_info.systemId = s.system;
        if (!s.check(SDL_CreateGPUXRSession(s.device, &session_info, &s.session), "SDL_CreateGPUXRSession", error))
            return false;

        // Prefer a floor-level origin so the grid and the player height need no guess.
        std::uint32_t space_count = 0;
        std::vector<XrReferenceSpaceType> spaces;
        if (XR_SUCCEEDED(s.enumerate_spaces(s.session, 0, &space_count, nullptr)) && space_count)
        {
            spaces.resize(space_count);
            if (XR_FAILED(s.enumerate_spaces(s.session, space_count, &space_count, spaces.data())))
                spaces.clear();
        }
        const auto offered = [&spaces](XrReferenceSpaceType type)
        { return std::find(spaces.begin(), spaces.end(), type) != spaces.end(); };
        s.space_type = s.local_floor_extension && offered(XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT)
                           ? XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT
                       : offered(XR_REFERENCE_SPACE_TYPE_STAGE) ? XR_REFERENCE_SPACE_TYPE_STAGE
                                                                : XR_REFERENCE_SPACE_TYPE_LOCAL;
        auto space_info = make<XrReferenceSpaceCreateInfo>(XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
        space_info.referenceSpaceType = s.space_type;
        space_info.poseInReferenceSpace.orientation.w = 1;
        if (!s.check(s.create_reference_space(s.session, &space_info, &s.space), "xrCreateReferenceSpace", error))
            return false;

        // Gameplay actions (semantic, runtime-rebindable) and the hand poses; the menu lot adds
        // its action set to this one attach.
        auto set_info = make<XrActionSetCreateInfo>(XR_TYPE_ACTION_SET_CREATE_INFO);
        std::strcpy(set_info.actionSetName, "gameplay");
        std::strcpy(set_info.localizedActionSetName, "Gameplay");
        if (!s.check(s.create_action_set(s.instance, &set_info, &s.action_set), "xrCreateActionSet", error))
            return false;
        s.hand_paths = {s.path("/user/hand/left"), s.path("/user/hand/right")};
        const auto action =
            [&s, &error](const char* name, const char* label, XrActionType type, bool per_hand, XrAction& out)
        {
            auto info = make<XrActionCreateInfo>(XR_TYPE_ACTION_CREATE_INFO);
            std::strcpy(info.actionName, name);
            std::strcpy(info.localizedActionName, label);
            info.actionType = type;
            info.countSubactionPaths = per_hand ? 2 : 0;
            info.subactionPaths = per_hand ? s.hand_paths.data() : nullptr;
            return s.check(s.create_action(s.action_set, &info, &out), "xrCreateAction", error);
        };
        auto& a = s.actions;
        constexpr auto boolean = XR_ACTION_TYPE_BOOLEAN_INPUT;
        if (!action("grip_pose", "Hand grip", XR_ACTION_TYPE_POSE_INPUT, true, s.grip_action) ||
            !action("aim_pose", "Hand aim", XR_ACTION_TYPE_POSE_INPUT, true, s.aim_action) ||
            !action("haptic", "Vibration", XR_ACTION_TYPE_VIBRATION_OUTPUT, true, a.haptic) ||
            !action("move", "Move", XR_ACTION_TYPE_VECTOR2F_INPUT, false, a.move) ||
            !action("turn_weapon", "Turn / change weapon", XR_ACTION_TYPE_VECTOR2F_INPUT, false, a.right_stick) ||
            !action("fire", "Fire", XR_ACTION_TYPE_FLOAT_INPUT, false, a.fire) ||
            !action("alt_fire", "Alternate fire", XR_ACTION_TYPE_FLOAT_INPUT, false, a.alt_fire) ||
            !action("use", "Use / back", boolean, false, a.use) ||
            !action("use_item", "Use item", boolean, false, a.use_item) ||
            !action("jump", "Jump / accept", boolean, false, a.jump) ||
            !action("crouch", "Crouch", boolean, false, a.crouch) ||
            !action("next_item", "Next item", boolean, false, a.next_item) ||
            !action("detonate", "Detonate", boolean, false, a.detonate) ||
            !action("run", "Run (toggle)", boolean, false, a.run) || !action("menu", "Menu", boolean, false, a.menu))
            return false;
        const auto suggest =
            [&s, &error](const char* profile, std::initializer_list<std::pair<XrAction, const char*>> list)
        {
            std::vector<XrActionSuggestedBinding> bindings;
            for (const auto& [bound, path] : list)
                bindings.push_back({bound, s.path(path)});
            auto suggested = make<XrInteractionProfileSuggestedBinding>(XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING);
            suggested.interactionProfile = s.path(profile);
            suggested.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
            suggested.suggestedBindings = bindings.data();
            return s.check(s.suggest_bindings(s.instance, &suggested), "xrSuggestInteractionProfileBindings", error);
        };
        // Right-handed layout (user-chosen, 2026-10-21): A jump/accept, B use/back, X next item,
        // Y use item, left grip detonate, left/right stick click run/crouch; weapon hand on the right.
        if (!suggest("/interaction_profiles/oculus/touch_controller",
                     {{s.grip_action, "/user/hand/left/input/grip/pose"},
                      {s.grip_action, "/user/hand/right/input/grip/pose"},
                      {s.aim_action, "/user/hand/left/input/aim/pose"},
                      {s.aim_action, "/user/hand/right/input/aim/pose"},
                      {a.haptic, "/user/hand/left/output/haptic"},
                      {a.haptic, "/user/hand/right/output/haptic"},
                      {a.move, "/user/hand/left/input/thumbstick"},
                      {a.right_stick, "/user/hand/right/input/thumbstick"},
                      {a.fire, "/user/hand/right/input/trigger/value"},
                      {a.alt_fire, "/user/hand/left/input/trigger/value"},
                      {a.jump, "/user/hand/right/input/a/click"},
                      {a.use, "/user/hand/right/input/b/click"},
                      {a.next_item, "/user/hand/left/input/x/click"},
                      {a.use_item, "/user/hand/left/input/y/click"},
                      {a.detonate, "/user/hand/left/input/squeeze/value"},
                      {a.run, "/user/hand/left/input/thumbstick/click"},
                      {a.crouch, "/user/hand/right/input/thumbstick/click"},
                      {a.menu, "/user/hand/left/input/menu/click"}}) ||
            !suggest("/interaction_profiles/khr/simple_controller",
                     {{s.grip_action, "/user/hand/left/input/grip/pose"},
                      {s.grip_action, "/user/hand/right/input/grip/pose"},
                      {s.aim_action, "/user/hand/left/input/aim/pose"},
                      {s.aim_action, "/user/hand/right/input/aim/pose"},
                      {a.haptic, "/user/hand/left/output/haptic"},
                      {a.haptic, "/user/hand/right/output/haptic"},
                      {a.fire, "/user/hand/right/input/select/click"},
                      {a.use, "/user/hand/left/input/select/click"},
                      {a.menu, "/user/hand/left/input/menu/click"}}))
            return false;
        auto attach = make<XrSessionActionSetsAttachInfo>(XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO);
        attach.countActionSets = 1;
        attach.actionSets = &s.action_set;
        if (!s.check(s.attach_action_sets(s.session, &attach), "xrAttachSessionActionSets", error))
            return false;
        for (std::size_t hand = 0; hand < 2; ++hand)
        {
            auto info = make<XrActionSpaceCreateInfo>(XR_TYPE_ACTION_SPACE_CREATE_INFO);
            info.subactionPath = s.hand_paths[hand];
            info.poseInActionSpace.orientation.w = 1;
            info.action = s.grip_action;
            if (!s.check(s.create_action_space(s.session, &info, &s.grip_spaces[hand]), "xrCreateActionSpace", error))
                return false;
            info.action = s.aim_action;
            if (!s.check(s.create_action_space(s.session, &info, &s.aim_spaces[hand]), "xrCreateActionSpace", error))
                return false;
        }

        // One two-layer swapchain at the runtime's recommended per-eye size: layer = eye.
        std::uint32_t view_count = 0;
        if (!s.check(s.enumerate_views(s.instance, s.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &view_count,
                                       nullptr),
                     "xrEnumerateViewConfigurationViews", error))
            return false;
        if (view_count != 2)
        {
            error = "OpenXR stereo view configuration reports " + std::to_string(view_count) + " views";
            return false;
        }
        std::array<XrViewConfigurationView, 2> views{};
        views.fill(make<XrViewConfigurationView>(XR_TYPE_VIEW_CONFIGURATION_VIEW));
        if (!s.check(s.enumerate_views(s.instance, s.system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &view_count,
                                       views.data()),
                     "xrEnumerateViewConfigurationViews", error))
            return false;
        s.width = std::max(views[0].recommendedImageRectWidth, views[1].recommendedImageRectWidth);
        s.height = std::max(views[0].recommendedImageRectHeight, views[1].recommendedImageRectHeight);
        if (!s.width || !s.height || s.width > views[0].maxImageRectWidth || s.height > views[0].maxImageRectHeight)
        {
            error = "OpenXR recommended view size is invalid";
            return false;
        }
        int format_count = 0;
        SDL_GPUTextureFormat* formats = SDL_GetGPUXRSwapchainFormats(s.device, s.session, &format_count);
        if (!formats || format_count <= 0)
        {
            SDL_free(formats);
            error = std::string("No OpenXR swapchain formats: ") + SDL_GetError();
            return false;
        }
        // An sRGB swapchain: the compositor treats UNORM images as linear.
        s.format = formats[0];
        for (int i = 0; i < format_count; ++i)
            if (formats[i] == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB ||
                formats[i] == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
            {
                s.format = formats[i];
                break;
            }
        SDL_free(formats);
        // The renderer writes gamma-encoded values (as into the desktop UNORM swapchain) and the
        // compositor reads sRGB images as encoded: keep the runtime's sRGB images but render through
        // UNORM views. Mutable-format images allow the view (core usage bit; the format list
        // narrows it where offered); the patched SDL keeps this explicit runtime format.
        const SDL_GPUTextureFormat srgb = s.format;
        const SDL_GPUTextureFormat unorm =
            srgb == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB   ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
            : srgb == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB ? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
                                                                : SDL_GPU_TEXTUREFORMAT_INVALID;
        const bool rgba = srgb == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        const std::array<VkFormat, 2> view_formats{rgba ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_B8G8R8A8_SRGB,
                                                   rgba ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_B8G8R8A8_UNORM};
        s.srgb = srgb;
        s.unorm = unorm;
        s.view_formats = view_formats;
        XrResult created = XR_ERROR_VALIDATION_FAILURE;
        if (unorm != SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            s.gamma_views = true;
            created = s.create_swapchain(s.width, s.height, 2, s.swapchain, s.images);
            if (XR_FAILED(created))
            {
                spdlog::warn("OpenXR: mutable-format sRGB swapchain refused ({}); colours are encoded twice",
                             s.result(created));
                s.gamma_views = false;
                s.swapchain = XR_NULL_HANDLE;
                s.images = nullptr;
            }
        }
        if (!s.gamma_views)
            created = s.create_swapchain(s.width, s.height, 2, s.swapchain, s.images);
        s.format = s.gamma_views ? unorm : srgb;
        if (!s.check(created, "SDL_CreateGPUXRSwapchain", error))
            return false;
        if (!s.load("xrEnumerateSwapchainImages", s.enumerate_images, error) ||
            !s.check(s.enumerate_images(s.swapchain, 0, &s.image_count, nullptr), "xrEnumerateSwapchainImages", error))
            return false;
        // Head-locked compositor layers (the HUD) live in VIEW space.
        auto view_info = make<XrReferenceSpaceCreateInfo>(XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
        view_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        view_info.poseInReferenceSpace.orientation.w = 1;
        if (!s.check(s.create_reference_space(s.session, &view_info, &s.view_space), "xrCreateReferenceSpace", error))
            return false;
        if (s.refresh_rate_extension)
        {
            float rate = 0;
            if (XR_SUCCEEDED(s.get_refresh_rate(s.session, &rate)))
                spdlog::info("OpenXR display refresh rate: {} Hz", rate);
        }
        spdlog::info("OpenXR session: {}", description());
        return true;
    }

    bool XrRuntime::poll(std::string& error)
    {
        auto& s = *state_;
        auto event = make<XrEventDataBuffer>(XR_TYPE_EVENT_DATA_BUFFER);
        for (;;)
        {
            event = make<XrEventDataBuffer>(XR_TYPE_EVENT_DATA_BUFFER);
            const XrResult polled = s.poll_event(s.instance, &event);
            if (polled == XR_EVENT_UNAVAILABLE)
                return true;
            if (!s.check(polled, "xrPollEvent", error))
                return false;
            switch (event.type)
            {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    spdlog::warn("OpenXR instance loss pending");
                    s.exit = true;
                    break;
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                {
                    const auto& changed = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                    s.phase = phase_from(changed.state);
                    spdlog::info("OpenXR session state: {}", xr_phase_name(s.phase));
                    if (changed.state == XR_SESSION_STATE_READY)
                    {
                        auto begin = make<XrSessionBeginInfo>(XR_TYPE_SESSION_BEGIN_INFO);
                        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        if (!s.check(s.begin_session(s.session, &begin), "xrBeginSession", error))
                            return false;
                        s.running = true;
                    }
                    else if (changed.state == XR_SESSION_STATE_STOPPING)
                    {
                        s.running = false;
                        if (!s.check(s.end_session(s.session), "xrEndSession", error))
                            return false;
                    }
                    else if (changed.state == XR_SESSION_STATE_EXITING ||
                             changed.state == XR_SESSION_STATE_LOSS_PENDING)
                        s.exit = true;
                    break;
                }
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                    spdlog::info("OpenXR reference space change pending (recentre)");
                    s.recentred = true;
                    break;
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    for (std::size_t hand = 0; hand < 2; ++hand)
                    {
                        auto profile = make<XrInteractionProfileState>(XR_TYPE_INTERACTION_PROFILE_STATE);
                        char text[XR_MAX_PATH_LENGTH] = {};
                        std::uint32_t length = 0;
                        if (XR_SUCCEEDED(s.current_profile(s.session, s.hand_paths[hand], &profile)) &&
                            profile.interactionProfile != XR_NULL_PATH &&
                            XR_SUCCEEDED(
                                s.path_to_string(s.instance, profile.interactionProfile, sizeof(text), &length, text)))
                            spdlog::info("OpenXR {} hand profile: {}", hand ? "right" : "left", text);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void XrRuntime::vibrate(std::size_t hand, float amplitude, std::int64_t nanoseconds)
    {
        auto& s = *state_;
        if (hand > 1 || s.phase != XrPhase::Focused || !s.apply_haptic)
            return;
        auto info = make<XrHapticActionInfo>(XR_TYPE_HAPTIC_ACTION_INFO);
        info.action = s.actions.haptic;
        info.subactionPath = s.hand_paths[hand];
        auto vibration = make<XrHapticVibration>(XR_TYPE_HAPTIC_VIBRATION);
        vibration.duration = nanoseconds;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
        vibration.amplitude = std::clamp(amplitude, 0.f, 1.f);
        s.apply_haptic(s.session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }
    bool XrRuntime::take_recentre()
    {
        return std::exchange(state_->recentred, false);
    }
    void XrRuntime::request_exit()
    {
        auto& s = *state_;
        if (s.running && XR_SUCCEEDED(s.request_exit_session(s.session)))
            return;
        s.exit = true;
    }

    bool XrRuntime::begin_frame(XrFrameInput& frame, std::string& error)
    {
        auto& s = *state_;
        frame = {};
        auto wait = make<XrFrameWaitInfo>(XR_TYPE_FRAME_WAIT_INFO);
        auto state = make<XrFrameState>(XR_TYPE_FRAME_STATE);
        if (!s.check(s.wait_frame(s.session, &wait, &state), "xrWaitFrame", error))
            return false;
        auto begin = make<XrFrameBeginInfo>(XR_TYPE_FRAME_BEGIN_INFO);
        if (!s.check(s.begin_frame(s.session, &begin), "xrBeginFrame", error))
            return false;
        frame.display_time = state.predictedDisplayTime;
        frame.display_period = state.predictedDisplayPeriod;
        frame.should_render = state.shouldRender == XR_TRUE;
        frame.focused = s.phase == XrPhase::Focused;
        if (!frame.should_render)
            return true;

        auto locate = make<XrViewLocateInfo>(XR_TYPE_VIEW_LOCATE_INFO);
        locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime = state.predictedDisplayTime;
        locate.space = s.space;
        auto view_state = make<XrViewState>(XR_TYPE_VIEW_STATE);
        std::array<XrView, 2> views{};
        views.fill(make<XrView>(XR_TYPE_VIEW));
        std::uint32_t located = 0;
        if (!s.check(s.locate_views(s.session, &locate, &view_state, 2, &located, views.data()), "xrLocateViews",
                     error))
            return false;
        frame.views_valid = located == 2 && (view_state.viewStateFlags & pose_valid) == pose_valid;
        for (std::size_t eye = 0; eye < 2 && frame.views_valid; ++eye)
        {
            frame.eye_pose[eye] = pose_from(views[eye].pose);
            frame.eye_fov[eye] = fov_from(views[eye].fov);
            frame.views_valid = xr::valid_fov(frame.eye_fov[eye]);
        }

        // Input is inactive outside FOCUSED (system menu): poses stay invalid, cubes hide.
        if (!frame.focused)
            return true;
        XrActiveActionSet active{s.action_set, XR_NULL_PATH};
        auto sync = make<XrActionsSyncInfo>(XR_TYPE_ACTIONS_SYNC_INFO);
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active;
        if (!s.check(s.sync_actions(s.session, &sync), "xrSyncActions", error))
            return false;
        const auto boolean = [&s](XrAction action)
        {
            auto info = make<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
            info.action = action;
            auto state = make<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
            return XR_SUCCEEDED(s.get_boolean(s.session, &info, &state)) && state.isActive && state.currentState;
        };
        const auto scalar = [&s](XrAction action)
        {
            auto info = make<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
            info.action = action;
            auto state = make<XrActionStateFloat>(XR_TYPE_ACTION_STATE_FLOAT);
            return XR_SUCCEEDED(s.get_float(s.session, &info, &state)) && state.isActive ? state.currentState : 0.f;
        };
        const auto vector = [&s](XrAction action)
        {
            auto info = make<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
            info.action = action;
            auto state = make<XrActionStateVector2f>(XR_TYPE_ACTION_STATE_VECTOR2F);
            return XR_SUCCEEDED(s.get_vector(s.session, &info, &state)) && state.isActive
                       ? std::array<float, 2>{state.currentState.x, state.currentState.y}
                       : std::array<float, 2>{};
        };
        auto& c = frame.controls;
        const auto& a = s.actions;
        c.move = vector(a.move);
        c.right_stick = vector(a.right_stick);
        c.fire = scalar(a.fire);
        c.alt_fire = scalar(a.alt_fire);
        c.use = boolean(a.use);
        c.use_item = boolean(a.use_item);
        c.jump = boolean(a.jump);
        c.crouch = boolean(a.crouch);
        c.next_item = boolean(a.next_item);
        c.detonate = boolean(a.detonate);
        c.run = boolean(a.run);
        c.menu = boolean(a.menu);
        for (std::size_t hand = 0; hand < 2; ++hand)
        {
            auto grip = make<XrSpaceLocation>(XR_TYPE_SPACE_LOCATION);
            if (XR_SUCCEEDED(s.locate_space(s.grip_spaces[hand], s.space, state.predictedDisplayTime, &grip)) &&
                (grip.locationFlags & pose_valid) == pose_valid)
            {
                frame.hands[hand].grip_valid = true;
                frame.hands[hand].grip = pose_from(grip.pose);
            }
            auto aim = make<XrSpaceLocation>(XR_TYPE_SPACE_LOCATION);
            if (XR_SUCCEEDED(s.locate_space(s.aim_spaces[hand], s.space, state.predictedDisplayTime, &aim)) &&
                (aim.locationFlags & pose_valid) == pose_valid)
            {
                frame.hands[hand].aim_valid = true;
                frame.hands[hand].aim = pose_from(aim.pose);
            }
        }
        return true;
    }

    SDL_GPUTexture* XrRuntime::acquire_color(std::string& error)
    {
        auto& s = *state_;
        return s.acquire(s.swapchain, s.images, s.image_count, s.image_acquired, error);
    }

    bool XrRuntime::create_quad(std::uint32_t width, std::uint32_t height, std::string& error)
    {
        auto& s = *state_;
        if (s.quad_swapchain || !width || !height)
        {
            error = "Invalid or repeated OpenXR quad creation";
            return false;
        }
        if (!s.check(s.create_swapchain(width, height, 1, s.quad_swapchain, s.quad_images), "SDL_CreateGPUXRSwapchain",
                     error) ||
            !s.check(s.enumerate_images(s.quad_swapchain, 0, &s.quad_count, nullptr), "xrEnumerateSwapchainImages",
                     error))
            return false;
        s.quad_width = width;
        s.quad_height = height;
        return true;
    }

    SDL_GPUTexture* XrRuntime::acquire_quad(std::string& error)
    {
        auto& s = *state_;
        if (!s.quad_swapchain)
        {
            error = "No OpenXR quad";
            return nullptr;
        }
        return s.acquire(s.quad_swapchain, s.quad_images, s.quad_count, s.quad_acquired, error);
    }

    bool XrRuntime::end_frame(const XrFrameInput& frame, bool rendered, std::string& error, const XrQuadPlacement* quad,
                              float fade)
    {
        auto& s = *state_;
        const bool quad_drawn = s.quad_acquired;
        if (!s.release(s.swapchain, s.image_acquired, error) || !s.release(s.quad_swapchain, s.quad_acquired, error))
            return false;
        std::array<XrCompositionLayerProjectionView, 2> views{};
        auto projection = make<XrCompositionLayerProjection>(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
        auto scale = make<XrCompositionLayerColorScaleBiasKHR>(XR_TYPE_COMPOSITION_LAYER_COLOR_SCALE_BIAS_KHR);
        auto panel = make<XrCompositionLayerQuad>(XR_TYPE_COMPOSITION_LAYER_QUAD);
        std::array<const XrCompositionLayerBaseHeader*, 2> layers{};
        auto end = make<XrFrameEndInfo>(XR_TYPE_FRAME_END_INFO);
        end.displayTime = frame.display_time;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if (rendered && frame.should_render && frame.views_valid)
        {
            for (std::size_t eye = 0; eye < 2; ++eye)
            {
                views[eye] = make<XrCompositionLayerProjectionView>(XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
                views[eye].pose = xr_pose(frame.eye_pose[eye]);
                views[eye].fov = xr_fov(frame.eye_fov[eye]);
                views[eye].subImage.swapchain = s.swapchain;
                views[eye].subImage.imageRect = {
                    {0, 0}, {static_cast<std::int32_t>(s.width), static_cast<std::int32_t>(s.height)}};
                views[eye].subImage.imageArrayIndex = static_cast<std::uint32_t>(eye);
            }
            projection.space = s.space;
            projection.viewCount = 2;
            projection.views = views.data();
            if (s.color_scale_extension && fade < 1)
            {
                const float f = std::clamp(fade, 0.f, 1.f);
                scale.colorScale = {f, f, f, 1};
                projection.next = &scale;
            }
            layers[end.layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
            if (quad && quad_drawn)
            {
                panel.layerFlags = quad->blend ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
                panel.space = quad->head_locked ? s.view_space : s.space;
                panel.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                panel.subImage.swapchain = s.quad_swapchain;
                panel.subImage.imageRect = {
                    {0, 0}, {static_cast<std::int32_t>(s.quad_width), static_cast<std::int32_t>(s.quad_height)}};
                panel.pose = xr_pose(quad->pose);
                panel.size = {quad->width, quad->height};
                layers[end.layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&panel);
            }
            end.layers = layers.data();
        }
        return s.check(s.end_frame(s.session, &end), "xrEndFrame", error);
    }

    bool XrRuntime::fades_in_compositor() const
    {
        return state_->color_scale_extension;
    }
} // namespace OpenBlood2
