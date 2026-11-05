#include "net/player_events.h"
#include "net/codec.h"
#include <algorithm>
#include <type_traits>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x45503242; // B2PE.
        bool object(ObjectId id, bool required = false)
        {
            return id ? id.generation && id.slot < 65536 : !required && !id.generation;
        }
        bool number(float v, float limit = 1e7f)
        {
            return std::isfinite(v) && std::abs(v) <= limit;
        }
        bool text(const std::string& s, std::size_t maximum)
        {
            return s.size() <= maximum && s.find('\0') == s.npos;
        }
        bool item(const std::optional<InventoryItemView>& i)
        {
            const auto path = [](const std::string& s)
            {
                return text(s, 100) && !s.empty() && s.front() != '/' && s.front() != '\\' && s.find(':') == s.npos &&
                       s.find("..") == s.npos;
            };
            return !i || (i->type >= 1 && i->type <= 9 && path(i->icon) && path(i->highlighted_icon) &&
                          text(i->name, 100) && (i->type == 6 ? !i->name_id : i->name_id >= 301 && i->name_id <= 308));
        }
        bool weapon(DBYTE type)
        {
            return type <= 17 || (type >= 49 && type <= 51);
        }
        void id(Writer& w, ObjectId value)
        {
            w.u32(value.slot);
            w.u32(value.generation);
        }
        ObjectId id(Reader& r)
        {
            return {r.u32(), r.u32()};
        }
        bool boolean(Reader& r, bool& out)
        {
            const auto v = r.u8();
            out = v != 0;
            return r.good() && v <= 1;
        }
        void write_item(Writer& w, const std::optional<InventoryItemView>& value)
        {
            w.u8(value.has_value());
            if (!value)
                return;
            w.u8(value->type);
            w.u8(value->charges);
            w.u32(value->name_id);
            w.text(value->icon, 100);
            w.text(value->highlighted_icon, 100);
            w.text(value->name, 100);
        }
        bool read_item(Reader& r, std::optional<InventoryItemView>& out)
        {
            bool present;
            if (!boolean(r, present))
                return false;
            if (!present)
                return true;
            InventoryItemView value;
            value.type = r.u8();
            value.charges = r.u8();
            value.name_id = r.u32();
            value.icon = r.text(100);
            value.highlighted_icon = r.text(100);
            value.name = r.text(100);
            out = std::move(value);
            return r.good();
        }
    } // namespace
    bool valid_private(const PrivateEvent& event)
    {
        if (!event.owner.world || !event.owner.player || !object(event.owner.object, true) || !event.sequence)
            return false;
        return std::visit(
            [](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, BloodbathStart>)
                    return true; // Original server draw, client selects modulo eight.
                else if constexpr (std::is_same_v<T, PlayerRotation>)
                    return number(v.pitch, 1e6f) && number(v.yaw, 1e6f);
                else if constexpr (std::is_same_v<T, PlayerStats>)
                    return number(v.health) && number(v.armor) && number(v.bullets) && number(v.ammo) && v.ammo >= 0 &&
                           number(v.alternate_ammo) && v.alternate_ammo >= 0 && number(v.max_health) &&
                           v.max_health >= 0 && number(v.max_armor) && v.max_armor >= 0 && weapon(v.weapon) &&
                           v.weapon_slot < 13 && v.ammo_type < 13 && v.alternate_ammo_type < 13 &&
                           std::all_of(v.weapon_slots.begin(), v.weapon_slots.end(), weapon);
                else if constexpr (std::is_same_v<T, MissionObjectives>)
                    return v.resource <= 65535 - 6000 + 1 && text(v.title, 200) && text(v.text, 200);
                else if constexpr (std::is_same_v<T, WorldExit>)
                    return v.type <= WorldExitType::Episode && text(v.next_world, 100);
                else if constexpr (std::is_same_v<T, BossHealth>)
                    return number(v.fraction);
                else if constexpr (std::is_same_v<T, Conversation>)
                    return !v.file.empty() && text(v.file, 199) && text(v.text, 499);
                else if constexpr (std::is_same_v<T, InventoryAction>)
                    return v.type == 1 || v.type == 2 || v.type == 3 || v.type == 5;
                else if constexpr (std::is_same_v<T, VoiceEvent>)
                    return v.character >= 0 && v.character < 4 && v.event >= 0 && v.event < (v.group ? 12 : 9);
                else if constexpr (std::is_same_v<T, MusicCommand>)
                    return v.type == MusicCommandType::Level
                               ? v.name.empty()
                               : v.type <= MusicCommandType::Break && !v.name.empty() && text(v.name, 49);
                else if constexpr (std::is_same_v<T, EyeEvent>)
                    return object(v.eye, v.active) && number(v.blind_seconds, 10) && v.blind_seconds >= 0;
                else if constexpr (std::is_same_v<T, OrbEvent>)
                    return object(v.orb, true);
                else if constexpr (std::is_same_v<T, AttachEvent>)
                    return object(v.creature, true) && object(v.enemy, true);
                else if constexpr (std::is_same_v<T, FlagEvent>)
                    return v.kind <= CTFEventKind::Captured && v.team <= 2 && v.points >= 0 && v.points <= 1000000 &&
                           object(v.capturer);
                else if constexpr (std::is_same_v<T, InventoryItems>)
                    return item(v.current) && item(v.previous) && item(v.next);
                else if constexpr (std::is_same_v<T, PlayerText>)
                    return text(v.text, 80);
                else if constexpr (std::is_same_v<T, WonkyVision>)
                    return number(v.seconds, 60) && v.seconds >= 0;
                else if constexpr (std::is_same_v<T, FinishHimVoice>)
                    return v.attacker_character < 4 && v.victim_character < 4;
                else if constexpr (std::is_same_v<T, FragEvent>)
                    return object(v.victim, true) && object(v.killer, true) && v.killer_frags >= -1000000 &&
                           v.killer_frags <= 1000000;
                else if constexpr (std::is_same_v<T, WeaponLoadout>)
                {
                    const auto ammo = [](float value) { return number(value) && value >= 0; };
                    return v.force_revision && v.character < 4 && v.slot < 13 && v.view_revision[0] &&
                           v.view_revision[1] && std::all_of(v.weapons.begin(), v.weapons.end(), weapon) &&
                           std::all_of(v.ammunition.begin(), v.ammunition.end(), ammo) &&
                           std::all_of(v.alternate_ammunition.begin(), v.alternate_ammunition.end(), ammo) &&
                           (!v.fire_allowed || (v.alive && v.input_enabled && !v.binoculars));
                }
                else if constexpr (std::is_same_v<T, CameraState>)
                    return v.force_revision && object(v.scripted) && object(v.eye) && object(v.orb) &&
                           v.settings.type <= 2 && number(v.eye_level, 1000) && v.eye_level >= 0;
                else
                    return true;
            },
            event.value);
    }
    bool is_private_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_private(const PrivateEvent& event, std::vector<std::uint8_t>& out)
    {
        if (!valid_private(event))
            return false;
        Writer w(private_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u8(std::uint8_t(event.value.index() + 1));
        w.u64(event.owner.world);
        w.u64(event.owner.player);
        id(w, event.owner.object);
        w.u64(event.sequence);
        w.u64(event.tick);
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, BloodbathStart>)
                    w.u16(v.index);
                else if constexpr (std::is_same_v<T, PlayerRotation>)
                {
                    w.f32(v.pitch);
                    w.f32(v.yaw);
                }
                else if constexpr (std::is_same_v<T, PlayerStats>)
                {
                    for (float f : {v.health, v.armor, v.bullets, v.ammo, v.alternate_ammo, v.max_health, v.max_armor})
                        w.f32(f);
                    for (DBYTE b : {v.weapon, v.weapon_slot, v.ammo_type, v.alternate_ammo_type})
                        w.u8(b);
                    for (auto b : v.weapon_slots)
                        w.u8(b);
                }
                else if constexpr (std::is_same_v<T, MissionObjectives>)
                {
                    w.u32(v.resource);
                    w.text(v.title, 200);
                    w.text(v.text, 200);
                }
                else if constexpr (std::is_same_v<T, WorldExit>)
                {
                    w.u8(DBYTE(v.type));
                    w.text(v.next_world, 100);
                }
                else if constexpr (std::is_same_v<T, BossHealth>)
                    w.f32(v.fraction);
                else if constexpr (std::is_same_v<T, Conversation>)
                {
                    w.u8(v.character);
                    w.text(v.file, 199);
                    w.text(v.text, 499);
                }
                else if constexpr (std::is_same_v<T, InventoryAction>)
                {
                    w.u8(v.type);
                    w.u8(v.active);
                }
                else if constexpr (std::is_same_v<T, ScreenFade>)
                    w.u8(v.in);
                else if constexpr (std::is_same_v<T, VoiceEvent>)
                {
                    w.u8(v.group);
                    w.u8(DBYTE(v.character));
                    w.u8(DBYTE(v.event));
                }
                else if constexpr (std::is_same_v<T, MusicCommand>)
                {
                    w.u8(DBYTE(v.type));
                    w.u8(v.level);
                    w.text(v.name, 49);
                }
                else if constexpr (std::is_same_v<T, ZoomView>)
                {
                    w.u8(v.on);
                    w.u8(v.type);
                }
                else if constexpr (std::is_same_v<T, EyeEvent>)
                {
                    id(w, v.eye);
                    w.u8(v.active);
                    w.f32(v.blind_seconds);
                }
                else if constexpr (std::is_same_v<T, OrbEvent>)
                {
                    id(w, v.orb);
                    w.u8(v.active);
                }
                else if constexpr (std::is_same_v<T, AttachEvent>)
                {
                    id(w, v.creature);
                    id(w, v.enemy);
                    w.u8(v.active);
                }
                else if constexpr (std::is_same_v<T, FlagEvent>)
                {
                    w.u8(DBYTE(v.kind));
                    w.u8(v.team);
                    w.u32(std::uint32_t(v.points));
                    id(w, v.capturer);
                }
                else if constexpr (std::is_same_v<T, InventoryItems>)
                {
                    write_item(w, v.current);
                    write_item(w, v.previous);
                    write_item(w, v.next);
                    w.u8(v.show);
                }
                else if constexpr (std::is_same_v<T, PlayerText>)
                    w.text(v.text, 80);
                else if constexpr (std::is_same_v<T, WonkyVision>)
                {
                    w.f32(v.seconds);
                    w.u8(v.no_move);
                }
                else if constexpr (std::is_same_v<T, FinishHimVoice>)
                {
                    w.u16(v.index);
                    w.u8(v.attacker_character);
                    w.u8(v.victim_character);
                }
                else if constexpr (std::is_same_v<T, FragEvent>)
                {
                    id(w, v.victim);
                    id(w, v.killer);
                    w.u32(std::uint32_t(v.killer_frags + 1000000));
                    w.u16(v.index);
                }
                else if constexpr (std::is_same_v<T, WeaponLoadout>)
                {
                    w.u64(v.force_revision);
                    for (const auto revision : v.view_revision)
                        w.u64(revision);
                    w.u8(v.character);
                    w.u8(v.slot);
                    for (const auto type : v.weapons)
                        w.u8(type);
                    for (const auto ammo : v.ammunition)
                        w.f32(ammo);
                    for (const auto ammo : v.alternate_ammunition)
                        w.f32(ammo);
                    w.u8(v.alive | v.input_enabled << 1 | v.fire_allowed << 2 | v.binoculars << 3);
                }
                else if constexpr (std::is_same_v<T, CameraState>)
                {
                    w.u64(v.force_revision);
                    id(w, v.scripted);
                    id(w, v.eye);
                    id(w, v.orb);
                    w.u8(v.settings.type);
                    w.u8(v.settings.player_movement | v.settings.hide_player << 1 | v.settings.listener << 2);
                    w.f32(v.eye_level);
                    w.u8(v.alive);
                }
            },
            event.value);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodePrivate decode_private(std::span<const std::uint8_t> bytes, PrivateEvent& out)
    {
        if (bytes.size() < 47 || bytes.size() > private_packet_limit)
            return DecodePrivate::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodePrivate::Malformed;
        if (r.u16() != protocol_version)
            return DecodePrivate::Protocol;
        const auto kind = r.u8();
        PrivateEvent event;
        event.owner.world = r.u64();
        event.owner.player = r.u64();
        event.owner.object = id(r);
        event.sequence = r.u64();
        event.tick = r.u64();
        bool ok = true;
        switch (kind)
        {
            case 1:
                event.value = BloodbathStart{r.u16()};
                break;
            case 2:
                event.value = PlayerRotation{r.f32(), r.f32()};
                break;
            case 3:
            {
                PlayerStats v;
                v.health = r.f32();
                v.armor = r.f32();
                v.bullets = r.f32();
                v.ammo = r.f32();
                v.alternate_ammo = r.f32();
                v.max_health = r.f32();
                v.max_armor = r.f32();
                v.weapon = r.u8();
                v.weapon_slot = r.u8();
                v.ammo_type = r.u8();
                v.alternate_ammo_type = r.u8();
                for (auto& b : v.weapon_slots)
                    b = r.u8();
                event.value = v;
                break;
            }
            case 4:
                event.value = MissionObjectives{r.u32(), r.text(200), r.text(200)};
                break;
            case 5:
            {
                WorldExit v;
                v.type = WorldExitType(r.u8());
                v.next_world = r.text(100);
                event.value = std::move(v);
                break;
            }
            case 6:
                event.value = BossHealth{r.f32()};
                break;
            case 7:
            {
                Conversation v;
                v.character = r.u8();
                v.file = r.text(199);
                v.text = r.text(499);
                event.value = std::move(v);
                break;
            }
            case 8:
            {
                InventoryAction v;
                v.type = r.u8();
                ok = boolean(r, v.active);
                event.value = v;
                break;
            }
            case 9:
            {
                ScreenFade v;
                ok = boolean(r, v.in);
                event.value = v;
                break;
            }
            case 10:
            {
                VoiceEvent v;
                ok = boolean(r, v.group);
                v.character = r.u8();
                v.event = r.u8();
                event.value = v;
                break;
            }
            case 11:
                event.value = VoiceStop{};
                break;
            case 12:
                event.value = PlayerDead{};
                break;
            case 13:
            {
                MusicCommand v;
                v.type = MusicCommandType(r.u8());
                v.level = r.u8();
                v.name = r.text(49);
                event.value = std::move(v);
                break;
            }
            case 14:
            {
                ZoomView v;
                ok = boolean(r, v.on);
                v.type = r.u8();
                event.value = v;
                break;
            }
            case 15:
            {
                EyeEvent v;
                v.eye = id(r);
                ok = boolean(r, v.active);
                v.blind_seconds = r.f32();
                event.value = v;
                break;
            }
            case 16:
            {
                AttachEvent v;
                v.creature = id(r);
                v.enemy = id(r);
                ok = boolean(r, v.active);
                event.value = v;
                break;
            }
            case 17:
            {
                OrbEvent v;
                v.orb = id(r);
                ok = boolean(r, v.active);
                event.value = v;
                break;
            }
            case 18:
            {
                FlagEvent v;
                v.kind = CTFEventKind(r.u8());
                v.team = r.u8();
                const auto points = r.u32();
                if (points > 1000000)
                    return DecodePrivate::Malformed;
                v.points = int(points);
                v.capturer = id(r);
                event.value = v;
                break;
            }
            case 19:
            {
                InventoryItems v;
                ok = read_item(r, v.current) && read_item(r, v.previous) && read_item(r, v.next) && boolean(r, v.show);
                event.value = std::move(v);
                break;
            }
            case 20:
                event.value = PlayerText{r.text(80)};
                break;
            case 21:
            {
                CameraState v;
                v.force_revision = r.u64();
                v.scripted = id(r);
                v.eye = id(r);
                v.orb = id(r);
                v.settings.type = r.u8();
                const auto flags = r.u8();
                if (flags > 7)
                    return DecodePrivate::Malformed;
                v.settings.player_movement = flags & 1;
                v.settings.hide_player = flags & 2;
                v.settings.listener = flags & 4;
                v.eye_level = r.f32();
                ok = boolean(r, v.alive);
                event.value = v;
                break;
            }
            case 22:
            {
                WeaponLoadout v;
                v.force_revision = r.u64();
                for (auto& revision : v.view_revision)
                    revision = r.u64();
                v.character = r.u8();
                v.slot = r.u8();
                for (auto& type : v.weapons)
                    type = r.u8();
                for (auto& ammo : v.ammunition)
                    ammo = r.f32();
                for (auto& ammo : v.alternate_ammunition)
                    ammo = r.f32();
                const auto flags = r.u8();
                if (flags > 15)
                    return DecodePrivate::Malformed;
                v.alive = flags & 1;
                v.input_enabled = flags & 2;
                v.fire_allowed = flags & 4;
                v.binoculars = flags & 8;
                event.value = v;
                break;
            }
            case 23:
            {
                WonkyVision v;
                v.seconds = r.f32();
                ok = boolean(r, v.no_move);
                event.value = v;
                break;
            }
            case 24:
                event.value = FinishHimVoice{r.u16(), r.u8(), r.u8()};
                break;
            case 25:
            {
                FragEvent v;
                v.victim = id(r);
                v.killer = id(r);
                const auto score = r.u32();
                if (score > 2000000)
                    return DecodePrivate::Malformed;
                v.killer_frags = int(score) - 1000000;
                v.index = r.u16();
                event.value = v;
                break;
            }
            default:
                return DecodePrivate::Malformed;
        }
        if (!ok || !r.done() || !valid_private(event))
            return DecodePrivate::Malformed;
        out = std::move(event);
        return DecodePrivate::Ok;
    }
} // namespace OpenBlood2::Net
