#include "engine/gi_placement.h"
#include "engine/atomic_file.h"
#include "engine/bounded_json.h"
#include "engine/gi_file.h"
#include "engine/resources.h"
#include "engine/user_directory.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <system_error>

namespace OpenBlood2
{
    namespace
    {
        namespace L = gi_placement_limits;
        constexpr std::string_view format_name = "openblood2-gi-placement";
        // Reflection seeds: rooms narrower or lower than this get no automatic probe.
        constexpr float min_room_width = 192, min_room_height = 96;
        constexpr float default_blend = 32;
        constexpr std::size_t max_room_candidates = 4096;

        float component(DVector v, int axis)
        {
            return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
        }
        void set_component(DVector& v, int axis, float value)
        {
            (axis == 0 ? v.x : axis == 1 ? v.y : v.z) = value;
        }
        bool coordinate(DVector v)
        {
            for (int a = 0; a < 3; ++a)
                if (!std::isfinite(component(v, a)) || std::abs(component(v, a)) > L::max_coordinate)
                    return false;
            return true;
        }
        bool valid_box(const GiBox& box)
        {
            if (!coordinate(box.min) || !coordinate(box.max))
                return false;
            for (int a = 0; a < 3; ++a)
                if (!(component(box.min, a) < component(box.max, a)))
                    return false;
            return true;
        }
        double volume(const GiBox& box)
        {
            double v = 1;
            for (int a = 0; a < 3; ++a)
                v *= std::max(0.0, double(component(box.max, a)) - component(box.min, a));
            return v;
        }
        // Intersection volume over the smaller volume.
        double overlap(const GiBox& a, const GiBox& b)
        {
            double inter = 1;
            for (int k = 0; k < 3; ++k)
                inter *= std::max(0.0, double(std::min(component(a.max, k), component(b.max, k))) -
                                           std::max(component(a.min, k), component(b.min, k)));
            const double smaller = std::min(volume(a), volume(b));
            return smaller > 0 ? inter / smaller : 0;
        }
        bool power_of_two(std::uint32_t v)
        {
            return v && !(v & (v - 1));
        }
        bool valid_world_path(const std::string& path)
        {
            std::string normalized;
            if (path.empty() || path.size() > gi_format::max_world_path || !resource_path(path, normalized) ||
                normalized != path || !path.ends_with(".dat"))
                return false;
            return std::none_of(path.begin(), path.end(), [](char c)
                                { return c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20; });
        }
        bool valid_reflection(const GiReflectionProbe& r, std::string& error)
        {
            if (!valid_box(r.influence) || !valid_box(r.projection))
                return error = "a reflection probe box is empty, inverted or out of range", false;
            if (!coordinate(r.capture) || !r.projection.contains(r.capture))
                return error = "a reflection probe capture point is outside its projection box", false;
            if (!std::isfinite(r.blend_distance) || r.blend_distance < 0 || r.blend_distance > L::max_blend)
                return error = "a reflection probe blend distance is outside [0, 1024]", false;
            if (r.priority < -L::max_priority || r.priority > L::max_priority)
                return error = "a reflection probe priority is outside [-1000, 1000]", false;
            if (!power_of_two(r.resolution) || r.resolution < L::min_resolution || r.resolution > L::max_resolution)
                return error = "a reflection probe resolution is not a power of two in [16, 256]", false;
            return true;
        }

        std::string number(float v)
        {
            if (v == 0)
                return "0"; // Also -0.
            char buffer[32];
            const auto result = std::to_chars(buffer, buffer + sizeof buffer, v);
            return std::string(buffer, result.ptr);
        }
        std::string vector_text(DVector v)
        {
            return "[" + number(v.x) + ", " + number(v.y) + ", " + number(v.z) + "]";
        }
        std::string box_text(const GiBox& box)
        {
            return "{\"min\": " + vector_text(box.min) + ", \"max\": " + vector_text(box.max) + "}";
        }

