#include "../brush_fixture.h"
#include "../player_fixture.h"
#include <cmath>
#include <limits>
#include <type_traits>

using namespace OpenBlood2;
static_assert(std::is_trivially_copyable_v<ClientMotionState>);
static_assert(std::is_trivially_copyable_v<LocalMoveReport>);
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
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool enter(GameSession& g, const std::filesystem::path& root)
    {
        std::string error;
        CHECK(test_player::setup(g, root));
        CHECK(g.enter_player(error));
        CHECK(g.step(1. / 60));
        return true;
    }
    PlayerMove forward(GameSession& g, float distance = 8)
    {
        auto destination = g.client_session.movement.body.position;
        destination.z += distance;
        return g.client_shell.move_mgr.WritePositionInfo(destination, {0, 0, 120});
    }
    bool separation(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        auto& local = g.local_movement;
        auto& state = g.client_session.movement;
        auto* authoritative = g.objects.get(g.client_shell.player);
        auto* player = g.server_shell.Player(g.local_client);
        const auto position = authoritative->state.m_Pos;
        const auto velocity = authoritative->velocity;
        const auto messages = player->movement_messages;
        const auto predictions = state.predictions, acknowledgements = state.acknowledgements;
        const auto request = forward(g);
        LocalMoveReport report;
        CHECK(local.predict(request, report));
        CHECK(state.predictions == predictions + 1 && state.acknowledgements == acknowledgements);
        CHECK(state.owner == g.client_session.world.reference(authoritative->id));
        CHECK(same(authoritative->state.m_Pos, position) && same(authoritative->velocity, velocity));
        CHECK(state.body.position.z > position.z + 7.9f && player->movement_messages == messages);
        CHECK(local.submit(g.server_shell, report));
        CHECK(same(state.body.position, authoritative->state.m_Pos));
        CHECK(same(authoritative->state.m_Pos, request.position)); // No extra velocity*dt on the server.
        CHECK(player->movement_messages == messages + 1 && state.acknowledgements == acknowledgements + 1);
        CHECK(!local.submit(g.server_shell, report));
        CHECK(player->movement_messages == messages + 1);
        // Generic physics must still skip the associated server player.
        const auto committed = authoritative->state.m_Pos;
        CHECK(g.server.advance_physics(.01));
        CHECK(same(authoritative->state.m_Pos, committed));
        const auto acknowledged = state.acknowledgements;
        const auto predicted = state.predictions;
        const auto substeps = g.client_shell.move_mgr.substeps;
        g.client_shell.movement_input.forward = true;
        CHECK(g.step(1. / 30));
        CHECK(g.client_shell.move_mgr.substeps == substeps + 4);
        CHECK(state.predictions == predicted + 4 && state.acknowledgements == acknowledged + 4);
        // A zero-time update refreshes corrections without generating a displacement/report.
        CHECK(g.client_shell.move_mgr.Update(false, {}, 0, 0, true));
        CHECK(state.predictions == predicted + 4);
        return true;
    }
    struct Sensor : BaseClass
    {
        unsigned touches = 0;
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override
        {
            if (message == MID_TOUCHNOTIFY)
            {
                ++touches;
                DVector impulse{17, 23, 29};
                GetServerDE()->SetVelocity(static_cast<HOBJECT>(data), &impulse);
            }
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    std::unique_ptr<BaseClass> make_sensor()
    {
        return std::make_unique<Sensor>();
    }
    ClassInfo sensor_class{"MovementSensor", nullptr, make_sensor, {}};
    bool contacts(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        CHECK(g.objects.register_class(sensor_class));
        auto* authoritative = g.objects.get(g.client_shell.player);
        ObjectCreateStruct create;
        create.m_Pos = authoritative->state.m_Pos;
        create.m_Pos.z += 4;
        create.m_Flags = FLAG_TOUCH_NOTIFY;
        auto h = g.objects.create(&sensor_class, create);
        DVector dims{1, 1, 1};
        CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
        auto* sensor = dynamic_cast<Sensor*>(g.server.HandleToObject(h));
        LocalMoveReport report;
        CHECK(g.local_movement.predict(forward(g), report));
        CHECK(sensor->touches == 0);
        CHECK(g.local_movement.submit(g.server_shell, report));
        CHECK(sensor->touches == 1 && same(authoritative->velocity, {17, 23, 29}));
        CHECK(same(g.client_session.movement.body.velocity, authoritative->velocity));
        CHECK(!g.local_movement.submit(g.server_shell, report) && sensor->touches == 1);
        return true;
    }
    bool pushing(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        const auto start = g.client_session.movement.body.position;
        const auto dims = g.client_session.movement.body.dimensions;
        const DVector center{start.x, 10.001f, start.z + dims.z + 12};
        auto model = test_brush::box("push", vector_subtract(center, {10, 10, 10}), vector_add(center, {10, 10, 10}));
        model.translation = center;
        g.world.models.push_back(std::move(model));
        auto h = g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World,
                                  {{"Name", std::string("push")}, {"Pushable", true}, {"HitPoints", 1000.f}});
        CHECK(h);
        auto* box = g.objects.get(h);
        const auto before = box->state.m_Pos;
        LocalMoveReport report;
        CHECK(g.local_movement.predict(forward(g, 10), report));
        CHECK(same(box->state.m_Pos, before) && box->body_pushes == 0);
        CHECK(g.local_movement.submit(g.server_shell, report));
        CHECK(box->state.m_Pos.z > before.z + 7 && box->body_pushes == 1);
        CHECK(same(g.client_session.movement.body.position, g.objects.get(g.client_shell.player)->state.m_Pos));
        return true;
    }
    bool stale(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        auto* player = g.server_shell.Player(g.local_client);
        auto* authoritative = g.objects.get(g.client_shell.player);
        const auto before = authoritative->state.m_Pos;
        const auto messages = player->movement_messages;
        LocalMoveReport report;
        CHECK(g.local_movement.predict(forward(g), report));
        // Byte code wraps to the same value, but the force revision must not.
        for (unsigned i = 0; i < 256; ++i)
            CHECK(player->TeleportClientToServerPos());
        CHECK(report.move.move_code == player->m_byClientMoveCode);
        CHECK(!g.local_movement.submit(g.server_shell, report));
        CHECK(same(before, authoritative->state.m_Pos) && messages == player->movement_messages);
        CHECK(g.pump_player_messages(player->entry_error));
        CHECK(g.local_movement.predict(forward(g), report));
        auto bad = report;
        ++bad.owner.world;
        CHECK(!g.local_movement.submit(g.server_shell, bad));
        CHECK(!g.local_movement.submit(g.server_shell, report));
        CHECK(g.local_movement.synchronize());
        CHECK(g.local_movement.predict(forward(g), report));
        bad = report;
        ++bad.owner.object.generation;
        CHECK(!g.local_movement.submit(g.server_shell, bad));
        CHECK(g.local_movement.synchronize());
        CHECK(g.local_movement.predict(forward(g), report));
        bad = report;
        bad.move.position.z += 100;
        CHECK(!g.local_movement.submit(g.server_shell, bad));
        CHECK(g.local_movement.synchronize());
        auto invalid = forward(g);
        invalid.position.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!g.local_movement.predict(invalid, report));
        CHECK(same(before, authoritative->state.m_Pos) && messages == player->movement_messages);
        CHECK(g.local_movement.predict(forward(g), report));
        const auto prior_binding = report;
        CHECK(g.local_movement.bind(g.local_client));
        CHECK(g.local_movement.force_position({before, player->m_byClientMoveCode}));
        CHECK(g.local_movement.predict(forward(g), report));
        CHECK(report.sequence > prior_binding.sequence && report.owner == prior_binding.owner);
        CHECK(!g.local_movement.submit(g.server_shell, prior_binding));
        CHECK(g.local_movement.synchronize());
        CHECK(g.local_movement.predict(forward(g), report));
        g.client_session.reset_local_world();
        CHECK(!g.local_movement.valid() && !g.local_movement.submit(g.server_shell, report));
        CHECK(same(before, authoritative->state.m_Pos));
        return true;
    }
    bool collision(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        auto* player = g.objects.get(g.client_shell.player);
        const auto before = player->state.m_Pos;
        ObjectCreateStruct wall;
        wall.m_Pos = before;
        wall.m_Pos.z += player->dimensions.z + 10;
        wall.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(g.objects.find_class("BaseClass"), wall);
        DVector dims{100, 100, 1};
        CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
        LocalMoveReport report;
        CHECK(g.local_movement.predict(forward(g, 100), report));
        CHECK(g.client_session.movement.body.position.z < before.z + 10);
        CHECK(same(player->state.m_Pos, before));
        CHECK(g.local_movement.submit(g.server_shell, report));
        CHECK(player->state.m_Pos.z < before.z + 10);
        CHECK(same(g.client_session.movement.body.position, player->state.m_Pos));
        // A refused starting overlap still reaches the authoritative diagnostics.
        player->state.m_Pos = wall.m_Pos;
        CHECK(g.local_movement.synchronize());
        const auto failed = g.client_session.movement.prediction_failures;
        CHECK(g.local_movement.predict(forward(g), report));
        CHECK(g.client_session.movement.prediction_failures == failed + 1);
        CHECK(!g.local_movement.submit(g.server_shell, report));
        CHECK(g.objects.error() == ObjectError::Unsupported);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        g.local_movement.verify_prediction = true;
        CHECK(enter(g, root));
        LocalMoveReport report;
        CHECK(g.local_movement.predict(forward(g), report));
        const auto id = g.objects.get(g.client_shell.player)->id;
        CHECK(g.objects.remove(g.client_shell.player));
        CHECK(g.objects.finish_frame());
        CHECK(!g.local_movement.valid());
        CHECK(!g.local_movement.submit(g.server_shell, report));
        CHECK(g.unload_world());
        CHECK(!g.client_session.movement.owner.object && g.client_session.movement.predictions == 0);
        CHECK(!g.objects.get(id));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    const auto root = std::filesystem::path(argv[2]);
    const bool ok = name == "separation"  ? separation(root)
                    : name == "contacts"  ? contacts(root)
                    : name == "pushing"   ? pushing(root)
                    : name == "stale"     ? stale(root)
                    : name == "collision" ? collision(root)
                                          : name == "lifetime" && lifetime(root);
    return ok ? 0 : 1;
}
