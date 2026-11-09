#include "app/multiplayer_screen.h"
#include "engine/world.h"
#include "game/localized_font.h"
#include "game/original/MenuIds.h"
#include "game/save_slots.h"
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <charconv>
#include <utility>

namespace OpenBlood2
{
    namespace
    {
        struct Label
        {
            std::string_view key;
            const char* english;
        };
        constexpr Label player_name{"network.name", "Name"}, address{"network.address", "Address or host:port"},
            port{"network.udp_port", "UDP port"}, score{"network.score_limit", "Score limit"},
            minutes{"network.time_limit", "Time limit (minutes)"},
            flags{"network.points_per_capture", "Points per capture"};
        constexpr Label modes[]{{"network.bloodbath", "Bloodbath"}, {"network.team", "Team"}, {"network.ctf", "CTF"}};
        constexpr Label teams[]{{"network.automatic", "Automatic"}, {"network.blue", "Blue"}, {"network.red", "Red"}};
        constexpr Label endings[]{{"network.frags_score", "Frags / score"},
                                  {"network.time", "Time"},
                                  {"network.first_limit", "First limit reached"},
                                  {"network.no_limit", "No limit"}};
        struct CharacterLabel
        {
            unsigned resource;
            const char* english;
        };
        constexpr CharacterLabel characters[]{{IDS_MENU_CHARSELECT_CALEB, "Caleb"},
                                              {IDS_MENU_CHARSELECT_OPHELIA, "Ophelia"},
                                              {IDS_MENU_CHARSELECT_ISHMAEL, "Ishmael"},
                                              {IDS_MENU_CHARSELECT_GABRIELLA, "Gabriella"}};

        std::size_t previous_character(const std::string& value, std::size_t cursor)
        {
            const char* position = value.data() + cursor;
            SDL_StepBackUTF8(value.data(), &position);
            return std::size_t(position - value.data());
        }

        std::size_t next_character(const std::string& value, std::size_t cursor)
        {
            const char* position = value.data() + cursor;
            std::size_t remaining = value.size() - cursor;
            SDL_StepUTF8(&position, &remaining);
            return std::size_t(position - value.data());
        }
    } // namespace

    const char* MultiplayerScreen::label(std::string_view key, const char* english) const
    {
        if (font_)
        {
            const auto value = locale_->text(key);
            if (!value.empty() && font_->supports(value))
                return value.data();
        }
        return english;
    }

    bool MultiplayerScreen::open(Menu& menu, const Resources& resources, const Config& config,
                                 const Localization& locale, const LocalizedFont* font, std::string& error)
    {
        close();
        if (!setup_.load(resources, config, error))
            return false;
        locale_ = &locale;
        font_ = font;
        if (!custom_name_)
        {
            Net::JoinProfile profile;
            profile.name = label("network.default_player_name", "Player");
            if (!Net::valid_profile(profile))
                profile.name = "Player";
            std::copy(profile.name.c_str(), profile.name.c_str() + profile.name.size() + 1, setup_.name.begin());
        }
        for (const auto& path : setup_.maps)
        {
            std::vector<std::uint8_t> header;
            std::string info, ignored;
            if (resources.read(path, header, ignored, 16 + 65536) && read_world_info(header, info, ignored))
                map_titles_.emplace(path, world_title(info, path));
        }
        refresh();
        menu.build(MenuPage::Bloodbath);
        return true;
    }

    MultiplayerScreen::~MultiplayerScreen()
    {
        close();
    }

    void MultiplayerScreen::close()
    {
        if (edit_window_)
            SDL_StopTextInput(edit_window_);
        edit_window_ = nullptr;
        browsing_ = false;
        browser_.close();
        page_ = MenuPage::Main;
        dirty_ = false;
        error_.clear();
        error_key_ = {};
        discovery_error_.clear();
        map_titles_.clear();
        lan_rows_.clear();
        pending.reset();
    }