        // Strict readers: the error names the key.
        bool exact_keys(const Json& object, std::initializer_list<std::string_view> names, std::string_view where,
                        std::string& error)
        {
            if (object.kind != Json::Kind::Object)
                return error = std::string(where) + " must be an object", false;
            for (const auto& [key, value] : object.members)
                if (std::find(names.begin(), names.end(), key) == names.end())
                    return error = std::string(where) + ": unknown key '" + key + "'", false;
            for (const auto name : names)
                if (!object.find(name))
                    return error = std::string(where) + ": missing key '" + std::string(name) + "'", false;
            return true;
        }
        bool integer(const Json& object, std::string_view key, double lo, double hi, std::int64_t& out,
                     std::string& error)
        {
            const auto* v = object.find(key);
            if (!v || v->kind != Json::Kind::Number || v->number != std::floor(v->number) || v->number < lo ||
                v->number > hi)
                return error = "'" + std::string(key) + "' must be an integer in [" + std::to_string(std::int64_t(lo)) +
                               ", " + std::to_string(std::int64_t(hi)) + "]",
                       false;
            out = std::int64_t(v->number);
            return true;
        }
        bool real(const Json& object, std::string_view key, double lo, double hi, float& out, std::string& error)
        {
            const auto* v = object.find(key);
            if (!v || v->kind != Json::Kind::Number || v->number < lo || v->number > hi)
                return error = "'" + std::string(key) + "' must be a number in [" + number(float(lo)) + ", " +
                               number(float(hi)) + "]",
                       false;
            out = float(v->number);
            return true;
        }
        bool boolean(const Json& object, std::string_view key, bool& out, std::string& error)
        {
            const auto* v = object.find(key);
            if (!v || v->kind != Json::Kind::Boolean)
                return error = "'" + std::string(key) + "' must be true or false", false;
            out = v->boolean;
            return true;
        }
        bool vector3(const Json& object, std::string_view key, DVector& out, std::string& error)
        {
            const auto* v = object.find(key);
            bool ok = v && v->kind == Json::Kind::Array && v->items.size() == 3;
            for (int a = 0; ok && a < 3; ++a)
            {
                const auto& item = v->items[std::size_t(a)];
                ok = item.kind == Json::Kind::Number && std::abs(item.number) <= L::max_coordinate;
                if (ok)
                    set_component(out, a, float(item.number));
            }
            if (!ok)
                error = "'" + std::string(key) + "' must be three coordinates within 1e6";
            return ok;
        }
        bool box(const Json& object, std::string_view key, GiBox& out, std::string& error)
        {
            const auto* v = object.find(key);
            return v && exact_keys(*v, {"min", "max"}, key, error) && vector3(*v, "min", out.min, error) &&
                   vector3(*v, "max", out.max, error);
        }
        bool digest_value(std::string_view text, Sha256Digest& digest)
        {
            if (text.size() != 64)
                return false;
            for (std::size_t i = 0; i < 32; ++i)
            {
                unsigned value = 0;
                for (int k = 0; k < 2; ++k)
                {
                    const char c = text[i * 2 + std::size_t(k)];
                    const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
                    if (d < 0)
                        return false;
                    value = value << 4 | unsigned(d);
                }
                digest[i] = std::uint8_t(value);
            }
            return true;
        }

        // Little-endian section bytes.
        class Out
        {
          public:
            explicit Out(std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
            void u32(std::uint32_t v)
            {
                for (int i = 0; i < 4; ++i)
                    bytes_.push_back(std::uint8_t(v >> (8 * i)));
            }
            void f32(float v)
            {
                std::uint32_t bits;
                std::memcpy(&bits, &v, 4);
                u32(bits);
            }
            void vec(DVector v)
            {
                f32(v.x);
                f32(v.y);
                f32(v.z);
            }

          private:
            std::vector<std::uint8_t>& bytes_;
        };
        class In
        {
          public:
            explicit In(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
            bool u32(std::uint32_t& v)
            {
                if (bytes_.size() - at_ < 4)
                    return false;
                v = 0;
                for (int i = 0; i < 4; ++i)
                    v |= std::uint32_t(bytes_[at_++]) << (8 * i);
                return true;
            }
            bool f32(float& v)
            {
                std::uint32_t bits;
                if (!u32(bits))
                    return false;
                std::memcpy(&v, &bits, 4);
                return true;
            }
            bool vec(DVector& v)
            {
                return f32(v.x) && f32(v.y) && f32(v.z);
            }
            std::size_t remaining() const
            {
                return bytes_.size() - at_;
            }

          private:
            std::span<const std::uint8_t> bytes_;
            std::size_t at_ = 0;
        };

