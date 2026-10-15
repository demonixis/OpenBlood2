#include "brush_smoke.h"
#include "game/original/RotatingBrush.h"
namespace OpenBlood2
{
    bool prepare_brush_smoke(GameSession& g, ObjectId& id)
    {
        WorldModel model;
        model.name = "brush_smoke_platform";
        model.translation = {0, 10050, 0};
        const std::array<DVector, 8> points{{{0, 10000, -2},
                                             {40, 10000, -2},
                                             {40, 10100, -2},
                                             {0, 10100, -2},
                                             {0, 10000, 2},
                                             {40, 10000, 2},
                                             {40, 10100, 2},
                                             {0, 10100, 2}}};
        const std::array<std::array<unsigned, 4>, 6> faces{
            {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 3, 7, 4}, {1, 5, 6, 2}, {0, 4, 5, 1}, {3, 2, 6, 7}}};
        const std::array<DVector, 6> normals{{{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}}};
        model.points.assign(points.begin(), points.end());
        for (unsigned i = 0; i < 6; ++i)
        {
            model.planes.push_back({normals[i], vector_dot(normals[i], points[faces[i][0]])});
            model.surfaces.push_back({});
            model.surfaces.back().plane = i;
            Polygon polygon;
            polygon.surface = i;
            for (auto index : faces[i])
                polygon.points.push_back(static_cast<std::uint16_t>(index));
            model.polygons.push_back(std::move(polygon));
        }
        g.world.models.push_back(std::move(model));

        auto h = g.objects.create(g.objects.find_class("RotatingBrush"), {}, Creation::World,
                                  {{"Name", std::string("brush_smoke_platform")},
                                   {"YAxisRevTime", 4.0f},
                                   {"StartOn", false},
                                   {"YAxisSpinUpTime", .03f},
                                   {"YAxisSpinDownTime", .02f},
                                   {"BoxPhysics", false},
                                   {"HitPoints", 50.0f}});
        if (!h)
            return false;
        id = g.objects.get(h)->id;
        auto rider = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                      {{"Name", std::string("brush_smoke_rider")},
                                       {"Pos", DVector{20, 10102.001f, 0}},
                                       {"Flags", std::int32_t(FLAG_SOLID)}});
        DVector dims{1, 2, 1};
        if (!rider)
            return false;
        g.server.SetObjectDims(rider, &dims);
        return true;
    }
    bool command_brush_smoke(GameSession& g, ObjectId id, const char* text)
    {
        const auto* r = g.objects.get(id);
        if (!r || r->life != Life::Alive)
            return false;
        auto str = g.server.CreateString(text);
        auto message = g.server.StartMessageToObject(r->game.get(), r->handle, 1000);
        bool ok =
            message && g.server.WriteToMessageHString(message, str) == DE_OK && g.server.EndMessage(message) == DE_OK;
        if (str)
            g.server.FreeString(str);
        return ok;
    }
    bool check_brush_smoke(GameSession& g, ObjectId id)
    {
        const auto* r = g.objects.get(id);
        const auto* b = r ? dynamic_cast<const RotatingBrush*>(r->game.get()) : nullptr;
        return b && b->rotating.angles.y > 0 && r->mover_moves > 0 && r->mover_riders > 0;
    }
} // namespace OpenBlood2
