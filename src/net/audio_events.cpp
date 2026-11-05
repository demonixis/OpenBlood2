#include "net/audio_events.h"
#include "engine/resources.h"
#include "net/admission.h"
#include "net/codec.h"
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x55413242; // B2AU.
        bool object(ObjectId id)
        {
            return id ? id.slot < 65536 && id.generation : !id.generation;
        }
    } // namespace
    bool valid_audio(const AudioEvent& event)
    {
        if (!event.world || !event.player || !event.sequence)
            return false;
        const auto id = std::visit([](const auto& v) { return v.id; }, event.value);
        if (!id || id >= (std::uint64_t(1) << 63))
            return false;
        const auto* v = std::get_if<SoundStart>(&event.value);
        if (!v)
            return true;
        const auto& p = v->parameters;
        std::string path;
        const auto finite = [](float x) { return std::isfinite(x) && std::abs(x) <= 1e6f; };
        return object(v->owner) && object(v->speaker) && finite(v->position.x) && finite(v->position.y) &&
               finite(v->position.z) && p.resource.size() <= 100 && resource_path(p.resource, path) &&
               p.space >= SoundSpace::Local && p.space <= SoundSpace::Spatial &&
               (p.space != SoundSpace::Local || !v->speaker) && finite(p.inner_radius) && p.inner_radius >= 0 &&
               finite(p.outer_radius) && p.outer_radius > 0 && std::isfinite(p.volume) && p.volume >= 0 &&
               p.volume <= 1 && v->rate >= 8000 && v->rate <= 96000 && v->channels >= 1 && v->channels <= 2 &&
               v->frames && v->frames <= 128u * 1024 * 1024 && std::isfinite(v->cursor) && v->cursor >= 0 &&
               v->cursor < double(v->frames) && (p.loop || !v->loops);
    }
    bool is_audio_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_audio(const AudioEvent& event, std::vector<std::uint8_t>& out)
    {
        if (!valid_audio(event))
            return false;
        Writer w(audio_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u8(std::uint8_t(event.value.index() + 1));
        w.u64(event.world);
        w.u64(event.player);
        w.u64(event.sequence);
        if (const auto* v = std::get_if<SoundStart>(&event.value))
        {
            w.u64(v->id);
            w.u32(v->owner.slot);
            w.u32(v->owner.generation);
            w.u32(v->speaker.slot);
            w.u32(v->speaker.generation);
            const auto& p = v->parameters;
            w.text(p.resource, 100);
            w.u8(std::uint8_t(p.space));
            w.f32(p.inner_radius);
            w.f32(p.outer_radius);
            w.f32(p.volume);
            w.u8(p.loop);
            w.u8(p.streaming);
            w.u8(p.priority);
            w.u8(p.reverb_requested);
            w.f32(v->position.x);
            w.f32(v->position.y);
            w.f32(v->position.z);
            w.u32(v->rate);
            w.u8(std::uint8_t(v->channels));
            w.u64(v->frames);
            w.u64(v->loops);
            w.f64(v->cursor);
        }
        else
        {
            const auto& end = std::get<SoundEnd>(event.value);
            w.u64(end.id);
            w.u8(end.completed);
        }
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeAudio decode_audio(std::span<const std::uint8_t> bytes, AudioEvent& out)
    {
        if (bytes.size() < 7 || bytes.size() > audio_packet_limit)
            return DecodeAudio::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeAudio::Malformed;
        if (r.u16() != protocol_version)
            return DecodeAudio::Protocol;
        const auto kind = r.u8();
        AudioEvent event;
        event.world = r.u64();
        event.player = r.u64();
        event.sequence = r.u64();
        if (kind == 1)
        {
            SoundStart v;
            v.id = r.u64();
            v.owner = {r.u32(), r.u32()};
            v.speaker = {r.u32(), r.u32()};
            auto& p = v.parameters;
            p.resource = r.text(100);
            p.space = SoundSpace(r.u8());
            p.inner_radius = r.f32();
            p.outer_radius = r.f32();
            p.volume = r.f32();
            const auto loop = r.u8(), stream = r.u8(), priority = r.u8(), reverb = r.u8();
            if (loop > 1 || stream > 1 || reverb > 1)
                return DecodeAudio::Malformed;
            p.loop = loop;
            p.streaming = stream;
            p.priority = priority;
            p.reverb_requested = reverb;
            v.position = {r.f32(), r.f32(), r.f32()};
            v.rate = r.u32();
            v.channels = r.u8();
            v.frames = r.u64();
            v.loops = r.u64();
            v.cursor = r.f64();
            event.value = std::move(v);
        }
        else if (kind == 2)
        {
            SoundEnd v;
            v.id = r.u64();
            const auto completed = r.u8();
            if (completed > 1)
                return DecodeAudio::Malformed;
            v.completed = completed;
            event.value = v;
        }
        else
            return DecodeAudio::Malformed;
        if (!r.done() || !valid_audio(event))
            return DecodeAudio::Malformed;
        out = std::move(event);
        return DecodeAudio::Ok;
    }
} // namespace OpenBlood2::Net
