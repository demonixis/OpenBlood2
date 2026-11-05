#include "app/network_controls.h"
#include "net/admission.h"
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
    bool codec()
    {
        const MovementIdentity owner{7, 9, {4, 3}, 123};
        std::vector<CommandMessage> messages;
        for (unsigned kind = 1; kind <= 10; ++kind)
        {
            CommandAction action{CommandKind(kind)};
            if (action.kind == CommandKind::WeaponSlot)
                action.argument = 9;
            if (action.kind == CommandKind::Item)
                action.argument = 5;
            if (action.kind == CommandKind::Use)
                action.aim = {.25f, 4.712389f}; // Authored start yaw above pi.
            messages.emplace_back(PlayerCommand{owner, kind, action});
        }
        messages.emplace_back(CommandAck{owner, 11, true});
        messages.emplace_back(CommandAck{owner, 12, false});
        std::minstd_rand random(49);
        for (const auto& message : messages)
        {
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_command(message, bytes) && is_command_packet(bytes));
            CHECK(bytes.size() == (std::holds_alternative<PlayerCommand>(message) ? 57 : 48));
            CHECK(bytes[0] == 'B' && bytes[1] == '2' && bytes[2] == 'P' && bytes[3] == 'C' && bytes[4] == 10);
            CommandMessage decoded = CommandAck{owner, 999, true};
            for (std::size_t size = 0; size < bytes.size(); ++size)
                CHECK(decode_command(std::span(bytes).first(size), decoded) == DecodeCommand::Malformed &&
                      std::get<CommandAck>(decoded).sequence == 999);
            CHECK(decode_command(bytes, decoded) == DecodeCommand::Ok && encode_command(decoded, again) &&
                  bytes == again);
            auto bad = bytes;
            bad.push_back(0);
            CHECK(decode_command(bad, decoded) == DecodeCommand::Malformed);
            bad = bytes;
            bad[4] = 9;
            CHECK(decode_command(bad, decoded) == DecodeCommand::Protocol);
            for (unsigned i = 0; i < 1500; ++i)
            {
                bad = bytes;
                bad[random() % bad.size()] ^= std::uint8_t(1u << (random() % 8));
                if (decode_command(bad, decoded) == DecodeCommand::Ok)
                    CHECK(encode_command(decoded, again) && bad == again);
            }
        }
        PlayerCommand command{owner, 1, {CommandKind::Use}};
        for (unsigned id = 0; id < 11; ++id)
        {
            auto bad = command;
            switch (id)
            {
                case 0:
                    bad.owner.force_revision = 0;
                    break;
                case 1:
                    bad.owner.object.generation = 0;
                    break;
                case 2:
                    bad.sequence = 0;
                    break;
                case 3:
                    bad.action.kind = CommandKind(0);
                    break;
                case 4:
                    bad.action.kind = CommandKind(11);
                    break;
                case 5:
                    bad.action.argument = 1;
                    break;
                case 6:
                    bad.action = {CommandKind::WeaponSlot, 10};
                    break;
                case 7:
                    bad.action = {CommandKind::Item, 6};
                    break;
                case 8:
                    bad.action.aim.pitch = std::numeric_limits<float>::quiet_NaN();
                    break;
                case 9:
                    bad.action = {CommandKind::NextWeapon, 0, {0, 1}};
                    break;
                case 10:
                    bad.action.aim.yaw = 1e7f;
                    break;
            }
            CHECK(!valid_command(bad));
        }
        std::cout << "Commands: ten actions, both ACK results, portable sizes, truncations, invalid fields and 18000 "
                     "mutations passed\n";
        return true;
    }
    bool input()
    {
        MovementControls controls;
        controls.focused = true;
        controls.bindings.entries.clear();
        CHECK(controls.bindings.add(InputSource::Key, SDL_SCANCODE_E, InputAction::Use));
        CHECK(controls.bindings.add(InputSource::Key, SDL_SCANCODE_M, InputAction::Medkit));
        CHECK(controls.bindings.add(InputSource::Key, SDL_SCANCODE_G, InputAction::FireAlternate));
        NetworkControls commands;
        std::vector<CommandAction> values;
        controls.keys[SDL_SCANCODE_E] = controls.keys[SDL_SCANCODE_M] = controls.keys[SDL_SCANCODE_G] = true;
        controls.pending_weapon_slot = 4;
        controls.pending[std::size_t(InputAction::NextItem)] = true;
        commands.sample(controls, false, {.25f, -1}, values);
        CHECK(values.size() == 5 && values[0].kind == CommandKind::WeaponSlot && values[0].argument == 4);
        CHECK(values[1].kind == CommandKind::Item && values[1].argument == 1);
        CHECK(values[2].kind == CommandKind::NextItem && values[3].kind == CommandKind::Use &&
              values[3].aim.pitch == .25f && values[4].kind == CommandKind::Alternate);
        for (unsigned i = 0; i < 240; ++i)
        {
            commands.sample(controls, false, {}, values);
            CHECK(values.empty());
        }
        commands.sample(controls, true, {}, values);
        CHECK(values.empty());
        commands.sample(controls, false, {}, values);
        CHECK(values.empty()); // Held presses cannot cross the menu.
        controls.keys = {};
        commands.sample(controls, false, {}, values);
        controls.keys[SDL_SCANCODE_E] = true;
        commands.sample(controls, false, {}, values);
        CHECK(values.size() == 1 && values[0].kind == CommandKind::Use);
        controls.focused = false;
        controls.pending_weapon_slot = 8;
        commands.sample(controls, false, {}, values);
        CHECK(values.empty() && controls.pending_weapon_slot == -1);
        controls.focused = true;
        commands.sample(controls, false, {}, values);
        CHECK(values.empty());
        std::cout << "Network actions: original ordering, one edge across 240 frames, menu/focus suppression passed\n";
        return true;
    }
} // namespace
int main()
{
    return codec() && input() ? 0 : 1;
}
