#include "engine/collision.h"
#include "engine/scene_effects.h"
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
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    SceneEffects scene;
    const EffectLightBounds box{{-1, -1, -1}, {1, 1, 1}, true};
    SceneEffectsUniform selected;
    for (unsigned i = 1; i <= 40; ++i)
        scene.lights.push_back({{{0, 0, 0}, {i / 100.f, 0, 0}, 10}, 0});
    // A second receiver's light survives even beyond the first receiver's desktop budget.
    scene.lights.push_back({{{1000, 0, 0}, {0, 1, 0}, 10}, 0});
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 32 && std::abs(selected.colors[0][0] - .4f) < 1e-6f);
    CHECK(std::abs(selected.colors[31][0] - .09f) < 1e-6f && scene.selection.dropped == 8);
    select_effect_lights(scene, box, {1000, 0, 0}, {}, false, selected);
    CHECK(selected.light_info[0] == 1 && selected.colors[0][1] == 1 && selected.colors[1][0] == 0);
    scene.light_budget = mobile_effect_lights;
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 12 && std::abs(selected.colors[11][0] - .29f) < 1e-6f);
    scene.light_budget = 0;
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 0 && selected.positions[0][3] == 0);

    // Filter the world-only mask before ranking, so it never steals model slots.
    scene = {};
    scene.light_budget = 12;
    for (unsigned i = 0; i < 35; ++i)
        scene.lights.push_back({{{0, 0, 0}, {1, 1, 1}, 20}, 32});
    scene.lights.push_back({{{0, 0, 0}, {0, .3f, 0}, 20}, 0});
    select_effect_lights(scene, box, {}, {}, true, selected);
    CHECK(selected.light_info[0] == 1 && selected.colors[0][1] == .3f);
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 12 && selected.colors[0][3] == 32);

    // Conservative OBB-distance selection catches a light at the end of a long rotated object.
    scene = {};
    scene.lights.push_back({{{1000, 0, -100}, {1, 0, 0}, 1}, 64});
    const DRotation rotation{{0, .70710678f, 0}, .70710678f};
    select_effect_lights(scene, {{-100, -1, -1}, {100, 1, 1}, true}, {1000, 0, 0}, rotation, true, selected);
    CHECK(selected.light_info[0] == 1 && selected.colors[0][3] == 64);
    select_effect_lights(scene, box, {1000, 0, 0}, rotation, true, selected);
    CHECK(selected.light_info[0] == 0);

    // Rank contribution, not just distance; equal weights retain creation order.
    scene = {};
    scene.light_budget = 2;
    scene.lights = {
        {{{10, 0, 0}, {.9f, 0, 0}, 10}, 0}, {{{0, 0, 0}, {.2f, 0, 0}, 10}, 0}, {{{.5f, 0, 0}, {.2f, 0, 0}, 10}, 0}};
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 2 && selected.positions[0][0] == 0 && selected.positions[1][0] == .5f);
    scene.uniform.fog_color = {.2f, .3f, .4f, 1};
    scene.lights = {{{{2, 0, 0}, {1, 1, 1}, 1}, 0}}; // Tangent to the box: zero possible contribution.
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 0 && selected.fog_color == scene.uniform.fog_color);
    select_effect_lights(scene, {}, {}, {}, false, selected); // Unknown bounds are conservative.
    CHECK(selected.light_info[0] == 1);
    scene.lights[0].light.radius = std::numeric_limits<float>::quiet_NaN();
    select_effect_lights(scene, box, {}, {}, false, selected);
    CHECK(selected.light_info[0] == 0);
    std::cout << "Per-receiver 12/32 selection, overlap, rotation, stable contribution order and flags pass\n";
}
