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
        std::vector<PrivateValue> values{
            BloodbathStart{1729},
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
            CameraState{123, {4, 5}, {6, 7}, {8, 9}, {1, false, true, false}, 34, true},
            WeaponLoadout{123, 1, 2, {1, 1}, {50, 50}, {50, 50}, true, true, true, false, {1, 1}},
            WonkyVision{15, true},
            FinishHimVoice{65535, 3, 0},
            FragEvent{{2, 3}, {4, 5}, -7, 65535},
            PlayerRoster{{{8, {9, 10}, "Caleb %s", 0, -7}, {15, {12, 13}, "Ophelia", 2, 10}}},
            LevelChanging{}};
        CHECK(values.size() == std::variant_size_v<PrivateValue>);
        std::mt19937 random(0xB2E7);
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            PrivateEvent value{{7, 8, {9, 10}}, 11, 12, values[index]};
            std::vector<std::uint8_t> bytes, again;
            CHECK(encode_private(value, bytes) && is_private_packet(bytes) && bytes.size() <= private_packet_limit);
            const std::vector<std::uint8_t> header{0x42, 0x32, 0x50, 0x45, 17, 0, std::uint8_t(index + 1)};
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
        value.value = WonkyVision{-1, true};
        CHECK(!valid_private(value));
        value.value = WonkyVision{std::numeric_limits<float>::quiet_NaN(), false};
        CHECK(!valid_private(value));
        value.value = FinishHimVoice{0, 4, 0};
        CHECK(!valid_private(value));
        value.value = FragEvent{{2, 3}, {}, 0, 0};
        CHECK(!valid_private(value));
        value.value = FragEvent{{2, 3}, {4, 5}, -1000001, 0};
        CHECK(!valid_private(value));
        return true;
    }
    bool roster()
    {
        const Joined owner{7, 1, {0, 1}};
        PlayerRoster state{{{1, owner.object, "Caleb", 0, 0}, {2, {1, 1}, "Ophelia %s", 2, -1}}};
        PrivateEvent event{owner, 1, 0, state};
        NetworkPlayerEvents inbox;
        std::string error;
        CHECK(inbox.begin(owner));
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Applied);
        CHECK(inbox.roster() == state.players && inbox.player({1, 1})->name == "Ophelia %s");
        ++event.sequence;
        event.value = FragEvent{{0, 1}, {1, 1}, 0, 0};
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Applied &&
              inbox.player({1, 1})->frags == 0);
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Ignored &&
              inbox.player({1, 1})->frags == 0);
        ++event.sequence;
        event.value = FragEvent{{1, 1}, {1, 1}, -1, 0};
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Applied &&
              inbox.player({1, 1})->frags == -1);
        ++event.sequence;
        state.players.pop_back();
        event.value = state;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Applied && !inbox.player({1, 1}));
        ++event.sequence;
        state.players.push_back({2, {1, 1}, "Revived old connection", 2, 0});
        event.value = state;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Refused && inbox.roster().size() == 1);
        state.players.back().player = 3;
        state.players.back().object.generation = 2;
        event.value = state;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Applied && !inbox.player({1, 1}));
        const auto saved = inbox.roster();
        ++event.sequence;
        state.players[0].object.generation = 2;
        event.value = state;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Refused && inbox.roster() == saved);
        state.players.erase(state.players.begin());
        event.value = state;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Refused && inbox.roster() == saved);
        event.owner.world = 8;
        CHECK(inbox.receive(event, error) == NetworkPlayerEvents::Reception::Ignored);
        CHECK(inbox.begin({8, 4, {0, 2}}) && inbox.roster().empty());

        state.players.clear();
        for (unsigned i = 0; i < 16; ++i)
            state.players.push_back({i + 1, {i, 1}, std::string(100, 'x'), DBYTE(i % 4), i % 2 ? -1000000 : 1000000});
        state.mode = 4;
        state.team_frags = {-1000000, 1000000};
        for (auto& p : state.players)
            p.team = DBYTE(1 + p.player % 2);
        event = {owner, 1, 0, state};
        std::vector<std::uint8_t> bytes, unchanged;
        CHECK(encode_private(event, bytes) && bytes.size() == 2041 && bytes.size() <= private_packet_limit);
        PrivateEvent decoded;
        CHECK(decode_private(bytes, decoded) == DecodePrivate::Ok && std::get<PlayerRoster>(decoded.value) == state);
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            CHECK(decode_private(std::span(bytes).first(i), decoded) == DecodePrivate::Malformed);
            CHECK(encode_private(decoded, unchanged) && unchanged == bytes);
        }
        for (const auto count : {0, 17, 255})
        {
            auto corrupt = bytes;
            corrupt[47] = std::uint8_t(count);
            CHECK(decode_private(corrupt, decoded) == DecodePrivate::Malformed);
        }
        const auto invalid = [&](PlayerRoster bad)
        {
            event.value = std::move(bad);
            return !valid_private(event) && !encode_private(event, unchanged);
        };
        auto bad = state;
        bad.team_frags[0] = -1000001;
        CHECK(invalid(bad));
        bad = state;
        bad.mode = 1;
        CHECK(invalid(bad));
        bad = state;
        bad.mode = 3;
        CHECK(invalid(bad));
        bad = state;
        bad.players[0].team = 0;
        CHECK(invalid(bad));
        bad.players[0].team = 3;
        CHECK(invalid(bad));
        bad = state;
        bad.players.back().name += 'x';
        CHECK(invalid(bad));
        bad = state;
        bad.players[0].name = "bad\nname";
        CHECK(invalid(bad));
        bad = state;
        bad.players[0].character = 4;
        CHECK(invalid(bad));
        bad = state;
        bad.players[0].frags = -1000001;
        CHECK(invalid(bad));
        bad = state;
        bad.players[1].player = 1;
        CHECK(invalid(bad));
        bad = state;
        bad.players[1].object = {0, 2};
        CHECK(invalid(bad));
        bad = state;
        bad.players[1].object.generation = 0;
        CHECK(invalid(bad));
        bad = state;
        bad.players.push_back({17, {16, 1}, "overflow", 0, 0});
        CHECK(invalid(bad));
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
    if (!codec() || !receiver() || !roster())
        return 1;
    std::cout << "27 private event tags: canonical LE bytes, every truncation, 40500 mutations, addressed HUD/inbox "
                 "bounds; atomic 16-player/2016-byte roster, scores, stale identity and generations passed\n";
}
