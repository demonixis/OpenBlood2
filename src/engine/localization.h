#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    class Resources;

    enum class Language : std::uint8_t
    {
        English,
        French,
        ChineseSimplified,
        German,
        Italian,
        Spanish,
        PortugueseBrazil,
        Korean,
    };

    inline constexpr std::array supported_languages{Language::English, Language::French, Language::ChineseSimplified,
                                                    Language::German, Language::Italian, Language::Spanish,
                                                    Language::PortugueseBrazil, Language::Korean};
    inline constexpr std::span<const Language> languages()
    {
        return supported_languages;
    }
    std::string_view language_tag(Language);
    std::string_view language_name(Language);
    std::optional<Language> parse_language(std::string_view tag);

    enum class TextKey : std::uint8_t
    {
        None,
        Language,
        Setup,
        Cheats,
        UnavailableOptions,
        FirstPerson,
        ThirdPerson,
        Count,
    };

    // Paths are fixed application-owned asset names relative to the packaged resources.
    bool read_application_asset(std::string_view relative, std::size_t max_size, std::vector<std::uint8_t>& out,
                                std::string& error);
    bool localization_resource_overridden(const Resources&, std::string_view path);

    class Localization
    {
      public:
        // Both load paths restore English on failure. A valid catalog may omit individual keys.
        bool load(Language, std::string& error);
        bool load_catalog(Language, std::string_view ini, std::string& error);
        Language language() const { return language_; }
        std::string_view text(TextKey, std::string_view english = {}) const;
        std::string_view text(std::string_view key, std::string_view english = {}) const;
        std::string_view text(unsigned resource_id, std::string_view english = {}) const;
        // Stock dialogue keys are the lowercase WAV filename without its extension.
        std::string_view dialogue(std::string_view wav_path) const;
        // Each numbered argument must appear in a valid translation. Substitutions are literal text.
        std::string format(std::string_view key, std::string_view english,
                           std::initializer_list<std::string_view> arguments) const;

      private:
        Language language_ = Language::English;
        std::map<std::string, std::string, std::less<>> labels_;
        std::map<unsigned, std::string> resource_labels_;
    };
} // namespace OpenBlood2
