#include "engine/world_view.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool parameters()
    {
        Surface s;
        s.has_effect = true;
        s.effect_name = "pAn";
        s.u = {2, 0, 0};
        s.v = {1, 0, -1};
        WorldSurfaceEffect e;
        CHECK(world_surface_effect(s, e) && e.speed == (std::array<float, 2>{}));
        s.effect_parameters = "+2";
        CHECK(world_surface_effect(s, e) && e.speed == (std::array<float, 2>{2, 0}));
        s.effect_parameters = " 2\t-4\n";
        CHECK(world_surface_effect(s, e));
        CHECK(e.origin_velocity.x == 0 && e.origin_velocity.z == 4);
        CHECK(e.texel_rate == (std::array<double, 2>{0, 4})); // Full basis dot products, not assumed unit/orthogonal.
        s.u = {1, 0, 0};
        s.v = {0, 0, 1};
        for (auto label : {"up", "LEFT", "Down", "right"})
        {
            s.effect_parameters = std::string(label) + " -1024";
            CHECK(world_surface_effect(s, e));
            CHECK(e.speed[0] == 0 && e.speed[1] == -1024 && e.origin_velocity.z == -1024);
            CHECK(e.texel_rate[0] == 0 && e.texel_rate[1] == 1024);
        }
        for (auto invalid :
             {"0 nan", "inf 0", "1e100 2", "1e-1000 2", "65537 0", "1 2 3", "1x 2", "? 2", "+-1", "\"1\" 2"})
        {
            s.effect_parameters = invalid;
            CHECK(!world_surface_effect(s, e) && !e.error.empty());
            CHECK(e.speed == (std::array<float, 2>{}) && e.texel_rate == (std::array<double, 2>{}));
            CHECK(e.parameters == invalid);
        }
        s.effect_parameters = "1 2";
        s.effect_name = "Pan ";
        CHECK(!world_surface_effect(s, e) && e.name == "Pan ");
        s.effect_name = "Pan";
        s.u.x = std::numeric_limits<float>::infinity();
        CHECK(!world_surface_effect(s, e));
        s.u.x = 100000;
        CHECK(!world_surface_effect(s, e));
        return true;
    }
    bool phase_clock()
    {
        std::array<float, 4> offset;
        CHECK(world_texture_offset({.25, -.5}, 0, offset) && offset == (std::array<float, 4>{}));
        CHECK(world_texture_offset({.25, -.5}, 1, offset) && offset[0] == .25f && offset[1] == -.5f);
        CHECK(world_texture_offset({.25, -.5}, 5, offset) && offset[0] == .25f && offset[1] == -.5f);
        CHECK(world_texture_offset({.25, -.5}, 100000001, offset) && offset[0] == .25f && offset[1] == -.5f);
        auto before = offset;
        CHECK(world_texture_offset({.25, -.5}, 100000001, offset) && offset == before);
        CHECK(world_texture_offset({.25, -.5}, 0, offset) && offset == (std::array<float, 4>{}));
        for (auto time : {-1., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
            CHECK(!world_texture_offset({}, time, offset) && offset == (std::array<float, 4>{}));
        CHECK(!world_texture_offset({1, std::numeric_limits<double>::quiet_NaN()}, 1, offset));
        CHECK(offset == (std::array<float, 4>{}));
        CHECK(!world_texture_offset({1e10, 0}, 0, offset));
        CHECK(!world_texture_offset({1e9, 0}, std::numeric_limits<double>::max(), offset));
        return true;
    }
    bool limits()
    {
        World w;
        w.models.emplace_back();
        Surface s;
        s.has_effect = true;
        s.effect_name = "Pan";
        s.effect_parameters = std::string(4096, '0');
        w.models[0].surfaces.resize(2046, s);
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(w, error) && mesh.effects.size() == 2046); // Invisible/unreferenced records retained.
        w.models[0].surfaces.push_back(s);
        CHECK(!mesh.build(w, error) && !error.empty() && mesh.effects.empty() && mesh.ranges.empty());
        s.effect_parameters = "";
        w.models[0].surfaces.assign(65537, s);
        CHECK(!mesh.build(w, error) && mesh.effects.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    std::string_view name = argv[1];
    return (name == "parameters" ? parameters()
            : name == "clock"    ? phase_clock()
            : name == "limits"   ? limits()
                                 : false)
               ? 0
               : 1;
}
