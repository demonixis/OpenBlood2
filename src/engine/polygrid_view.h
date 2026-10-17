#pragma once
#include "engine/world_view.h"
#include <map>
namespace OpenBlood2
{
    struct PolyGridViewEntry
    {
        std::uint64_t id = 0;
        std::string texture, error;
        bool visible = false;
        std::uint32_t range = UINT32_MAX;
        unsigned width = 0;
    };
    class PolyGridView
    {
      public:
        PolyGridView()
        {
            clear();
        }
        void clear();
        void begin_frame();
        bool add(std::uint64_t, bool, NodePose, DVector, unsigned, std::span<const std::int8_t>,
                 const std::array<DVector, 256>&, std::array<float, 2>, std::array<float, 2>, float, std::string_view,
                 const Resources&, std::string&);
        WorldMesh mesh;
        std::vector<WorldDraw> draws;
        std::vector<PolyGridViewEntry> entries;
        std::uint64_t revision = 0;

      private:
        std::map<std::string, std::uint32_t> materials_;
    };
} // namespace OpenBlood2
