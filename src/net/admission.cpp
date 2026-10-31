#include "net/admission.h"
#include "game/game_settings.h"
#include "net/codec.h"
#include "net/transport.h"
#include <algorithm>
#include <type_traits>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x394d3242; // B2M9, explicit little endian.
        enum class Kind : std::uint8_t
        {
            Hello = 1,
            WorldOffer,
            Ready,
            Joined,
            Heartbeat,
            BaselineReady
        };
        bool valid(const WorldOffer& v)
        {
            // The remote peer names only an already-mounted retail map, never a path
            // to download or open outside the resource index.
            if (!v.world || !network_game_type(v.mode) || !v.capacity || v.capacity > max_players ||
                v.path.size() > 100 || !v.path.starts_with("worlds/multi/") || !v.path.ends_with(".dat"))
                return false;
            const auto name = std::string_view(v.path).substr(13, v.path.size() - 17);
            return name.starts_with(v.mode == GAMETYPE_CTF ? "ctf_" : "bb_") &&
                   std::all_of(name.begin(), name.end(), [](unsigned char c)
                               { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
        }
        bool valid(const AdmissionMessage& message)
        {
            return std::visit(
                [](const auto& v)
                {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, Hello>)
                        return v.edition <= Edition::Nightmares;
                    else if constexpr (std::is_same_v<T, WorldOffer>)
                        return valid(v);
                    else if constexpr (std::is_same_v<T, Ready>)
                        return v.world && valid_profile(v.profile);
                    else if constexpr (std::is_same_v<T, Joined>)
                        return v.world && v.player && v.object.slot < 65536 && v.object.generation;
                    else if constexpr (std::is_same_v<T, BaselineReady>)
                        return v.world && v.sequence;
                    else
                        return v.world != 0;
                },
                message);
        }
    } // namespace
    bool valid_profile(const JoinProfile& p)
    {
        return !p.name.empty() && p.name.size() <= 100 && p.character < 4 && p.skin < 7 && p.team <= 2 &&
               std::all_of(
                   p.name.begin(), p.name.end(), [](unsigned char c) { return c >= 32 && c != 127; }) &&
               std::all_of(
                   p.attributes.begin(), p.attributes.end(), [](auto v) { return v >= 1 && v <= 5; }) &&
               std::all_of(
                   p.weapons.begin(), p.weapons.end(),
                   [](auto v) { return v == 0 || ((v & 127) >= 1 && (v & 127) <= 17); });
    }
    bool encode_admission(const AdmissionMessage& message, std::vector<std::uint8_t>& bytes)
    {
        if (!valid(message))
            return false;
        Writer writer(admission_packet_limit);
        writer.u32(magic);
        writer.u16(protocol_version);
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, Hello>)
                {
                    writer.u8(std::uint8_t(Kind::Hello));
                    writer.u8(std::uint8_t(v.edition));
                    writer.raw(v.content);
                }
                else if constexpr (std::is_same_v<T, WorldOffer>)
                {
                    writer.u8(std::uint8_t(Kind::WorldOffer));
                    writer.u64(v.world);
                    writer.u8(v.mode);
                    writer.u8(v.capacity);
                    writer.text(v.path, 100);
                }
                else if constexpr (std::is_same_v<T, Ready>)
                {
                    writer.u8(std::uint8_t(Kind::Ready));
                    writer.u64(v.world);
                    writer.text(v.profile.name, 100);
                    writer.u8(v.profile.character);
                    writer.u8(v.profile.skin);
                    writer.raw(v.profile.attributes);
                    writer.raw(v.profile.weapons);
                    writer.u8(v.profile.team);
                }
                else if constexpr (std::is_same_v<T, Joined>)
                {
                    writer.u8(std::uint8_t(Kind::Joined));
                    writer.u64(v.world);
                    writer.u64(v.player);
                    writer.u32(v.object.slot);
                    writer.u32(v.object.generation);
                }
                else if constexpr (std::is_same_v<T, Heartbeat>)
                {
                    writer.u8(std::uint8_t(Kind::Heartbeat));
                    writer.u64(v.world);
                }
                else
                {
                    writer.u8(std::uint8_t(Kind::BaselineReady));
                    writer.u64(v.world);
                    writer.u64(v.sequence);
                }
            },
            message);
        if (!writer.good())
            return false;
        bytes = writer.bytes();
        return true;
    }
    DecodeAdmission decode_admission(std::span<const std::uint8_t> bytes, AdmissionMessage& out)
    {
        if (bytes.size() < 7 || bytes.size() > admission_packet_limit)
            return DecodeAdmission::Malformed;
        Reader reader(bytes);
        if (reader.u32() != magic)
            return DecodeAdmission::Malformed;
        if (reader.u16() != protocol_version)
            return DecodeAdmission::Protocol;
        AdmissionMessage next;
        switch (Kind(reader.u8()))
        {
            case Kind::Hello:
            {
                Hello v;
                v.edition = Edition(reader.u8());
                const auto digest = reader.raw(v.content.size());
                std::copy(digest.begin(), digest.end(), v.content.begin());
                next = v;
                break;
            }
            case Kind::WorldOffer:
            {
                WorldOffer v;
                v.world = reader.u64();
                v.mode = reader.u8();
                v.capacity = reader.u8();
                v.path = reader.text(100);
                next = std::move(v);
                break;
            }
            case Kind::Ready:
            {
                Ready v;
                v.world = reader.u64();
                v.profile.name = reader.text(100);
                v.profile.character = reader.u8();
                v.profile.skin = reader.u8();
                for (auto& field : v.profile.attributes)
                    field = reader.u8();
                for (auto& field : v.profile.weapons)
                    field = reader.u8();
                v.profile.team = reader.u8();
                next = std::move(v);
                break;
            }
            case Kind::Joined:
            {
                Joined v;
                v.world = reader.u64();
                v.player = reader.u64();
                v.object = {reader.u32(), reader.u32()};
                next = v;
                break;
            }
            case Kind::Heartbeat:
                next = Heartbeat{reader.u64()};
                break;
            case Kind::BaselineReady:
                next = BaselineReady{reader.u64(), reader.u64()};
                break;
            default:
                return DecodeAdmission::Malformed;
        }
        if (!reader.done() || !valid(next))
            return DecodeAdmission::Malformed;
        out = std::move(next);
        return DecodeAdmission::Ok;
    }
    const char* reject_text(std::uint32_t reason)
    {
        switch (Reject(reason))
        {
            case Reject::Malformed:
                return "Malformed admission message";
            case Reject::Protocol:
                return "Protocol version mismatch";
            case Reject::Edition:
                return "Retail The Chosen is required";
            case Reject::Content:
                return "Mounted content SHA-256 mismatch";
            case Reject::Order:
                return "Unexpected admission message";
            case Reject::Capacity:
                return "Server is full";
            case Reject::Timeout:
                return "Session timed out";
            case Reject::World:
                return "World generation mismatch";
            case Reject::Mode:
                return "Player admission is unavailable in this mode";
            case Reject::Player:
                return "Player initialization failed";
            case Reject::Shutdown:
                return "Server stopped";
        }
        return "Connection closed";
    }
} // namespace OpenBlood2::Net
