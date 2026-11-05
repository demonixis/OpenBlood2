#pragma once
#include "engine/object_id.h"
#include "engine/sound_types.h"
#include <span>
#include <variant>
#include <vector>
namespace OpenBlood2::Net
{
    inline constexpr std::size_t audio_packet_limit = 256, audio_queue_limit = 512;
    struct SoundStart
    {
        std::uint64_t id = 0;
        ObjectId owner, speaker;
        SoundParameters parameters;
        DVector position{};
        unsigned rate = 0, channels = 0;
        std::uint64_t frames = 0, loops = 0;
        double cursor = 0;
    };
    struct SoundEnd
    {
        std::uint64_t id = 0;
        // Natural server completion retires its handle, but must not truncate
        // a one-shot whose client playback began after network delivery.
        bool completed = false;
    };
    struct AudioEvent
    {
        std::uint64_t world = 0, player = 0, sequence = 0;
        std::variant<SoundStart, SoundEnd> value;
    };
    bool valid_audio(const AudioEvent&);
    bool is_audio_packet(std::span<const std::uint8_t>);
    bool encode_audio(const AudioEvent&, std::vector<std::uint8_t>&);
    enum class DecodeAudio
    {
        Ok,
        Malformed,
        Protocol
    };
    DecodeAudio decode_audio(std::span<const std::uint8_t>, AudioEvent&);
} // namespace OpenBlood2::Net
