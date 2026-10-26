#include "engine/sprite.h"
#include "engine/formats/reader.h"
#include <cmath>
namespace OpenBlood2
{
    bool read_sprite(std::span<const std::uint8_t> bytes, Sprite& out, std::string& error)
    {
        error.clear();
        Reader r(bytes);
        Sprite s;
        const auto count = r.u32();
        s.fps = r.u32();
        const auto transparent = r.u32(), translucent = r.u32();
        s.color_key = r.u32();
        if (bytes.size() > 1024 * 1024 || !r.good() || count == 0 || count > 4096 || s.fps == 0 || transparent > 1 ||
            translucent > 1)
        {
            error = "Invalid SPR header/count/rate/flags";
            return false;
        }
        s.transparent = transparent != 0;
        s.translucent = translucent != 0;
        for (unsigned i = 0; i < count; ++i)
        {
            auto name = r.string(1000);
            std::string canonical;
            if (!r.good() || !resource_path(name, canonical) || !canonical.ends_with(".dtx"))
            {
                error = "Invalid SPR frame resource";
                return false;
            }
            s.frames.push_back(std::move(name));
        }
        if (!r.good() || r.remaining())
        {
            error = "Trailing or truncated SPR bytes";
            return false;
        }
        out = std::move(s);
        return true;
    }
    bool advance_sprite(const Sprite& s, SpriteTrack& track, double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60 || s.fps == 0 || s.frames.size() > 4096 ||
            !std::isfinite(track.milliseconds) || track.milliseconds < 0)
            return false;
        if (s.frames.empty())
        {
            track.has_frame = false; // Zero frames: zero animation length, no current frame.
            return true;
        }
        // Later LithTech SPR clock uses integer milliseconds per frame. Fractional
        // milliseconds are retained here across simulation ticks, independent of UI.
        const auto period = 1000 / s.fps;
        if (!period)
        {
            track.has_frame = false; // spr_UpdateTracker: zero animation length, no current frame.
            return true;
        }
        track.has_frame = true;
        const double duration = period * s.frames.size();
        track.milliseconds = std::fmod(track.milliseconds + seconds * 1000, duration);
        track.frame = static_cast<unsigned>(track.milliseconds / period);
        return true;
    }
    bool SpriteStore::load(std::string_view path, std::shared_ptr<const Sprite>& out, std::string& error,
                           bool sprite_object)
    {
        out.reset();
        error.clear();
        const auto* resource = resources_.find(path);
        std::string canonical;
        if (sprite_object && resource && resource_path(path, canonical) && !canonical.ends_with(".spr") &&
            resource->size <= 1024 * 1024)
        {
            // spr_Create over a non-SPR file: the header fields are read as they are. Only the
            // zero-frame case is accepted (no frame name is read); any other count is refused.
            std::vector<std::uint8_t> bytes;
            if (!resources_.read(path, bytes, error, 20))
                return false;
            Reader r(bytes);
            const auto count = r.u32(), rate = r.u32(), transparent = r.u32(), translucent = r.u32(), key = r.u32();
            if (!r.good() || count != 0 || rate == 0)
            {
                error = "Non-SPR sprite resource does not read as an empty SPR: " + std::string(path);
                return false;
            }
            auto empty = std::make_shared<Sprite>();
            empty->fps = rate;
            empty->transparent = std::uint8_t(transparent) != 0; // SpriteAnim::m_bKeyed is a uint8.
            empty->translucent = std::uint8_t(translucent) != 0;
            empty->color_key = key;
            out = std::move(empty);
            return true;
        }
        if (!resource || !resource_path(path, canonical) || !canonical.ends_with(".spr") ||
            resource->size > 1024 * 1024)
        {
            error = "Missing, oversized or non-SPR sprite resource: " + std::string(path);
            return false;
        }
        const auto key = path_text(resource->archive) + ":" + std::to_string(resource->offset) + ":" +
                         std::to_string(resource->size);
        std::shared_ptr<const Sprite> loaded;
        if (auto it = cache_.find(key); it != cache_.end())
            loaded = it->second.lock();
        if (!loaded)
        {
            std::vector<std::uint8_t> bytes;
            auto decoded = std::make_shared<Sprite>();
            if (!resources_.read(path, bytes, error) || !read_sprite(bytes, *decoded, error))
                return false;
            loaded = std::move(decoded);
        }
        // Recheck exact mount resolution even on a cache hit. Presence is not decoding.
        for (const auto& frame : loaded->frames)
            if (!resources_.find(frame))
            {
                error = "SPR frame resource not mounted: " + frame;
                return false;
            }
        cache_[key] = loaded;
        out = std::move(loaded);
        return true;
    }
} // namespace OpenBlood2
