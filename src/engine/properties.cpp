#include "engine/properties.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace OpenBlood2
{
    namespace
    {
        char lower(char c)
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c;
        }
    } // namespace
    // LithTech SetupMatrixEuler uses yaw * pitch * roll. Preserve source Euler
    // values separately for GenericProp.m_Vec (GameStartPoint consumes them).
    DRotation rotation_from_euler(DVector v)
    {
        const float sx = std::sin(v.x / 2), cx = std::cos(v.x / 2);
        const float sy = std::sin(v.y / 2), cy = std::cos(v.y / 2);
        const float sz = std::sin(v.z / 2), cz = std::cos(v.z / 2);
        return {{cy * sx * cz + sy * cx * sz, sy * cx * cz - cy * sx * sz, cy * cx * sz - sy * sx * cz},
                cy * cx * cz + sy * sx * sz};
    }

    bool CaseLess::operator()(std::string_view a, std::string_view b) const
    {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                            [](char x, char y) { return lower(x) < lower(y); });
    }
    std::string property_text(const PropertyValue& value)
    {
        return std::visit(
            [](const auto& v) -> std::string
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>)
                    return v;
                else if constexpr (std::is_same_v<T, bool>)
                    return v ? "true" : "false";
                else if constexpr (std::is_arithmetic_v<T>)
                    return std::to_string(v);
                else
                {
                    std::ostringstream out;
                    out.imbue(std::locale::classic());
                    if constexpr (std::is_same_v<T, DVector>)
                        out << v.x << ' ' << v.y << ' ' << v.z;
                    else if constexpr (std::is_same_v<T, EulerAngles>)
                        out << "Euler " << v.radians.x << ' ' << v.radians.y << ' ' << v.radians.z;
                    else
                        out << v.m_Vec.x << ' ' << v.m_Vec.y << ' ' << v.m_Vec.z << ' ' << v.m_Spin;
                    return out.str();
                }
            },
            value);
    }
} // namespace OpenBlood2
