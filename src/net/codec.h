#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace OpenBlood2::Net
{
    // Explicit little-endian encoding, independent of alignment, pointer size and
    // host endianness. Limits are supplied by the message schema/transport channel.
    class Writer
    {
      public:
        explicit Writer(std::size_t limit) : limit_(limit) {}
        bool good() const
        {
            return good_;
        }
        const std::vector<std::uint8_t>& bytes() const
        {
            return bytes_;
        }
        void u8(std::uint8_t value)
        {
            raw({&value, 1});
        }
        void u16(std::uint16_t value)
        {
            integer(value, 2);
        }
        void u32(std::uint32_t value)
        {
            integer(value, 4);
        }
        void u64(std::uint64_t value)
        {
            integer(value, 8);
        }
        void f32(float value)
        {
            if (!std::isfinite(value))
                good_ = false;
            else
                u32(std::bit_cast<std::uint32_t>(value));
        }
        void f64(double value)
        {
            if (!std::isfinite(value))
                good_ = false;
            else
                u64(std::bit_cast<std::uint64_t>(value));
        }
        void raw(std::span<const std::uint8_t> value)
        {
            if (!good_ || value.size() > limit_ - bytes_.size())
            {
                good_ = false;
                return;
            }
            bytes_.insert(bytes_.end(), value.begin(), value.end());
        }
        void text(std::string_view value, std::size_t maximum)
        {
            if (value.size() > maximum || value.size() > UINT16_MAX || value.find('\0') != value.npos)
            {
                good_ = false;
                return;
            }
            u16(std::uint16_t(value.size()));
            raw({reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
        }

      private:
        std::size_t limit_;
        bool good_ = true;
        std::vector<std::uint8_t> bytes_;
        void integer(std::uint64_t value, unsigned size)
        {
            for (unsigned i = 0; i < size; ++i)
                u8(std::uint8_t(value >> (i * 8)));
        }
    };
    class Reader
    {
      public:
        explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
        bool good() const
        {
            return good_;
        }
        bool done() const
        {
            return good_ && position_ == bytes_.size();
        }
        std::size_t remaining() const
        {
            return bytes_.size() - position_;
        }
        std::uint8_t u8()
        {
            return std::uint8_t(integer(1));
        }
        std::uint16_t u16()
        {
            return std::uint16_t(integer(2));
        }
        std::uint32_t u32()
        {
            return std::uint32_t(integer(4));
        }
        std::uint64_t u64()
        {
            return integer(8);
        }
        float f32()
        {
            const float value = std::bit_cast<float>(u32());
            if (!std::isfinite(value))
                good_ = false;
            return good_ ? value : 0;
        }
        double f64()
        {
            const double value = std::bit_cast<double>(u64());
            if (!std::isfinite(value))
                good_ = false;
            return good_ ? value : 0;
        }
        std::span<const std::uint8_t> raw(std::size_t count)
        {
            if (!good_ || count > remaining())
            {
                good_ = false;
                return {};
            }
            const auto result = bytes_.subspan(position_, count);
            position_ += count;
            return result;
        }
        std::string text(std::size_t maximum)
        {
            const auto length = u16();
            if (length > maximum)
            {
                good_ = false;
                return {};
            }
            const auto value = raw(length);
            if (!good_ || value.empty())
                return {};
            std::string result(reinterpret_cast<const char*>(value.data()), value.size());
            if (result.find('\0') != result.npos)
            {
                good_ = false;
                return {};
            }
            return result;
        }

      private:
        std::span<const std::uint8_t> bytes_;
        std::size_t position_ = 0;
        bool good_ = true;
        std::uint64_t integer(unsigned count)
        {
            const auto value = raw(count);
            std::uint64_t result = 0;
            for (unsigned i = 0; i < value.size(); ++i)
                result |= std::uint64_t(value[i]) << (i * 8);
            return result;
        }
    };
} // namespace OpenBlood2::Net
