#include "net/admission.h"
#include "net/movement.h"
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
    bool run()
    {
        const MovementIdentity owner{3, 17, {4, 9}, 300};
        MovementSetup setup;
        setup.owner = owner;
        setup.sequence = 2;
        setup.code = 44;
        setup.character = 3;
        setup.dimensions = {16, 41, 16};
        setup.position = {1, 41, 2};
        setup.gravity = {0, -1000, 0};
        setup.move_speed = 280;
        setup.eye_level = 34;
        MovementReport report;
        report.owner = owner;
        report.sequence = 42;
        report.move = {44, {1, 41, 2}, {0, 0, 280}, {0, 0, 400}, true, 255};
        report.controls = {{true, true, true, true, true, true, true, true, .5f, -.25f}, true, .75f};
        report.pitch = -.5f;
        report.origin = {1, 41, 1};
        report.steps = {{{0, 0, .5f}, {0, 0, 280}, {0, 0, 400}, 1.0f / 120, true, 3},
                        {{0, -.1f, .5f}, {0, -10, 280}, {0, 0, 400}, 1.0f / 240, false, 0}};
        report.impulse_sequence = 117;
        MovementAck ack{owner, 42, 2, report.move, {16, 41, 16}, 118};
        MovementImpulse impulse{owner, 118, {12, 400, -10}};
        std::mt19937 random(0x4d39504d);
        for (const MovementMessage& message :
             {MovementMessage(setup), MovementMessage(report), MovementMessage(ack), MovementMessage(impulse)})
        {
            std::vector<std::uint8_t> bytes, roundtrip;
            CHECK(encode_movement(message, bytes) && bytes.size() <= movement_packet_limit &&
                  is_movement_packet(bytes));
            const std::array<std::size_t, 4> sizes{98, 209, 114, 59};
            CHECK(bytes.size() == sizes[message.index()]);
            const std::array<std::uint8_t, 7> prefix{0x42,
                                                     0x32,
                                                     0x4d,
                                                     0x50,
                                                     std::uint8_t(protocol_version),
                                                     std::uint8_t(protocol_version >> 8),
                                                     std::uint8_t(message.index() + 1)};
            CHECK(std::equal(prefix.begin(), prefix.end(), bytes.begin()));
            MovementMessage decoded;
            CHECK(decode_movement(bytes, decoded) == DecodeMovement::Ok);
            CHECK(encode_movement(decoded, roundtrip) && roundtrip == bytes);
            CHECK(std::visit([&](const auto& v) { return v.owner == owner; }, decoded));
            const auto saved = bytes;
            auto refuse = [&](const std::vector<std::uint8_t>& invalid)
            {
                if (decode_movement(invalid, decoded) == DecodeMovement::Ok)
                    return false;
                std::vector<std::uint8_t> after;
                return encode_movement(decoded, after) && after == saved;
            };
            for (std::size_t n = 0; n < bytes.size(); ++n)
                CHECK(refuse(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + n)));
            bytes.push_back(0);
            CHECK(refuse(bytes));
            bytes = saved;
            bytes[6] = 99;
            CHECK(refuse(bytes));
            bytes = saved;
            bytes[4] ^= 1;
            CHECK(decode_movement(bytes, decoded) == DecodeMovement::Protocol);
            bytes = saved;
            for (unsigned i = 0; i < 10000; ++i)
            {
                auto mutated = bytes;
                mutated[random() % mutated.size()] ^= std::uint8_t(1u << (random() % 8));
                MovementMessage candidate;
                if (decode_movement(mutated, candidate) == DecodeMovement::Ok)
                    CHECK(valid_movement(candidate) && encode_movement(candidate, roundtrip) && roundtrip == mutated);
            }
        }
        std::vector<std::uint8_t> bytes{1, 2, 3};
        auto unchanged = bytes;
        setup.owner.force_revision = 0;
        CHECK(!encode_movement(setup, bytes) && bytes == unchanged);
        setup.owner = owner;
        setup.dimensions.y = 0;
        CHECK(!encode_movement(setup, bytes) && bytes == unchanged);
        // The largest report (one 0.1 s client frame plus an unsent remainder)
        // fits the unreliable State channel; more steps or a longer substep do not.
        report.steps.resize(movement_step_limit, report.steps[0]);
        std::vector<std::uint8_t> largest;
        CHECK(encode_movement(report, largest) && largest.size() <= movement_packet_limit);
        report.steps.push_back(report.steps[0]);
        CHECK(!encode_movement(report, bytes) && bytes == unchanged);
        report.steps.resize(2);
        report.steps[1].seconds = 1.0f / 60;
        CHECK(!encode_movement(report, bytes) && bytes == unchanged);
        report.steps[1].seconds = 0;
        CHECK(!encode_movement(report, bytes) && bytes == unchanged);
        report.steps[1].seconds = 1.0f / 120;
        report.steps[1].offset.x = std::numeric_limits<float>::infinity();
        CHECK(!encode_movement(report, bytes) && bytes == unchanged);
        report.steps[1].offset.x = 0;
        report.controls.yaw = std::numeric_limits<float>::quiet_NaN();
        CHECK(!encode_movement(report, bytes) && bytes == unchanged);
        ack.owner.object.generation = 0;
        CHECK(!encode_movement(ack, bytes) && bytes == unchanged);
        std::cout << "Four movement messages; all truncations and 40000 seeded mutations passed\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
