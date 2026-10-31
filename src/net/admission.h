#pragma once
#include "engine/object_id.h"
#include "engine/sha256.h"
#include <array>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace OpenBlood2::Net
{
    inline constexpr std::uint16_t protocol_version = 23;
    inline constexpr std::size_t admission_packet_limit = 256;
    enum class Edition : std::uint8_t
    {
        Retail,
        Demo,
        Nightmares
    };
    enum class Reject : std::uint32_t
    {
        Malformed = 16,
        Protocol,
        Edition,
        Content,
        Order,
        Capacity,
        Timeout,
        World,
        Mode,
        Player,
        Shutdown
    };
    struct Hello
    {
        Edition edition = Edition::Retail;
        Sha256Digest content{};
        bool operator==(const Hello&) const = default;
    };
    struct WorldOffer
    {
        std::uint64_t world = 0;
        std::uint8_t mode = 1, capacity = 16;
        std::string path;
        bool operator==(const WorldOffer&) const = default;
    };
    struct JoinProfile
    {
        std::string name = "Player";
        std::uint8_t character = 0, skin = 1;
        std::array<std::uint8_t, 4> attributes{5, 3, 3, 1};
        // Original SendPlayerInitMessage absent-B2C multiplayer fallback.
        std::array<std::uint8_t, 9> weapons{1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::uint8_t team = 0; // Original preference: auto, blue, red (0/1/2).
        bool operator==(const JoinProfile&) const = default;
    };
    struct Ready
    {
        std::uint64_t world = 0;
        JoinProfile profile;
        bool operator==(const Ready&) const = default;
    };
    struct Joined
    {
        std::uint64_t world = 0, player = 0;
        ObjectId object;
        bool operator==(const Joined&) const = default;
    };
    struct Heartbeat
    {
        std::uint64_t world = 0;
        bool operator==(const Heartbeat&) const = default;
    };
    struct BaselineReady
    {
        std::uint64_t world = 0, sequence = 0;
        bool operator==(const BaselineReady&) const = default;
    };
    using AdmissionMessage = std::variant<Hello, WorldOffer, Ready, Joined, Heartbeat, BaselineReady>;
    enum class DecodeAdmission
    {
        Ok,
        Malformed,
        Protocol
    };
    bool valid_profile(const JoinProfile&);
    bool encode_admission(const AdmissionMessage&, std::vector<std::uint8_t>&);
    // Atomic output; validates length/values before the server can allocate a player.
    DecodeAdmission decode_admission(std::span<const std::uint8_t>, AdmissionMessage&);
    const char* reject_text(std::uint32_t reason);
} // namespace OpenBlood2::Net