        bool excluded(const GiPlacement& placement, DVector p)
        {
            bool any_include = false, included = false;
            for (const auto& volume : placement.volumes)
            {
                if (!volume.include && volume.box.contains(p))
                    return true;
                any_include |= volume.include;
                included |= volume.include && volume.box.contains(p);
            }
            return any_include && !included;
        }
        // Relocation candidates, nearest first: the 26 neighbours at 1/4 then at 3/8 of the spacing.
        std::vector<DVector> relocation_offsets(std::uint32_t spacing)
        {
            struct Offset
            {
                double length;
                DVector d;
            };
            std::vector<Offset> offsets;
            for (const double scale : {spacing / 4.0, spacing * 3 / 8.0})
                for (int x = -1; x <= 1; ++x)
                    for (int y = -1; y <= 1; ++y)
                        for (int z = -1; z <= 1; ++z)
                            if (x || y || z)
                                offsets.push_back({scale * std::sqrt(double(x * x + y * y + z * z)),
                                                   {float(x * scale), float(y * scale), float(z * scale)}});
            std::stable_sort(offsets.begin(), offsets.end(),
                             [](const Offset& a, const Offset& b) { return a.length < b.length; });
            std::vector<DVector> out;
            for (const auto& o : offsets)
                out.push_back(o.d);
            return out;
        }
        struct LatticeRun
        {
            GiLightLattice lattice;
            GiPlacementStats stats;
            std::vector<GiRejectedNode> rejected;
            bool over_budget = false;
        };
        bool run_lattice(const GiPlacementScene& scene, const GiPlacement& placement, std::uint32_t spacing,
                         bool keep_rejected, LatticeRun& run, std::string& error)
        {
            run = {};
            auto& lattice = run.lattice;
            auto& stats = run.stats;
            lattice.spacing = spacing;
            const auto& bounds = scene.bounds();
            std::uint64_t nodes = 1;
            for (int a = 0; a < 3; ++a)
            {
                const double lo = std::floor(double(component(bounds.min, a)) / spacing) * spacing;
                const double count = std::floor((double(component(bounds.max, a)) - lo) / spacing) + 1;
                set_component(lattice.origin, a, float(lo));
                lattice.dims[std::size_t(a)] = std::uint32_t(count);
                nodes *= std::uint64_t(count);
            }
            if (nodes > L::max_nodes)
            {
                error = "The probe lattice has " + std::to_string(nodes) + " nodes at spacing " +
                        std::to_string(spacing) + " (limit " + std::to_string(L::max_nodes) + ")";
                return false;
            }
            stats.nodes = nodes;
            const auto offsets = relocation_offsets(spacing);
            const float clearance = placement.clearance;
            for (std::uint32_t node = 0; node < nodes; ++node)
            {
                const DVector p = lattice.node_position(node);
                if (excluded(placement, p))
                {
                    ++stats.excluded;
                    continue;
                }
                const auto reason = scene.classify(p, clearance);
                if (reason == GiNodeRejection::None)
                    lattice.probes.push_back({node, p, false});
                else
                {
                    // A node in empty space moves only to a point it sees; one in a wall cannot see.
                    const bool sees = reason == GiNodeRejection::Clearance || reason == GiNodeRejection::WorldModel;
                    bool found = false;
                    for (const auto& offset : offsets)
                    {
                        const DVector q{p.x + offset.x, p.y + offset.y, p.z + offset.z};
                        if (excluded(placement, q) || scene.classify(q, clearance) != GiNodeRejection::None ||
                            (sees && !scene.visible(p, q)))
                            continue;
                        lattice.probes.push_back({node, q, true});
                        ++stats.relocated;
                        found = true;
                        break;
                    }
                    if (!found)
                    {
                        switch (reason)
                        {
                        case GiNodeRejection::Solid: ++stats.solid; break;
                        case GiNodeRejection::Unknown: ++stats.unknown; break;
                        case GiNodeRejection::Clearance: ++stats.clearance; break;
                        default: ++stats.world_model; break;
                        }
                        if (keep_rejected && reason != GiNodeRejection::Solid)
                            run.rejected.push_back({p, reason});
                    }
                }
                if (lattice.probes.size() > placement.budget)
                {
                    run.over_budget = true;
                    break;
                }
            }
            stats.probes = lattice.probes.size();
            return true;
        }
        GiReflectionProbe probe_for_room(const GiBox& box, bool open, DVector capture)
        {
            GiReflectionProbe probe;
            probe.capture = capture;
            probe.influence = probe.projection = box;
            float smallest = L::max_blend;
            for (int a = 0; a < 3; ++a)
                smallest = std::min(smallest, component(box.max, a) - component(box.min, a));
            probe.blend_distance = std::min(default_blend, smallest / 4);
            probe.box_projection = !open;
            return probe;
        }
    } // namespace

    GiPlacement default_gi_placement(std::string world_path, const Sha256Digest& world)
    {
        GiPlacement placement;
        placement.world_path = std::move(world_path);
        placement.world = world;
        return placement;
    }

    bool validate_gi_placement(const GiPlacement& p, std::string& error)
    {
        const auto fail = [&](const std::string& reason)
        {
            error = "Invalid GI placement: " + reason;
            return false;
        };
        if (!valid_world_path(p.world_path))
            return fail("the world is not a normalized .dat resource path");
        if (p.spacing < L::min_spacing || p.spacing > L::max_spacing || p.spacing % 8)
            return fail("the spacing is not a multiple of 8 in [32, 1024]");
        if (p.budget < 1 || p.budget > L::max_budget)
            return fail("the budget is outside [1, 262144]");
        if (!std::isfinite(p.clearance) || p.clearance < 0 || p.clearance > L::max_clearance)
            return fail("the clearance is outside [0, 64]");
        if (p.volumes.size() > L::max_volumes)
            return fail("more than 256 volumes");
        for (const auto& volume : p.volumes)
            if (!valid_box(volume.box))
                return fail("a volume is empty, inverted or out of range");
        if (p.reflection_cap > L::max_reflections)
            return fail("the reflection cap is above 42");
        if (p.reflections.size() > p.reflection_cap)
            return fail("more reflection probes than the cap");
        std::string reason;
        for (const auto& r : p.reflections)
            if (!valid_reflection(r, reason))
                return fail(reason);
        return true;
    }

