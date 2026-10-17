#pragma once
#include "cpp_aggregate_de.h"
#include "game/weapon_messages.h"
#include <span>
#include <string>
#include <vector>

// Original CMovement aggregate and obstacle path algorithm. Not the client MoveMgr.
// Selected source rules, bounded searches and adaptations: AI_NAVIGATION_CONTRACTS.md.
class CMovement : public Aggregate
{
  public:
    bool Init(HOBJECT);
    void Term();
    bool Update();
    DVector GetPos() const
    {
        return position;
    }
    DVector GetLastPos() const
    {
        return last_position;
    }
    DRotation GetRotation() const
    {
        return rotation;
    }
    DVector GetRightVector() const
    {
        return right;
    }
    DVector GetUpVector() const
    {
        return up;
    }
    DVector GetForwardVector() const
    {
        return forward;
    }
    bool GetNextPathPoint(DVector&) const;
    bool MoveToNextPathPoint(); // False on empty or after consuming the last point, as in source.
    bool CalculatePath(DVector destination);
    bool ClearToPoint(DVector start, DVector destination, DVector dimensions, OpenBlood2::SegmentHit&);
    bool ConsolidatePath(); // Original optional method; CalculatePath does not call it.
    std::span<const DVector> path() const
    {
        return points;
    }
    enum class Outcome
    {
        None,
        Direct,
        Detour,
        Blocked,
        Invalid,
        Unavailable,
        Limit,
        Clear,
        Obstructed
    };
    Outcome outcome = Outcome::None;
    std::string error;
    bool initialized = false, sampled = false;
    std::uint64_t samples = 0, sample_failures = 0, calculations = 0, failures = 0;
    unsigned rays = 0, iterations = 0, added_points = 0;
    DVector requested_destination{};
    std::uint64_t alignment_fallbacks = 0;
    DVector alignment_direction{}, alignment_up{}, alignment_right{}; // Last successful fallback, read-only history.
    // Original FindTurn stops recursion after ten accumulated points; the typed
    // service additionally bounds loops/rays, including tiny authored dimensions.
    static constexpr unsigned max_iterations = 4096, max_rays = 8192, max_points = 64;

  protected:
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;

  private:
    HOBJECT owner = nullptr;
    DVector position{}, last_position{}, right{1, 0, 0}, up{0, 1, 0}, forward{0, 0, 1};
    DRotation rotation;
    std::vector<DVector> points;
    bool fail(Outcome, const char*);
    bool ready();
    bool frame_axes(DVector direction, DVector& forward_axis, DVector& right_axis);
    bool clear_to_point(DVector, DVector, DVector, OpenBlood2::SegmentHit&);
    bool add(DVector);
    bool cast(DVector, DVector, OpenBlood2::SegmentHit&);
    bool segment(DVector, DVector, OpenBlood2::SegmentHit&, bool clearance = false);
    bool shortest_turn(DVector, DVector& facing, float, DVector&);
    bool turn(DVector, DVector, DVector, float, float, DVector&);
};
