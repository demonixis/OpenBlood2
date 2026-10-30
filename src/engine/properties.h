#pragma once
#include "game/legacy/basedefs_de.h"
#include <map>
#include <string>
#include <string_view>
#include <variant>

namespace OpenBlood2
{
    struct CaseLess
    {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const;
    };
    struct EulerAngles
    {
        DVector radians;
    };
    DRotation rotation_from_euler(DVector radians);
    bool align_rotation(DVector forward, DRotation&);
    bool alignment_axes(DVector direction, DVector up, DVector& forward, DVector& right, bool& fallback);
    bool align_rotation(DVector forward, DVector up, DRotation&);
    using PropertyValue = std::variant<std::string, float, bool, std::int32_t, DVector, DRotation, EulerAngles>;
    using Properties = std::map<std::string, PropertyValue, CaseLess>;
    struct PropertySpec
    {
        std::string name;
        PropertyValue value;
        bool defaulted = true;
        std::size_t max_string = MAX_CS_FILENAME_LEN;
    };
    std::string property_text(const PropertyValue&);
} // namespace OpenBlood2