    bool write_gi_placement(const GiPlacement& p, std::string& text, std::string& error)
    {
        if (!validate_gi_placement(p, error))
            return false;
        std::string o = "{\n  \"format\": \"" + std::string(format_name) + "\",\n  \"version\": 1,\n";
        o += "  \"world\": \"" + p.world_path + "\",\n  \"dat_sha256\": \"" + digest_text(p.world) + "\",\n";
        o += "  \"light_probes\": {\n    \"spacing\": " + std::to_string(p.spacing) +
             ",\n    \"budget\": " + std::to_string(p.budget) + ",\n    \"clearance\": " + number(p.clearance) +
             ",\n    \"volumes\": [";
        for (std::size_t i = 0; i < p.volumes.size(); ++i)
            o += std::string(i ? "," : "") + "\n      {\"mode\": \"" + (p.volumes[i].include ? "include" : "exclude") +
                 "\", \"min\": " + vector_text(p.volumes[i].box.min) + ", \"max\": " +
                 vector_text(p.volumes[i].box.max) + "}";
        o += p.volumes.empty() ? "]\n  },\n" : "\n    ]\n  },\n";
        o += "  \"reflection_probes\": {\n    \"cap\": " + std::to_string(p.reflection_cap) + ",\n    \"probes\": [";
        for (std::size_t i = 0; i < p.reflections.size(); ++i)
        {
            const auto& r = p.reflections[i];
            o += std::string(i ? "," : "") + "\n      {\n        \"auto\": " + (r.automatic ? "true" : "false") +
                 ",\n        \"capture\": " + vector_text(r.capture) + ",\n        \"influence\": " +
                 box_text(r.influence) + ",\n        \"projection\": " + box_text(r.projection) +
                 ",\n        \"blend_distance\": " + number(r.blend_distance) +
                 ",\n        \"priority\": " + std::to_string(r.priority) +
                 ",\n        \"box_projection\": " + (r.box_projection ? "true" : "false") +
                 ",\n        \"resolution\": " + std::to_string(r.resolution) + "\n      }";
        }
        o += p.reflections.empty() ? "]\n  }\n}\n" : "\n    ]\n  }\n}\n";
        text = std::move(o);
        return true;
    }

    bool read_gi_placement(std::string_view text, GiPlacement& out, std::string& error)
    {
        const auto fail = [&](const std::string& reason)
        {
            error = "Invalid GI placement: " + reason;
            return false;
        };
        if (text.size() > L::max_text)
            return fail("larger than 256 KiB");
        Json root;
        if (!parse_json(text, root, {6, 8192, 256}, "Invalid GI placement", error))
            return false;
        std::string reason;
        GiPlacement p;
        std::int64_t value = 0;
        if (!exact_keys(root, {"format", "version", "world", "dat_sha256", "light_probes", "reflection_probes"},
                        "the document", reason))
            return fail(reason);
        const auto* format = root.find("format");
        if (format->kind != Json::Kind::String || format->text != format_name)
            return fail("'format' must be \"" + std::string(format_name) + "\"");
        if (!integer(root, "version", 1, 1, value, reason))
            return fail("unsupported version");
        const auto* world = root.find("world");
        const auto* digest = root.find("dat_sha256");
        if (world->kind != Json::Kind::String || !valid_world_path(world->text))
            return fail("'world' must be a normalized .dat resource path");
        if (digest->kind != Json::Kind::String || !digest_value(digest->text, p.world))
            return fail("'dat_sha256' must be 64 lowercase hexadecimal digits");
        p.world_path = world->text;
        const auto& light = *root.find("light_probes");
        if (!exact_keys(light, {"spacing", "budget", "clearance", "volumes"}, "light_probes", reason) ||
            !integer(light, "spacing", L::min_spacing, L::max_spacing, value, reason))
            return fail(reason);
        p.spacing = std::uint32_t(value);
        if (!integer(light, "budget", 1, L::max_budget, value, reason))
            return fail(reason);
        p.budget = std::uint32_t(value);
        if (!real(light, "clearance", 0, L::max_clearance, p.clearance, reason))
            return fail(reason);
        const auto& volumes = *light.find("volumes");
        if (volumes.kind != Json::Kind::Array || volumes.items.size() > L::max_volumes)
            return fail("'volumes' must be an array of at most 256 volumes");
        for (const auto& item : volumes.items)
        {
            GiProbeVolume volume;
            if (!exact_keys(item, {"mode", "min", "max"}, "a volume", reason) ||
                !vector3(item, "min", volume.box.min, reason) || !vector3(item, "max", volume.box.max, reason))
                return fail(reason);
            const auto* mode = item.find("mode");
            if (mode->kind != Json::Kind::String || (mode->text != "include" && mode->text != "exclude"))
                return fail("a volume 'mode' must be \"include\" or \"exclude\"");
            volume.include = mode->text == "include";
            p.volumes.push_back(volume);
        }
        const auto& reflections = *root.find("reflection_probes");
        if (!exact_keys(reflections, {"cap", "probes"}, "reflection_probes", reason) ||
            !integer(reflections, "cap", 0, L::max_reflections, value, reason))
            return fail(reason);
        p.reflection_cap = std::uint32_t(value);
        const auto& probes = *reflections.find("probes");
        if (probes.kind != Json::Kind::Array || probes.items.size() > L::max_reflections)
            return fail("'probes' must be an array of at most 42 probes");
        for (const auto& item : probes.items)
        {
            GiReflectionProbe r;
            if (!exact_keys(item,
                            {"auto", "capture", "influence", "projection", "blend_distance", "priority",
                             "box_projection", "resolution"},
                            "a reflection probe", reason) ||
                !boolean(item, "auto", r.automatic, reason) || !vector3(item, "capture", r.capture, reason) ||
                !box(item, "influence", r.influence, reason) || !box(item, "projection", r.projection, reason) ||
                !real(item, "blend_distance", 0, L::max_blend, r.blend_distance, reason) ||
                !integer(item, "priority", -L::max_priority, L::max_priority, value, reason))
                return fail(reason);
            r.priority = std::int32_t(value);
            if (!boolean(item, "box_projection", r.box_projection, reason) ||
                !integer(item, "resolution", L::min_resolution, L::max_resolution, value, reason))
                return fail(reason);
            r.resolution = std::uint32_t(value);
            p.reflections.push_back(r);
        }
        if (!validate_gi_placement(p, error))
            return false;
        out = std::move(p);
        return true;
    }

