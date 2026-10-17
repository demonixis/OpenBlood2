#pragma once
#include "game/original/SparksFX.h"
namespace OpenBlood2
{
    struct SparkReport
    {
        std::uint64_t id = 0, effect = 0;
        SparkSettings settings;
        std::string source, error;
        bool ended = false;
    };
    class Sparks
    {
      public:
        std::uint64_t publish(const SparkSettings&, std::string_view, const Resources&, double, std::string&);
        bool advance(double);
        void clear();
        std::size_t live_particles() const;
        std::vector<CSparksFX> active;
        std::vector<SparkReport> reports;
        std::uint64_t started = 0, retired = 0, failed = 0;
        std::string update_error;

      private:
        std::minstd_rand random_{1};
        std::uint64_t next_id_ = 1, next_event_ = 1;
    };
} // namespace OpenBlood2
