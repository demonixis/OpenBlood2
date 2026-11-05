#include "client/network_weapons.h"
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
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    WeaponLoadout loadout()
    {
        WeaponLoadout state;
        state.force_revision = 300;
        state.view_revision = {1, 1};
        state.character = 0;
        state.slot = 1;
        state.weapons = {1, 1};
        state.ammunition = {50, 50};
        state.alternate_ammunition = {50, 50};
        state.alive = state.input_enabled = state.fire_allowed = true;
        return state;
    }
    const Joined owner{7, 1, {2, 4}};
    bool wire()
    {
        std::string error;
        PrivateEvent event{owner, 1, 600, loadout()}, decoded;
        std::vector<std::uint8_t> bytes;
        CHECK(encode_private(event, bytes) && bytes.size() == 92 && bytes[6] == 22);
        CHECK(bytes[47] == 44 && bytes[48] == 1); // Explicit LE force revision 300.
        CHECK(decode_private(bytes, decoded) == DecodePrivate::Ok &&
              std::get<WeaponLoadout>(decoded.value) == loadout());
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            decoded = {owner, 999, 1, BloodbathStart{3}};
            CHECK(decode_private(std::span(bytes).first(size), decoded) == DecodePrivate::Malformed &&
                  decoded.sequence == 999 && std::holds_alternative<BloodbathStart>(decoded.value));
        }
        auto bad = bytes;
        bad.back() = 16;
        CHECK(decode_private(bad, decoded) == DecodePrivate::Malformed);
        bad = bytes;
        bad.push_back(0);
        CHECK(decode_private(bad, decoded) == DecodePrivate::Malformed);
        bad = bytes;
        bad[4] = 8;
        CHECK(decode_private(bad, decoded) == DecodePrivate::Protocol);
        for (unsigned case_id = 0; case_id < 10; ++case_id)
        {
            auto state = loadout();
            switch (case_id)
            {
                case 0:
                    state.force_revision = 0;
                    break;
                case 1:
                    state.character = 4;
                    break;
                case 2:
                    state.slot = 13;
                    break;
                case 3:
                    state.weapons[1] = 18;
                    break;
                case 4:
                    state.ammunition[1] = -1;
                    break;
                case 5:
                    state.alternate_ammunition[0] = std::numeric_limits<float>::infinity();
                    break;
                case 6:
                    state.alive = false;
                    break;
                case 7:
                    state.input_enabled = false;
                    break;
                case 8:
                    state.binoculars = true;
                    break;
                case 9:
                    state.view_revision[1] = 0;
                    break;
            }
            event.value = state;
            CHECK(!valid_private(event));
        }
        std::minstd_rand random(71);
        for (unsigned i = 0; i < 5000; ++i)
        {
            auto mutated = bytes;
            mutated[random() % mutated.size()] ^= std::uint8_t(1u << (random() % 8));
            if (decode_private(mutated, decoded) == DecodePrivate::Ok)
            {
                std::vector<std::uint8_t> roundtrip;
                CHECK(encode_private(decoded, roundtrip) && roundtrip == mutated);
            }
        }
        std::cout
            << "Weapon loadout codec: 92 bytes, all truncations, invalid fields/flags/version, 5000 mutations passed\n";
        return true;
    }
    bool receive(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ModelStore models(resources);
        NetworkWeapons client;
        using Reception = NetworkWeapons::Reception;
        CHECK(client.begin(owner) && !client.ready(300));
        auto state = loadout();
        PrivateEvent event{owner, 1, 600, state};
        CHECK(client.receive(event, models, resources, error) == Reception::Applied && client.ready(300));
        CHECK(client.views()[0].type == 1 && client.views()[1].type == 1 && client.views()[1].left);
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(client.update(300, 1. / 60, error));
        CHECK(client.views()[0].state == WS_REST && client.views()[1].state == WS_REST && !client.views()[0].shots);
        const auto animation = client.views()[0].animation;
        const auto old_model = client.views()[0].model;
        state.ammunition = {10, 10};
        event = {owner, 2, 780, state};
        CHECK(client.receive(event, models, resources, error) == Reception::Applied);
        CHECK(client.views()[0].animation.revision == animation.revision &&
              client.views()[0].animation.time == animation.time && client.views()[0].model == old_model &&
              client.state()->ammunition[0] == 10);
        CHECK(client.receive(event, models, resources, error) == Reception::Ignored);
        event.owner.world = 6;
        ++event.sequence;
        CHECK(client.receive(event, models, resources, error) == Reception::Ignored);
        event.owner = owner;
        state.force_revision = 299;
        event.value = state;
        CHECK(client.receive(event, models, resources, error) == Reception::Ignored);
        state.force_revision = 301;
        state.weapons = {4, 0};
        state.view_revision = {2, 2};
        event.value = state;
        Resources absent;
        ModelStore absent_models(absent);
        CHECK(client.receive(event, absent_models, absent, error) == Reception::Refused);
        CHECK(client.views()[0].model == old_model && client.views()[1].type == 1 && client.ready(300));
        CHECK(client.receive(event, models, resources, error) == Reception::Applied && client.ready(301));
        CHECK(client.views()[0].type == 4 && !client.views()[1].model && client.views()[0].state == WS_DRAW);
        const auto time = client.time();
        const auto updates = client.updates;
        CHECK(client.update(300, 1. / 60, error) && client.time() == time && client.updates == updates);
        CHECK(client.update(301, 1. / 60, error) && client.time() > time && client.updates == updates + 1);
        CHECK(!client.update(301, std::numeric_limits<double>::quiet_NaN(), error));
        state.character = 1;
        ++state.view_revision[0];
        event.value = state;
        ++event.sequence;
        CHECK(client.receive(event, models, resources, error) == Reception::Applied &&
              client.views()[0].character == 1);
        CHECK(client.views()[0].skin_path.find("O_") != std::string::npos);
        state.alive = state.fire_allowed = false;
        event.value = state;
        ++event.sequence;
        CHECK(client.receive(event, models, resources, error) == Reception::Applied);
        const auto frame = client.views()[0].animation;
        CHECK(client.update(301, 1. / 60, error) && client.views()[0].animation.time == frame.time);
        // Same-type recreation is identified even if A -> B -> A happened
        // between simulation ticks. The other hand retains its animation.
        state.alive = state.fire_allowed = true;
        state.view_revision[0] += 2;
        event.value = state;
        ++event.sequence;
        CHECK(client.receive(event, models, resources, error) == Reception::Applied &&
              client.views()[0].state == WS_DRAW);
        for (unsigned tick = 0; tick < 180; ++tick)
            CHECK(client.update(301, 1. / 60, error));
        const auto retained = client.views()[0].animation;
        state.weapons[1] = WEAP_BERETTA;
        event.value = state;
        ++event.sequence;
        CHECK(client.receive(event, models, resources, error) == Reception::Refused); // Missing revision.
        ++state.view_revision[1];
        event.value = state;
        CHECK(client.receive(event, models, resources, error) == Reception::Applied);
        CHECK(client.views()[0].animation.time == retained.time &&
              client.views()[0].animation.revision == retained.revision && client.views()[1].state == WS_DRAW);
        CHECK(client.begin({8, 2, {2, 5}}) && !client.ready(301) && !client.views()[0].model);
        CHECK(client.receive(event, models, resources, error) == Reception::Ignored);
        std::cout << "Independent loadout receiver: dual views, ammo retention, atomic resource failure, "
                     "character/death, force and world checks passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return wire() && (argc == 1 || (argc == 2 && receive(argv[1]))) ? 0 : 1;
}
