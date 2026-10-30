#include "client/network_world.h"
#include "net/transport.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
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
    bool impaired()
    {
        std::string error;
        Resources resources;
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        unsigned cases = 0;
        for (unsigned rtt : {0u, 50u, 150u})
            for (unsigned loss : {0u, 1u, 5u})
                for (unsigned jitter : {0u, 20u})
                {
                    NetworkWorldReceiver receiver;
                    WorldSnapshot initial;
                    initial.world = 1;
                    initial.sequence = 1;
                    ObjectState object;
                    object.id = {0, 1};
                    initial.objects = {object};
                    CHECK(receiver.begin(1) && receiver.receive(initial, geometry, models, sprites, error));
                    struct Delivery
                    {
                        double at;
                        std::vector<std::uint8_t> bytes;
                    };
                    std::vector<Delivery> queue;
                    std::mt19937 random(9 + rtt + loss + jitter);
                    for (unsigned tick = 1; tick <= 200; ++tick)
                    {
                        const auto lost = random() % 100 < loss;
                        if (lost && tick != 200)
                            continue; // Terminal refresh proves recovery.
                        WorldState state{1, 1, tick + 1, tick * .05, {motion_values(object)}};
                        state.objects[0].pose.position.x = float(tick);
                        std::vector<std::uint8_t> bytes;
                        CHECK(encode_world_state(state, bytes));
                        const auto variation = jitter ? int(random() % (2 * jitter + 1)) - int(jitter) : 0;
                        queue.push_back(
                            {state.time + std::max(0.0, (double(rtt) / 2 + variation) / 1000), std::move(bytes)});
                    }
                    std::stable_sort(queue.begin(), queue.end(),
                                     [](const auto& a, const auto& b) { return a.at < b.at; });
                    float last = 0;
                    for (const auto& delivery : queue)
                    {
                        WorldState state;
                        CHECK(decode_world_state(delivery.bytes, state));
                        CHECK(receiver.receive(state, error) != NetworkWorldReceiver::StateResult::Refused);
                        ClientWorld displayed;
                        CHECK(receiver.sample(std::max(0.0, delivery.at - double(rtt) / 2000 - .1), {}, displayed,
                                              error));
                        const auto x = displayed.get(ObjectId{0, 1})->pose.position.x;
                        CHECK(x >= last && x <= receiver.world().get(ObjectId{0, 1})->pose.position.x);
                        last = x;
                    }
                    CHECK(receiver.world().get(ObjectId{0, 1})->pose.position.x == 200);
                    ++cases;
                }
        std::cout << cases << " synthetic pose schedules: RTT 0/50/150 ms, loss 0/1/5%, jitter 0/20 ms\n";
        return true;
    }
    bool run()
    {
        std::string error_;
        WorldSnapshot initial;
        initial.world = 7;
        initial.sequence = 10;
        initial.time = 1;
        initial.player = {0, 1};
        initial.player_alive = true;
        for (unsigned i = 0; i < 25; ++i)
        {
            ObjectState object;
            object.id = {i, 1};
            initial.objects.push_back(object);
        }
        std::vector<std::vector<std::uint8_t>> packets;
        CHECK(encode_state_packets(initial, 10, packets) && packets.size() == 4);
        CHECK(packets[0].size() == 1132 && packets[3].size() == 664);
        for (const auto& packet : packets)
        {
            CHECK(packet.size() <= packet_limit(Channel::State));
            WorldState decoded;
            CHECK(decode_world_state(packet, decoded));
            std::vector<std::uint8_t> canonical;
            CHECK(encode_world_state(decoded, canonical) && canonical == packet);
            for (std::size_t i = 0; i < packet.size(); ++i)
            {
                decoded.sequence = 999;
                CHECK(!decode_world_state(std::span(packet).first(i), decoded) && decoded.sequence == 999);
            }
        }
        std::mt19937 random(9124);
        for (unsigned i = 0; i < 20000; ++i)
        {
            auto bytes = packets[random() % packets.size()];
            bytes[random() % bytes.size()] ^= std::uint8_t(1u << (random() % 8));
            WorldState decoded;
            if (decode_world_state(bytes, decoded))
            {
                std::vector<std::uint8_t> canonical;
                CHECK(encode_world_state(decoded, canonical) && canonical == bytes);
            }
        }
        Resources resources;
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        NetworkWorldReceiver receiver;
        CHECK(receiver.begin(7) && receiver.receive(initial, geometry, models, sprites, error_));
        using Status = NetworkWorldReceiver::StateResult;
        WorldState state{7, 10, 11, 1.05, {motion_values(initial.objects[0]), motion_values(initial.objects[1])}};
        state.objects[0].pose.position.x = state.objects[1].pose.position.x = 10;
        state.objects[1].pose.rotation = {{0, 0, 0}, -1}; // Same orientation, opposite quaternion sign.
        CHECK(receiver.receive(state, error_) == Status::Applied);
        ClientWorld displayed;
        CHECK(receiver.sample(1.025, {0, 1}, displayed, error_));
        CHECK(std::abs(displayed.get(ObjectId{1, 1})->pose.position.x - 5) < 1e-4);
        CHECK(displayed.get(ObjectId{0, 1})->pose.position.x == 10);
        CHECK(std::abs(displayed.get(ObjectId{1, 1})->pose.rotation.m_Spin) == 1);
        CHECK(receiver.sample(0, {0, 1}, displayed, error_) && displayed.get(ObjectId{1, 1})->pose.position.x == 0);
        CHECK(receiver.sample(9, {0, 1}, displayed, error_) && displayed.get(ObjectId{1, 1})->pose.position.x == 10);
        CHECK(receiver.receive(state, error_) == Status::Ignored);
        // The missing sequence needs no recovery; packets are absolute.
        auto dropped = state;
        dropped.sequence = 12;
        dropped.time = 1.1;
        state.sequence = 13;
        state.time = 1.15;
        state.objects[1].pose.position.x = 30;
        CHECK(receiver.receive(state, error_) == Status::Applied);
        CHECK(receiver.receive(dropped, error_) == Status::Ignored);
        CHECK(receiver.sample(1.1, {0, 1}, displayed, error_) &&
              std::abs(displayed.get(ObjectId{1, 1})->pose.position.x - 20) < 1e-4);
        // Independent datagrams of one publication can arrive in either order.
        auto first = state, second = state;
        first.sequence = second.sequence = 14;
        first.time = second.time = 1.2;
        first.objects.resize(1);
        first.objects[0].pose.position.x = 40;
        second.objects.erase(second.objects.begin());
        second.objects[0].pose.position.x = 40;
        CHECK(receiver.receive(second, error_) == Status::Applied &&
              receiver.receive(first, error_) == Status::Applied);
        auto future = second;
        future.membership = 16;
        future.sequence = 17;
        future.time = 1.35;
        future.objects[0].pose_revision = 1;
        future.objects[0].pose.position.x = 110;
        CHECK(receiver.receive(future, error_) == Status::Ignored);
        auto before = snapshot_values(receiver.world().frame());
        before.sequence = 10;
        auto after = before;
        after.sequence = 16;
        after.time = 1.3;
        after.objects[1].pose_revision = 1;
        after.objects[1].pose.position.x = 100;
        WorldPatch patch;
        bool changed = false;
        CHECK(make_world_patch(before, after, patch, changed) && changed && patch.values.objects.size() == 1);
        CHECK(receiver.receive(patch, geometry, models, sprites, error_));
        CHECK(receiver.sample(1.1, {0, 1}, displayed, error_) && displayed.get(ObjectId{1, 1})->pose.position.x == 100);
        CHECK(receiver.receive(state, error_) == Status::Ignored);
        CHECK(receiver.receive(future, error_) == Status::Applied);
        CHECK(receiver.sample(1.325, {0, 1}, displayed, error_) &&
              std::abs(displayed.get(ObjectId{1, 1})->pose.position.x - 105) < 1e-4);
        auto bad = future;
        ++bad.sequence;
        bad.objects[0].id.generation = 2;
        CHECK(receiver.receive(bad, error_) == Status::Ignored && !receiver.world().get(ObjectId{1, 2}));
        bad = future;
        ++bad.sequence;
        bad.objects[0].pose_revision = 0;
        CHECK(receiver.receive(bad, error_) == Status::Ignored);
        bad = future;
        ++bad.sequence;
        bad.objects[0].alpha = std::numeric_limits<float>::infinity();
        const auto sequence = receiver.world().frame().sequence;
        CHECK(receiver.receive(bad, error_) == Status::Refused && receiver.world().frame().sequence == sequence);
        CHECK(!receiver.sample(std::nan(""), {}, displayed, error_));
        for (unsigned i = 0; i < 20; ++i)
        {
            ++future.sequence;
            future.time += .05;
            future.objects[0].pose.position.x += 1;
            CHECK(receiver.receive(future, error_) == Status::Applied);
        }
        CHECK(receiver.sample(0, {}, displayed, error_) && displayed.get(ObjectId{1, 1})->pose.position.x == 123);
        CHECK(receiver.begin(8));
        CHECK(receiver.receive(future, error_) == Status::Ignored && !receiver.world().get(ObjectId{1, 1}));
        std::cout << "Bounded absolute poses, 20000 codec mutations, loss/reorder/duplicates, interpolation and "
                     "reliable snap\n";
        return true;
    }
} // namespace
int main()
{
    return run() && impaired() ? 0 : 1;
}
