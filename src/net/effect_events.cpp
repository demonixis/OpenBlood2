#include "net/effect_events.h"
#include "net/admission.h"
#include "net/codec.h"
#include <algorithm>
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x46453242; // B2EF.
        bool vector(DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x) <= 1e7f &&
                   std::abs(v.y) <= 1e7f && std::abs(v.z) <= 1e7f;
        }
        float length2(DVector v)
        {
            return v.x * v.x + v.y * v.y + v.z * v.z;
        }
        bool positive(DVector v)
        {
            return vector(v) && v.x >= 0 && v.y >= 0 && v.z >= 0;
        }
        bool color(DVector v, float high)
        {
            return positive(v) && v.x <= high && v.y <= high && v.z <= high;
        }
        bool range(float v, float lo, float hi)
        {
            return std::isfinite(v) && v >= lo && v <= hi;
        }
        bool name(std::string_view s, std::size_t limit)
        {
            return s.size() <= limit && s.find('\0') == s.npos;
        }
        bool resource(const std::optional<std::string>& s)
        {
            return !s || name(*s, 1000);
        }
        bool valid(const ShotgunEffectSettings& s)
        {
            return vector(s.source) && vector(s.destination) && vector(s.forward) && vector(s.normal) &&
                   length2(s.forward) >= 1e-12f && length2(s.normal) >= 1e-12f && color(s.color1, 255) &&
                   color(s.color2, 255) && unsigned(s.surface) <= 255 &&
                   (s.ammo == AMMO_BULLET || s.ammo == AMMO_SHELL || s.ammo == AMMO_BMG) &&
                   (s.clip_polygon.empty() || s.clip_polygon.size() >= 3) && s.clip_polygon.size() <= 512 &&
                   std::all_of(s.clip_polygon.begin(), s.clip_polygon.end(), vector);
        }
        bool valid(const ExplosionSpriteSettings& s)
        {
            return vector(s.position) && vector(s.normal) && positive(s.scale1) && positive(s.scale2) &&
                   range(s.duration, 0, 600) && s.duration > 0 && range(s.alpha, 0, 1) && s.fade <= 5 && s.align <= 2 &&
                   resource(s.resource);
        }
        bool valid(const SmokeImpactSettings& s)
        {
            return vector(s.position) && vector(s.normal) && vector(s.velocity) && vector(s.deceleration) &&
                   vector(s.gravity) && positive(s.scale1) && positive(s.scale2) && color(s.color, 1) &&
                   std::isfinite(s.offset) &&
                   vector({s.position.x + s.normal.x * s.offset, s.position.y + s.normal.y * s.offset,
                           s.position.z + s.normal.z * s.offset}) &&
                   range(s.duration, 0, 600) && s.duration > 0 && range(s.delay, 0, 600) && range(s.alpha, 0, 1) &&
                   resource(s.resource);
        }
        bool valid(const WeaponSpriteSettings& s)
        {
            return (!s.flash || valid(*s.flash)) && (!s.smoke || valid(*s.smoke));
        }
        bool valid(const SparkSettings& s)
        {
            return vector(s.position) && vector(s.direction) && length2(s.direction) >= 1e-12f &&
                   color(s.color1, 255) && color(s.color2, 255) && s.count <= 255 && range(s.duration, 0, 10) &&
                   s.duration > 0 && range(s.emission_radius, 0, 100) && range(s.radius, 0, 10000) && s.radius > 0 &&
                   range(s.gravity, -10000, 10000) && name(s.texture, 1000);
        }
        bool valid(const BloodSpurtRecipe& r)
        {
            const auto& s = r.settings;
            return vector(s.position) && vector(s.min_velocity) && vector(s.max_velocity) && range(s.lifetime, 0, 10) &&
                   s.lifetime > 0 && range(s.fade_time, 0, s.lifetime) && s.fade_time < s.lifetime && s.texture <= 3;
        }
        bool valid(const BloodSplatSettings& s)
        {
            const auto q = s.rotation;
            const float length = length2(q.m_Vec) + q.m_Spin * q.m_Spin;
            return vector(s.position) && std::isfinite(length) && std::abs(length - 1) <= .001f &&
                   range(s.scale, 0, 1000) && range(s.grow_scale, 0, .1f) && resource(s.resource);
        }
        void write(Writer& w, DVector v)
        {
            w.f32(v.x);
            w.f32(v.y);
            w.f32(v.z);
        }
        DVector read_vector(Reader& r)
        {
            return {r.f32(), r.f32(), r.f32()};
        }
        void write(Writer& w, const std::optional<std::string>& s)
        {
            w.u8(s.has_value());
            if (s)
                w.text(*s, 1000);
        }
        bool read(Reader& r, std::optional<std::string>& s)
        {
            const auto present = r.u8();
            if (present > 1)
                return false;
            if (present)
                s = r.text(1000);
            return r.good();
        }
        void write(Writer& w, const ShotgunEffectSettings& s)
        {
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
        }
        bool read(Reader& r, ShotgunEffectSettings& s)
        {
            s.source = read_vector(r);
            s.destination = read_vector(r);
            s.forward = read_vector(r);
            s.normal = read_vector(r);
            s.color1 = read_vector(r);
            s.color2 = read_vector(r);
            s.surface = SurfaceType(r.u8());
            const auto eject = r.u8(), left = r.u8();
            s.ammo = r.u8();
            const auto count = r.u16();
            if (eject > 1 || left > 1 || count > 512 || std::size_t(count) * 12 > r.remaining())
                return false;
            s.eject = eject;
            s.left = left;
            s.clip_polygon.reserve(count);
            for (unsigned i = 0; i < count; ++i)
                s.clip_polygon.push_back(read_vector(r));
            return r.good();
        }
        void write(Writer& w, const ExplosionSpriteSettings& s)
        {
            write(w, s.position);
            write(w, s.normal);
            write(w, s.scale1);
            write(w, s.scale2);
            w.f32(s.duration);
            w.f32(s.alpha);
            w.u8(s.waveform);
            w.u8(s.fade);
            w.u8(s.align);
            write(w, s.resource);
        }
        bool read(Reader& r, ExplosionSpriteSettings& s)
        {
            s.position = read_vector(r);
            s.normal = read_vector(r);
            s.scale1 = read_vector(r);
            s.scale2 = read_vector(r);
            s.duration = r.f32();
            s.alpha = r.f32();
            s.waveform = r.u8();
            s.fade = r.u8();
            s.align = r.u8();
            return read(r, s.resource);
        }
        void write(Writer& w, const SmokeImpactSettings& s)
        {
            write(w, s.position);
            write(w, s.normal);
            write(w, s.velocity);
            write(w, s.deceleration);
            write(w, s.gravity);
            write(w, s.scale1);
            write(w, s.scale2);
            write(w, s.color);
            w.f32(s.offset);
            w.f32(s.duration);
            w.f32(s.delay);
            w.f32(s.alpha);
            w.u8(s.fade | (s.rotate << 1) | (s.move << 2) | (s.update_scale << 3));
            write(w, s.resource);
        }
        bool read(Reader& r, SmokeImpactSettings& s)
        {
            s.position = read_vector(r);
            s.normal = read_vector(r);
            s.velocity = read_vector(r);
            s.deceleration = read_vector(r);
            s.gravity = read_vector(r);
            s.scale1 = read_vector(r);
            s.scale2 = read_vector(r);
            s.color = read_vector(r);
            s.offset = r.f32();
            s.duration = r.f32();
            s.delay = r.f32();
            s.alpha = r.f32();
            const auto flags = r.u8();
            if (flags > 15)
                return false;
            s.fade = flags & 1;
            s.rotate = flags & 2;
            s.move = flags & 4;
            s.update_scale = flags & 8;
            return read(r, s.resource);
        }
        void write(Writer& w, const WeaponSpriteSettings& s)
        {
            w.u8(s.flash.has_value() | (s.smoke.has_value() << 1) | (s.bubbles_unavailable << 2));
            if (s.flash)
                write(w, *s.flash);
            if (s.smoke)
                write(w, *s.smoke);
        }
        bool read(Reader& r, WeaponSpriteSettings& s)
        {
            const auto flags = r.u8();
            if (flags > 7)
                return false;
            s.bubbles_unavailable = flags & 4;
            if ((flags & 1) && !read(r, s.flash.emplace()))
                return false;
            if ((flags & 2) && !read(r, s.smoke.emplace()))
                return false;
            return r.good();
        }
        void write(Writer& w, const SparkSettings& s)
        {
            write(w, s.position);
            write(w, s.direction);
            write(w, s.color1);
            write(w, s.color2);
            w.u8(std::uint8_t(s.count));
            w.f32(s.duration);
            w.f32(s.emission_radius);
            w.f32(s.radius);
            w.f32(s.gravity);
            w.text(s.texture, 1000);
        }
        bool read(Reader& r, SparkSettings& s)
        {
            s.position = read_vector(r);
            s.direction = read_vector(r);
            s.color1 = read_vector(r);
            s.color2 = read_vector(r);
            s.count = r.u8();
            s.duration = r.f32();
            s.emission_radius = r.f32();
            s.radius = r.f32();
            s.gravity = r.f32();
            s.texture = r.text(1000);
            return r.good();
        }
        void write(Writer& w, const BloodSpurtRecipe& b)
        {
            const auto& s = b.settings;
            write(w, s.position);
            write(w, s.min_velocity);
            write(w, s.max_velocity);
            w.f32(s.lifetime);
            w.f32(s.fade_time);
            w.u8(std::uint8_t(s.texture));
            w.u8(b.immediate);
        }
        bool read(Reader& r, BloodSpurtRecipe& b)
        {
            auto& s = b.settings;
            s.position = read_vector(r);
            s.min_velocity = read_vector(r);
            s.max_velocity = read_vector(r);
            s.lifetime = r.f32();
            s.fade_time = r.f32();
            s.texture = r.u8();
            const auto immediate = r.u8();
            if (immediate > 1)
                return false;
            b.immediate = immediate;
            return r.good();
        }
        void write(Writer& w, const BloodSplatSettings& s)
        {
            write(w, s.position);
            write(w, s.rotation.m_Vec);
            w.f32(s.rotation.m_Spin);
            w.f32(s.scale);
            w.f32(s.grow_scale);
            write(w, s.resource);
        }
        bool read(Reader& r, BloodSplatSettings& s)
        {
            s.position = read_vector(r);
            s.rotation.m_Vec = read_vector(r);
            s.rotation.m_Spin = r.f32();
            s.scale = r.f32();
            s.grow_scale = r.f32();
            return read(r, s.resource);
        }
    } // namespace
    bool valid_effect(const EffectEvent& e)
    {
        const bool source = e.source ? e.source.slot < 65536 && e.source.generation
                                     : e.source == ObjectId{} && (std::holds_alternative<BloodSpurtRecipe>(e.recipe) ||
                                                                  std::holds_alternative<BloodSplatSettings>(e.recipe));
        return e.world && e.player && e.sequence && source && name(e.name, 100) &&
               std::visit([](const auto& s) { return valid(s); }, e.recipe);
    }
    bool is_effect_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_effect(const EffectEvent& e, std::vector<std::uint8_t>& out)
    {
        if (!valid_effect(e))
            return false;
        Writer w(effect_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u8(std::uint8_t(e.recipe.index() + 1));
        w.u64(e.world);
        w.u64(e.player);
        w.u64(e.sequence);
        w.u32(e.source.slot);
        w.u32(e.source.generation);
        w.text(e.name, 100);
        std::visit([&](const auto& s) { write(w, s); }, e.recipe);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeEffect decode_effect(std::span<const std::uint8_t> bytes, EffectEvent& out)
    {
        if (bytes.size() < 7 || bytes.size() > effect_packet_limit)
            return DecodeEffect::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeEffect::Malformed;
        if (r.u16() != protocol_version)
            return DecodeEffect::Protocol;
        const auto kind = r.u8();
        EffectEvent e;
        e.world = r.u64();
        e.player = r.u64();
        e.sequence = r.u64();
        e.source = {r.u32(), r.u32()};
        e.name = r.text(100);
        bool ok = false;
        switch (kind)
        {
            case 1:
                ok = read(r, e.recipe.emplace<ShotgunEffectSettings>());
                break;
            case 2:
                ok = read(r, e.recipe.emplace<WeaponSpriteSettings>());
                break;
            case 3:
                ok = read(r, e.recipe.emplace<SparkSettings>());
                break;
            case 4:
                ok = read(r, e.recipe.emplace<BloodSpurtRecipe>());
                break;
            case 5:
                ok = read(r, e.recipe.emplace<BloodSplatSettings>());
                break;
            default:
                return DecodeEffect::Malformed;
        }
        if (!ok || !r.done() || !valid_effect(e))
            return DecodeEffect::Malformed;
        out = std::move(e);
        return DecodeEffect::Ok;
    }
} // namespace OpenBlood2::Net
