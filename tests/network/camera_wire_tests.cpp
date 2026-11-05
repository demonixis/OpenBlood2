#include "client/network_camera.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
using Reception = NetworkCamera::Reception;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        std::string error;
        ClientWorld world;
        ClientCollisionWorld collision;
        CHECK(world.begin(7) && collision.begin(7));
        ClientWorldFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        frame.player = {0, 1};
        frame.player_alive = true;
        ClientCollisionFrame geometry;
        geometry.world = 7;
        geometry.sequence = 1;
        geometry.primary = std::make_shared<CollisionWorld>();
        for (unsigned i = 0; i < 5; ++i)
        {
            ClientObject object;
            object.id = {i, 1};
            object.pose.position = {float(i * 100), 41, 0};
            frame.objects.push_back(object);
            ClientCollider box;
            box.id = object.id;
            box.position = object.pose.position;
            box.dimensions = {16, 41, 16};
            geometry.objects.push_back(box);
        }
        CHECK(world.apply(std::move(frame), error) && collision.apply(std::move(geometry), error));
        const Joined owner{7, 1, {0, 1}};
        ClientMovement predicted;
        predicted.owner = {7, {0, 1}};
        predicted.body.position = {10, 41, 20};
        predicted.body.dimensions = {16, 41, 16};
        NetworkCamera receiver;
        CHECK(receiver.begin(owner));
        CameraState state{300, {2, 1}, {}, {}, {1, false, true, false}, 34, true};
        std::uint64_t sequence = 0, last_tick = 0;
        const auto send = [&](const PrivateValue& value, std::uint64_t tick = 150)
        {
            last_tick = std::max(last_tick, tick);
            PrivateEvent event{owner, ++sequence, last_tick, value};
            std::vector<std::uint8_t> bytes;
            PrivateEvent decoded;
            return encode_private(event, bytes) && decode_private(bytes, decoded) == DecodePrivate::Ok &&
                   receiver.receive(decoded, error) == Reception::Applied;
        };
        auto update = [&](double time = 3, std::uint64_t force = 300, bool zoom = false)
        { return receiver.update(world, collision, predicted, force, .2f, 1, zoom, time, error); };
        CHECK(update() == Reception::Deferred && !receiver.ready());
        CHECK(send(state) && update() == Reception::Applied);
        CHECK(receiver.camera().kind == ClientCameraKind::Scripted && receiver.camera().view.object == state.scripted);
        CHECK(receiver.camera().view.position.x == 200 && receiver.camera().listener_position.x == 10);
        CHECK(!receiver.camera().player_visible && !receiver.camera().weapons_visible &&
              !receiver.camera().look_allowed());
        state.eye = {3, 1};
        state.orb = {4, 1};
        CHECK(send(state) && update() == Reception::Applied);
        CHECK(receiver.camera().kind == ClientCameraKind::Eye && receiver.camera().view.position.x == 300);
        CHECK(receiver.camera().listener_position.x == 300 && receiver.camera().look_allowed());
        state.eye = {};
        CHECK(send(state) && update() == Reception::Applied && receiver.camera().kind == ClientCameraKind::Orb);
        state.alive = false;
        CHECK(send(state) && update() == Reception::Applied && receiver.camera().kind == ClientCameraKind::Scripted);
        CHECK(!receiver.camera().death.active); // Slow death has not sent PlayerDead yet.
        CHECK(send(PlayerDead{}, 162) && update() == Reception::Applied);
        CHECK(receiver.camera().kind == ClientCameraKind::Scripted && !receiver.camera().player_visible);
        state.scripted = {};
        CHECK(send(state) && update() == Reception::Applied && receiver.camera().kind == ClientCameraKind::Death);
        CHECK(std::abs(receiver.camera().death.fraction - .3f) < .00001f);
        CHECK(send(PlayerDead{}, 240) && update(3.9) == Reception::Applied);
        CHECK(std::abs(receiver.camera().death.start - 2.7f) < .00001f); // Repeated death does not restart the orbit.
        NetworkCamera humiliated;
        CHECK(humiliated.begin(owner));
        CHECK(humiliated.receive({owner, 1, 150, state}, error) == Reception::Applied);
        CHECK(humiliated.receive({owner, 2, 150, WonkyVision{15, true}}, error) == Reception::Applied);
        CHECK(humiliated.receive({owner, 3, 180, PlayerDead{}}, error) == Reception::Applied);
        CHECK(humiliated.update(world, collision, predicted, 300, 0, 0, false, 3.5, error) == Reception::Applied);
        CHECK(humiliated.camera().kind == ClientCameraKind::Death && humiliated.camera().player_visible &&
              !humiliated.camera().weapons_visible);
        // A duplicate notification neither restarts the orbit nor loses no-hide.
        CHECK(humiliated.receive({owner, 4, 210, PlayerDead{}}, error) == Reception::Applied);
        CHECK(humiliated.update(world, collision, predicted, 300, 0, 0, false, 4, error) == Reception::Applied &&
              humiliated.camera().player_visible && humiliated.camera().death.start == 3);
        auto revived = state;
        revived.force_revision = 301;
        revived.alive = true;
        revived.orb = {};
        CHECK(humiliated.receive({owner, 5, 240, revived}, error) == Reception::Applied);
        CHECK(humiliated.update(world, collision, predicted, 301, 0, 0, false, 4, error) == Reception::Applied &&
              !humiliated.camera().death.active && !humiliated.camera().player_visible);
        NetworkCamera future;
        CHECK(future.begin(owner));
        CHECK(future.receive({owner, 1, 150, state}, error) == Reception::Applied);
        CHECK(future.update(world, collision, predicted, 300, 0, 0, false, 3, error) == Reception::Applied);
        const auto view = future.camera().view.position;
        CHECK(future.receive({owner, 2, 240, PlayerDead{}}, error) == Reception::Applied);
        CHECK(future.update(world, collision, predicted, 300, 0, 0, false, 3.9, error) == Reception::Deferred);
        CHECK(future.camera().view.position.x == view.x && future.camera().frame().time == 3);
        CHECK(future.update(world, collision, predicted, 300, 0, 0, false, 4, error) == Reception::Applied);
        state.alive = true;
        state.force_revision = 301;
        state.orb = {};
        CHECK(send(state, 240) && update(4) == Reception::Deferred);
        predicted.body.position = {100, 200, 300};
        CHECK(update(4, 301) == Reception::Applied && receiver.camera().kind == ClientCameraKind::FirstPerson);
        CHECK(!receiver.camera().death.active && receiver.camera().view.position.x == 100 &&
              receiver.camera().view.position.y == 234);
        CHECK(world.get(owner.object)->pose.position.x == 0); // Predicted eye never edits the replica.
        receiver.camera().third_person = true;
        CHECK(update(4, 301) == Reception::Applied && receiver.camera().kind == ClientCameraKind::Chase);
        CHECK(update(4, 301, true) == Reception::Applied && receiver.camera().kind == ClientCameraKind::FirstPerson);
        receiver.camera().xr_view = true;
        CHECK(update(4, 301) == Reception::Applied && !receiver.camera().third_person &&
              !receiver.camera().can_toggle(true));
        const auto last = receiver.camera().frame().sequence;
        CHECK(update(3, 301) == Reception::Refused && receiver.camera().frame().sequence == last);
        CHECK(update(std::numeric_limits<double>::quiet_NaN(), 301) == Reception::Refused);
        state.orb = {4, 2};
        CHECK(send(state, 240) && update(4, 301) == Reception::Deferred);
        CHECK(receiver.camera().frame().sequence == last); // A reused slot cannot satisfy the previous generation.
        state.orb = {4, 1};
        CHECK(send(state, 240) && update(4, 301) == Reception::Applied);
        PrivateEvent duplicate{owner, sequence, 240, state};
        CHECK(receiver.receive(duplicate, error) == Reception::Ignored);
        ++duplicate.sequence;
        duplicate.owner.world = 6;
        CHECK(receiver.receive(duplicate, error) == Reception::Ignored);
        NetworkCamera precision;
        CHECK(precision.begin(owner));
        CHECK(precision.receive({owner, 1, 240, state}, error) == Reception::Applied);
        const double fraction = 1. / 60;
        CHECK(double(float(fraction)) > fraction);
        CHECK(precision.update(world, collision, predicted, 301, 0, 0, false, fraction, error) == Reception::Applied);
        CHECK(precision.update(world, collision, predicted, 301, 0, 0, false, fraction, error) == Reception::Applied);
        CHECK(precision.update(world, collision, predicted, 301, 0, 0, false, std::nextafter(fraction, 0.), error) ==
              Reception::Refused);
        CHECK(precision.update(world, collision, predicted, 301, 0, 0, false, std::nextafter(fraction, 1.), error) ==
              Reception::Applied);
        CHECK(receiver.begin({8, 2, {0, 2}}) && !receiver.ready());
        CHECK(update(4, 301) == Reception::Deferred);
        std::cout << "Wire camera: replica poses/predicted eye, source priority/listener/death/chase/XR, force and "
                     "ordering passed\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
