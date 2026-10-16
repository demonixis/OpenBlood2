// Original BaseCharacter.cpp:108-219; bounded numeric parsing and explicit
// service outcomes replace unsafe fixed buffers/atof. See CHARACTER_MOTION_CONTRACTS.md.
#include "PlayerObj.h"
#include "Sparam.h"
#include "engine/collision.h"
#include <charconv>
#include <cmath>

namespace
{
    bool number(std::string_view text, float& value)
    {
        constexpr std::string_view spaces = " \t\r\n\f\v";
        const auto first = text.find_first_not_of(spaces);
        if (first == text.npos || text.size() > 31)
            return false;
        text = text.substr(first, text.find_last_not_of(spaces) - first + 1);
        if (text.front() == '+')
        {
            text.remove_prefix(1);
            if (!text.empty() && text.front() == '-')
                return false;
        }
        if (text.empty() || text.front() == '+')
            return false;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        return ec == std::errc{} && end == text.data() + text.size() && std::isfinite(value) && std::abs(value) <= 1e7f;
    }
} // namespace

bool CBaseCharacter::OnStringKey(ArgList* args)
{
    auto* server = GetServerDE();
    if (!server || !args || args->argc < 1 || !args->argv || !args->argv[0])
        return false;
    // The original handler examines argv[0] only; do not merge separate words.
    const std::string_view key(args->argv[0]);
    constexpr const char* names[]{"RotY", "ForwVel", "RightVel", "UpVel", "AddX", "DimsX"};
    std::array<bool, 6> present{};
    std::array<float, 6> values{};
    DVector add{}, dims{};
    bool recognized = false, valid = true;
    for (unsigned i = 0; i < 6; ++i)
    {
        std::string_view value;
        const auto status = Sparam_Get(key, names[i], value);
        present[i] = status != SparamStatus::Missing;
        recognized |= present[i];
        if (present[i] && (status != SparamStatus::Found || !number(value, values[i])))
            valid = false;
    }
    if (!recognized)
        return false;
    ++motion_keys;
    motion_key.assign(key.substr(0, 4096));
    motion_error.clear();
    motion_applied = 0;
    auto fail = [&](const char* error)
    {
        ++motion_failures;
        motion_error = error;
        return true; // Recognized failure, never reported as an unavailable unknown key.
    };
    auto component = [&](const char* name, float& out)
    {
        std::string_view value;
        return Sparam_Get(key, name, value) == SparamStatus::Found && number(value, out);
    };
    if (present[4])
    {
        add.x = values[4];
        valid &= component("AddY", add.y) && component("AddZ", add.z);
    }
    if (present[5])
    {
        dims.x = values[5];
        valid &= component("DimsY", dims.y) && component("DimsZ", dims.z);
        valid &= dims.x >= 0 && dims.y >= 0 && dims.z >= 0;
    }
    if (!valid)
        return fail("Invalid/missing motion parameter; no motion action applied");
    auto applied = [&](unsigned i)
    {
        motion_applied |= 1u << i;
        ++motion_actions;
    };
    DRotation rotation;
    if (server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return fail("Character rotation unavailable");
    if (present[0])
    {
        // EulerRotateY rotates around the current local up axis (later LithTech
        // math reference). Compose on the right, not around fixed world Y.
        const float half = values[0] * .5f;
        rotation = OpenBlood2::compose_rotation(rotation, {{0, std::sin(half), 0}, std::cos(half)});
        if (server->SetObjectRotation(m_hObject, &rotation) != DE_OK)
            return fail("Character rotation rejected");
        applied(0);
    }
    constexpr DVector axes[]{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}};
    for (unsigned i = 1; i <= 3; ++i)
        if (present[i])
        {
            auto velocity = OpenBlood2::vector_scale(OpenBlood2::rotate_vector(rotation, axes[i - 1]), values[i]);
            if (server->SetVelocity(m_hObject, &velocity) != DE_OK)
                return fail("Character velocity rejected");
            applied(i); // Original independent setters overwrite; they do not add.
        }
    if (present[4])
    {
        DVector position;
        server->GetObjectPos(m_hObject, &position);
        position = OpenBlood2::vector_add(position, add); // World-space delta, not rotated.
        if (server->MoveObject(m_hObject, &position) != DE_OK)
            return fail("Character displacement failed; earlier actions remain applied");
        applied(4); // Success can be collision-clipped; inspect actual position/contact.
    }
    if (present[5])
    {
        if (server->SetObjectDims(m_hObject, &dims) != DE_OK)
            return fail("Character dimensions rejected; earlier actions remain applied");
        applied(5); // Source SetObjectDims keeps the center; not player foot-anchored ResizeObject.
    }
    return true;
}