    void MultiplayerScreen::start(bool host)
    {
        Options options;
        if (setup_.request(host, options, error_, &error_key_))
            pending = std::move(options);
    }

    void MultiplayerScreen::refresh()
    {
        if (!browsing_)
            browsing_ = browser_.open(false, discovery_error_);
        refresh_ = -10;
    }

    std::string MultiplayerScreen::map_label(const std::string& path) const
    {
        if (font_)
        {
            const auto value = locale_->world(path);
            if (!value.empty() && font_->supports(value))
                return std::string(value);
        }
        const auto title = map_titles_.find(path);
        return title == map_titles_.end() ? path : title->second;
    }

    void MultiplayerScreen::update(Menu& menu)
    {
        if (!menu.in_multiplayer())
        {
            if (page_ != MenuPage::Main)
                close();
            return;
        }
        if (menu.page != page_)
        {
            page_ = menu.page;
            dirty_ = true;
            error_.clear();
            error_key_ = {};
        }
        const double now = double(SDL_GetTicksNS()) / 1e9;
        if (browsing_)
        {
            if (now - refresh_ >= 2)
            {
                browser_.query(now, discovery_error_);
                refresh_ = now;
            }
            std::string receive_error;
            if (!browser_.pump(now, nullptr, receive_error))
            {
                discovery_error_ = std::move(receive_error);
                browsing_ = false;
                browser_.close();
            }
        }
        if (std::exchange(dirty_, false) || menu.page == MenuPage::MultiplayerLan)
            rebuild(menu);

        std::string status;
        if (!error_.empty())
        {
            status = label(error_key_, error_.c_str());
            if (font_ && error_key_ == "network.invalid_host")
            {
                auto detail = locale_->format(error_key_, error_, {std::to_string(Net::discovery_port)});
                if (font_->supports(detail))
                    status = std::move(detail);
            }
        }
        else if (menu.page == MenuPage::MultiplayerLan)
        {
            if (!discovery_error_.empty())
            {
                status = discovery_error_;
                if (font_)
                {
                    auto detail = locale_->format("network.discovery_failed", status, {discovery_error_});
                    if (font_->supports(detail))
                        status = std::move(detail);
                }
            }
            else if (lan_rows_.empty())
                status = label("network.searching", "Searching. If a game does not appear, connect to its address.");
        }
        else if (menu.page == MenuPage::MultiplayerRotation)
            status = label("network.rotation_help",
                           "The starting map plays first. It repeats in a loop with the list below.");
        else if (menu.page == MenuPage::MultiplayerHost)
            status =
                label("network.lan_help", "The game is visible on the local network. Leaving as host ends the game.");
        if (error_.empty() && menu.in_world &&
            (menu.page == MenuPage::Bloodbath || menu.page == MenuPage::MultiplayerHost ||
             menu.page == MenuPage::MultiplayerJoin))
        {
            if (!status.empty())
                status += '\n';
            status +=
                label("network.replaces_solo", "Starting a multiplayer game ends the current single-player session.");
        }
        if (menu.multiplayer_status != status)
        {
            menu.multiplayer_status = std::move(status);
            ++menu.revision;
        }
    }