    bool gi_placement_hash(const GiPlacement& placement, Sha256Digest& digest, std::string& error)
    {
        std::string text;
        if (!write_gi_placement(placement, text, error))
            return false;
        text += "placement_algorithm " + std::to_string(gi_placement_algorithm) + "\n";
        digest = sha256({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
        return true;
    }

    bool gi_placement_name(std::string_view world_path, std::string& name)
    {
        std::string path;
        if (!resource_path(world_path, path) || !valid_world_path(path))
            return false;
        name = path.substr(0, path.size() - 4) + ".json";
        return true;
    }

    bool gi_placement_directory(const std::filesystem::path& configured, std::filesystem::path& directory,
                                std::string& error)
    {
        if (!configured.empty())
        {
            directory = configured;
            return true;
        }
#ifdef BLOOD2_GI_PROJECT_DIR
        // GI tool builds: the checkout's committed data/gi folder, when it exists (decision 8).
        std::error_code ec;
        const auto project = path_from_utf8(BLOOD2_GI_PROJECT_DIR);
        if (std::filesystem::is_directory(project, ec))
        {
            directory = project / "placement";
            return true;
        }
#endif
        std::filesystem::path user;
        if (!user_directory(user, error))
            return false;
        directory = user / "gi" / "placement";
        return true;
    }

    const char* gi_placement_status_name(GiPlacementStatus status)
    {
        switch (status)
        {
        case GiPlacementStatus::Missing: return "missing";
        case GiPlacementStatus::Ready: return "ready";
        case GiPlacementStatus::Foreign: return "foreign";
        case GiPlacementStatus::Failed: return "failed";
        }
        return "unknown";
    }

    GiPlacementStatus load_gi_placement(const std::filesystem::path& directory, std::string_view world_path,
                                        const Sha256Digest& world, GiPlacement& placement, std::string& detail)
    {
        detail.clear();
        std::string name, normalized;
        if (!gi_placement_name(world_path, name) || !resource_path(world_path, normalized))
        {
            detail = "Invalid world path " + std::string(world_path);
            return GiPlacementStatus::Failed;
        }
        const auto path = directory / path_from_utf8(name);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            if (!ec)
                return GiPlacementStatus::Missing;
            detail = "Cannot inspect " + path_text(path) + ": " + ec.message();
            return GiPlacementStatus::Failed;
        }
        std::vector<std::uint8_t> bytes;
        if (!read_file_bounded(path, L::max_text, bytes, detail))
            return GiPlacementStatus::Failed;
        GiPlacement read;
        if (!read_gi_placement({reinterpret_cast<const char*>(bytes.data()), bytes.size()}, read, detail))
            return GiPlacementStatus::Failed;
        if (read.world_path != normalized)
        {
            detail = "Made for " + read.world_path;
            return GiPlacementStatus::Foreign;
        }
        if (read.world != world)
        {
            detail = "Made for DAT " + digest_text(read.world).substr(0, 12);
            return GiPlacementStatus::Foreign;
        }
        placement = std::move(read);
        return GiPlacementStatus::Ready;
    }

    bool save_gi_placement(const std::filesystem::path& directory, const GiPlacement& placement, std::string& error)
    {
        std::string text, name;
        if (!write_gi_placement(placement, text, error))
            return false;
        if (!gi_placement_name(placement.world_path, name))
            return error = "Invalid GI placement world path", false;
        return write_file_atomic(directory / path_from_utf8(name),
                                 {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}, "GI placement",
                                 error);
    }

    const char* gi_node_rejection_name(GiNodeRejection reason)
    {
        switch (reason)
        {
        case GiNodeRejection::None: return "none";
        case GiNodeRejection::Excluded: return "excluded";
        case GiNodeRejection::Solid: return "solid";
        case GiNodeRejection::Unknown: return "unknown";
        case GiNodeRejection::Clearance: return "clearance";
        case GiNodeRejection::WorldModel: return "world model";
        }
        return "unknown";
    }

    bool GiPlacementScene::build(const World& world, const CollisionWorld& collision, std::string& error)
    {
        *this = {};
        if (world.models.empty() || world.models[0].bsp_nodes.empty())
            return error = "The world has no primary BSP tree", false;
        model_bounds_ = world_model_bounds(world);
        const auto& primary = model_bounds_[0];
        if (primary.min.x > primary.max.x || !coordinate(primary.min) || !coordinate(primary.max))
        {
            *this = {};
            return error = "The primary model has no points within 1e6", false;
        }
        bounds_ = {primary.min, primary.max};
        const auto roles = world_model_roles(world);
        for (std::uint32_t i = 1; i < model_bounds_.size() && i < roles.size(); ++i)
        {
            const auto role = roles[i].role;
            if (model_bounds_[i].min.x > model_bounds_[i].max.x)
                continue;
            if (role == WorldModelRole::Mover || role == WorldModelRole::Destructible ||
                role == WorldModelRole::Unbound)
                blockers_.push_back(i);
            if (role == WorldModelRole::Mover)
                doors_.push_back(i);
        }
        world_ = &world;
        collision_ = &collision;
        return true;
    }

    GiNodeRejection GiPlacementScene::classify(DVector p, float clearance) const
    {
        if (!world_ || !coordinate(p))
            return GiNodeRejection::Unknown;
        const auto contents = point_contents(world_->models[0], p);
        if (contents != PointContents::Empty)
            return contents == PointContents::Solid ? GiNodeRejection::Solid : GiNodeRejection::Unknown;
        if (clearance > 0)
        {
            bool clear = false;
            if (!point_clear(*collision_, p, clearance, clear))
                return GiNodeRejection::Unknown;
            if (!clear)
                return GiNodeRejection::Clearance;
        }
        for (const auto i : blockers_)
        {
            const auto& b = model_bounds_[i];
            if (p.x < b.min.x - clearance || p.x > b.max.x + clearance || p.y < b.min.y - clearance ||
                p.y > b.max.y + clearance || p.z < b.min.z - clearance || p.z > b.max.z + clearance)
                continue;
            const auto& model = world_->models[i];
            if (model.bsp_nodes.empty() || point_contents(model, p) != PointContents::Empty)
                return GiNodeRejection::WorldModel; // No tree: its bounds count as solid.
            if (clearance > 0)
                for (int corner = 0; corner < 8; ++corner)
                {
                    const DVector q{p.x + (corner & 1 ? clearance : -clearance),
                                    p.y + (corner & 2 ? clearance : -clearance),
                                    p.z + (corner & 4 ? clearance : -clearance)};
                    if (point_contents(model, q) == PointContents::Solid)
                        return GiNodeRejection::WorldModel;
                }
        }
        return GiNodeRejection::None;
    }

    bool GiPlacementScene::visible(DVector from, DVector to) const
    {
        if (!collision_)
            return false;
        CollisionHit hit;
        return collision_->sweep(from, to, {0, 0, 0}, hit) == CollisionQuery::Miss;
    }

    bool GiPlacementScene::fit_room(DVector p, GiBox& box, bool& open) const
    {
        open = false;
        if (!world_ || !coordinate(p))
            return false;
        GiBox out{p, p};
        for (int a = 0; a < 3; ++a)
            for (const int sign : {-1, 1})
            {
                const double c = component(p, a);
                const double limit = sign < 0 ? c - component(bounds_.min, a) : component(bounds_.max, a) - c;
                double d = 0;
                if (limit <= 0)
                    open = true;
                else
                {
                    DVector to = p;
                    set_component(to, a, float(c + sign * (limit + 1)));
                    CollisionHit hit;
                    const auto result = collision_->sweep(p, to, {0, 0, 0}, hit);
                    if (result == CollisionQuery::Invalid)
                        return false;
                    // A hit at most one unit past the bounds is their outer wall: still enclosed.
                    if (result == CollisionQuery::Hit)
                    {
                        d = hit.start_solid ? 0 : hit.fraction * (limit + 1);
                        if (d >= limit - 1e-3)
                            d = limit;
                    }
                    else
                    {
                        d = limit;
                        open = true;
                    }
                }
                // A closed door at the authored pose ends the room (by its bounds).
                for (const auto i : doors_)
                {
                    const auto& b = model_bounds_[i];
                    const bool across = [&]
                    {
                        for (int k = 0; k < 3; ++k)
                            if (k != a &&
                                (component(p, k) < component(b.min, k) || component(p, k) > component(b.max, k)))
                                return false;
                        return true;
                    }();
                    if (!across)
                        continue;
                    const double t = sign > 0 ? component(b.min, a) - c : c - component(b.max, a);
                    if (t >= 0 && t < d)
                        d = t;
                }
                // Faces on a 1/16-unit grid keep placement files short and stable.
                const double face = std::round((c + sign * d) * 16) / 16;
                set_component(sign < 0 ? out.min : out.max, a, float(sign < 0 ? std::min(face, c) : std::max(face, c)));
            }
        box = out;
        return true;
    }

    DVector GiLightLattice::node_position(std::uint32_t node) const
    {
        const std::uint64_t nx = dims[0], ny = dims[1];
        const std::uint64_t x = node % nx, y = node / nx % ny, z = node / (nx * ny);
        return {float(double(origin.x) + double(spacing) * double(x)),
                float(double(origin.y) + double(spacing) * double(y)),
                float(double(origin.z) + double(spacing) * double(z))};
    }

    bool resolve_light_probes(const GiPlacementScene& scene, const GiPlacement& placement, GiLightLattice& lattice,
                              GiPlacementStats& stats, std::vector<GiRejectedNode>* rejected, std::string& error)
    {
        if (!validate_gi_placement(placement, error))
            return false;
        static constexpr double ladder[]{1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4};
        std::vector<std::pair<std::uint32_t, std::uint64_t>> over;
        std::uint32_t previous = 0;
        LatticeRun run;
        for (const double factor : ladder)
        {
            const auto spacing = std::uint32_t(std::lround(placement.spacing * factor / 8)) * 8;
            if (spacing == previous)
                continue;
            previous = spacing;
            if (!run_lattice(scene, placement, spacing, rejected != nullptr, run, error))
                return false;
            if (!run.over_budget)
            {
                lattice = std::move(run.lattice);
                stats = std::move(run.stats);
                stats.over_budget = std::move(over);
                if (rejected)
                    *rejected = std::move(run.rejected);
                return true;
            }
            over.emplace_back(spacing, run.lattice.probes.size());
        }
        error = "The budget of " + std::to_string(placement.budget) + " light probes is exceeded even at spacing " +
                std::to_string(previous);
        return false;
    }

    bool place_reflection_probes(const GiPlacementScene& scene, const GiLightLattice& lattice, GiPlacement& placement,
                                 GiPlacementStats& stats, std::string& error)
    {
        if (!validate_gi_placement(placement, error))
            return false;
        std::vector<GiReflectionProbe> result;
        for (const auto& r : placement.reflections)
            if (!r.automatic)
                result.push_back(r);
        stats.manual = std::uint32_t(result.size());
        stats.automatic = stats.open = 0;
        stats.room_candidates = stats.small_rooms = stats.overlapping = stats.capped = 0;
        struct Candidate
        {
            GiBox box;
            bool open = false;
            double volume = 0;
            std::size_t index = 0;
            DVector point{};
        };
        std::vector<Candidate> candidates;
        const std::size_t n = lattice.probes.size();
        const std::size_t stride = std::max<std::size_t>(1, (n + max_room_candidates - 1) / max_room_candidates);
        for (std::size_t i = 0; i < n; i += stride)
        {
            Candidate c;
            c.index = i;
            c.point = lattice.probes[i].position;
            if (!scene.fit_room(c.point, c.box, c.open))
                return error = "Invalid room query at a light probe", false;
            ++stats.room_candidates;
            if (component(c.box.max, 0) - component(c.box.min, 0) < min_room_width ||
                component(c.box.max, 2) - component(c.box.min, 2) < min_room_width ||
                component(c.box.max, 1) - component(c.box.min, 1) < min_room_height)
            {
                ++stats.small_rooms;
                continue;
            }
            c.volume = volume(c.box);
            candidates.push_back(c);
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b)
                         {
                             if (a.open != b.open)
                                 return !a.open;
                             if (a.volume != b.volume)
                                 return a.volume > b.volume;
                             return a.index < b.index;
                         });
        struct Accepted
        {
            GiBox box;
            int kind; // 0 manual, 1 enclosed, 2 open.
        };
        std::vector<Accepted> accepted;
        for (const auto& r : result)
            accepted.push_back({r.influence, 0});
        for (const auto& c : candidates)
        {
            const int kind = c.open ? 2 : 1;
            const bool covered = std::any_of(accepted.begin(), accepted.end(), [&](const Accepted& a)
                                             { return (a.kind == 0 || a.kind == kind) && overlap(a.box, c.box) > .5; });
            if (covered)
            {
                ++stats.overlapping;
                continue;
            }
            if (result.size() >= placement.reflection_cap)
            {
                ++stats.capped;
                accepted.push_back({c.box, kind}); // Its duplicates count as overlapping, not capped.
                continue;
            }
            const auto middle = [](float a, float b) { return float(std::round((double(a) + b) * 8) / 16); };
            const DVector center{middle(c.box.min.x, c.box.max.x), middle(c.box.min.y, c.box.max.y),
                                 middle(c.box.min.z, c.box.max.z)};
            const bool use_center =
                scene.classify(center, placement.clearance) == GiNodeRejection::None && scene.visible(c.point, center);
            result.push_back(probe_for_room(c.box, c.open, use_center ? center : c.point));
            accepted.push_back({c.box, kind});
            ++stats.automatic;
            stats.open += c.open;
        }
        GiPlacement updated = placement;
        updated.reflections = std::move(result);
        if (!validate_gi_placement(updated, error))
            return false;
        placement = std::move(updated);
        return true;
    }

