#include "app/sprite_scene.h"
#include "sprite_view_fixture.h"
#include <cmath>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool near(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    bool geometry(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_sprite_view::mount(resources, root, error));
        auto sprite = test_sprite_view::manifest();
        SpriteTrack track{35, 0};
        SpriteView scene;
        const auto camera = rotation_from_euler({0, .7f, 0});
        CHECK(scene.begin_frame({1, 2, 3}, camera, error));
        CHECK(scene.add(&sprite, track, {{4, 5, 6}, {}}, {.5f, 2, 1}, FLAG_VISIBLE, {1, 1}, resources, error));
        CHECK(scene.draws.size() == 1 && scene.mesh.vertices.size() == 6);
        CHECK(scene.entries[0].width == 4 && scene.entries[0].height == 2 && scene.entries[0].zero_alpha == 2 &&
              scene.entries[0].partial_alpha == 2 && scene.entries[0].error.empty());
        CHECK(near(scene.mesh.vertices[0].position.x, -2) && near(scene.mesh.vertices[0].position.y, 4));
        CHECK(scene.mesh.vertices[0].uv[0] == 0 && scene.mesh.vertices[0].uv[1] == 0);
        CHECK(near(scene.draws[0].rotation.m_Vec.y, camera.m_Vec.y));
        CHECK(track.frame == 0 && track.milliseconds == 35);
        auto revision = scene.revision;
        CHECK(scene.add(&sprite, track, {{4, 5, 6}, {}}, {1, 1, 1},
                        FLAG_VISIBLE | FLAG_ROTATEABLESPRITE | FLAG_SPRITECHROMAKEY, {2, 1}, resources, error));
        CHECK(near(scene.draws[1].rotation.m_Vec.y, 0) && scene.revision == revision);
        CHECK(scene.begin_frame({}, {}, error));
        for (int z : {2, 6, 4, 6})
            CHECK(scene.add(&sprite, track, {{0, 0, float(z)}, {}}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        scene.sort();
        CHECK(scene.draws[0].range == 1 && scene.draws[1].range == 3 && scene.draws[2].range == 2 &&
              scene.draws[3].range == 0);
        CHECK(scene.begin_frame({}, {}, error));
        CHECK(scene.add(&sprite, {99, 3}, {}, {1, 1, 1}, 0, {}, resources, error));
        CHECK(scene.draws.empty() && !scene.entries[0].visible && scene.entries[0].error.empty());
        CHECK(scene.revision == revision && scene.mesh.materials.size() == 2);
        CHECK(scene.add(&sprite, track, {}, {0, 1, 1}, FLAG_VISIBLE, {}, resources, error) && scene.draws.empty());
        CHECK(scene.add(&sprite, {0, 0, false}, {}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        CHECK(scene.draws.empty() && !scene.entries.back().visible && scene.entries.back().error.empty());
        scene.clear();
        CHECK(scene.revision > revision && scene.mesh.materials.size() == 1 && scene.entries.empty());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        Resources resources;
        std::string error;
        CHECK(test_sprite_view::mount(resources, root, error));
        auto sprite = test_sprite_view::manifest();
        SpriteView scene;
        CHECK(!scene.add(&sprite, {}, {}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        CHECK(scene.begin_frame({}, {}, error));
        CHECK(scene.add(nullptr, {}, {}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        CHECK(!scene.entries.back().error.empty());
        for (unsigned frame : {1u, 2u, 4u})
        {
            CHECK(scene.add(&sprite, {0, frame}, {}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
            CHECK(!scene.entries.back().error.empty() && scene.draws.empty());
        }
        for (unsigned bit : {5u, 6u, 7u, 12u})
        {
            CHECK(scene.add(&sprite, {}, {}, {1, 1, 1}, FLAG_VISIBLE | (1u << bit), {}, resources, error));
            CHECK(!scene.entries.back().error.empty() && scene.draws.empty());
        }
        CHECK(scene.add(&sprite, {}, {}, {-1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        CHECK(!scene.entries.back().error.empty());
        const float nan = std::numeric_limits<float>::quiet_NaN();
        CHECK(scene.add(&sprite, {}, {{nan, 0, 0}, {}}, {1, 1, 1}, FLAG_VISIBLE, {}, resources, error));
        CHECK(!scene.entries.back().error.empty() && scene.draws.empty());
        CHECK(!scene.begin_frame({nan, 0, 0}, {}, error) && scene.entries.empty());
        CHECK(scene.begin_frame({}, {}, error));
        for (unsigned i = 0; i < SpriteView::max_entries; ++i)
            CHECK(scene.add(&sprite, {}, {}, {1, 1, 1}, 0, {}, resources, error));
        CHECK(!scene.add(&sprite, {}, {}, {1, 1, 1}, 0, {}, resources, error) && scene.entries.empty() &&
              scene.draws.empty());
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession game;
        std::string error;
        Config config;
        config.data_dir = root;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        SpriteView scene;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(game.request_world("01_SubwayA", error) && game.enter_player(error) && game.step(.02));
            CHECK(update_sprite_scene(game, scene, error) && scene.entries.size() == 12 && scene.draws.size() == 12);
            CHECK(scene.mesh.materials.size() == 2 && scene.mesh.vertices.size() == 72);
            const auto& entry = scene.entries.front();
            CHECK(entry.error.empty() && entry.rotatable && entry.zero_alpha > 0 && entry.partial_alpha > 0);
            std::cout << "SubwayA frame=" << entry.frame_path << " " << entry.width << "x" << entry.height
                      << " zero=" << entry.zero_alpha << " partial=" << entry.partial_alpha << "\n";
            const auto id = entry.object;
            auto* record = game.objects.get(id);
            CHECK(record);
            const auto milliseconds = record->sprite_track.milliseconds;
            record->activity = OBJSTATE_INACTIVE;
            CHECK(update_sprite_scene(game, scene, error) && scene.draws.size() == 12);
            CHECK(record->sprite_track.milliseconds == milliseconds);
            record->state.m_Flags &= ~FLAG_VISIBLE;
            CHECK(update_sprite_scene(game, scene, error) && scene.draws.size() == 11 && scene.entries.size() == 12);
            CHECK(game.objects.remove(record->handle));
            CHECK(update_sprite_scene(game, scene, error) && scene.draws.size() == 11 && scene.entries.size() == 11);
            CHECK(game.objects.finish_frame() && game.unload_world());
            std::string unsupported;
            CHECK(!game.run_world(unsupported));
            scene.clear();
            CHECK(update_sprite_scene(game, scene, error) && scene.entries.empty() && scene.draws.empty());
            CHECK(scene.mesh.materials.size() == 1 && game.server.string_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    return (name == "geometry"   ? geometry(argv[2])
            : name == "failures" ? failures(argv[2])
                                 : name == "subway" && subway(argv[2]))
               ? 0
               : 1;
}
