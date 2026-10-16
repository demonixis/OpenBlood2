#pragma once
#include "app/inspector.h"
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    // UI-only smoke driver for the existing gpu.movement scenario. Game input
    // still enters through SDL events in main; no game state is modified here.
    struct InspectorControlsSmoke
    {
        ImVec2 collapse_button{};
        bool folded = false;
        std::uint64_t updates = 0;

        void before_frame(std::uint64_t frame)
        {
            if (frame == 172 || frame == 173 || frame == 187 || frame == 188)
            {
                auto& io = ImGui::GetIO();
                io.AddMousePosEvent(collapse_button.x, collapse_button.y);
                io.AddMouseButtonEvent(0, frame == 172 || frame == 187);
            }
        }
        bool after_draw(std::uint64_t frame, const GameSession& game, const Inspector& inspector)
        {
            if (frame < 170 || frame > 195)
                return true;
            const auto* window = ImGui::FindWindowByName("Blood II | In-game inspector");
            if (!window || (window->Flags & (ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)))
                return false;
            // Native title-bar arrow location in the pinned Dear ImGui layout.
            const auto& style = ImGui::GetStyle();
            collapse_button = {window->Pos.x + style.FramePadding.x + ImGui::GetFontSize() * .5f,
                               window->Pos.y + window->TitleBarHeight * .5f};
            if (frame == 176)
            {
                folded = inspector.collapsed && window->Size.y < 80 && inspector.keyboard_movement;
                updates = game.client_shell.updates;
                if (!folded)
                {
                    spdlog::error("Inspector title-arrow click did not fold the window");
                    return false;
                }
            }
            if (frame == 185)
            {
                if (!folded || !inspector.collapsed || game.client_shell.updates <= updates ||
                    !game.client_shell.movement_input.crouch || !game.client_shell.move_mgr.ready())
                    return false;
                spdlog::info("Collapsed inspector: live updates={}, original keyboard crouch active",
                             game.client_shell.updates - updates);
            }
            if (frame == 195)
            {
                const bool restored = folded && !inspector.collapsed && window->Size.y > 300;
                spdlog::info("Inspector title-arrow fold/unfold: restored={}", restored);
                return restored;
            }
            return true;
        }
    };
} // namespace OpenBlood2