    void MultiplayerScreen::rebuild(Menu& menu)
    {
        std::vector<MenuItem> rows;
        const auto row = [&](int option, std::string text, bool enabled = true, bool adjustable = false)
        {
            auto& item = rows.emplace_back();
            item.command = option;
            item.literal = std::move(text);
            item.enabled = enabled;
            item.adjustable = adjustable;
        };
        const auto action = [&](Option option, std::string_view key, const char* english, bool enabled = true)
        { row(option, label(key, english), enabled); };
        const auto original = [&](Option option, unsigned id, const char* english, std::string_view key)
        {
            row(option, english);
            rows.back().string_id = id;
            rows.back().localization_key = key;
        };
        const auto value = [&](Option option, Label field, std::string_view current, bool adjustable = true)
        {
            row(option, std::string(label(field.key, field.english)) + "  [" + std::string(current) + "]", true,
                adjustable);
        };
        const auto choice = [&](Option option, Label field, Label current)
        { value(option, field, label(current.key, current.english)); };
        const auto toggle = [&](Option option, Label field, bool on)
        { value(option, field, on ? label("menu.on_short", "On") : label("menu.off_short", "Off")); };
        const auto maps = menu.page == MenuPage::MultiplayerHost || menu.page == MenuPage::MultiplayerRotation
                              ? setup_.mode_maps()
                              : std::vector<std::string>{};
        std::optional<Net::ServerAddress> selected_server;
        if (menu.page == MenuPage::MultiplayerLan && std::size_t(menu.selected) < menu.items.size())
        {
            const int i = menu.items[menu.selected].command - LanServer;
            if (i >= 0 && std::size_t(i) < lan_rows_.size())
                selected_server = lan_rows_[i];
        }
        int selected = -1;
        switch (menu.page)
        {
            case MenuPage::Bloodbath:
                original(Join, IDS_MENU_BLOODBATH_JOIN, "JOIN GAME", "network.join");
                original(Host, IDS_MENU_BLOODBATH_HOST, "HOST GAME", "network.host");
                original(Profile, IDS_MENU_BLOODBATH_CHARSETUP, "CHARACTER SETUP", "network.character_setup");
                break;
            case MenuPage::MultiplayerProfile:
            {
                const auto& character = characters[setup_.character];
                const auto name = locale_->text(character.resource, character.english);
                value(Name, player_name, setup_.name.data(), false);
                value(Character, {"network.character", "Character"},
                      font_ && font_->supports(name) ? name : character.english);
                choice(Team, {"network.preferred_team", "Preferred team"}, teams[setup_.team]);
                break;
            }
            case MenuPage::MultiplayerHost:
                choice(Mode, {"network.mode", "Mode"}, modes[setup_.mode]);
                if (maps.empty())
                    row(Map, label("network.no_map", "No map"), false);
                else
                    value(Map, {"network.map", "Map"}, map_label(maps[setup_.map]));
                value(Port, port, std::to_string(setup_.port));
                choice(End, {"network.match_end", "Match end"}, endings[setup_.end]);
                if (setup_.end == 0 || setup_.end == 2)
                    value(Score, score, std::to_string(setup_.frags));
                if (setup_.end == 1 || setup_.end == 2)
                    value(Minutes, minutes, std::to_string(setup_.minutes));
                if (setup_.mode)
                {
                    toggle(FriendlyFire, {"network.friendly_fire", "Friendly fire"}, setup_.friendly_fire);
                    toggle(NegativeFrags, {"network.negative_team_frags", "Negative team frags"},
                           setup_.negative_team_frags);
                }
                if (setup_.mode == 2)
                {
                    value(FlagValue, flags, std::to_string(setup_.flag_value));
                    toggle(OnlyFlags, {"network.only_flag_scores", "Only flag captures score"},
                           setup_.only_flag_scores);
                    toggle(TeamSize, {"network.scale_team_size", "Scale score by team size"}, setup_.use_team_size);
                }
                action(Rotation, "network.rotation", "Map rotation");
                action(Start, "network.start_game", "Start game", !maps.empty());
                break;
            case MenuPage::MultiplayerRotation:
                if (!maps.empty())
                    value(Map, {"network.map", "Map"}, map_label(maps[setup_.map]));
                action(AddMap, "network.add_map", "Add selected map", !maps.empty() && setup_.rotation.size() < 49);
                action(ClearRotation, "network.clear", "Clear", !setup_.rotation.empty());
                for (std::size_t i = 0; i < setup_.rotation.size(); ++i)
                    row(-1, std::to_string(i + 1) + ". " + map_label(setup_.rotation[i]));
                break;
            case MenuPage::MultiplayerJoin:
                value(Address, address, setup_.address.data(), false);
                action(Connect, "network.connect", "Connect");
                action(Lan, "network.lan_games", "Local network games");
                break;
            case MenuPage::MultiplayerLan:
                action(Refresh, "network.refresh", "Refresh");
                lan_rows_.clear();
                for (const auto& s : browser_.servers())
                {
                    const bool compatible = s.offer.protocol == Net::protocol_version;
                    const bool full = s.offer.players >= s.offer.capacity;
                    if (selected_server && selected_server->host == s.address.host &&
                        selected_server->port == s.address.port)
                        selected = int(rows.size());
                    row(compatible && !full ? LanServer + int(lan_rows_.size()) : -1,
                        s.offer.name + "  [" + std::to_string(s.offer.players) + "/" +
                            std::to_string(s.offer.capacity) + "]  " + s.address.host + ":" +
                            std::to_string(s.address.port));
                    const auto& mode = modes[s.offer.mode == 1 ? 0 : s.offer.mode == 2 ? 2 : 1];
                    std::string details = std::string(label(mode.key, mode.english)) + " / " + map_label(s.offer.world);
                    if (!compatible)
                        details += " / " + std::string(label("network.incompatible", "Incompatible game version"));
                    else if (full)
                        details += " / " + std::string(label("network.full", "Game full"));
                    row(-1, std::move(details));
                    lan_rows_.push_back(s.address);
                }
                if (selected_server && selected < 0)
                    selected = 0;
                break;
            default:
                return;
        }
        if (menu.page != MenuPage::Bloodbath)
            action(Back, "network.back", "Back");
        menu.set_multiplayer_rows(std::move(rows));
        if (selected >= 0 && selected != menu.selected && menu.enabled(menu.items[selected]))
        {
            menu.selected = selected;
            ++menu.revision;
        }
    }

