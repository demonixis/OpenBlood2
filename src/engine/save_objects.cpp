#include "engine/save_objects.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace OpenBlood2
{
    namespace
    {
        namespace R = save_records;
        bool fail(SaveError& error, SaveFault fault, std::string text)
        {
            if (error.fault == SaveFault::None)
                error = {fault, std::move(text)};
            return false;
        }
        bool same(float a, float b)
        {
            return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
        }
        bool same(double a, double b)
        {
            return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
        }
        bool same(DVector a, DVector b)
        {
            return same(a.x, b.x) && same(a.y, b.y) && same(a.z, b.z);
        }
        bool same(DRotation a, DRotation b)
        {
            return same(a.m_Vec, b.m_Vec) && same(a.m_Spin, b.m_Spin);
        }
        bool finite(DVector v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }
        bool unit(DRotation q)
        {
            const double norm = double(q.m_Vec.x) * q.m_Vec.x + double(q.m_Vec.y) * q.m_Vec.y +
                                double(q.m_Vec.z) * q.m_Vec.z + double(q.m_Spin) * q.m_Spin;
            return finite(q.m_Vec) && std::isfinite(q.m_Spin) && std::abs(norm - 1) <= .001;
        }
        bool text(const std::string& value, std::size_t max)
        {
            return value.size() <= max && value.find('\0') == std::string::npos;
        }
        bool channel(float v)
        {
            return std::isfinite(v) && v >= 0 && v <= 1;
        }
        bool delay(double v)
        {
            return std::isfinite(v) && v >= 0 && v <= R::max_delay;
        }
        bool known_type(std::uint16_t type)
        {
            return type == OT_NORMAL || type == OT_MODEL || type == OT_WORLDMODEL || type == OT_SPRITE ||
                   type == OT_LIGHT || type == OT_CONTAINER;
        }

        class Out
        {
          public:
            explicit Out(std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
            void u8(std::uint8_t v)
            {
                bytes_.push_back(v);
            }
            void flag(bool v)
            {
                u8(v ? 1 : 0);
            }
            void u16(std::uint16_t v)
            {
                u8(std::uint8_t(v));
                u8(std::uint8_t(v >> 8));
            }
            void u32(std::uint32_t v)
            {
                for (unsigned shift = 0; shift < 32; shift += 8)
                    u8(std::uint8_t(v >> shift));
            }
            void u64(std::uint64_t v)
            {
                for (unsigned shift = 0; shift < 64; shift += 8)
                    u8(std::uint8_t(v >> shift));
            }
            void f32(float v)
            {
                u32(std::bit_cast<std::uint32_t>(v));
            }
            void f64(double v)
            {
                u64(std::bit_cast<std::uint64_t>(v));
            }
            void vector(DVector v)
            {
                f32(v.x);
                f32(v.y);
                f32(v.z);
            }
            void rotation(DRotation q)
            {
                vector(q.m_Vec);
                f32(q.m_Spin);
            }
            void bytes(std::span<const std::uint8_t> v)
            {
                u32(std::uint32_t(v.size()));
                bytes_.insert(bytes_.end(), v.begin(), v.end());
            }
            void text(const std::string& v)
            {
                bytes({reinterpret_cast<const std::uint8_t*>(v.data()), v.size()});
            }

          private:
            std::vector<std::uint8_t>& bytes_;
        };

        // The first failure latches; later reads return zero values and change nothing.
        class In
        {
          public:
            In(std::span<const std::uint8_t> bytes, const char* section, SaveError& error)
                : bytes_(bytes), section_(section), error_(error)
            {
            }
            bool good() const
            {
                return good_;
            }
            bool bad(SaveFault fault, const std::string& what)
            {
                if (good_)
                    fail(error_, fault, std::string(section_) + ": " + what);
                good_ = false;
                return false;
            }
            std::span<const std::uint8_t> take(std::size_t n)
            {
                if (!good_)
                    return {};
                if (n > bytes_.size() - pos_)
                {
                    bad(SaveFault::Truncated, "payload ends early");
                    return {};
                }
                auto out = bytes_.subspan(pos_, n);
                pos_ += n;
                return out;
            }
            std::uint64_t number(unsigned size)
            {
                auto b = take(size);
                std::uint64_t v = 0;
                for (unsigned i = 0; i < b.size(); ++i)
                    v |= std::uint64_t(b[i]) << (8 * i);
                return v;
            }
            std::uint8_t u8()
            {
                return std::uint8_t(number(1));
            }
            bool flag(const char* what)
            {
                const auto v = u8();
                if (good_ && v > 1)
                    bad(SaveFault::Format, std::string(what) + " is not 0 or 1");
                return v == 1;
            }
            std::uint16_t u16()
            {
                return std::uint16_t(number(2));
            }
            std::uint32_t u32()
            {
                return std::uint32_t(number(4));
            }
            float f32()
            {
                return std::bit_cast<float>(u32());
            }
            double f64()
            {
                return std::bit_cast<double>(number(8));
            }
            DVector vector()
            {
                DVector v;
                v.x = f32();
                v.y = f32();
                v.z = f32();
                return v;
            }
            DRotation rotation()
            {
                DRotation q;
                q.m_Vec = vector();
                q.m_Spin = f32();
                return q;
            }
            // Lengths are checked against the field bound and the remaining bytes before allocating.
            std::span<const std::uint8_t> bytes(std::size_t max, const char* what)
            {
                const auto n = u32();
                if (good_ && n > max)
                {
                    bad(SaveFault::Bound, std::string(what) + " exceeds " + std::to_string(max) + " bytes");
                    return {};
                }
                return take(n);
            }
            std::string text(std::size_t max, const char* what)
            {
                const auto b = bytes(max, what);
                std::string out(b.begin(), b.end());
                if (good_ && out.find('\0') != std::string::npos)
                {
                    bad(SaveFault::Format, std::string(what) + " contains NUL");
                    return {};
                }
                return out;
            }
            std::uint32_t count(std::size_t max, const char* what)
            {
                const auto n = u32();
                if (good_ && n > max)
                    bad(SaveFault::Bound,
                        std::string(what) + " count " + std::to_string(n) + " exceeds " + std::to_string(max));
                return good_ ? n : 0;
            }
            bool finish()
            {
                if (good_ && pos_ != bytes_.size())
                    bad(SaveFault::Format, std::to_string(bytes_.size() - pos_) + " unread payload bytes");
                return good_;
            }

          private:
            std::span<const std::uint8_t> bytes_;
            std::size_t pos_ = 0;
            const char* section_;
            SaveError& error_;
            bool good_ = true;
        };

        void write_record(Out& out, const SaveEngineRecord& r)
        {
            out.text(r.class_name);
            out.u16(r.type);
            out.u32(r.flags);
            out.u32(r.user_flags);
            out.vector(r.position);
            out.rotation(r.rotation);
            out.vector(r.scale);
            out.text(r.name);
            out.text(r.filename);
            out.text(r.skin);
            out.u16(r.container_code);
            out.u32(r.world_model_index);
            out.rotation(r.world_model_basis);
            out.vector(r.color);
            out.f32(r.alpha);
            out.vector(r.dimensions);
            out.vector(r.velocity);
            out.vector(r.acceleration);
            out.u8(r.body);
            out.f32(r.body_properties.mass);
            out.f32(r.body_properties.friction);
            out.flag(r.body_properties.pushable);
            out.flag(r.body_properties.rotating_debris);
            out.flag(r.body_properties.polygon_bounds);
            out.u8(r.blocking_priority);
            out.u32(r.activity);
            out.flag(r.auto_deactivate);
            out.f64(r.deactivation_period);
            out.f64(r.deactivation_remaining);
            out.flag(r.scheduled);
            out.f64(r.update_remaining);
            out.u32(r.animation);
            out.f64(r.animation_time);
            out.flag(r.animation_looping);
            out.flag(r.animation_done);
            out.flag(r.animation_first);
            out.bytes(r.hidden_nodes);
            out.f64(r.sprite_milliseconds);
            out.u32(r.sprite_frame);
            out.flag(r.sprite_has_frame);
            out.flag(r.in_sky);
            out.u32(r.sky_slot);
            out.flag(r.local_client);
            out.bytes(r.payload);
        }
        void read_record(In& in, SaveEngineRecord& r)
        {
            r.class_name = in.text(R::max_class_name, "class name");
            r.type = in.u16();
            r.flags = in.u32();
            r.user_flags = in.u32();
            r.position = in.vector();
            r.rotation = in.rotation();
            r.scale = in.vector();
            r.name = in.text(R::max_string, "object name");
            r.filename = in.text(R::max_string, "filename");
            r.skin = in.text(R::max_string, "skin");
            r.container_code = in.u16();
            r.world_model_index = in.u32();
            r.world_model_basis = in.rotation();
            r.color = in.vector();
            r.alpha = in.f32();
            r.dimensions = in.vector();
            r.velocity = in.vector();
            r.acceleration = in.vector();
            r.body = in.u8();
            r.body_properties.mass = in.f32();
            r.body_properties.friction = in.f32();
            r.body_properties.pushable = in.flag("pushable");
            r.body_properties.rotating_debris = in.flag("rotating debris");
            r.body_properties.polygon_bounds = in.flag("polygon bounds");
            r.blocking_priority = in.u8();
            r.activity = in.u32();
            r.auto_deactivate = in.flag("automatic deactivation");
            r.deactivation_period = in.f64();
            r.deactivation_remaining = in.f64();
            r.scheduled = in.flag("scheduled");
            r.update_remaining = in.f64();
            r.animation = in.u32();
            r.animation_time = in.f64();
            r.animation_looping = in.flag("animation looping");
            r.animation_done = in.flag("animation done");
            r.animation_first = in.flag("animation first");
            const auto hidden = in.bytes(R::max_hidden_nodes, "hidden nodes");
            r.hidden_nodes.assign(hidden.begin(), hidden.end());
            r.sprite_milliseconds = in.f64();
            r.sprite_frame = in.u32();
            r.sprite_has_frame = in.flag("sprite frame");
            r.in_sky = in.flag("sky membership");
            r.sky_slot = in.u32();
            r.local_client = in.flag("local client");
            const auto payload = in.bytes(R::max_payload, "game payload");
            r.payload.assign(payload.begin(), payload.end());
        }
        bool valid_attachment(const SaveAttachment& a, std::size_t records, std::string& error)
        {
            if (a.parent >= records || a.child >= records)
                error = "index outside the record list";
            else if (a.parent == a.child)
                error = "an object attached to itself";
            else if (!text(a.node, R::max_string))
                error = "invalid node name";
            else if (!finite(a.offset) || !unit(a.rotation))
                error = "invalid offset or rotation";
            else
                return true;
            return false;
        }
    } // namespace

    bool SaveEngineRecord::operator==(const SaveEngineRecord& o) const
    {
        return class_name == o.class_name && type == o.type && flags == o.flags && user_flags == o.user_flags &&
               same(position, o.position) && same(scale, o.scale) && same(rotation, o.rotation) && name == o.name &&
               filename == o.filename && skin == o.skin && container_code == o.container_code &&
               world_model_index == o.world_model_index && same(world_model_basis, o.world_model_basis) &&
               same(color, o.color) && same(alpha, o.alpha) && same(dimensions, o.dimensions) &&
               same(velocity, o.velocity) && same(acceleration, o.acceleration) && body == o.body &&
               same(body_properties.mass, o.body_properties.mass) &&
               same(body_properties.friction, o.body_properties.friction) &&
               body_properties.pushable == o.body_properties.pushable &&
               body_properties.rotating_debris == o.body_properties.rotating_debris &&
               body_properties.polygon_bounds == o.body_properties.polygon_bounds &&
               blocking_priority == o.blocking_priority && activity == o.activity &&
               auto_deactivate == o.auto_deactivate && same(deactivation_period, o.deactivation_period) &&
               same(deactivation_remaining, o.deactivation_remaining) && scheduled == o.scheduled &&
               same(update_remaining, o.update_remaining) && animation == o.animation &&
               same(animation_time, o.animation_time) && animation_looping == o.animation_looping &&
               animation_done == o.animation_done && animation_first == o.animation_first &&
               hidden_nodes == o.hidden_nodes && same(sprite_milliseconds, o.sprite_milliseconds) &&
               sprite_frame == o.sprite_frame && sprite_has_frame == o.sprite_has_frame && in_sky == o.in_sky &&
               sky_slot == o.sky_slot && local_client == o.local_client && payload == o.payload;
    }
    bool SaveClock::operator==(const SaveClock& o) const
    {
        return same(time, o.time) && same(frame_time, o.frame_time) && same(world_origin, o.world_origin) &&
               same(global_force, o.global_force) && generator == o.generator;
    }
    bool SaveAttachment::operator==(const SaveAttachment& o) const
    {
        return parent == o.parent && child == o.child && node == o.node && same(offset, o.offset) &&
               same(rotation, o.rotation);
    }

    bool valid_save_record(const SaveEngineRecord& r, std::string& error)
    {
        auto bad = [&](const char* why)
        {
            error = why;
            return false;
        };
        error.clear();
        if (r.class_name.empty() || !text(r.class_name, R::max_class_name))
            return bad("invalid class name");
        if (!known_type(r.type))
            return bad("unknown object type");
        if (!finite(r.position) || !finite(r.scale))
            return bad("nonfinite position or scale");
        if (!unit(r.rotation))
            return bad("rotation is not a unit quaternion");
        if (!text(r.name, R::max_string) || !text(r.filename, R::max_string) || !text(r.skin, R::max_string))
            return bad("invalid name, filename or skin");
        const bool bsp = r.type == OT_WORLDMODEL || r.type == OT_CONTAINER;
        if (bsp && (r.world_model_index == R::no_index || r.filename.empty() || !unit(r.world_model_basis)))
            return bad("world or container model without a BSP binding or basis");
        if (!bsp && (r.world_model_index != R::no_index || !same(r.world_model_basis, DRotation{})))
            return bad("BSP binding on an object that is not a world or container model");
        if (!channel(r.color.x) || !channel(r.color.y) || !channel(r.color.z) || !channel(r.alpha))
            return bad("color or alpha outside 0..1");
        if (!finite(r.dimensions) || r.dimensions.x < 0 || r.dimensions.y < 0 || r.dimensions.z < 0)
            return bad("invalid dimensions");
        if (!finite(r.velocity) || !finite(r.acceleration))
            return bad("nonfinite velocity or acceleration");
        if ((r.body & ~(R::world_body | R::character_body | R::model_body)) || std::popcount(r.body) > 1)
            return bad("invalid body kind");
        if (!std::isfinite(r.body_properties.mass) || r.body_properties.mass < 0 ||
            !std::isfinite(r.body_properties.friction) || r.body_properties.friction < 0)
            return bad("invalid mass or friction");
        if (r.activity > OBJSTATE_AUTODEACTIVATE_NOW)
            return bad("unknown activity state");
        if (!std::isfinite(r.deactivation_period) || r.deactivation_period < 0 || r.deactivation_period > 1e6 ||
            !delay(r.deactivation_remaining))
            return bad("invalid deactivation period or remaining time");
        if ((r.auto_deactivate || r.activity == OBJSTATE_AUTODEACTIVATE_NOW) && r.deactivation_period <= 0)
            return bad("automatic deactivation without a positive period");
        if (!delay(r.update_remaining) || (!r.scheduled && r.update_remaining != 0))
            return bad("invalid next-update delay");
        if (!std::isfinite(r.animation_time) || r.animation_time < 0)
            return bad("invalid animation time");
        if (r.hidden_nodes.size() > R::max_hidden_nodes)
            return bad("too many hidden-node entries");
        for (auto v : r.hidden_nodes)
            if (v > 1)
                return bad("hidden-node entry is not 0 or 1");
        if (!std::isfinite(r.sprite_milliseconds) || r.sprite_milliseconds < 0 || r.sprite_frame >= R::max_sprite_frame)
            return bad("invalid sprite track");
        if (r.sky_slot != R::no_index && (!r.in_sky || r.sky_slot >= R::sky_slots))
            return bad("invalid sky slot");
        if (r.payload.size() > R::max_payload)
            return bad("game payload exceeds 64 KiB");
        return true;
    }
    bool valid_save_clock(const SaveClock& c, std::string& error)
    {
        error.clear();
        if (!std::isfinite(c.time) || c.time < 0 || !std::isfinite(c.frame_time) || c.frame_time < 0)
            error = "invalid simulation or frame time";
        else if (!std::isfinite(c.world_origin) || c.world_origin < 0 || c.world_origin > c.time)
            error = "world-time origin outside 0..time";
        else if (!finite(c.global_force))
            error = "nonfinite global force";
        else if (c.generator < 1 || c.generator > 2147483646u)
            error = "gameplay generator state outside 1..2147483646";
        else
            return true;
        return false;
    }

    bool encode_save_clock(const SaveClock& clock, std::vector<std::uint8_t>& bytes, SaveError& error)
    {
        error = {};
        std::string why;
        if (!valid_save_clock(clock, why))
            return fail(error, SaveFault::Format, "CLCK: " + why);
        std::vector<std::uint8_t> out;
        Out o(out);
        o.f64(clock.time);
        o.f64(clock.frame_time);
        o.f64(clock.world_origin);
        o.vector(clock.global_force);
        o.u32(clock.generator);
        bytes = std::move(out);
        return true;
    }
    bool decode_save_clock(std::span<const std::uint8_t> bytes, SaveClock& clock, SaveError& error)
    {
        error = {};
        In in(bytes, "CLCK", error);
        SaveClock c;
        c.time = in.f64();
        c.frame_time = in.f64();
        c.world_origin = in.f64();
        c.global_force = in.vector();
        c.generator = in.u32();
        if (!in.finish())
            return false;
        std::string why;
        if (!valid_save_clock(c, why))
            return fail(error, SaveFault::Format, "CLCK: " + why);
        clock = c;
        return true;
    }
    bool encode_save_records(std::span<const SaveEngineRecord> records, std::vector<std::uint8_t>& bytes,
                             SaveError& error)
    {
        error = {};
        if (records.size() > R::max_records)
            return fail(error, SaveFault::Bound, "OBJS: more than 65,536 records");
        std::size_t local = 0;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            std::string why;
            if (!valid_save_record(records[i], why))
                return fail(error, SaveFault::Format, "OBJS record " + std::to_string(i) + ": " + why);
            local += records[i].local_client;
        }
        if (local > 1)
            return fail(error, SaveFault::Format, "OBJS: more than one local-client record");
        std::vector<std::uint8_t> out;
        Out o(out);
        o.u32(std::uint32_t(records.size()));
        for (const auto& r : records)
            write_record(o, r);
        if (out.size() > save_format::max_file)
            return fail(error, SaveFault::Bound, "OBJS: payload exceeds the 64 MiB file bound");
        bytes = std::move(out);
        return true;
    }
    bool decode_save_records(std::span<const std::uint8_t> bytes, std::vector<SaveEngineRecord>& records,
                             SaveError& error)
    {
        error = {};
        In in(bytes, "OBJS", error);
        const auto count = in.count(R::max_records, "record");
        std::vector<SaveEngineRecord> out;
        std::size_t local = 0;
        for (std::uint32_t i = 0; i < count && in.good(); ++i)
        {
            SaveEngineRecord r;
            read_record(in, r);
            std::string why;
            if (in.good() && !valid_save_record(r, why))
                in.bad(SaveFault::Format, "record " + std::to_string(i) + ": " + why);
            local += r.local_client;
            out.push_back(std::move(r));
        }
        if (!in.finish())
            return false;
        if (local > 1)
            return fail(error, SaveFault::Format, "OBJS: more than one local-client record");
        records = std::move(out);
        return true;
    }
    bool encode_save_links(std::span<const SaveLink> links, std::size_t records, std::vector<std::uint8_t>& bytes,
                           SaveError& error)
    {
        error = {};
        if (links.size() * 8 + 4 > save_format::max_links)
            return fail(error, SaveFault::Bound, "LINK: payload exceeds 16 MiB");
        for (const auto& l : links)
            if (l.owner >= records || l.target >= records || l.owner == l.target)
                return fail(error, SaveFault::Format, "LINK: invalid record index");
        std::vector<std::uint8_t> out;
        Out o(out);
        o.u32(std::uint32_t(links.size()));
        for (const auto& l : links)
        {
            o.u32(l.owner);
            o.u32(l.target);
        }
        bytes = std::move(out);
        return true;
    }
    bool decode_save_links(std::span<const std::uint8_t> bytes, std::size_t records, std::vector<SaveLink>& links,
                           SaveError& error)
    {
        error = {};
        In in(bytes, "LINK", error);
        const auto count = in.count((save_format::max_links - 4) / 8, "link");
        std::vector<SaveLink> out;
        for (std::uint32_t i = 0; i < count && in.good(); ++i)
        {
            SaveLink l{in.u32(), in.u32()};
            if (in.good() && (l.owner >= records || l.target >= records || l.owner == l.target))
                in.bad(SaveFault::Format, "link " + std::to_string(i) + " has an invalid record index");
            for (std::size_t j = 0; in.good() && j < out.size(); ++j)
                if (out[j] == l)
                    in.bad(SaveFault::Format, "duplicate link " + std::to_string(i));
            out.push_back(l);
        }
        if (!in.finish())
            return false;
        links = std::move(out);
        return true;
    }
    bool encode_save_attachments(std::span<const SaveAttachment> attachments, std::size_t records,
                                 std::vector<std::uint8_t>& bytes, SaveError& error)
    {
        error = {};
        std::vector<std::uint8_t> out;
        Out o(out);
        o.u32(std::uint32_t(attachments.size()));
        for (std::size_t i = 0; i < attachments.size(); ++i)
        {
            std::string why;
            if (!valid_attachment(attachments[i], records, why))
                return fail(error, SaveFault::Format, "ATCH attachment " + std::to_string(i) + ": " + why);
            const auto& a = attachments[i];
            o.u32(a.parent);
            o.u32(a.child);
            o.text(a.node);
            o.vector(a.offset);
            o.rotation(a.rotation);
        }
        if (out.size() > save_format::max_attachments)
            return fail(error, SaveFault::Bound, "ATCH: payload exceeds 16 MiB");
        bytes = std::move(out);
        return true;
    }
    bool decode_save_attachments(std::span<const std::uint8_t> bytes, std::size_t records,
                                 std::vector<SaveAttachment>& attachments, SaveError& error)
    {
        error = {};
        In in(bytes, "ATCH", error);
        // 32 bytes is the smallest attachment (empty node name).
        const auto count = in.count((save_format::max_attachments - 4) / 32, "attachment");
        std::vector<SaveAttachment> out;
        std::vector<bool> attached(records);
        for (std::uint32_t i = 0; i < count && in.good(); ++i)
        {
            SaveAttachment a;
            a.parent = in.u32();
            a.child = in.u32();
            a.node = in.text(R::max_string, "node name");
            a.offset = in.vector();
            a.rotation = in.rotation();
            std::string why;
            if (in.good() && !valid_attachment(a, records, why))
                in.bad(SaveFault::Format, "attachment " + std::to_string(i) + ": " + why);
            // The engine attaches each child to one parent.
            if (in.good() && attached[a.child])
                in.bad(SaveFault::Format, "attachment " + std::to_string(i) + ": child attached twice");
            if (in.good())
                attached[a.child] = true;
            out.push_back(std::move(a));
        }
        if (!in.finish())
            return false;
        attachments = std::move(out);
        return true;
    }

    double remaining_delay(double clock, double deadline)
    {
        return deadline > clock ? deadline - clock : 0.0;
    }
    double restored_deadline(double clock, double remaining)
    {
        // clock + remaining can round to a neighbour of the original deadline. Choose the
        // nearest candidate that yields the saved remaining delay again, so a save taken
        // immediately after a full restore reproduces the same bytes.
        const double sum = clock + remaining;
        if (remaining <= 0 || remaining_delay(clock, sum) == remaining)
            return remaining <= 0 ? clock : sum;
        double lower = sum, upper = sum;
        for (int step = 0; step < 4; ++step)
        {
            lower = std::nextafter(lower, -INFINITY);
            upper = std::nextafter(upper, INFINITY);
            if (remaining_delay(clock, upper) == remaining)
                return upper;
            if (remaining_delay(clock, lower) == remaining)
                return lower;
        }
        return sum;
    }

    const char* save_store_slot_name(SaveStoreSlot slot)
    {
        switch (slot)
        {
        case SaveStoreSlot::Current:
            return "Current";
        case SaveStoreSlot::Auto:
            return "Auto";
        case SaveStoreSlot::Keep:
            return "Keep";
        }
        return "Unknown";
    }
} // namespace OpenBlood2
