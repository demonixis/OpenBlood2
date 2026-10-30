#pragma once
#include "engine/model.h"
#include "engine/object_id.h"
#include "engine/sprite.h"
#include <array>

namespace OpenBlood2
{
    // A reference may only be resolved in its world, even when a replacement registry
    // assigns the same slot and generation. No original handle or game instance lives here.
    struct ClientObjectId
    {
        std::uint64_t world = 0;
        ObjectId object;
        bool operator==(const ClientObjectId&) const = default;
    };
    struct ClientObject
    {
        ObjectId id;
        std::uint16_t type = 0;
        std::uint32_t flags = 0, user_flags = 0;
        NodePose pose;
        DVector scale{1, 1, 1}, color{};
        float alpha = 1;
        std::string filename, skin;
        // Immutable decoded assets are shared only by the immediate local transport.
        // A future wire codec must send resource names, never these process-local references.
        std::shared_ptr<const Model> model;
        std::shared_ptr<const Sprite> sprite;
        AnimationTrack animation;
        SpriteTrack sprite_track;
        std::vector<std::uint8_t> hidden_nodes;
        ObjectId parent;
        bool attached = false, attachment_resolved = false;
        std::uint32_t world_model_index = UINT32_MAX;
        bool world_model_bound = false;
    };
    struct ClientSky
    {
        DVector min{-10, -10, -10}, max{10, 10, 10}, view_min{}, view_max{};
        std::array<ObjectId, 30> objects{};
    };
    struct ClientOwnedEffect
    {
        std::uint64_t id = 0;
        ObjectId owner;
    };
    struct ClientWorldFrame
    {
        std::uint64_t world = 0, sequence = 0;
        double time = 0;
        std::vector<ClientObject> objects; // Strict slot order, including hidden/inactive objects.
        ClientSky sky;
        DVector model_add{}, model_dir_add{};
        ObjectId player, seeing_eye, orb;
        bool player_alive = false;
        std::vector<ClientOwnedEffect> polygrids, particle_streams;
    };
    // Passive replica. Applying a complete local frame replaces membership atomically;
    // missing/pending objects disappear. No scheduling, model keys or gameplay callbacks.
    class ClientWorld
    {
      public:
        static constexpr std::size_t object_limit = 65536;
        bool begin(std::uint64_t world);
        bool apply(ClientWorldFrame&&, std::string& error);
        const ClientWorldFrame& frame() const
        {
            return frame_;
        }
        const ClientObject* get(ClientObjectId) const;
        // Current-frame lookups for presentation entries, never a cross-world reference.
        const ClientObject* get(ObjectId) const;
        ClientObjectId reference(ObjectId id) const
        {
            return {frame_.world, id};
        }

      private:
        ClientWorldFrame frame_;
    };
} // namespace OpenBlood2
