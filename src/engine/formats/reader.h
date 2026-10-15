#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include "game/legacy/basedefs_de.h"

namespace blood2 {
// An owning caller supplies the bytes. Failed reads latch an error and never
// move outside this section; no unaligned casts, host-endian reads or exceptions.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return bytes_.size() - pos_; }
    bool good() const { return good_; }
    bool require(bool condition) { good_ &= condition; return good_; }
    bool skip(std::size_t n) { if (!require(n <= remaining())) return false; pos_ += n; return true; }
    std::span<const std::uint8_t> take(std::size_t n) {
        if (!require(n <= remaining())) return {};
        auto out = bytes_.subspan(pos_, n); pos_ += n; return out;
    }
    std::uint8_t u8() { auto b = take(1); return b.empty() ? 0 : b[0]; }
    std::uint16_t u16() { auto a = u8(); auto b = u8(); return a | (std::uint16_t(b) << 8); }
    std::uint32_t u32() { auto a = u16(); auto b = u16(); return a | (std::uint32_t(b) << 16); }
    float real() { float f = std::bit_cast<float>(u32()); require(std::isfinite(f)); return f; }
    DVector vec() { const float x = real(), y = real(), z = real(); return {x,y,z}; }
    std::string text(std::size_t n, std::size_t max = 4096) {
        if (!require(n <= max)) return {};
        auto b = take(n); std::string s(b.begin(), b.end()); require(s.find('\0') == std::string::npos); return s;
    }
    std::string string(std::size_t max = 4096) { return text(u16(), max); }
    std::string cstring(std::size_t max = 4096) {
        std::string s;
        while (good_ && remaining()) {
            auto c = u8(); if (!c) return s;
            if (!require(s.size() < max)) return {};
            s.push_back(static_cast<char>(c));
        }
        good_ = false; return {};
    }
private:
    std::span<const std::uint8_t> bytes_;
    std::size_t pos_ = 0;
    bool good_ = true;
};
}
