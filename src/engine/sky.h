#pragma once
#include "engine/world_view.h"
namespace OpenBlood2
{
    struct SkyDefinition
    {
        DVector min{-10, -10, -10}, max{10, 10, 10}, view_min{}, view_max{};
    };
    // Non-owning checked identities. Original MAX_SKYOBJECTS is 30 (indices 0..29).
    struct SkyState
    {
        SkyDefinition definition;
        bool defined = false;
        std::array<ObjectId, 30> objects{};
        std::vector<ObjectId> registered; // Original INSKY survives replacement of its slot.
        std::uint64_t definitions = 0, additions = 0;
        std::string error;
        bool set(const SkyDefinition&);
        bool add(const Objects&, HOBJECT, unsigned);
        bool remove(const Objects&, HOBJECT);
        bool validate(const Objects&, HOBJECT, unsigned, std::string&) const;
        bool draws(const World&, const WorldMesh&, const Objects&, DVector eye, std::vector<WorldDraw>&,
                   DVector& sky_eye, std::string&) const;
    };
    bool sky_view_position(const SkyDefinition&, DVector world_min, DVector world_max, DVector eye, DVector& out);
} // namespace OpenBlood2
