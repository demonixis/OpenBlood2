#pragma once
#include "client/world.h"
#include <span>
#include <variant>

namespace OpenBlood2::Net
{
    inline constexpr std::size_t world_snapshot_limit = 1024 * 1024;
    // Observable values only. No original object, decoded asset or GPU pointer.
    struct ObjectState
    {
        ObjectId id;
        std::uint64_t pose_revision = 0;
        std::uint16_t type = 0;
        std::uint32_t flags = 0, user_flags = 0;
        NodePose pose;
        DVector scale{1, 1, 1}, color{};
        float alpha = 1;
        std::string filename, skin;
        bool has_model = false, has_sprite = false;
        AnimationTrack animation;
        SpriteTrack sprite_track;
        std::vector<std::uint8_t> hidden_nodes;
        ObjectId parent;
        bool attached = false, attachment_resolved = false;
        std::uint32_t world_model_index = UINT32_MAX;
        bool world_model_bound = false;
    };
    struct WorldSnapshot
    {
        std::uint64_t world = 0, sequence = 0;
        double time = 0;
        std::vector<ObjectState> objects;
        ClientSky sky;
        DVector model_add{}, model_dir_add{};
        ObjectId player, seeing_eye, orb;
        bool player_alive = false;
        std::vector<ClientOwnedEffect> polygrids, particle_streams;
    };
    WorldSnapshot snapshot_values(const ClientWorldFrame&);
    ClientWorldFrame snapshot_frame_values(const WorldSnapshot&); // Assets still require local resolution.
    bool valid_snapshot(const WorldSnapshot&);
    bool encode_world_snapshot(const WorldSnapshot&, std::vector<std::uint8_t>&);
    // Both operations are bounded and commit output only on complete success.
    bool decode_world_snapshot(std::span<const std::uint8_t>, WorldSnapshot&);
    struct WorldPatch
    {
        std::uint64_t base = 0;
        WorldSnapshot values; // Complete frame headers, but only new/changed metadata records.
        std::vector<ObjectId> removed;
    };
    using WorldUpdate = std::variant<WorldSnapshot, WorldPatch>;
    bool make_world_patch(const WorldSnapshot& before, const WorldSnapshot& after, WorldPatch&, bool& changed);
    bool encode_world_patch(const WorldPatch&, std::vector<std::uint8_t>&);
    bool decode_world_patch(std::span<const std::uint8_t>, WorldPatch&);

    enum class BaselineResult
    {
        Refused,
        Pending,
        Complete
    };
    bool encode_baseline_chunks(const WorldSnapshot&, std::vector<std::vector<std::uint8_t>>&);
    bool encode_patch_chunks(const WorldPatch&, std::vector<std::vector<std::uint8_t>>&);
    bool encode_world_chunks(std::uint64_t world, std::uint64_t sequence, std::span<const std::uint8_t>,
                             std::vector<std::vector<std::uint8_t>>&);
    // One reliable, ordered Baseline-channel frame at a time. This is not State
    // reordering/interpolation. A refused chunk never changes an accepted frame.
    class BaselineReceiver
    {
      public:
        explicit BaselineReceiver(std::uint64_t world) : world_(world) {}
        BaselineResult receive(std::span<const std::uint8_t>, WorldSnapshot&);
        BaselineResult receive(std::span<const std::uint8_t>, WorldUpdate&);
        std::size_t pending_bytes() const
        {
            return bytes_.size();
        }

      private:
        std::uint64_t world_, sequence_ = 0, completed_ = 0;
        std::uint32_t total_ = 0;
        std::vector<std::uint8_t> bytes_;
    };
} // namespace OpenBlood2::Net
