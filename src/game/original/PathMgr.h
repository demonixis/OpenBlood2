#pragma once
#include "cpp_server_de.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Original PathListData::Copy, adapted to owned values instead of string handles.
struct PathListData
{
    bool Copy(ServerDE&, HOBJECT, std::string& error);
    DVector position{};
    std::string name;
    std::optional<std::string> action_target, action_message;
    HOBJECT source = nullptr; // Inspection identity only, never used to execute a path.
};
using PathList = std::vector<PathListData>;

// Selected original PathMgr rules. Save/load and AI path traversal remain unavailable.
class PathMgr
{
  public:
    static constexpr std::size_t max_points = 4096;
    explicit PathMgr(ServerDE& server) : server_(server) {}
    bool BuildPathList();
    void ClearPathList();
    bool GetPath(std::string_view base, PathList& output, std::string& error) const;
    const PathList& entries() const
    {
        return paths_;
    }
    std::size_t size() const
    {
        return paths_.size();
    }
    const std::string& error() const
    {
        return error_;
    }

  private:
    ServerDE& server_;
    PathList paths_;
    std::string error_;
};
