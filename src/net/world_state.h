#pragma once
#include "net/world_snapshot.h"

namespace OpenBlood2::Net
{
    struct ObjectMotion
    {
        ObjectId id;
        std::uint64_t pose_revision = 0;
        NodePose pose;
        DVector scale{1, 1, 1}, color{};
        float alpha = 1;
        AnimationTrack animation;
        SpriteTrack sprite_track;
    };
    // Independently usable packets, no unreliable whole-world assembly. Every
    // record is absolute and qualified by the last reliable membership revision.
    struct WorldState
    {
        std::uint64_t world = 0, membership = 0, sequence = 0;
        double time = 0;
        std::vector<ObjectMotion> objects;
    };
    inline constexpr std::size_t state_objects_per_packet = 10;
    ObjectMotion motion_values(const ObjectState&);
    ObjectMotion motion_values(const ClientObject&);
    void apply_motion(const ObjectMotion&, ClientObject&);
    bool valid_state(const WorldState&);
    bool encode_world_state(const WorldState&, std::vector<std::uint8_t>&);
    bool decode_world_state(std::span<const std::uint8_t>, WorldState&);
    bool encode_state_packets(const WorldSnapshot&, std::uint64_t membership, std::vector<std::vector<std::uint8_t>>&);
} // namespace OpenBlood2::Net
