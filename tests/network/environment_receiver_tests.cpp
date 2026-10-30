#include "../brush_fixture.h"
#include "client/collision.h"
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
    bool run()
    {
        // No game library, object registry, or original callbacks are linked here.
        ClientCollisionWorld client;
        CHECK(client.begin(7));
        ClientCollisionFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        frame.primary = std::make_shared<CollisionWorld>();
        ClientCollider player;
        player.id = {0, 1};
        frame.objects.push_back(player);
        auto shape = std::make_shared<ContainerShape>();
        std::string error;
        auto mesh = test_brush::box("volume", {-10, -10, -10}, {10, 10, 10});
        mesh.translation = {100, 200, 300};
        CHECK(shape->build(mesh, {}, error));
        for (unsigned i = 1; i <= 6; ++i)
        {
            ClientCollider volume;
            volume.id = {i, 1};
            volume.container = true;
            volume.position = mesh.translation;
            volume.shape = shape;
            volume.environment = {i == 1 ? CC_WATER : CC_LADDER, {float(i), 0, -float(i)}, -200, .25f, i == 1};
            frame.objects.push_back(volume);
        }
        CHECK(client.apply(std::move(frame), error));
        ClientMotionState body;
        body.dimensions = {1, 1, 1};
        const ClientObjectId owner{7, player.id};
        ClientContainerState state;
        CHECK(client.environment(owner, body, {}, state));
        CHECK(state.count == 4 && state.total == 6 && state.head.code == CC_NOTHING);
        CHECK(state.head.hidden && state.head.object == (ObjectId{1, 1}));
        CHECK(state.body[0].code == CC_WATER && state.body[3].object == (ObjectId{4, 1}));
        CHECK(state.body[3].current.x == 4 && state.body[0].viscosity == .25f);
        CHECK(client.environment(owner, body, {0, 12, 0}, state));
        CHECK(state.count == 4 && !state.head.object); // Body and head are independent.
        body.position = {100, 200, 300};
        CHECK(client.environment(owner, body, body.position, state));
        CHECK(state.total == 0); // The DAT translation must not be applied twice.
        body.position = {};
        frame = client.frame();
        ++frame.sequence;
        frame.objects[1].environment = {CC_NOTHING, {}, 0, 0, false};
        CHECK(client.apply(std::move(frame), error));
        CHECK(client.environment(owner, body, {}, state));
        CHECK(state.head.object == (ObjectId{1, 1}) && state.head.code == CC_NOTHING && !state.head.hidden);
        CHECK(state.head.gravity == 0); // Built-in code zero is still a recognized volume.
        frame = client.frame();
        ++frame.sequence;
        frame.objects[1].environment->code = static_cast<ContainerCode>(65535);
        CHECK(client.apply(std::move(frame), error));
        CHECK(client.environment(owner, body, {}, state) && unsigned(state.head.code) == 65535);
        CHECK(!IsLiquid(state.head.code) && !IsFreeMovement(state.head.code));
        auto reject = [&](ClientCollisionFrame next)
        {
            const auto sequence = client.frame().sequence;
            return !client.apply(std::move(next), error) && client.frame().sequence == sequence;
        };
        for (int field = 0; field < 5; ++field)
        {
            frame = client.frame();
            ++frame.sequence;
            auto& e = *frame.objects[1].environment;
            if (field == 0)
                e.current.x = std::numeric_limits<float>::quiet_NaN();
            if (field == 1)
                e.gravity = std::numeric_limits<float>::infinity();
            if (field == 2)
                e.viscosity = -1;
            if (field == 3)
                e.viscosity = 2;
            if (field == 4)
                frame.objects[1].container = false;
            CHECK(reject(std::move(frame)));
        }
        CHECK(!client.environment({8, player.id}, body, {}, state) && state.total == 0);
        CHECK(!client.environment({7, {0, 2}}, body, {}, state));
        CHECK(!client.environment(owner, body, {0, std::numeric_limits<float>::infinity(), 0}, state));
        body.dimensions.x = -1;
        CHECK(!client.environment(owner, body, {}, state));
        body.dimensions = {.1f, .1f, .1f};
        auto concave = std::make_shared<ContainerShape>();
        auto concave_mesh =
            test_brush::prism("L", {{0, 0, 0}, {4, 0, 0}, {4, 0, 1}, {1, 0, 1}, {1, 0, 4}, {0, 0, 4}}, 0, 4);
        // The importer accepts convex polygon faces; split the two concave caps.
        concave_mesh.polygons.resize(6);
        concave_mesh.surfaces.resize(6);
        concave_mesh.planes.resize(6);
        for (float y : {0.f, 4.f})
        {
            DVector normal{0, y == 0 ? -1.f : 1.f, 0};
            test_brush::face(concave_mesh, {{0, y, 0}, {4, y, 0}, {4, y, 1}, {0, y, 1}}, normal);
            test_brush::face(concave_mesh, {{0, y, 1}, {1, y, 1}, {1, y, 4}, {0, y, 4}}, normal);
        }
        CHECK(concave->build(concave_mesh, {}, error));
        frame = client.frame();
        ++frame.sequence;
        frame.objects.resize(2);
        frame.objects[1].shape = concave;
        frame.objects[1].environment = {CC_CONVEYOR, {200, 0, 0}, 0, 0, false};
        CHECK(client.apply(std::move(frame), error));
        body.position = {3, 2, 3};
        CHECK(client.environment(owner, body, body.position, state) && state.total == 0);
        body.position = {.5f, 2, 3};
        CHECK(client.environment(owner, body, body.position, state) && state.total == 1);
        CHECK(state.head.code == CC_CONVEYOR);
        frame = client.frame();
        ++frame.sequence;
        frame.objects[1].environment.reset();
        CHECK(client.apply(std::move(frame), error));
        CHECK(client.environment(owner, body, body.position, state) && state.total == 0);
        CHECK(client.begin(8));
        CHECK(!client.environment(owner, body, body.position, state));
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