    int* MultiplayerScreen::number(Option option, int& minimum, int& maximum)
    {
        minimum = 0;
        maximum = int(MultiplayerRules::frag_limit);
        switch (option)
        {
            case Port:
                minimum = 1;
                maximum = 65535;
                return &setup_.port;
            case Score:
                return &setup_.frags;
            case Minutes:
                maximum = int(MultiplayerRules::minute_limit);
                return &setup_.minutes;
            case FlagValue:
                return &setup_.flag_value;
            default:
                return nullptr;
        }
    }

    void MultiplayerScreen::command(Menu& menu, const MenuCommand& command, SDL_Window* window)
    {
        // Resolve only rows from the current page. Back may have rebuilt it in this event batch.
        if (menu.page != page_ || pending)
            return;
        const auto option = Option(command.option);
        const int direction = command.direction == 0 ? 1 : command.direction;
        const auto cycle = [direction](int& index, int count)
        {
            if (count > 0)
                index = (index + count + direction) % count;
        };
        int minimum, maximum;
        error_.clear();
        error_key_ = {};
        dirty_ = true;
        if (int* value = number(option, minimum, maximum))
        {
            if (command.direction)
            {
                int next = *value + direction;
                if (option == Port && next == Net::discovery_port)
                    next += direction;
                *value = std::clamp(next, minimum, maximum);
            }
            else
                begin_edit(menu, option, window);
            return;
        }
        switch (option)
        {
            case Join:
                menu.build(MenuPage::MultiplayerJoin);
                break;
            case Host:
                menu.build(MenuPage::MultiplayerHost);
                break;
            case Profile:
                menu.build(MenuPage::MultiplayerProfile);
                break;
            case Rotation:
                menu.build(MenuPage::MultiplayerRotation);
                break;
            case Lan:
                menu.build(MenuPage::MultiplayerLan);
                break;
            case Back:
                menu.back();
                break;
            case Name:
            case Address:
                begin_edit(menu, option, window);
                break;
            case Character:
                cycle(setup_.character, 4);
                break;
            case Team:
                cycle(setup_.team, 3);
                break;
            case Mode:
                cycle(setup_.mode, 3);
                setup_.map = 0;
                setup_.rotation.clear();
                break;
            case Map:
                cycle(setup_.map, int(setup_.mode_maps().size()));
                break;
            case End:
                cycle(setup_.end, 4);
                break;
            case FriendlyFire:
                setup_.friendly_fire = !setup_.friendly_fire;
                break;
            case NegativeFrags:
                setup_.negative_team_frags = !setup_.negative_team_frags;
                break;
            case OnlyFlags:
                setup_.only_flag_scores = !setup_.only_flag_scores;
                break;
            case TeamSize:
                setup_.use_team_size = !setup_.use_team_size;
                break;
            case AddMap:
            {
                const auto maps = setup_.mode_maps();
                if (!maps.empty() && setup_.rotation.size() < 49)
                    setup_.rotation.push_back(maps[setup_.map]);
                break;
            }
            case ClearRotation:
                setup_.rotation.clear();
                break;
            case Start:
                start(true);
                break;
            case Connect:
                start(false);
                break;
            case Refresh:
                refresh();
                break;
            default:
            {
                const int i = command.option - LanServer;
                if (i >= 0 && std::size_t(i) < lan_rows_.size())
                {
                    const auto& server = lan_rows_[i];
                    const std::string endpoint = server.host + ":" + std::to_string(server.port);
                    std::copy(endpoint.c_str(), endpoint.c_str() + endpoint.size() + 1, setup_.address.begin());
                    start(false);
                }
                break;
            }
        }
    }

