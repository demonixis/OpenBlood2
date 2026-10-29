// Selected original ObjectDLL/movement.cpp:29-534 (Monolith, 1997/1998).
#include "Movement.h"
#include "cpp_engineobjects_de.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
namespace
{
    float distance(DVector a, DVector b)
    {
        auto d = vector_subtract(a, b);
        return std::hypot(d.x, d.y, d.z);
    }
} // namespace
bool CMovement::frame_axes(DVector direction, DVector& forward_axis, DVector& right_axis)
{
    bool fallback = false;
    if (!alignment_axes(direction, up, forward_axis, right_axis, fallback))
        return false;
    if (fallback)
    {
        ++alignment_fallbacks;
        alignment_direction = forward_axis;
        alignment_up = up;
        alignment_right = right_axis;
    }
    return true;
}
bool CMovement::fail(Outcome status, const char* why)
{
    outcome = status;
    error = why;
    ++failures;
    return false;
}
bool CMovement::ready()
{
    auto* server = BaseClass::GetServerDE();
    return server && initialized && sampled && server->IsObjectAlive(owner);
}
bool CMovement::Init(HOBJECT object)
{
    auto* server = BaseClass::GetServerDE();
    if (!server || !server->IsObjectAlive(object))
        return fail(Outcome::Invalid, "Movement initialization requires a live owner");
    Term();
    error.clear();
    outcome = Outcome::None;
    rays = iterations = 0;
    owner = object;
    initialized = true;
    sampled = false;
    last_position = {};
    // Source leaves the initial current transform uninitialized. Sample explicitly
    // instead of permitting navigation to read indeterminate data on its first tick.
    if (!Update())
    {
        initialized = false;
        owner = nullptr;
        return false;
    }
    last_position = {};
    return true;
}
void CMovement::Term()
{
    points.clear();
    added_points = 0;
}
bool CMovement::Update()
{
    auto* server = BaseClass::GetServerDE();
    DVector pos;
    DRotation rot;
    if (!server || !initialized || !server->IsObjectAlive(owner) || server->GetObjectPos(owner, &pos) != DE_OK ||
        server->GetObjectRotation(owner, &rot) != DE_OK)
    {
        ++sample_failures;
        sampled = false;
        return fail(Outcome::Unavailable, "Movement owner/transform unavailable");
    }
    last_position = sampled ? position : DVector{};
    position = pos;
    rotation = rot;
    right = rotate_vector({1, 0, 0}, rot);
    up = rotate_vector({0, 1, 0}, rot);
    forward = rotate_vector({0, 0, 1}, rot);
    sampled = true;
    ++samples;
    return true;
}
DDWORD CMovement::EngineMessageFn(LPBASECLASS object, DDWORD message, void* data, DFLOAT value)
{
    if (message == MID_UPDATE && initialized)
        Update();
    if (message == MID_SAVEOBJECT)
        Save(*static_cast<HMESSAGEWRITE*>(data), static_cast<DDWORD>(value));
    if (message == MID_LOADOBJECT)
        Load(object, *static_cast<HMESSAGEREAD*>(data), static_cast<DDWORD>(value));
    return Aggregate::EngineMessageFn(object, message, data, value);
}
void CMovement::Save(HMESSAGEWRITE h, DDWORD)
{
    auto* server = BaseClass::GetServerDE();
    // NULL for an owner that never initialized this aggregate, such as the player.
    server->WriteToLoadSaveMessageObject(h, initialized ? owner : nullptr);
    server->WriteToMessageRotation(h, &rotation);
    for (const auto* v : {&right, &up, &forward, &position, &last_position})
        server->WriteToMessageVector(h, v);
    server->WriteToMessageDWord(h, static_cast<DDWORD>(points.size()));
    for (const auto& point : points)
        server->WriteToMessageVector(h, &point);
}
void CMovement::Load(LPBASECLASS object, HMESSAGEREAD h, DDWORD)
{
    auto* server = BaseClass::GetServerDE();
    HOBJECT saved_owner = nullptr;
    server->ReadFromLoadSaveMessageObject(h, &saved_owner);
    DRotation rot;
    server->ReadFromMessageRotation(h, &rot);
    DVector axes[5];
    for (auto& v : axes)
        server->ReadFromMessageVector(h, &v);
    const DDWORD count = server->ReadFromMessageDWord(h);
    if (count > max_points) // The original list has no bound; the typed path keeps its budget.
    {
        server->RejectPayload(h, "CMovement Load: path exceeds the movement point budget");
        return;
    }
    std::vector<DVector> path(count);
    for (auto& point : path)
        server->ReadFromMessageVector(h, &point);
    const auto norm = vector_dot(rot.m_Vec, rot.m_Vec) + rot.m_Spin * rot.m_Spin;
    bool finite = std::isfinite(norm);
    for (const auto& v : axes)
        finite = finite && physics_vector(v);
    for (const auto& v : path)
        finite = finite && physics_vector(v);
    if ((saved_owner && (!object || saved_owner != object->m_hObject)) || !finite)
    {
        server->RejectPayload(h, "CMovement Load: the owner is not this object or the cached transform is invalid");
        return;
    }
    owner = saved_owner;
    initialized = sampled = saved_owner != nullptr; // The saved transform is the last sample.
    rotation = rot;
    right = axes[0];
    up = axes[1];
    forward = axes[2];
    position = axes[3];
    last_position = axes[4];
    points = std::move(path); // Empty after initial update: the original appends to an empty list.
    added_points += count;    // AddPosToPathList also counts each loaded point.
}
bool CMovement::GetNextPathPoint(DVector& out) const
{
    if (points.empty())
        return false;
    out = points.front();
    return true;
}
bool CMovement::MoveToNextPathPoint()
{
    if (points.empty())
        return false;
    points.erase(points.begin());
    return !points.empty();
}
bool CMovement::add(DVector p)
{
    if (!physics_vector(p))
        return fail(Outcome::Invalid, "Path point outside collision coordinate domain");
    if (points.size() == max_points)
        return fail(Outcome::Limit, "Movement path point budget exhausted");
    points.push_back(p);
    ++added_points;
    return true;
}
bool CMovement::segment(DVector from, DVector to, SegmentHit& hit, bool clearance)
{
    if (++rays > max_rays)
        return fail(Outcome::Limit, "Movement ray budget exhausted");
    const auto result = BaseClass::GetServerDE()->IntersectSegment(
        from, to, clearance ? owner : nullptr, hit, false, SegmentObjects::SolidOnly,
        clearance ? SegmentFilter::IgnoreCharacters : SegmentFilter::None);
    if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
        return fail(result == SegmentResult::Invalid ? Outcome::Invalid : Outcome::Unavailable,
                    "Movement segment service failed");
    return result == SegmentResult::Hit;
}
bool CMovement::cast(DVector from, DVector direction, SegmentHit& hit)
{
    if (++rays > max_rays)
        return fail(Outcome::Limit, "Movement ray budget exhausted");
    const auto result = BaseClass::GetServerDE()->CastRay(from, direction, hit);
    if (result == SegmentResult::Invalid || result == SegmentResult::Unavailable)
        return fail(result == SegmentResult::Invalid ? Outcome::Invalid : Outcome::Unavailable,
                    "Movement infinite-ray service failed");
    return result == SegmentResult::Hit;
}
bool CMovement::ClearToPoint(DVector start, DVector destination, DVector dimensions, SegmentHit& hit)
{
    error.clear();
    rays = iterations = 0;
    outcome = Outcome::None;
    const bool clear = clear_to_point(start, destination, dimensions, hit);
    if (error.empty())
        outcome = clear ? Outcome::Clear : Outcome::Obstructed;
    return clear;
}
bool CMovement::clear_to_point(DVector start, DVector destination, DVector dimensions, SegmentHit& hit)
{
    hit = {};
    if (!ready() || !physics_vector(start) || !physics_vector(destination) || !physics_vector(dimensions) ||
        dimensions.x < 0 || dimensions.y < 0 || dimensions.z < 0)
        return fail(Outcome::Invalid, "Invalid movement clearance request");
    DVector f, r;
    // A coincident destination leaves nothing to cross: the source's zero-length side
    // segments cannot hit, so the point is clear.
    if (vector_dot(vector_subtract(destination, start), vector_subtract(destination, start)) < 1e-12f)
        return true;
    if (!frame_axes(vector_subtract(destination, start), f, r))
        return fail(Outcome::Invalid, "Movement clearance direction/up is degenerate");
    const float width = std::hypot(dimensions.x, dimensions.z) + .1f;
    // Source deliberately checks only two side rays, right before left. It is
    // not a swept body or a check for an obstacle between the two rays.
    for (auto sign : {1.f, -1.f})
    {
        const auto offset = vector_scale(r, sign * width);
        const bool hit_object = segment(vector_add(start, offset), vector_add(destination, offset), hit, true);
        if (!error.empty() || hit_object)
            return false;
    }
    return true;
}
bool CMovement::shortest_turn(DVector start, DVector& facing, float step, DVector& out)
{
    DVector f, r;
    // An obstruction reported without a normal (the ray started inside it) gives no
    // side to search: an ordinary planning refusal, never an actor failure.
    if (!frame_axes(facing, f, r))
        return fail(Outcome::Blocked, "Obstacle normal is degenerate");
    const auto left = vector_scale(r, -1);
    SegmentHit hit;
    float left_distance = 0, right_distance = 0;
    if (cast(start, left, hit))
        left_distance = distance(start, hit.point);
    if (!error.empty())
        return false;
    if (cast(start, r, hit))
        right_distance = distance(start, hit.point);
    if (!error.empty())
        return false;
    const auto extension = vector_scale(f, step + 5);
    out = start;
    for (float width = step; !(width >= right_distance && width >= left_distance);)
    {
        if (++iterations > max_iterations)
            return fail(Outcome::Limit, "Movement turn-search budget exhausted");
        for (unsigned side = 0; side < 2; ++side)
        {
            if (width >= (side == 0 ? right_distance : left_distance))
                continue;
            const auto axis = side == 0 ? r : left;
            const auto from = vector_add(start, vector_scale(axis, width));
            const bool blocked = segment(from, vector_add(from, extension), hit);
            if (!error.empty())
                return false;
            if (!blocked)
            {
                out = vector_add(from, vector_scale(axis, width / 2));
                facing = side == 0 ? left : r;
                return true;
            }
        }
        const auto next = width + step;
        if (!(next > width))
            return fail(Outcome::Limit, "Movement search increment lost precision");
        width = next;
    }
    return true;
}
bool CMovement::turn(DVector start, DVector test_direction, DVector move_direction, float step, float test_length,
                     DVector& out)
{
    SegmentHit hit;
    float maximum = 0;
    if (cast(start, move_direction, hit))
        maximum = distance(start, hit.point);
    if (!error.empty())
        return false;
    DVector current = start;
    float travelled = 0;
    for (; !((travelled + step) >= maximum);)
    {
        if (++iterations > max_iterations)
            return fail(Outcome::Limit, "Movement recursive-turn budget exhausted");
        current = vector_add(current, vector_scale(move_direction, step));
        const bool blocked = segment(current, vector_add(current, vector_scale(test_direction, test_length)), hit);
        if (!error.empty())
            return false;
        if (!blocked)
        {
            out = vector_add(current, vector_scale(move_direction, step));
            return true;
        }
        const float next = travelled + step;
        if (!(next > travelled))
            return fail(Outcome::Limit, "Movement turn increment lost precision");
        travelled = next;
    }
    if (added_points >= 10 || distance(current, start) <= 0)
    {
        out = current;
        return true;
    }
    if (!add(current))
        return false;
    return turn(current, move_direction, vector_scale(test_direction, -1), step, (maximum - travelled) + 1, out);
}
bool CMovement::CalculatePath(DVector destination)
{
    ++calculations;
    error.clear();
    outcome = Outcome::None;
    rays = iterations = 0;
    requested_destination = destination;
    // Source rejects the world origin before clearing the old route.
    if (!ready() || !physics_vector(destination) || distance({}, destination) <= 0)
        return fail(Outcome::Invalid, "Path requires a sampled live owner and nonzero finite destination");
    Term();
    DVector dimensions;
    if (BaseClass::GetServerDE()->GetObjectDims(owner, &dimensions) != DE_OK)
        return fail(Outcome::Unavailable, "Path owner dimensions unavailable");
    const float width = std::hypot(dimensions.x, dimensions.z) + .1f;
    SegmentHit hit;
    if (clear_to_point(position, destination, dimensions, hit))
    {
        if (!add(destination))
            return false;
        outcome = Outcome::Direct;
        return true;
    }
    if (!error.empty())
        return false;
    const auto contact = vector_add(hit.point, vector_scale(hit.normal, width - .1f));
    if (!add(contact))
        return false;
    auto normal = vector_scale(hit.normal, -1);
    auto facing = normal;
    DVector next;
    if (!shortest_turn(contact, facing, width, next))
        return false;
    if (distance(next, contact) <= 0)
        return fail(Outcome::Blocked, "No source lateral turn found");
    if (!add(next) || !turn(next, facing, normal, width, width, next))
        return false;
    if (distance(next, destination) <= 0)
        return fail(Outcome::Blocked, "Source rejects turn coincident with destination");
    if (!clear_to_point(next, destination, dimensions, hit))
    {
        if (!error.empty())
            return false;
        return fail(Outcome::Blocked, "Source final detour clearance failed");
    }
    if (!add(next) || !add(destination))
        return false;
    outcome = Outcome::Detour;
    return true;
}
bool CMovement::ConsolidatePath()
{
    error.clear();
    rays = iterations = 0;
    if (!ready())
        return fail(Outcome::Invalid, "Path consolidation requires a live sampled owner");
    SegmentHit hit;
    if (points.size() >= 2)
    {
        bool blocked = segment(position, points[1], hit);
        if (!error.empty())
            return false;
        if (!blocked)
            points.erase(points.begin());
    }
    std::size_t first = 0, second = 2;
    while (second < points.size())
    {
        bool blocked = segment(points[first], points[second], hit);
        if (!error.empty())
            return false;
        if (!blocked)
            points.erase(points.begin() + first + 1); // second retains the next source node's new index.
        else
        {
            ++first;
            ++second;
        }
    }
    return true;
}
