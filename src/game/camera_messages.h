#pragma once
#include "engine/camera_types.h"
namespace OpenBlood2
{
    struct CameraView
    {
        HOBJECT object = nullptr;
        CameraSettings settings;
        DVector position{};
        DRotation rotation{};
    };
} // namespace OpenBlood2
