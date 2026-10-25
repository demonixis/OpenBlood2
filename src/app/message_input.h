#pragma once
#include <SDL3/SDL_events.h>
#include <optional>
#include <string>

namespace OpenBlood2
{
    // CInputLine's 80-byte buffer and ASCII entry, over SDL text events instead of
    // virtual-key translation. Text is never interpreted as a console command.
    struct MessageInput
    {
        bool active = false;
        std::string text;
        std::optional<std::string> submitted;
        void cancel()
        {
            active = false;
            text.clear();
        }
        void open()
        {
            cancel();
            submitted.reset();
            active = true;
        }
        void event(const SDL_Event& e)
        {
            if (e.type == SDL_EVENT_TEXT_INPUT && e.text.text)
            {
                for (const unsigned char c : std::string_view(e.text.text))
                    if (c >= 32 && c <= 126 && text.size() < 79)
                        text += char(c);
            }
            if (e.type != SDL_EVENT_KEY_DOWN)
                return;
            switch (e.key.scancode)
            {
                case SDL_SCANCODE_ESCAPE:
                    cancel();
                    break;
                case SDL_SCANCODE_HOME:
                    text.clear();
                    break;
                case SDL_SCANCODE_BACKSPACE:
                    if (!text.empty())
                        text.pop_back();
                    break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    if (!e.key.repeat)
                    {
                        if (!text.empty())
                            submitted = text;
                        cancel();
                    }
                    break;
                default:
                    break;
            }
        }
    };
} // namespace OpenBlood2
