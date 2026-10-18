# Platform lifetime diagnostics

`platform_lifetime_tests` isolates the SDL/platform boundary from the engine and
original game. It links only the pinned SDL target. No game, renderer, asset,
ImGui, audio mixer or engine object is constructed.

The `video`, `window` and `gpu` modes perform one or three complete lifetimes.
All initialize the same video/gamepad subsystems as `Runtime`; the latter two
create a window, and `gpu` creates a debug SDL_GPU device, claims the window,
submits an empty command buffer and waits. Cleanup waits for the GPU, releases
the window claim, destroys the device and window, then calls `SDL_Quit`.
Partial initialization failures take the same explicit cleanup path. No device
or connection is deliberately kept alive to change the leak detector's roots.

The executable prints the actual video driver/device and reports whether SDL's
own allocation tracker is available. An unavailable tracker is not zero leaks.
Only LeakSanitizer measures leaked host allocations in these experiments; this
test makes no claim about total GPU memory or vendor-internal allocation counts.

With `BLOOD2_GPU_TESTS=ON`, CTest registers `gpu.platform_video`,
`gpu.platform_window` and `gpu.platform_gpu`, each using three cycles, a real
display, serial execution and the unchanged 30-second ceiling. The executable
is also built when GPU tests are not registered, allowing explicit sanitizer
probes. All modes use normal process exit and retain sanitizer failures.

## Current Linux result

The NVIDIA 615.78.08 Vulkan stack reports leaks after GPU destruction in this
baseline and in the full train route. Video/window-only sanitizer probes pass.
A separate system-Vulkan reproducer without SDL also reports the same allocation
families. Loader maps identify all formerly unknown frames in NVIDIA glcore/glsi;
D-Bus allocations are kept separately. This localizes the reproducible failure
outside the game, but does not prove that every possible application path is clean
or turn the failing GPU sanitizer processes into passes.

Do not add leak suppressions, disable leak detection, retain unloaded libraries,
or set `SDL_SHUTDOWN_DBUS_ON_QUIT` in production to hide this result. The pinned
SDL header explicitly reserves that hint for debugging because other libraries
can still use D-Bus. An isolated hint experiment did not resolve the report.
No new platform-specific production dependency or driver workaround is introduced.

See [measurements and exact commands](../../../Documentation/Implementation/m2_gpu_lifetime.md).
