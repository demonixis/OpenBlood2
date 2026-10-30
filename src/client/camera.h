#pragma once
#include "client/collision.h"
#include "engine/camera_types.h"
#include "game/original/PlayerDeathCamera.h"
namespace OpenBlood2
{
    struct ClientCameraTarget
    {
        ObjectId object;
        DVector position{};
        DRotation rotation{};
    };
    struct ClientCameraDescriptor : ClientCameraTarget
    {
        CameraSettings settings;
        bool active = false;
    };
    struct ClientCameraFrame
    {
        std::uint64_t world = 0, sequence = 0;
        ClientCameraTarget player, eye, orb;
        float time = 0, eye_level = 0;
        bool alive = false;
        std::vector<ClientCameraDescriptor> cameras; // Original descriptor creation order.
    };
    struct ClientCameraView : ClientCameraTarget
    {
        CameraSettings settings;
    };
    enum class ClientCameraKind
    {
        FirstPerson,
        Scripted,
        Eye,
        Orb,
        Death,
        Chase
    };
    class ClientCamera
    {
      public:
        bool begin(std::uint64_t world);
        bool receive(ClientCameraFrame&&, std::string& error);
        void forget_owner()
        {
            frame_.player = {};
            frame_.eye = {};
            frame_.orb = {};
            frame_.alive = false;
            frame_.cameras.clear();
        }
        const ClientCameraFrame& frame() const
        {
            return frame_;
        }
        bool select(const ClientCollisionWorld&, float pitch, float yaw, bool zoom, std::string& error);
        bool chase(const ClientCollisionWorld&, std::string& error);
        bool can_toggle(bool in_world) const;
        bool look_allowed() const
        {
            return !death.active && (!view.object || kind == ClientCameraKind::Eye || kind == ClientCameraKind::Orb);
        }
        DVector position{}, camera_position{}, listener_position{};
        ClientCameraView view;
        ClientCameraKind kind = ClientCameraKind::FirstPerson;
        bool player_visible = false, weapons_visible = true;
        bool third_person = false, third_person_active = false, xr_view = false;
        CPlayerDeathCamera death;

      private:
        ClientCameraFrame frame_;
    };
} // namespace OpenBlood2
