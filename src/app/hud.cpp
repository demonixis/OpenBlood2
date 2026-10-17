#include "app/hud.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <imgui.h>
namespace OpenBlood2
{
    HudState hud_state(const PlayerStats& s, bool in_world, bool external_camera)
    {
        HudState h;
        if (!in_world || external_camera)
            return h;
        const auto* w = weapon_definition(s.weapon);
        if (!w || !std::isfinite(s.health) || !std::isfinite(s.armor) || !std::isfinite(s.ammo) ||
            !std::isfinite(s.alternate_ammo))
            return h;
        // Original stats use word-sized health/armor, with an additional ammo display cap of 999.
        const auto count = [](float n) { return static_cast<unsigned>(std::clamp(n, 0.f, 65535.f)); };
        h.visible = true;
        h.health = count(s.health);
        h.armor = count(s.armor);
        h.ammo = count(std::min(s.ammo, 999.f));
        h.alternate_ammo = count(std::min(s.alternate_ammo, 999.f));
        h.uses_ammo = s.ammo_type != AMMO_NONE;
        h.alternate_pool = h.uses_ammo && s.alternate_ammo_type != AMMO_NONE && s.alternate_ammo_type != s.ammo_type;
        h.slot = s.weapon_slot;
        h.slots = s.weapon_slots;
        h.weapon = w->m_szWeaponName;
        return h;
    }
    bool draw_hud(const HudState& h)
    {
        if (!h.visible)
            return false;
        auto* viewport = ImGui::GetMainViewport();
        if (viewport->Size.x < 320 || viewport->Size.y < 160)
            return false;
        auto* draw = ImGui::GetBackgroundDrawList();
        const float scale = std::clamp(viewport->Size.x / 1000.f, .65f, 1.5f);
        const float x = viewport->Pos.x, y = viewport->Pos.y + viewport->Size.y;
        const float pad = 14 * scale, height = 76 * scale;
        const ImU32 bg = IM_COL32(12, 12, 16, 205), white = IM_COL32(245, 235, 225, 255);
        const ImU32 dim = IM_COL32(170, 165, 160, 255), red = IM_COL32(220, 60, 55, 255);
        auto text = [&](float px, float py, float size, ImU32 color, const char* value)
        { draw->AddText(ImGui::GetFont(), size * scale, {px, py}, color, value); };
        draw->AddRectFilled({x + pad, y - height - pad}, {x + viewport->Size.x - pad, y - pad}, bg, 5 * scale);
        char value[96];
        std::snprintf(value, sizeof(value), "HEALTH  %u", h.health);
        text(x + pad * 2, y - height - pad + 10 * scale, 21, h.health <= 25 ? red : white, value);
        std::snprintf(value, sizeof(value), "ARMOR   %u", h.armor);
        text(x + pad * 2, y - pad - 28 * scale, 18, dim, value);
        const float right = x + viewport->Size.x - 190 * scale;
        if (h.uses_ammo)
            std::snprintf(value, sizeof(value), "AMMO  %u", h.ammo);
        else
            std::snprintf(value, sizeof(value), "AMMO  --");
        text(right, y - height - pad + 10 * scale, 21, h.uses_ammo && !h.ammo ? red : white, value);
        if (h.alternate_pool)
        {
            std::snprintf(value, sizeof(value), "ALT    %u", h.alternate_ammo);
            text(right, y - pad - 28 * scale, 18, dim, value);
        }
        // Narrow windows keep health/ammo; the weapon strip is optional presentation.
        if (viewport->Size.x >= 680)
        {
            const float center = x + viewport->Size.x * .5f;
            const auto width = ImGui::GetFont()->CalcTextSizeA(18 * scale, FLT_MAX, 0, h.weapon.c_str()).x;
            text(center - width * .5f, y - height - pad + 10 * scale, 18, white, h.weapon.c_str());
            for (unsigned i = 0; i < 10; ++i)
            {
                const float px = center + (int(i) - 5) * 23 * scale;
                const auto color = h.slot == i ? white : (h.slots[i] ? red : IM_COL32(80, 80, 85, 255));
                value[0] = i == 9 ? '0' : char('1' + i);
                value[1] = 0;
                text(px, y - pad - 28 * scale, 18, color, value);
            }
        }
        return true;
    }
} // namespace OpenBlood2
