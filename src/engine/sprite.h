#pragma once
#include "engine/resources.h"
#include <memory>
namespace OpenBlood2
{
    struct Sprite
    {
        unsigned fps = 0;
        bool transparent = false, translucent = false;
        std::uint32_t color_key = 0;
        std::vector<std::string> frames; // Resource names, NOT decoded textures.
    };
    struct SpriteTrack
    {
        double milliseconds = 0;
        unsigned frame = 0;
        bool has_frame = true; // Original zero-length animation clears the current frame on update.
    };
    bool read_sprite(std::span<const std::uint8_t>, Sprite&, std::string&);
    bool advance_sprite(const Sprite&, SpriteTrack&, double seconds);
    class SpriteStore
    {
      public:
        explicit SpriteStore(Resources& resources) : resources_(resources) {}
        // `sprite_object`: an OT_SPRITE's filename. The reference LithTech LoadSprite parses any
        // file as an SPR (spr_Create, whatever its extension); a non-SPR resource whose header
        // reads as zero frames (a DTX: resource type 0) is then a valid empty sprite with no
        // current frame. Other callers, and SPR files, keep the strict reader.
        bool load(std::string_view, std::shared_ptr<const Sprite>&, std::string&, bool sprite_object = false);
        void clear()
        {
            cache_.clear();
        }

      private:
        Resources& resources_;
        std::map<std::string, std::weak_ptr<const Sprite>> cache_;
    };
} // namespace OpenBlood2
