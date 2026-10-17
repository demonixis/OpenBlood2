#pragma once
#include "engine/runtime.h"
namespace OpenBlood2
{
    // One context shared by the always-available HUD and optional inspector.
    struct UiRuntime
    {
        explicit UiRuntime(Runtime& value) : runtime(value) {}
        ~UiRuntime();
        UiRuntime(const UiRuntime&) = delete;
        UiRuntime& operator=(const UiRuntime&) = delete;
        bool initialize(std::string&);
        Runtime& runtime;
        bool context = false, platform = false, renderer = false;
    };
} // namespace OpenBlood2
