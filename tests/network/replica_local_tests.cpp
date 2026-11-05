#include "../brush_fixture.h"
#include "../player_fixture.h"
#include "client/object_scene.h"
#include "client/world_view.h"
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    GameSession* callback_session = nullptr;
    bool callback_refused = false;
    struct PublicationProbe : BaseClass
    {
        DDWORD EngineMessageFn(DDWORD message, void*, float) override
        {
            if (message == MID_UPDATE)
            {
                std::string error;
                callback_refused = !callback_session->publish_client_world(error) && !error.empty();
            }
            return 1;
        }
    };
    std::unique_ptr<BaseClass> make_probe()
    {
        return std::make_unique<PublicationProbe>();
    }
    const ClassInfo probe_type{"PublicationProbe", nullptr, make_probe, {}};
    bool local(const char* root)
    {
        GameSession game;
        CHECK(test_player::setup(game, root, false));
        const auto create = [&]()
        {
            return game.objects.create_props(game.objects.find_class("Model"), {},
                                             "Filename models/characters/caleb.abc; Skin skins/characters/caleb1.dtx");
        };
        const auto parent = create(), child = create();
        CHECK(parent && child);
        HATTACHMENT attachment = 0;
        const DVector offset{};
        const DRotation rotation{};
        CHECK(game.objects.attach(parent, child, "", &offset, &rotation, &attachment) == DE_OK);
        CHECK(game.objects.refresh_attachments());
        auto* source = game.objects.get(parent);
        const auto id = source->id, child_id = game.objects.get(child)->id;
        source->color = {.1f, .2f, .3f};
        source->alpha = .25f;
        source->hidden_nodes = {1, 0};
        source->activity = OBJSTATE_INACTIVE;
        source->animation.time = 25;
        source->user_flags = 1024;
        std::string error;
        CHECK(game.publish_client_world(error));
        const auto reference = game.client_session.world.reference(id);
        const auto& replica = game.client_session.world;
        CHECK(replica.get(reference) && replica.get(reference)->color.y == .2f);
        CHECK(replica.get(child_id)->attached && replica.get(child_id)->attachment_resolved &&
              replica.get(child_id)->parent == id);
        CHECK(replica.get(id)->hidden_nodes == source->hidden_nodes && replica.get(id)->alpha == .25f);
        const auto frozen = replica; // Independent ownership, not a view into ObjectRecord.
        const auto clock = game.objects.time();
        const auto updates = source->updates;
        DVector teleported{99, 0, 0};
        CHECK(game.server.TeleportObject(parent, &teleported) == DE_OK && source->pose_revision);
        const auto pose_revision = source->pose_revision;
        CHECK(game.server.TeleportObject(parent, &teleported) == DE_OK && source->pose_revision == pose_revision);
        // A follower's small per-update teleport stays continuous (interpolated).
        teleported.x = 90;
        CHECK(game.server.TeleportObject(parent, &teleported) == DE_OK && source->pose_revision == pose_revision);
        teleported.x = 99;
        CHECK(game.server.TeleportObject(parent, &teleported) == DE_OK && source->pose_revision == pose_revision);
        CHECK(game.objects.refresh_attachments());
        source->hidden_nodes[0] = 0;
        source->animation.time = 75;
        CHECK(replica.get(id)->pose.position.x == 0 && replica.get(id)->animation.time == 25);
        ModelView scene;
        CHECK(append_client_models(replica, true, scene, game.resources, error));
        CHECK(source->updates == updates && game.objects.time() == clock && source->animation.time == 75);
        CHECK(game.publish_client_world(error));
        CHECK(replica.get(id)->pose.position.x == 99 && replica.get(id)->hidden_nodes[0] == 0);
        CHECK(replica.get(id)->pose_revision == pose_revision && replica.get(child_id)->pose_revision == pose_revision);
        CHECK(frozen.get(id)->pose_revision == 0 && frozen.get(child_id)->pose_revision == 0);
        CHECK(frozen.get(id)->pose.position.x == 0 && frozen.get(id)->hidden_nodes[0] == 1);
        CHECK(game.objects.remove(parent));
        CHECK(game.publish_client_world(error) && !replica.get(reference)); // Pending removal, before reclamation.
        CHECK(game.objects.finish_frame());
        const auto replacement = create();
        CHECK(replacement && game.publish_client_world(error));
        CHECK(!replica.get(reference));
        CHECK(game.objects.register_class(probe_type));
        const auto probe = game.objects.create(&probe_type, {});
        CHECK(probe && game.objects.set_next_update(probe, .001f));
        const auto sequence = replica.frame().sequence;
        callback_session = &game;
        CHECK(game.objects.advance(.01));
        CHECK(callback_refused && replica.frame().sequence == sequence);
        callback_session = nullptr;
        const auto epoch = replica.frame().world;
        CHECK(game.unload_world());
        CHECK(replica.frame().world != epoch && replica.frame().objects.empty());
        scene.begin_frame();
        CHECK(append_client_models(frozen, true, scene, game.resources, error));
        CHECK(scene.entries.size() == 2); // Immutable decoded model lifetime survives server teardown.
        return true;
    }
    bool bindings()
    {
        GameSession game;
        CHECK(game.initialize());
        game.world.min = {-100, -100, -100};
        game.world.max = {100, 100, 100};
        game.world.models.emplace_back();
        game.world.models.push_back(test_brush::box("sky", {-10, -10, 10}, {10, 10, 30}));
        const auto sky = game.objects.create(
            game.objects.find_class("DemoSkyWorldModel"), {}, Creation::World,
            {{"Name", std::string("sky")}, {"Index", std::int32_t(3)}, {"SkyDims", DVector{50, 50, 50}}});
        CHECK(sky);
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(game.world, error));
        auto compare = [&]()
        {
            if (!game.publish_client_world(error))
                return false;
            std::vector<WorldDraw> expected, actual;
            WorldViewStats a, b;
            if (!world_draws(mesh, game.objects, expected, a, error) ||
                !world_draws(mesh, game.client_session.world, actual, b, error))
                return false;
            if (expected.size() != actual.size() || a.hidden != b.hidden || a.unbound != b.unbound ||
                a.triangles != b.triangles || a.containers != b.containers)
                return false;
            DVector old_eye, new_eye;
            if (!game.server.sky.draws(game.world, mesh, game.objects, {20, 10, 0}, expected, old_eye, error) ||
                !client_sky_draws(game.world, mesh, game.client_session.world, {20, 10, 0}, actual, new_eye, error))
                return false;
            if (expected.size() != actual.size() || old_eye.x != new_eye.x || old_eye.y != new_eye.y ||
                old_eye.z != new_eye.z)
                return false;
            for (std::size_t i = 0; i < actual.size(); ++i)
                if (expected[i].range != actual[i].range || expected[i].position.x != actual[i].position.x ||
                    expected[i].rotation.m_Spin != actual[i].rotation.m_Spin || expected[i].alpha != actual[i].alpha)
                    return false;
            return true;
        };
        CHECK(compare());
        auto* r = game.objects.get(sky);
        r->state.m_Pos.x += 7;
        r->state.m_Rotation = rotation_from_euler({0, .2f, 0});
        r->alpha = .4f;
        CHECK(compare());
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(compare());
        r->state.m_Flags = FLAG_VISIBLE;
        CHECK(compare());
        r->world_model_index = UINT32_MAX;
        CHECK(game.publish_client_world(error));
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        CHECK(!world_draws(mesh, game.client_session.world, draws, stats, error) && draws.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 3 && (std::string(argv[1]) == "local" ? local(argv[2]) : bindings()) ? 0 : 1;
}
