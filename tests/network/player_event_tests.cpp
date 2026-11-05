#include "client/network_player_events.h"
#include <iostream>
#include <limits>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
static_assert(!std::is_constructible_v<PrivateValue, OrbView>);
static_assert(!std::is_constructible_v<PrivateValue, SeeingEyeView>);
static_assert(!std::is_constructible_v<PrivateValue, CreatureAttach>);
static_assert(!std::is_constructible_v<PrivateValue, CTFEvent>);
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
    bool codec()
    {
        PlayerStats stats;
        stats.health = 75;
        stats.armor = 30;
        stats.bullets = 42;
        stats.ammo = 42;
        stats.alternate_ammo = 10;
        stats.weapon = 1;
        stats.weapon_slot = 2;
        stats.ammo_type = 1;
        stats.alternate_ammo_type = 9;
        stats.weapon_slots = {1, 2, 3, 4, 5, 6, 7, 8, 9, 17};
        InventoryItems items;
        items.current = InventoryItemView{1, "medi.pcx", "medi_h.pcx", "", 301, 45};
        items.previous = InventoryItemView{6, "key.pcx", "key_h.pcx", "Lab key", 0, 1};
        items.next = items.current;
        items.show = true;
        std::vector<PrivateValue> values{BloodbathStart{1729},
                                         PlayerRotation{-.25f, 1.5f},
                                         stats,
                                         MissionObjectives{4, "title", "text"},
                                         WorldExit{"multi/bb_crypt", WorldExitType::World},
                                         BossHealth{.75f},
                                         Conversation{"voice.wav", "words", 4},
                                         InventoryAction{3, true},
                                         ScreenFade{true},
                                         VoiceEvent{false, 3, 8},
                                         VoiceStop{},
                                         PlayerDead{},
                                         MusicCommand{MusicCommandType::MotifLoop, 2, "Combat"},
                                         ZoomView{true, 98},
                                         EyeEvent{{4, 5}, true, 1.5f},
                                         AttachEvent{{6, 7}, {8, 9}, false},
                                         OrbEvent{{10, 11}, true},
                                         FlagEvent{CTFEventKind::Captured, 2, 3, {12, 13}},
                                         items,
                                         PlayerText{"hello"},
                                         CameraState{123, {4, 5}, {6, 7}, {8, 9}, {1, false, true, false}, 34, true}};
        CHECK(values.size() == std::variant_size_v<PrivateValue>);
        std::mt19937 random(0xB2E7);
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            PrivateEvent value{{7, 8, {9, 10}}, 11, 12, values[index]};
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_private(value, bytes) && is_private_packet(bytes) && bytes.size() <= private_packet_limit);
            const std::vector<std::uint8_t> header{0x42, 0x32, 0x50, 0x45, 9, 0, std::uint8_t(index + 1)};
            CHECK(std::equal(header.begin(), header.end(), bytes.begin()));
            PrivateEvent decoded;
            CHECK(decode_private(bytes, decoded) == DecodePrivate::Ok && encode_private(decoded, again) &&
                  bytes == again);
            CHECK(decoded.owner == value.owner && decoded.sequence == 11 && decoded.tick == 12);
            if (index == 2)
                CHECK(std::get<PlayerStats>(decoded.value) == stats);
            if (index == 18)
                CHECK(std::get<InventoryItems>(decoded.value) == items);
            const auto sentinel = again;
            for (std::size_t length = 0; length < bytes.size(); ++length)
            {
                CHECK(decode_private(std::span(bytes).first(length), decoded) == DecodePrivate::Malformed);
                CHECK(encode_private(decoded, again) && again == sentinel);
            }
            auto corrupt = bytes;
            corrupt[4] = 6;
            CHECK(decode_private(corrupt, decoded) == DecodePrivate::Protocol);
            corrupt = bytes;
            corrupt.push_back(0);
            CHECK(decode_private(corrupt, decoded) == DecodePrivate::Malformed);
            for (int i = 0; i < 1500; ++i)
            {
                corrupt = bytes;
                for (unsigned n = 1 + random() % 4; n; --n)
                    corrupt[random() % corrupt.size()] = std::uint8_t(random());
                if (decode_private(corrupt, decoded) == DecodePrivate::Ok)
                    CHECK(encode_private(decoded, again) && again == corrupt);
            }
        }
        PrivateEvent value{{7, 8, {9, 10}}, 1, 0, ScreenFade{}};
        std::vector<std::uint8_t> bytes;
        CHECK(encode_private(value, bytes));
        bytes[47] = 2;
        CHECK(decode_private(bytes, value) == DecodePrivate::Malformed);
        value.value = PlayerStats{};
        std::get<PlayerStats>(value.value).health = std::numeric_limits<float>::quiet_NaN();
        CHECK(!valid_private(value));
        value.value = OrbEvent{};
        CHECK(!valid_private(value));
        value.value = EyeEvent{{}, false, 10};
        CHECK(valid_private(value));
        value.value = EyeEvent{{}, true, 0};
        CHECK(!valid_private(value));
        value.value = PlayerText{std::string(81, 'x')};
        CHECK(!valid_private(value));
        value.value = MusicCommand{MusicCommandType::Level, 255, ""};
        CHECK(valid_private(value)); // Original client decides whether a level exists.
        value.value = items;
        std::get<InventoryItems>(value.value).current->icon = "../outside.pcx";
        CHECK(!valid_private(value));
        return true;
    }
    bool receiver()
    {
        NetworkPlayerEvents a, b;
        const Joined first{7, 1, {0, 1}}, second{7, 2, {1, 1}};
        CHECK(a.begin(first) && b.begin(second));
        std::string error;
        PrivateEvent event{first, 1, 3, PlayerStats{}};
        std::get<PlayerStats>(event.value).health = 75;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        CHECK(b.receive(event, error) == NetworkPlayerEvents::Reception::Ignored && b.hud().stats.health == 0);
        CHECK(a.hud().stats.health == 75);
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Ignored && a.pending() == 1);
        InventoryItems items;
        items.current = InventoryItemView{1, "medi.pcx", "medi_h.pcx", "", 301, 25};
        items.show = true;
        event.value = items;
        event.sequence = 2;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        CHECK(a.hud().item_messages == 1 && a.hud().item_shows == 1 && !a.hud().items.show);
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Ignored && a.hud().item_shows == 1);
        event.sequence = 4;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Refused && a.sequence() == 2);
        event.sequence = 3;
        event.tick = 2;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Refused && a.sequence() == 2);
        event.tick = 3;
        event.value = OrbEvent{{10, 42}, false};
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        std::vector<PrivateEvent> delivered;
        a.take(delivered);
        CHECK(delivered.size() == 3 && !a.pending() &&
              std::get<OrbEvent>(delivered.back().value).orb == ObjectId(10, 42));
        for (unsigned i = 0; i < private_queue_limit; ++i)
        {
            ++event.sequence;
            event.value = PlayerText{"bounded"};
            CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        }
        ++event.sequence;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Refused);
        a.take(delivered);
        CHECK(delivered.size() == private_queue_limit &&
              a.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        event.owner.object.generation = 2;
        ++event.sequence;
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Ignored);
        CHECK(a.begin({8, 3, {0, 2}}) && !a.pending() && !a.sequence() && a.hud().stats.health == 0);
        CHECK(a.receive(event, error) == NetworkPlayerEvents::Reception::Ignored);
        return true;
    }
} // namespace
int main()
{
    if (!codec() || !receiver())
        return 1;
    std::cout << "21 private event tags: canonical LE bytes, every truncation, 31500 mutations, addressed HUD/inbox "
                 "bounds passed\n";
}
