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
    };
    bool read_sprite(std::span<const std::uint8_t>, Sprite&, std::string&);
    bool advance_sprite(const Sprite&, SpriteTrack&, double seconds);
    class SpriteStore
    {
      public:
        explicit SpriteStore(Resources& resources) : resources_(resources) {}
        bool load(std::string_view, std::shared_ptr<const Sprite>&, std::string&);
        void clear()
        {
            cache_.clear();
        }

      private:
        Resources& resources_;
        std::map<std::string, std::weak_ptr<const Sprite>> cache_;
    };
} // namespace OpenBlood2