    void MultiplayerScreen::begin_edit(Menu& menu, Option field, SDL_Window* window)
    {
        if (editing())
            return;
        if (!SDL_StartTextInput(window))
        {
            error_ = "Text input unavailable";
            error_key_ = "hud.text_input_unavailable";
            return;
        }
        edit_window_ = window;
        field_ = field;
        int minimum, maximum;
        if (const int* value = number(field, minimum, maximum))
            draft_ = std::to_string(*value);
        else
            draft_ = field == Name ? setup_.name.data() : setup_.address.data();
        cursor_ = draft_.size();
        select_all_ = true;
        show_edit(menu);
    }

    void MultiplayerScreen::cancel_edit(Menu& menu)
    {
        if (!editing())
            return;
        SDL_StopTextInput(edit_window_);
        edit_window_ = nullptr;
        menu.multiplayer_edit.reset();
        ++menu.revision;
    }

    void MultiplayerScreen::finish_edit(Menu& menu)
    {
        int minimum, maximum;
        if (int* value = number(field_, minimum, maximum))
        {
            int parsed;
            const auto end = draft_.data() + draft_.size();
            const auto result = std::from_chars(draft_.data(), end, parsed);
            if (result.ec != std::errc{} || result.ptr != end || parsed < minimum || parsed > maximum ||
                (field_ == Port && parsed == Net::discovery_port))
            {
                const auto lower = std::to_string(minimum), upper = std::to_string(maximum);
                error_ = "Enter a value from " + lower + " to " + upper + ".";
                auto detail = font_ ? locale_->format("network.numeric_range", error_, {lower, upper}) : error_;
                if (field_ == Port && result.ec == std::errc{} && parsed == Net::discovery_port)
                {
                    const auto reserved = std::to_string(Net::discovery_port);
                    error_ = "Port " + reserved + " is reserved for LAN discovery.";
                    detail = font_ ? locale_->format("network.discovery_port_reserved", error_, {reserved}) : error_;
                }
                if (font_ && font_->supports(detail))
                    error_ = std::move(detail);
                cancel_edit(menu);
                return;
            }
            *value = parsed;
        }
        else
        {
            char* target = field_ == Name ? setup_.name.data() : setup_.address.data();
            if (field_ == Name && draft_ != target)
                custom_name_ = true;
            std::copy(draft_.c_str(), draft_.c_str() + draft_.size() + 1, target);
        }
        dirty_ = true;
        cancel_edit(menu);
    }

