// Bounded source-level adaptation of ObjectDLL/Sparam.cpp (1996 Monolith).
#pragma once
#include <string_view>

enum class SparamStatus
{
    Missing,
    Found,
    Invalid
};
// Original case-sensitive first "[id:value]" lookup. Output is borrowed and is
// unchanged on failure. No Windows/locale API or unbounded destination buffer.
SparamStatus Sparam_Get(std::string_view source, std::string_view id, std::string_view& value);
