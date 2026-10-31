#pragma once
namespace OpenBlood2::Net
{
    // Shared by reliable ENet hosts and connectionless LAN sockets (including WSAStartup).
    bool acquire_enet_runtime();
    void release_enet_runtime();
} // namespace OpenBlood2::Net
