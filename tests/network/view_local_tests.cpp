#include "../brush_fixture.h"
#include "../player_fixture.h"
#include "game/original/CameraObj.h"
#include <random>
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
    bool enter(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root) && g.enter_player(error));
        CHECK(g.step(1. / 60)); // Original inventory publishes its selected weapon on the first playing tick.
        return true;
    }
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool boundary(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        auto h = g.objects.create(
            g.objects.find_class("CameraObj"), {}, Creation::World,
            {{"StartActive", true}, {"HidePlayer", true}, {"Type", 1}, {"Pos", DVector{100, 150, 200}}});
        CHECK(h && g.client_shell.UpdateCameraSelection());
        auto* player = g.server_shell.Player(g.local_client);
        auto& camera = g.client_session.camera;
        CHECK(player->camera_movement_blocked && camera.view.object == g.objects.get(h)->id);
        CHECK(&g.client_shell.view == &camera.view && &g.client_shell.stats == &g.client_session.hud.stats);
        const auto frame = camera.frame();
        LocalCameraCommand command{{frame.world, frame.player.object}, frame.sequence, {}};
        CHECK(!g.local_presentation.commit(g.local_client, command)); // Already consumed by shell selection.
        CHECK(g.local_presentation.publish(g.local_client, nullptr, nullptr));
        command.sequence = camera.frame().sequence;
        ++command.owner.world;
        CHECK(!g.local_presentation.commit(g.local_client, command));
        --command.owner.world;
        // A client-rendered view cannot remove the authoritative scripted camera gate.
        camera.view = {};
        CHECK(g.local_presentation.commit(g.local_client, command) && player->camera_movement_blocked);
        CHECK(g.local_presentation.publish(g.local_client, nullptr, nullptr));
        command.sequence = camera.frame().sequence;
        g.objects.remove(h);
        CHECK(!g.local_presentation.commit(g.local_client, command));
        CHECK(g.client_shell.UpdateCameraSelection() && !player->camera_movement_blocked && !camera.view.object);
        auto stats = g.client_shell.stats;
        stats.health = 37;
        stats.max_health = 150;
        CHECK(g.server.SendToClient(g.local_client, stats) == DE_OK);
        std::string error;
        CHECK(g.pump_player_messages(error));
        const auto hud = g.client_shell.HudSnapshot();
        CHECK(hud.stats == stats && hud.time == g.server.GetTime() && hud.world_revision == frame.world);
        CHECK(hud.visible && hud.crosshair_visible && !g.client_shell.HudSnapshot(true).crosshair_visible);
        const auto sequence = g.client_session.hud.sequence();
        stats.max_health = -1;
        CHECK(!g.local_presentation.deliver(g.local_client, stats));
        CHECK(g.client_session.hud.sequence() == sequence && g.client_shell.stats.health == 37);
        CHECK(g.leave_player() && !g.client_session.hud.owner().object && !camera.frame().player.object);
        CHECK(!g.client_shell.HudSnapshot().visible && !g.client_shell.HudSnapshot().crosshair_visible);
        CHECK(!g.local_presentation.commit(g.local_client, command));
        CHECK(g.enter_player(error) && g.step(1. / 60) && g.client_shell.HudSnapshot().visible);
        CHECK(g.client_session.hud.owner().object != command.owner.object);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        ClientSession detached;
        {
            GameSession g;
            CHECK(enter(g, root));
            g.client_shell.third_person = true;
            CHECK(g.client_shell.UpdateCameraSelection());
            auto* player = g.objects.get(g.client_shell.player);
            const auto position = player->state.m_Pos;
            const auto velocity = player->velocity;
            const auto reports = g.server_shell.Player(g.local_client)->movement_messages;
            g.server.trace_enabled = true;
            const auto calls = g.server.service_calls;
            for (unsigned i = 0; i < 100; ++i)
            {
                std::string error;
                CHECK(g.client_session.camera.select(g.client_session.collision, .2f, .4f, false, error));
                CHECK(g.client_shell.HudSnapshot().visible);
            }
            CHECK(g.server.service_calls == calls && same(position, player->state.m_Pos) &&
                  same(velocity, player->velocity));
            CHECK(g.server_shell.Player(g.local_client)->movement_messages == reports);
            detached.camera = g.client_session.camera;
            detached.collision = g.client_session.collision;
            detached.hud = g.client_session.hud;
            detached.world = g.client_session.world;
            detached.time = g.client_session.time;
            detached.movement = g.client_session.movement;
            CHECK(g.unload_world());
        }
        std::string error;
        CHECK(detached.camera.select(detached.collision, .2f, .4f, false, error) &&
              detached.camera.third_person_active);
        CHECK(hud_state(detached.hud.stats, true, false).visible);
        detached.camera.death.Begin(detached.camera.frame().time);
        auto frame = detached.camera.frame();
        ++frame.sequence;
        frame.time += .5f;
        CHECK(detached.camera.receive(std::move(frame), error));
        CHECK(detached.camera.select(detached.collision, 0, 0, false, error));
        CHECK(detached.camera.kind == ClientCameraKind::Death &&
              std::abs(detached.camera.death.fraction - .5f) < 1e-6f);
        return true;
    }
    bool rays(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(enter(g, root));
        for (unsigned i = 0; i < 4; ++i)
        {
            ObjectCreateStruct state;
            state.m_Pos = {float(int(i) * 70 - 120), 75, 150};
            state.m_Flags = i == 0 ? FLAG_RAYHIT : FLAG_SOLID;
            auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
            DVector dims{20, 75, 20};
            CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
        }
        for (const auto type : {"Water", "Container"})
        {
            g.world.models.push_back(test_brush::box(type, {-30, 0, -40}, {30, 130, -20}));
            Properties props{{"Name", std::string(type)}};
            if (std::string_view(type) == "Water")
                props["ShowSurface"] = false;
            auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
            CHECK(h);
            g.objects.get(h)->state.m_Flags |= FLAG_SOLID | FLAG_RAYHIT;
        }
        CHECK(g.local_presentation.publish(g.local_client, nullptr, nullptr));
        const auto owner = g.client_session.hud.owner();
        std::mt19937 random(71413);
        const auto point = [&]()
        { return DVector{float(int(random() % 401) - 200), float(random() % 200), float(int(random() % 401) - 200)}; };
        for (unsigned i = 0; i < 1000; ++i)
        {
            const auto from = point(), to = point();
            for (const auto mode : {SegmentObjects::SolidOnly, SegmentObjects::SolidOrRayHit, SegmentObjects::All})
                for (const auto filter : {SegmentFilter::None, SegmentFilter::IgnoreVolumes})
                {
                    const auto start = i % 2 ? SegmentStart::IncludeInterior : SegmentStart::SkipInterior;
                    SegmentHit server;
                    ClientSegmentHit client;
                    const auto a =
                        g.server.IntersectSegment(from, to, g.client_shell.player, server, false, mode, filter, start);
                    const auto b = g.client_session.collision.intersect(from, to, owner, client, mode,
                                                                        filter == SegmentFilter::IgnoreVolumes, start);
                    CHECK(a == b);
                    if (a == SegmentResult::Hit)
                    {
                        const auto* object = g.objects.get(server.object);
                        CHECK(client.object == (object ? object->id : ObjectId{}));
                        CHECK(client.fraction == server.fraction && same(client.point, server.point) &&
                              same(client.normal, server.normal));
                        CHECK(client.polygon == server.polygon && client.texture_flags == server.texture_flags);
                    }
                }
        }
        return true;
    }
    bool private_worlds(const std::filesystem::path& root)
    {
        for (const auto* world : {"01_SubwayA", "02_CityHub"})
        {
            GameSession g;
            CHECK(g.initialize());
            Config config;
            config.data_dir = root;
            std::string error;
            CHECK(g.resources.mount_game(config, error) && g.request_world(world, error) && g.enter_player(error));
            for (unsigned i = 0; i < 900; ++i)
            {
                g.client_shell.third_person = i >= 780;
                CHECK(g.step(1. / 60));
                const auto& camera = g.client_session.camera;
                const auto original = g.server.GetCameraView();
                const auto* target = g.objects.get(original.object);
                CHECK(camera.view.object == (target ? target->id : ObjectId{}));
                if (target)
                    CHECK(same(camera.view.position, original.position));
                const auto hud = g.client_shell.HudSnapshot();
                CHECK(hud.time == g.server.GetTime() && hud.stats == g.client_shell.stats);
                CHECK(hud.visible == !bool(original.object));
                CHECK(g.client_session.movement.prediction_failures == 0);
                CHECK(g.client_session.movement.predictions == g.client_session.movement.acknowledgements);
            }
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    const std::filesystem::path root = argv[2];
    return (mode == "boundary"   ? boundary(root)
            : mode == "lifetime" ? lifetime(root)
            : mode == "rays"     ? rays(root)
            : mode == "private"  ? private_worlds(root)
                                 : false)
               ? 0
               : 1;
}
