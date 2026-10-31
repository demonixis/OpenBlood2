#include "net/ballistic_events.h"
#include "net/admission.h"
#include "net/codec.h"
#include <algorithm>
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x46423242; // B2BF.
        bool vector(DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) <= 1e7f &&
                   std::abs(v.y) <= 1e7f && std::abs(v.z) <= 1e7f;
        }
        bool color(DVector v)
        {
            return vector(v) && v.x >= 0 && v.x <= 255 && v.y >= 0 && v.y <= 255 && v.z >= 0 && v.z <= 255;
        }
        void write(Writer& w, DVector v)
        {
            w.f32(v.x);
            w.f32(v.y);
            w.f32(v.z);
        }
        DVector read(Reader& r)
        {
            return {r.f32(), r.f32(), r.f32()};
        }
    } // namespace
    bool valid_ballistic(const BallisticEvent& e)
    {
        const auto& s = e.settings;
        return e.world && e.player && e.sequence && e.source && e.source.slot < 65536 && e.source.generation &&
               e.name.size() <= 100 && e.name.find('\0') == e.name.npos && vector(s.source) && vector(s.destination) &&
               vector(s.forward) && vector(s.normal) &&
               (s.forward.x * s.forward.x + s.forward.y * s.forward.y + s.forward.z * s.forward.z) >= 1e-12f &&
               (s.normal.x * s.normal.x + s.normal.y * s.normal.y + s.normal.z * s.normal.z) >= 1e-12f &&
               color(s.color1) && color(s.color2) && unsigned(s.surface) <= 255 &&
               (s.ammo == AMMO_BULLET || s.ammo == AMMO_SHELL || s.ammo == AMMO_BMG) &&
               (s.clip_polygon.empty() || s.clip_polygon.size() >= 3) && s.clip_polygon.size() <= 512 &&
               std::all_of(s.clip_polygon.begin(), s.clip_polygon.end(), vector);
    }
    bool is_ballistic_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_ballistic(const BallisticEvent& e, std::vector<std::uint8_t>& out)
    {
        if (!valid_ballistic(e))
            return false;
        Writer w(ballistic_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u8(1);
        w.u64(e.world);
        w.u64(e.player);
        w.u64(e.sequence);
        w.u32(e.source.slot);
        w.u32(e.source.generation);
        w.text(e.name, 100);
        const auto& s = e.settings;
        write(w, s.source);
        write(w, s.destination);
        write(w, s.forward);
        write(w, s.normal);
        write(w, s.color1);
        write(w, s.color2);
        w.u8(std::uint8_t(s.surface));
        w.u8(s.eject);
        w.u8(s.left);
        w.u8(s.ammo);
        w.u16(std::uint16_t(s.clip_polygon.size()));
        for (auto p : s.clip_polygon)
            write(w, p);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeBallistic decode_ballistic(std::span<const std::uint8_t> bytes, BallisticEvent& out)
    {
        if (bytes.size() < 7 || bytes.size() > ballistic_packet_limit)
            return DecodeBallistic::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeBallistic::Malformed;
        if (r.u16() != protocol_version)
            return DecodeBallistic::Protocol;
        if (r.u8() != 1)
            return DecodeBallistic::Malformed;
        BallisticEvent e;
        e.world = r.u64();
        e.player = r.u64();
        e.sequence = r.u64();
        e.source = {r.u32(), r.u32()};
        e.name = r.text(100);
        auto& s = e.settings;
        s.source = read(r);
        s.destination = read(r);
        s.forward = read(r);
        s.normal = read(r);
        s.color1 = read(r);
        s.color2 = read(r);
        s.surface = SurfaceType(r.u8());
        const auto eject = r.u8(), left = r.u8();
        s.ammo = r.u8();
        const auto count = r.u16();
        if (eject > 1 || left > 1 || count > 512 || std::size_t(count) * 12 > r.remaining())
            return DecodeBallistic::Malformed;
        s.eject = eject;
        s.left = left;
        s.clip_polygon.reserve(count);
        for (unsigned i = 0; i < count; ++i)
            s.clip_polygon.push_back(read(r));
        if (!r.done() || !valid_ballistic(e))
            return DecodeBallistic::Malformed;
        out = std::move(e);
        return DecodeBallistic::Ok;
    }
} // namespace OpenBlood2::Net
