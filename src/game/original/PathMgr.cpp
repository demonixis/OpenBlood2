// Original PathMgr.cpp and PathListData.cpp (Monolith, 1998).
// See docs/contracts/path_manager_contracts.md for preserved ordering and ownership adaptations.
#include "PathMgr.h"
#include "PathPoint.h"
#include <algorithm>
#include <utility>
namespace
{
    unsigned char lower(unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
    }
} // namespace
bool PathListData::Copy(ServerDE& server, HOBJECT object, std::string& error)
{
    error.clear();
    auto* point = server.IsObjectAlive(object) ? dynamic_cast<PathPoint*>(server.HandleToObject(object)) : nullptr;
    if (!point)
    {
        error = "PathPoint is missing, pending or has the wrong class";
        return false;
    }
    const char* object_name = server.GetObjectName(object);
    if (!object_name)
    {
        error = "PathPoint name is unavailable";
        return false;
    }
    PathListData value;
    value.source = object;
    value.name = object_name;
    server.GetObjectPos(object, &value.position);
    auto copy = [&](HSTRING handle, std::optional<std::string>& target)
    {
        if (!handle)
            return true;
        const auto* text = server.GetStringData(handle);
        if (!text)
            return false;
        target = text;
        return true;
    };
    if (!copy(point->GetActionTarget(), value.action_target) || !copy(point->GetActionMessage(), value.action_message))
    {
        error = "PathPoint action string is unavailable";
        return false;
    }
    *this = std::move(value);
    return true;
}
void PathMgr::ClearPathList()
{
    paths_.clear();
    error_.clear();
}
bool PathMgr::BuildPathList()
{
    ClearPathList();
    const auto type = server_.GetClass("PathPoint");
    if (!type)
    {
        error_ = "PathPoint class is unavailable";
        return false;
    }
    PathList snapshot;
    for (auto h = server_.GetNextObject(nullptr); h; h = server_.GetNextObject(h))
    {
        if (!server_.IsKindOf(server_.GetObjectClass(h), type))
            continue;
        if (snapshot.size() == max_points)
        {
            error_ = "PathPoint snapshot exceeds the 4096-point limit";
            return false;
        }
        PathListData entry;
        if (!entry.Copy(server_, h, error_))
            return false;
        snapshot.push_back(std::move(entry));
        // Source comment mentions SMELL filtering/removal, but neither is executed.
    }
    paths_ = std::move(snapshot); // Failed builds publish no partial world snapshot.
    return true;
}
bool PathMgr::GetPath(std::string_view base, PathList& output, std::string& error) const
{
    error.clear();
    if (base.empty() || base.size() > 100 || base.find('\0') != base.npos)
    {
        error = "Path prefix is empty, contains NUL or exceeds 100 bytes";
        return false; // Source preflight failures preserve the caller's old list.
    }
    if (paths_.empty())
    {
        error = "PathPoint snapshot is empty";
        return false;
    }
    // Copy the prefix before clearing output: callers may supply output[0].name.
    const std::string prefix(base);
    output.clear();
    // AddToSortedList starts at the null-data sentinel and always inserts after it.
    // The observed source algorithm reverses enumeration; it never compares names.
    for (auto i = paths_.rbegin(); i != paths_.rend(); ++i)
    {
        if (i->name.size() <= prefix.size() || i->name[prefix.size()] < '0' || i->name[prefix.size()] > '9')
            continue;
        if (std::equal(prefix.begin(), prefix.end(), i->name.begin(),
                       [](unsigned char a, unsigned char b) { return lower(a) == lower(b); }))
            output.push_back(*i); // Owned copies replace the source's borrowed PathListData pointers.
    }
    if (output.empty())
    {
        error = "No PathPoint matches this prefix followed by a digit";
        return false;
    }
    return true;
}
