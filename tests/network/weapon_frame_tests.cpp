#include "net/weapon_frames.h"
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
        const MovementIdentity owner{7, 9, {4, 3}, 300};
        const WeaponFrame frame{owner, 11, 55, {.25f, 4.712389f}, true, true};
        const WeaponFrameAck ack{owner, 11, {3, 2}, 1};
        std::minstd_rand random(59);
        for (const WeaponFrameMessage& message : {WeaponFrameMessage(frame), WeaponFrameMessage(ack)})
        {
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_weapon_frame(message, bytes) && is_weapon_frame_packet(bytes));
            CHECK(bytes.size() == (std::holds_alternative<WeaponFrame>(message) ? 64 : 71));
            CHECK(bytes[0] == 'B' && bytes[1] == '2' && bytes[2] == 'W' && bytes[3] == 'F' && bytes[4] == 17);
            CHECK(bytes[31] == 44 && bytes[32] == 1); // LE force revision 300.
            WeaponFrameMessage decoded = WeaponFrameAck{owner, 999};
            for (std::size_t size = 0; size < bytes.size(); ++size)
                CHECK(decode_weapon_frame(std::span(bytes).first(size), decoded) == DecodeWeaponFrame::Malformed &&
                      std::get<WeaponFrameAck>(decoded).sequence == 999);
            CHECK(decode_weapon_frame(bytes, decoded) == DecodeWeaponFrame::Ok && encode_weapon_frame(decoded, again) &&
                  again == bytes);
            auto bad = bytes;
            bad.push_back(0);
            CHECK(decode_weapon_frame(bad, decoded) == DecodeWeaponFrame::Malformed);
            bad = bytes;
            bad[4] = 10;
            CHECK(decode_weapon_frame(bad, decoded) == DecodeWeaponFrame::Protocol);
            bad = bytes;
            bad[6] = 3;
            CHECK(decode_weapon_frame(bad, decoded) == DecodeWeaponFrame::Malformed);
            if (std::holds_alternative<WeaponFrame>(message))
            {
                bad = bytes;
                bad.back() = 4;
                CHECK(decode_weapon_frame(bad, decoded) == DecodeWeaponFrame::Malformed);
            }
            for (unsigned i = 0; i < 10000; ++i)
            {
                bad = bytes;
                bad[random() % bad.size()] ^= std::uint8_t(1u << (random() % 8));
                if (decode_weapon_frame(bad, decoded) == DecodeWeaponFrame::Ok)
                    CHECK(encode_weapon_frame(decoded, again) && bad == again);
            }
        }
        for (unsigned id = 0; id < 9; ++id)
        {
            auto bad = frame;
            switch (id)
            {
                case 0:
                    bad.owner.world = 0;
                    break;
                case 1:
                    bad.owner.player = 0;
                    break;
                case 2:
                    bad.owner.object.generation = 0;
                    break;
                case 3:
                    bad.owner.object.slot = 65536;
                    break;
                case 4:
                    bad.owner.force_revision = 0;
                    break;
                case 5:
                    bad.sequence = 0;
                    break;
                case 6:
                    bad.state = 0;
                    break;
                case 7:
                    bad.aim.pitch = std::numeric_limits<float>::quiet_NaN();
                    break;
                case 8:
                    bad.aim.yaw = 1e7f;
                    break;
            }
            std::vector<std::uint8_t> bytes{123};
            CHECK(!encode_weapon_frame(bad, bytes) && bytes == std::vector<std::uint8_t>{123});
        }
        std::cout << "Weapon frames/ACK: explicit LE 64/71 bytes, truncations, atomic invalid-field refusal, "
                     "20000 mutations passed\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
