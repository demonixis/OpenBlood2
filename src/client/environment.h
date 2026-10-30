#pragma once
#include "client/movement.h"
#include "engine/container_types.h"
#include <array>
namespace OpenBlood2
{
    struct ClientContainerInfo : ContainerEnvironment
    {
        ObjectId object;
    };
    struct ClientContainerState
    {
        std::array<ClientContainerInfo, 4> body{};
        std::size_t count = 0, total = 0;
        ClientContainerInfo head{};
    };
} // namespace OpenBlood2
