#pragma once
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    // LithTech string keys contain commands separated by ';', with quoted arguments.
    // Storage and argv live through the synchronous callback; they never alias the
    // immutable ABC data. Malformed/overlong commands are rejected, not truncated.
    inline bool model_commands(std::string_view text, std::vector<std::vector<std::string>>& out)
    {
        std::vector<std::string> words;
        std::size_t at = 0;
        while (at < text.size())
        {
            while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
                ++at;
            if (at == text.size())
                break;
            if (text[at] == ';')
            {
                if (!words.empty())
                {
                    if (out.size() == 64)
                        return false;
                    out.push_back(std::move(words));
                }
                words.clear();
                ++at;
                continue;
            }
            std::string word;
            if (text[at] == '"')
            {
                ++at;
                while (at < text.size() && text[at] != '"')
                    word.push_back(text[at++]);
                if (at == text.size())
                    return false;
                ++at;
            }
            else
                while (at < text.size() && text[at] != ';' && !std::isspace(static_cast<unsigned char>(text[at])))
                    word.push_back(text[at++]);
            if (words.size() == 64 || out.size() == 64)
                return false;
            words.push_back(std::move(word));
        }
        if (!words.empty())
        {
            if (out.size() == 64)
                return false;
            out.push_back(std::move(words));
        }
        return true;
    }
} // namespace OpenBlood2