    bool fit_reflection_probe(const GiPlacementScene& scene, DVector capture, GiReflectionProbe& probe,
                              std::string& error)
    {
        if (scene.classify(capture, 0) != GiNodeRejection::None)
            return error = "The capture point is in solid space", false;
        GiBox box;
        bool open = false;
        if (!scene.fit_room(capture, box, open) || !valid_box(box))
            return error = "No free space around the capture point", false;
        auto fitted = probe_for_room(box, open, capture);
        fitted.automatic = false;
        std::string reason;
        if (!valid_reflection(fitted, reason))
            return error = "The fitted probe is invalid: " + reason, false;
        probe = fitted;
        return true;
    }

    bool encode_gi_placement_section(const GiLightLattice& lattice, std::span<const GiReflectionProbe> reflections,
                                     std::vector<std::uint8_t>& payload, std::string& error)
    {
        const auto fail = [&](const std::string& reason)
        {
            error = "Invalid GI placement section: " + reason;
            return false;
        };
        std::uint64_t nodes = 1;
        for (const auto d : lattice.dims)
            nodes *= d;
        if (lattice.spacing < 8 || lattice.spacing > 4 * L::max_spacing || lattice.spacing % 8 || !nodes ||
            nodes > L::max_nodes || !coordinate(lattice.origin))
            return fail("invalid lattice");
        if (lattice.probes.size() > L::max_budget || reflections.size() > L::max_reflections)
            return fail("too many probes");
        const float reach = float(lattice.spacing) * 3 / 8;
        for (std::size_t i = 0; i < lattice.probes.size(); ++i)
        {
            const auto& probe = lattice.probes[i];
            if (probe.node >= nodes || (i && probe.node <= lattice.probes[i - 1].node))
                return fail("light probe nodes are not ascending within the lattice");
            const auto node = lattice.node_position(probe.node);
            if (!coordinate(probe.position) || std::abs(probe.position.x - node.x) > reach ||
                std::abs(probe.position.y - node.y) > reach || std::abs(probe.position.z - node.z) > reach ||
                (!probe.relocated && (probe.position.x != node.x || probe.position.y != node.y ||
                                      probe.position.z != node.z)))
                return fail("a light probe is away from its node");
        }
        std::string reason;
        for (const auto& r : reflections)
            if (!valid_reflection(r, reason))
                return fail(reason);
        std::vector<std::uint8_t> bytes;
        Out out(bytes);
        out.u32(1);
        out.u32(lattice.spacing);
        out.vec(lattice.origin);
        for (const auto d : lattice.dims)
            out.u32(d);
        out.u32(std::uint32_t(lattice.probes.size()));
        for (const auto& probe : lattice.probes)
        {
            out.u32(probe.node);
            out.vec(probe.position);
            out.u32(probe.relocated ? 1 : 0);
        }
        out.u32(std::uint32_t(reflections.size()));
        for (const auto& r : reflections)
        {
            out.vec(r.capture);
            out.vec(r.influence.min);
            out.vec(r.influence.max);
            out.vec(r.projection.min);
            out.vec(r.projection.max);
            out.f32(r.blend_distance);
            out.u32(std::uint32_t(r.priority));
            out.u32(r.resolution);
            out.u32((r.box_projection ? 1u : 0u) | (r.automatic ? 2u : 0u));
        }
        payload = std::move(bytes);
        return true;
    }

