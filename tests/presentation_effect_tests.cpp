#include "brush_fixture.h"
#include "engine/mark_clip.h"
#include "engine/sprite_view.h"
#include "engine/transparency.h"
#include "game/original/NapalmProjectile.h"
#include "game/original/VolumeBrush.h"
#include "game/presentation_effects.h"
#include "game/server_adapter.h"
#include <iostream>
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
    GameSession game;
    CHECK(game.initialize());
    auto empty = gather_scene_effects(game, {}, {});
    CHECK(!empty.uniform.light_info[0] && !empty.uniform.fog_color[3]);
    for (unsigned i = 0; i < 20; ++i)
    {
        CLightFX light;
        light.position = {float(i * 100), 0, 0};
        light.color = {1, .5f, 0};
        light.radius = 100;
        light.visible = true;
        game.server.light_effects.push_back(light);
    }
    auto lights = gather_scene_effects(game, {}, {});
    CHECK(lights.light_candidates == 20 && lights.light_dropped == 4 && lights.uniform.light_info[0] == 16);
    CHECK(lights.uniform.positions[0][0] == 0 && lights.uniform.positions[0][3] == 100);
    CHECK(lights.uniform.colors[0][1] == .5f);
    game.server.light_effects[0].visible = false;
    lights = gather_scene_effects(game, {}, {});
    CHECK(lights.light_candidates == 19 && lights.uniform.positions[0][0] == 100);
    game.server.light_effects[1].settings.flags = 128;
    game.server.light_effects[2].settings.flags = 8 | 32 | 64;
    lights = gather_scene_effects(game, {}, {});
    CHECK(lights.light_candidates == 18 && lights.fog_lights_unsupported == 1 && lights.shadow_lights_unshadowed == 1);
    CHECK(lights.uniform.colors[0][3] == 104);
    game.server.light_effects.clear();
    std::string error;
    CHECK(game.server.explosion_lights.publish(25, {}, {0, 1, 0}, "test", 0, error) && error.empty());
    CHECK(gather_scene_effects(game, {}, {}).light_candidates == 1);
    CHECK(game.server.explosion_lights.advance(1));
    CHECK(gather_scene_effects(game, {}, {}).light_candidates == 0);
    game.world.models.push_back(test_brush::box("water", {-100, -100, -100}, {100, 100, 100}));
    auto handle = game.objects.create(game.objects.find_class("Water"), {}, Creation::World,
                                      {{"Name", std::string("water")},
                                       {"ShowSurface", false},
                                       {"FogEnable", true},
                                       {"FogNearZ", -20.9f},
                                       {"FogFarZ", 200.9f},
                                       {"FogColor", DVector{64, 128, 255}}});
    CHECK(handle);
    auto fog = gather_scene_effects(game, {}, {});
    CHECK(fog.uniform.fog_color[3] == 1 && fog.uniform.fog_eye_near[3] == -20 && fog.uniform.fog_forward_far[3] == 200);
    CHECK(fog.uniform.fog_color[0] == 64.f / 255 && fog.uniform.fog_forward_far[2] == 1);
    CHECK(!gather_scene_effects(game, {0, 0, 200}, {}).uniform.fog_color[3]);
    auto* brush = dynamic_cast<VolumeBrush*>(game.objects.get(handle)->game.get());
    CHECK(brush);
    game.server.SetGameConVar("FogEnable", "1");
    game.server.SetGameConVar("FogNearZ", "10");
    game.server.SetGameConVar("FogFarZ", "1000");
    game.server.SetGameConVar("FogR", "255");
    CHECK(gather_scene_effects(game, {0, 0, 200}, {}).uniform.fog_color[0] == 1);
    CHECK(gather_scene_effects(game, {}, {}).uniform.fog_color[0] == 64.f / 255);
    brush->fog = false;
    CHECK(!gather_scene_effects(game, {}, {}).uniform.fog_color[3]);
    brush->fog = true;
    game.server.SetGameConVar("FogEnable", "0");
    brush->fog_far = -30;
    CHECK(gather_scene_effects(game, {}, {}).invalid_fog && !gather_scene_effects(game, {}, {}).uniform.fog_color[3]);
    CHECK(game.unload_world());
    CHECK(!gather_scene_effects(game, {}, {}).uniform.fog_color[3]);
    // Owned projectile light follows its current pose and disappears on impact/removal.
    struct LightProbe : CNapalmProjectile
    {
        DDWORD EngineMessageFn(DDWORD id, void* data, float value) override
        {
            (void)id;
            (void)data;
            (void)value;
            return 1;
        }
    };
    ClassInfo probe{"LightProbe",
                    game.objects.find_class("BaseClass"),
                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<LightProbe>(); },
                    {}};
    CHECK(game.objects.register_class(probe));
    const auto projectile_handle = game.objects.create(&probe, {});
    CHECK(projectile_handle);
    auto* record = game.objects.get(projectile_handle);
    auto* projectile = dynamic_cast<CNapalmProjectile*>(record->game.get());
    CHECK(projectile);
    projectile->configured = true;
    projectile->failed = false;
    record->state.m_Pos = {50, 60, 70};
    auto owner_light = gather_scene_effects(game, {}, {});
    CHECK(owner_light.light_candidates == 1 && owner_light.uniform.positions[0][0] == 50 &&
          owner_light.uniform.positions[0][3] == 100);
    projectile->exploded = true;
    CHECK(!gather_scene_effects(game, {}, {}).light_candidates);
    CHECK(game.unload_world());

    // A mark straddling a polygon edge keeps its original UVs and plane bias.
    std::vector<WorldVertex> quad;
    const WorldVertex corners[] = {{{-2, 2, 0}, {1, 1, 1}, {0, 0}},
                                   {{2, 2, 0}, {1, 1, 1}, {1, 0}},
                                   {{2, -2, 0}, {1, 1, 1}, {1, 1}},
                                   {{-2, -2, 0}, {1, 1, 1}, {0, 1}}};
    for (auto i : {0, 1, 2, 0, 2, 3})
        quad.push_back(corners[i]);
    std::vector<DVector> polygon{{-4, -4, -1.25f}, {0, -4, -1.25f}, {0, 4, -1.25f}, {-4, 4, -1.25f}};
    std::vector<WorldVertex> clipped;
    CHECK(clip_mark(quad, {}, polygon, clipped, error) && clipped.size() == 6);
    for (auto v : clipped)
        CHECK(v.position.x <= 0 && v.position.z == 0 && v.uv[0] <= .5f);
    std::reverse(polygon.begin(), polygon.end());
    CHECK(clip_mark(quad, {}, polygon, clipped, error) && clipped.size() == 6);
    WorldDraw moved;
    moved.position = {100, 30, 40};
    moved.rotation.m_Vec = {0, .70710678f, 0};
    moved.rotation.m_Spin = .70710678f;
    for (auto& point : polygon)
        point = vector_add(moved.position, rotate_vector(point, moved.rotation));
    CHECK(clip_mark(quad, moved, polygon, clipped, error) && clipped.size() == 6);
    polygon.assign(513, {});
    CHECK(!clip_mark(quad, {}, polygon, clipped, error) && clipped.empty());

    // Sprites sharing one uploaded batch split around other transparent families,
    // then consecutive compatible slices merge again without changing the vertex order.
    std::vector<TransparentDraw> queue;
    WorldDraw sprite;
    sprite.batch = 0;
    sprite.vertex_count = 6;
    sprite.alpha = .5f;
    queue.push_back({TransparentKind::Sprite, sprite, 10});
    sprite.first_vertex = 6;
    queue.push_back({TransparentKind::Sprite, sprite, 8});
    sprite.first_vertex = 12;
    queue.push_back({TransparentKind::Sprite, sprite, 7});
    queue.push_back({TransparentKind::World, {}, 9});
    queue.push_back({TransparentKind::Model, {}, 6});
    queue.push_back({TransparentKind::Explosion, {}, 5});
    queue.push_back({TransparentKind::PolyGrid, {}, 4});
    sort_transparency(queue);
    CHECK(queue.size() == 6 && queue[0].kind == TransparentKind::Sprite && queue[1].kind == TransparentKind::World);
    CHECK(queue[2].draw.first_vertex == 6 && queue[2].draw.vertex_count == 12 &&
          queue[5].kind == TransparentKind::PolyGrid);
    return 0;
}
