#pragma once
#include "engine/hud_surface.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

struct SDL_AudioStream;

namespace OpenBlood2
{
    struct Config;
    class SmackerDecoder;

    class StartupMovies
    {
      public:
        StartupMovies();
        ~StartupMovies();
        StartupMovies(const StartupMovies&) = delete;
        StartupMovies& operator=(const StartupMovies&) = delete;

        void start(const Config&, float gain);
        void advance(double seconds, bool focused, bool skip);
        bool active() const { return decoder_ != nullptr; }
        std::string_view name() const;
        double elapsed() const { return elapsed_; }
        const HudSurface& surface() const;
        std::uint64_t revision() const { return revision_; }
        std::uint64_t submitted_audio() const { return submitted_audio_; }

      private:
        void next_clip();
        void stop_clip();
        void publish_frame();
        void queue_audio();
        void stop_audio();

        std::filesystem::path root_, movies_;
        std::unique_ptr<SmackerDecoder> decoder_;
        HudSurface scaled_;
        SDL_AudioStream* audio_ = nullptr;
        bool audio_initialized_ = false, focused_ = false;
        unsigned current_ = 0, count_ = 0;
        float gain_ = 1;
        double elapsed_ = 0;
        std::uint64_t revision_ = 0, submitted_audio_ = 0;
    };
} // namespace OpenBlood2
