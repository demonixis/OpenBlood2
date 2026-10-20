#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace OpenBlood2
{
    using Sha256Digest = std::array<std::uint8_t, 32>;
    // Portable FIPS 180-4 SHA-256 over byte spans; no host byte order or alignment assumptions.
    class Sha256
    {
      public:
        Sha256();
        void update(std::span<const std::uint8_t> bytes);
        Sha256Digest finish(); // The object is reset afterwards.

      private:
        void block(const std::uint8_t* data);
        std::array<std::uint32_t, 8> state_{};
        std::array<std::uint8_t, 64> buffer_{};
        std::uint64_t length_ = 0;
        std::size_t used_ = 0;
    };
    Sha256Digest sha256(std::span<const std::uint8_t> bytes);
    std::string digest_text(const Sha256Digest&); // Lowercase hexadecimal.
} // namespace OpenBlood2
