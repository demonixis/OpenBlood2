# Local OpenXR swapchain-format patch for pinned SDL 6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1.
# VULKAN_CreateXRSwapchain overwrites XrSwapchainCreateInfo::format with the SDL texture format,
# so an sRGB swapchain always gets sRGB views and the renderer's gamma-encoded output is encoded
# a second time (washed-out image). With this patch a non-zero format chosen by the caller is
# kept for the runtime's images while SDL's format selects the texture and view format. The
# caller pairs an sRGB swapchain with UNORM views through XR_KHR_vulkan_swapchain_format_list
# (mutable-format images). Local only: SDL does not accept AI-written contributions. Reassess
# when updating SDL.
set(path "${SDL_SOURCE_DIR}/src/gpu/vulkan/SDL_gpu_vulkan.c")
file(SHA256 "${path}" hash)
set(original "e5ef4b92edf2798db6f7c473f59bb362025b52eb0d4b5da044f49365f482a51a")
set(patched "be01f50d395cb9904a49a9e8a11f07be3d49ad7ccc1d372824bfbd21bb76b20d")
if(hash STREQUAL patched)
    return()
endif()
if(NOT hash STREQUAL original)
    message(FATAL_ERROR "SDL Vulkan backend changed; review the local OpenXR swapchain-format patch")
endif()
file(READ "${path}" source)
string(REPLACE
    "    XrSwapchainCreateInfo createInfo = *oldCreateInfo;\n    createInfo.format = SDLToVK_TextureFormat[format];\n"
    "    XrSwapchainCreateInfo createInfo = *oldCreateInfo;\n    if (createInfo.format == 0) { /* OpenBlood2: keep an explicit runtime format. */\n        createInfo.format = SDLToVK_TextureFormat[format];\n    }\n"
    source "${source}")
file(CONFIGURE OUTPUT "${path}" CONTENT "${source}" @ONLY NEWLINE_STYLE LF)
file(SHA256 "${path}" hash)
if(NOT hash STREQUAL patched)
    message(FATAL_ERROR "SDL OpenXR swapchain-format patch did not produce the expected source (${hash})")
endif()
