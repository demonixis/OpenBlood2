#pragma once
#include "client/collision.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <tuple>

// Test-only ground route search over the independent collision replica. It probes
// copies of the body; only the test client's ordinary CMoveMgr inputs move a player.
// Bounded 32-unit nodes, eight-unit collision probes and supported drops up to 768.
// No server registry, game callback, teleport, jump/ladder solver or asset mutation.
namespace test_ctf
{
    using namespace OpenBlood2;
    struct RoutePlanner
    {
        struct Node
        {
            ClientMotionState body;
            int x, z, parent;
            float cost;
        };
        struct Entry
        {
            float score;
            unsigned index;
            bool operator<(const Entry& b) const
            {
                return score > b.score;
            }
        };
        std::vector<Node> nodes;
        std::priority_queue<Entry> open;
        std::map<std::tuple<int, int, int>, float> best;
        std::vector<DVector> route;
        DVector goal, origin;
        unsigned visited = 0;
        float distance(DVector a) const
        {
            return std::hypot(goal.x - a.x, goal.z - a.z);
        }
        void begin(const ClientMotionState& body, DVector target)
        {
            *this = {};
            goal = target;
            origin = body.position;
            nodes.push_back({body, 0, 0, -1, 0});
            open.push({distance(origin), 0});
            best[{0, 0, int(std::round(origin.y))}] = 0;
        }
        bool step(const ClientCollisionWorld& world, ClientObjectId owner)
        {
            for (unsigned work = 0; work < 64 && !open.empty(); ++work)
            {
                auto index = open.top().index;
                open.pop();
                auto node = nodes[index];
                auto key = std::tuple(node.x, node.z, int(std::round(node.body.position.y)));
                if (node.cost > best[key])
                    continue;
                ++visited;
                if (distance(node.body.position) < 40 && std::abs(node.body.position.y - goal.y) < 70)
                {
                    for (int i = int(index); i >= 0; i = nodes[i].parent)
                        route.push_back(nodes[i].body.position);
                    std::reverse(route.begin(), route.end());
                    auto end = goal;
                    end.y = route.back().y;
                    route.push_back(end);
                    return true;
                }
                for (auto [dx, dz] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}})
                {
                    const int x = node.x + dx, z = node.z + dz;
                    if (std::abs(x) > 300 || std::abs(z) > 300)
                        continue;
                    DVector target{origin.x + x * 32, node.body.position.y, origin.z + z * 32};
                    ClientMotionState next;
                    next = node.body;
                    bool reached = true;
                    for (unsigned part = 1; part <= 4; ++part)
                    {
                        DVector dest{node.body.position.x + (target.x - node.body.position.x) * float(part) / 4,
                                     next.position.y,
                                     node.body.position.z + (target.z - node.body.position.z) * float(part) / 4};
                        if (next.standing && next.ground_normal.y > .5f)
                            dest.y -= ((dest.x - next.position.x) * next.ground_normal.x +
                                       (dest.z - next.position.z) * next.ground_normal.z) /
                                      next.ground_normal.y;
                        ClientMotionState moved;
                        const auto delta = vector_subtract(dest, next.position);
                        if (!world.predict(owner, next, dest, vector_scale(delta, 10), {}, moved) ||
                            std::hypot(moved.position.x - dest.x, moved.position.z - dest.z) > 1)
                        {
                            reached = false;
                            break;
                        }
                        ClientCollisionHit hit;
                        auto below = moved.position;
                        below.y -= 768;
                        if (world.sweep(owner, moved.position, below, moved.dimensions, hit, {}, .01) !=
                                CollisionQuery::Hit ||
                            hit.start_solid || hit.normal.y < .5)
                        {
                            reached = false;
                            break;
                        }
                        moved.position = vector_add(hit.point, vector_scale(hit.normal, .001f));
                        if (!world.support(owner, moved) || !moved.standing)
                        {
                            reached = false;
                            break;
                        }
                        next = moved;
                    }
                    if (!reached)
                        continue;
                    auto k = std::tuple(x, z, int(std::round(next.position.y)));
                    float cost = node.cost + std::hypot(float(dx), float(dz)) * 32 +
                                 std::abs(next.position.y - node.body.position.y) * .3f;
                    auto old = best.find(k);
                    if (old != best.end() && old->second <= cost)
                        continue;
                    if (nodes.size() >= 60000)
                        return false;
                    best[k] = cost;
                    nodes.push_back({next, x, z, int(index), cost});
                    open.push({cost + distance(next.position), unsigned(nodes.size() - 1)});
                }
            }
            return !open.empty() && visited < 30000;
        }
    };
} // namespace test_ctf
