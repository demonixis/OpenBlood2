#pragma once
#include "client/frag_table.h"
#include <SDL3/SDL.h>
namespace OpenBlood2
{
    // Network-only overlay state; showing scores never gates movement or pauses the host.
    struct ScoreboardControl
    {
        bool visible = false, dead = false;
        DBYTE team_column = 0; // CFragInfo::TurnOn cycles 1/2; no implicit first selection.
        void event(const SDL_Event& event, SDL_WindowID window, bool accepts_input)
        {
            if (accepts_input &&
                ((event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == window &&
                  event.key.scancode == SDL_SCANCODE_TAB && !event.key.repeat) ||
                 (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)))
            {
                visible = !visible;
                if (visible)
                    team_column = team_column % 2 + 1;
            }
        }
        void receive(const Net::PrivateEvent& event)
        {
            if (std::holds_alternative<Net::LevelChanging>(event.value))
                visible = true;
            if (std::holds_alternative<PlayerDead>(event.value))
            {
                dead = true;
                visible = true;
            }
            else if (const auto* state = std::get_if<Net::CameraState>(&event.value); state && state->alive && dead)
            {
                dead = false;
                visible = false;
            } // Validated revival/rescue state; never slow-death health heuristics.
        }
    };
    class ScoreboardRenderer
    {
      public:
        explicit ScoreboardRenderer(SDL_GPUDevice* device) : device_(device) {}
        ~ScoreboardRenderer();
        ScoreboardRenderer(const ScoreboardRenderer&) = delete;
        ScoreboardRenderer& operator=(const ScoreboardRenderer&) = delete;
        bool draw(const ClientFragTable&, std::string& error);

      private:
        SDL_GPUDevice* device_;
        SDL_GPUTexture* texture_ = nullptr;
        SDL_GPUTransferBuffer* transfer_ = nullptr;
        std::uint64_t revision_ = UINT64_MAX;
        unsigned width_ = 0, height_ = 0;
    };
} // namespace OpenBlood2
