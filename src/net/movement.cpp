#include "net/movement.h"
#include "net/admission.h"
#include "net/codec.h"
#include <type_traits>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x504d3242; // B2MP, distinct from admission and object poses.
        bool bounded(float value, float limit = 1e7f)
        {
            return std::isfinite(value) && std::abs(value) <= limit;
        }
        bool vector(DVector v)
        {
            return bounded(v.x) && bounded(v.y) && bounded(v.z);
        }
        bool dimensions(DVector v)
        {
            return vector(v) && v.x > 0 && v.y > 0 && v.z > 0;
        }
        void write_vector(Writer& w, DVector v)
        {
            w.f32(v.x);
            w.f32(v.y);
            w.f32(v.z);
        }
        DVector read_vector(Reader& r)
        {
            return {r.f32(), r.f32(), r.f32()};
        }
        bool move(const PlayerMove& m)
        {
            return vector(m.position) && vector(m.velocity) && vector(m.acceleration);
        }
        void write_move(Writer& w, const PlayerMove& m)
        {
            w.u8(m.move_code);
            write_vector(w, m.position);
            write_vector(w, m.velocity);
            write_vector(w, m.acceleration);
            w.u8(m.on_ground);
            w.u8(m.surface);
        }
        bool read_move(Reader& r, PlayerMove& m)
        {
            m.move_code = r.u8();
            m.position = read_vector(r);
            m.velocity = read_vector(r);
            m.acceleration = read_vector(r);
            const auto ground = r.u8();
            m.on_ground = ground != 0;
            m.surface = r.u8();
            return r.good() && ground <= 1;
        }
        std::uint16_t flags(const PlayerControls& c)
        {
            const auto& m = c.movement;
            return std::uint16_t(m.forward | m.backward << 1 | m.left << 2 | m.right << 3 | m.run << 4 | m.jump << 5 |
                                 m.crouch << 6 | m.mouse_move << 7 | c.firing << 8);
        }
        void read_flags(std::uint16_t f, PlayerControls& c)
        {
            auto& m = c.movement;
            m.forward = f & 1;
            m.backward = f & 2;
            m.left = f & 4;
            m.right = f & 8;
            m.run = f & 16;
            m.jump = f & 32;
            m.crouch = f & 64;
            m.mouse_move = f & 128;
            c.firing = f & 256;
        }
    } // namespace
    bool valid_movement(const MovementMessage& message)
    {
        return std::visit(
            [](const auto& v)
            {
                const auto& o = v.owner;
                if (!o.world || !o.player || !o.object || !o.object.generation || o.object.slot >= 65536 ||
                    !o.force_revision || !v.sequence)
                    return false;
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, MovementSetup>)
                    return v.character < 4 && bounded(v.eye_level, 1000) && v.eye_level >= 0 && vector(v.position) &&
                           dimensions(v.dimensions) && vector(v.gravity) && bounded(v.move_speed, 10000) &&
                           v.move_speed >= 0 && bounded(v.jump_speed, 10000) && v.jump_speed >= 0;
                else if constexpr (std::is_same_v<T, MovementReport>)
                {
                    if (!move(v.move) || !bounded(v.controls.yaw, 1e6f) || !bounded(v.pitch, 1e6f) ||
                        !bounded(v.controls.movement.mouse_forward) || !bounded(v.controls.movement.mouse_strafe) ||
                        !vector(v.origin) || v.steps.size() > movement_step_limit)
                        return false;
                    for (const auto& s : v.steps)
                        if (!vector(s.offset) || !vector(s.velocity) || !vector(s.acceleration) ||
                            !std::isfinite(s.seconds) || s.seconds <= 0 || s.seconds > movement_step_seconds)
                            return false;
                    return true;
                }
                else
                    return v.setup_sequence && move(v.accepted) && dimensions(v.dimensions);
            },
            message);
    }
    bool is_movement_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_movement(const MovementMessage& message, std::vector<std::uint8_t>& out)
    {
        if (!valid_movement(message))
            return false;
        Writer w(movement_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                w.u8(std::is_same_v<T, MovementSetup> ? 1 : std::is_same_v<T, MovementReport> ? 2 : 3);
                w.u64(v.owner.world);
                w.u64(v.owner.player);
                w.u32(v.owner.object.slot);
                w.u32(v.owner.object.generation);
                w.u64(v.owner.force_revision);
                w.u64(v.sequence);
                if constexpr (std::is_same_v<T, MovementSetup>)
                {
                    w.u8(v.code);
                    w.u8(v.character);
                    w.u8(v.crouched | v.can_move << 1);
                    w.f32(v.eye_level);
                    write_vector(w, v.position);
                    write_vector(w, v.dimensions);
                    write_vector(w, v.gravity);
                    w.f32(v.move_speed);
                    w.f32(v.jump_speed);
                }
                else if constexpr (std::is_same_v<T, MovementReport>)
                {
                    write_move(w, v.move);
                    w.u16(flags(v.controls));
                    w.f32(v.controls.movement.mouse_forward);
                    w.f32(v.controls.movement.mouse_strafe);
                    w.f32(v.controls.yaw);
                    w.f32(v.pitch);
                    write_vector(w, v.origin);
                    w.u8(std::uint8_t(v.steps.size()));
                    for (const auto& s : v.steps)
                    {
                        write_vector(w, s.offset);
                        write_vector(w, s.velocity);
                        write_vector(w, s.acceleration);
                        w.f32(s.seconds);
                        w.u8(s.on_ground);
                        w.u8(s.surface);
                    }
                }
                else
                {
                    w.u64(v.setup_sequence);
                    write_move(w, v.accepted);
                    write_vector(w, v.dimensions);
                }
            },
            message);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeMovement decode_movement(std::span<const std::uint8_t> bytes, MovementMessage& out)
    {
        if (bytes.size() < 7 || bytes.size() > movement_packet_limit)
            return DecodeMovement::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeMovement::Malformed;
        if (r.u16() != protocol_version)
            return DecodeMovement::Protocol;
        const auto kind = r.u8();
        MovementIdentity owner;
        owner.world = r.u64();
        owner.player = r.u64();
        owner.object = {r.u32(), r.u32()};
        owner.force_revision = r.u64();
        const auto sequence = r.u64();
        MovementMessage next;
        if (kind == 1)
        {
            MovementSetup v;
            v.owner = owner;
            v.sequence = sequence;
            v.code = r.u8();
            v.character = r.u8();
            const auto f = r.u8();
            if (f > 3)
                return DecodeMovement::Malformed;
            v.crouched = f & 1;
            v.can_move = f & 2;
            v.eye_level = r.f32();
            v.position = read_vector(r);
            v.dimensions = read_vector(r);
            v.gravity = read_vector(r);
            v.move_speed = r.f32();
            v.jump_speed = r.f32();
            next = v;
        }
        else if (kind == 2)
        {
            MovementReport v;
            v.owner = owner;
            v.sequence = sequence;
            if (!read_move(r, v.move))
                return DecodeMovement::Malformed;
            const auto f = r.u16();
            if (f > 511)
                return DecodeMovement::Malformed;
            read_flags(f, v.controls);
            v.controls.movement.mouse_forward = r.f32();
            v.controls.movement.mouse_strafe = r.f32();
            v.controls.yaw = r.f32();
            v.pitch = r.f32();
            v.origin = read_vector(r);
            const auto count = r.u8();
            if (!r.good() || count > movement_step_limit)
                return DecodeMovement::Malformed;
            v.steps.resize(count);
            for (auto& s : v.steps)
            {
                s.offset = read_vector(r);
                s.velocity = read_vector(r);
                s.acceleration = read_vector(r);
                s.seconds = r.f32();
                const auto ground = r.u8();
                if (ground > 1)
                    return DecodeMovement::Malformed;
                s.on_ground = ground != 0;
                s.surface = r.u8();
            }
            next = std::move(v);
        }
        else if (kind == 3)
        {
            MovementAck v;
            v.owner = owner;
            v.sequence = sequence;
            v.setup_sequence = r.u64();
            if (!read_move(r, v.accepted))
                return DecodeMovement::Malformed;
            v.dimensions = read_vector(r);
            next = v;
        }
        else
            return DecodeMovement::Malformed;
        if (!r.done() || !valid_movement(next))
            return DecodeMovement::Malformed;
        out = std::move(next);
        return DecodeMovement::Ok;
    }
} // namespace OpenBlood2::Net