    bool decode_gi_placement_section(std::span<const std::uint8_t> payload, GiLightLattice& lattice_out,
                                     std::vector<GiReflectionProbe>& reflections_out, std::string& error)
    {
        const auto fail = [&](const std::string& reason)
        {
            error = "Invalid GI placement section: " + reason;
            return false;
        };
        In in(payload);
        std::uint32_t version = 0, count = 0;
        GiLightLattice lattice;
        if (!in.u32(version) || version != 1)
            return fail("unsupported version");
        if (!in.u32(lattice.spacing) || !in.vec(lattice.origin) || !in.u32(lattice.dims[0]) ||
            !in.u32(lattice.dims[1]) || !in.u32(lattice.dims[2]) || !in.u32(count))
            return fail("truncated");
        if (count > L::max_budget || std::uint64_t(count) * 20 > in.remaining())
            return fail("too many light probes");
        for (std::uint32_t i = 0; i < count; ++i)
        {
            GiLightProbe probe;
            std::uint32_t flags = 0;
            if (!in.u32(probe.node) || !in.vec(probe.position) || !in.u32(flags))
                return fail("truncated");
            if (flags > 1)
                return fail("unknown light probe flags");
            probe.relocated = flags & 1;
            lattice.probes.push_back(probe);
        }
        std::vector<GiReflectionProbe> reflections;
        if (!in.u32(count))
            return fail("truncated");
        if (count > L::max_reflections)
            return fail("too many reflection probes");
        for (std::uint32_t i = 0; i < count; ++i)
        {
            GiReflectionProbe r;
            std::uint32_t priority = 0, flags = 0;
            if (!in.vec(r.capture) || !in.vec(r.influence.min) || !in.vec(r.influence.max) ||
                !in.vec(r.projection.min) || !in.vec(r.projection.max) || !in.f32(r.blend_distance) ||
                !in.u32(priority) || !in.u32(r.resolution) || !in.u32(flags))
                return fail("truncated");
            if (flags > 3)
                return fail("unknown reflection probe flags");
            r.priority = std::int32_t(priority);
            r.box_projection = flags & 1;
            r.automatic = flags & 2;
            reflections.push_back(r);
        }
        if (in.remaining())
            return fail("trailing bytes");
        // The encoder's checks validate the decoded values.
        std::vector<std::uint8_t> check;
        if (!encode_gi_placement_section(lattice, reflections, check, error))
            return false;
        lattice_out = std::move(lattice);
        reflections_out = std::move(reflections);
        return true;
    }
} // namespace OpenBlood2