    void MultiplayerScreen::show_edit(Menu& menu)
    {
        // Keep the caret visible on the fixed-size canvas, including for long hostnames.
        std::size_t first = cursor_, last = cursor_;
        int count = 0;
        for (; count < 17 && first > 0; ++count)
            first = previous_character(draft_, first);
        for (; count < 34 && last < draft_.size(); ++count)
            last = next_character(draft_, last);
        for (; count < 34 && first > 0; ++count)
            first = previous_character(draft_, first);
        std::string text = first > 0 ? "..." : "";
        if (select_all_)
            text = "[ " + text + draft_.substr(first) + " ]";
        else
            text += draft_.substr(first, cursor_ - first) + "_" + draft_.substr(cursor_, last - cursor_);
        if (last < draft_.size())
            text += "...";
        if (menu.multiplayer_edit != text)
        {
            menu.multiplayer_edit = std::move(text);
            ++menu.revision;
        }
    }

    void MultiplayerScreen::edit_event(Menu& menu, const SDL_Event& event, bool accept, bool back)
    {
        if (back)
        {
            cancel_edit(menu);
            return;
        }
        if (accept)
        {
            finish_edit(menu);
            return;
        }
        const auto insert = [&](std::string_view input)
        {
            const bool numeric = field_ != Name && field_ != Address;
            const std::size_t capacity = field_ == Name ? setup_.name.size() - 1
                                         : numeric      ? 7
                                                        : setup_.address.size() - 1;
            const char* next = input.data();
            std::size_t remaining = input.size();
            std::string text;
            const auto available = capacity - (select_all_ ? 0 : draft_.size());
            while (remaining)
            {
                const char* start = next;
                const auto c = SDL_StepUTF8(&next, &remaining);
                if (c < 32 || (c >= 127 && c <= 159) || c == SDL_INVALID_UNICODE_CODEPOINT ||
                    (numeric && (c < '0' || c > '9')) || (field_ == Address && c > 126))
                    continue;
                const auto bytes = std::size_t(next - start);
                if (text.size() + bytes > available)
                    break;
                text.append(start, bytes);
            }
            if (text.empty())
                return;
            if (select_all_)
            {
                draft_.clear();
                cursor_ = 0;
                select_all_ = false;
            }
            draft_.insert(cursor_, text);
            cursor_ += text.size();
        };
        if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text)
            insert(event.text.text);
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            const bool shortcut = (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
            switch (event.key.scancode)
            {
                case SDL_SCANCODE_A:
                    if (shortcut)
                    {
                        select_all_ = true;
                        cursor_ = draft_.size();
                    }
                    break;
                case SDL_SCANCODE_V:
                    if (shortcut)
                    {
                        char* clipboard = SDL_GetClipboardText();
                        if (clipboard)
                            insert(clipboard);
                        SDL_free(clipboard);
                    }
                    break;
                case SDL_SCANCODE_HOME:
                    cursor_ = 0;
                    select_all_ = false;
                    break;
                case SDL_SCANCODE_END:
                    cursor_ = draft_.size();
                    select_all_ = false;
                    break;
                case SDL_SCANCODE_LEFT:
                    cursor_ = select_all_ ? 0 : previous_character(draft_, cursor_);
                    select_all_ = false;
                    break;
                case SDL_SCANCODE_RIGHT:
                    cursor_ = select_all_ ? draft_.size() : next_character(draft_, cursor_);
                    select_all_ = false;
                    break;
                case SDL_SCANCODE_BACKSPACE:
                case SDL_SCANCODE_DELETE:
                    if (select_all_)
                    {
                        draft_.clear();
                        cursor_ = 0;
                        select_all_ = false;
                    }
                    else if (event.key.scancode == SDL_SCANCODE_BACKSPACE)
                    {
                        const auto previous = previous_character(draft_, cursor_);
                        draft_.erase(previous, cursor_ - previous);
                        cursor_ = previous;
                    }
                    else
                        draft_.erase(cursor_, next_character(draft_, cursor_) - cursor_);
                    break;
                default:
                    break;
            }
        }
        show_edit(menu);
    }
} // namespace OpenBlood2
