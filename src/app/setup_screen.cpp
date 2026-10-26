#include "app/setup_screen.h"
#include "build_info.h"
#include "engine/user_directory.h"
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_platform.h>
#include <algorithm>
#include <imgui.h>
#include <mutex>
#include <spdlog/spdlog.h>

#if !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
#define BLOOD2_SETUP_DIALOGS 1 // Native folder/file pickers (SDL_DIALOG) return filesystem paths.
#else
#define BLOOD2_SETUP_DIALOGS 0
#endif

namespace OpenBlood2
{
    namespace fs = std::filesystem;
    namespace
    {
        const ImVec4 accent{0.80f, 0.12f, 0.10f, 1}, dim{0.62f, 0.60f, 0.58f, 1}, failure{0.95f, 0.35f, 0.30f, 1};
        const char* edition_label(Edition edition)
        {
            return edition == Edition::Demo         ? "Blood II demo (1998, two levels)"
                   : edition == Edition::Nightmares ? "Blood II: The Nightmare Levels (expansion levels not ported yet)"
                                                    : "Blood II: The Chosen";
        }
        const char* instructions()
        {
#if defined(SDL_PLATFORM_IOS)
            return "Copy your Blood II folder (the one holding BLOOD2.REZ) into the OpenBlood2 folder: Files app > "
                   "On My iPhone (or iPad) > OpenBlood2, or Finder > your device > Files. Then tap Rescan. No copy of "
                   "Blood II? Download the free demo below.";
#elif defined(SDL_PLATFORM_ANDROID)
            return "Copy your Blood II files (with BLOOD2.REZ) into /sdcard/openblood2/game, or the demo's "
                   "(B2DEMO.REZ) into /sdcard/openblood2/demo, then tap Rescan. No copy of Blood II? Download the "
                   "free demo below.";
#else
            return "Choose the folder of your Blood II installation (CD, GOG or Steam): the one holding BLOOD2.REZ. "
                   "No copy of Blood II? Download the free demo below.";
#endif
        }
        // The mods folder of an installation, any case.
        fs::path mods_folder(const fs::path& directory)
        {
            std::error_code ec;
            for (auto it = fs::directory_iterator(directory, ec); !ec && it != fs::directory_iterator{};
                 it.increment(ec))
            {
                auto name = it->path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
                if (name == "mods" && it->is_directory(ec))
                    return it->path();
            }
            return {};
        }
        void section(const char* title)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
    } // namespace

    // A native picker's completion, delivered on any thread.
    struct SetupScreen::Dialog
    {
        std::mutex mutex;
        bool pending = false, ready = false;
        std::string path, error;
        static void SDLCALL completed(void* userdata, const char* const* files, int)
        {
            auto& self = *static_cast<Dialog*>(userdata);
            std::lock_guard lock(self.mutex);
            if (!files)
                self.error = SDL_GetError();
            else if (files[0])
            {
                const auto size = SDL_strnlen(files[0], 4097);
                if (size > 4096 || !valid_setup_text(std::string_view(files[0], size), 4096))
                    self.error = "The selected path is invalid or exceeds 4096 bytes";
                else
                    self.path.assign(files[0], size);
            }
            self.ready = true;
        }
    };

    SetupScreen::SetupScreen() : dialog_(std::make_unique<Dialog>()) {}
    SetupScreen::~SetupScreen()
    {
        installer_.cancel();
        // A native dialog still open keeps its completion target alive until it returns.
        std::lock_guard lock(dialog_->mutex);
        if (dialog_->pending && !dialog_->ready)
            (void)dialog_.release();
    }

    bool SetupScreen::open(const Config* running, std::string& error)
    {
        from_game_ = running != nullptr;
        running_.clear();
        if (running)
            running_.push_back({running->demo         ? Edition::Demo
                                : running->nightmares ? Edition::Nightmares
                                                      : Edition::Retail,
                                running->data_dir});
        message.clear();
        if (!user_directory(user_, error) || !setup_path(file_, error))
            return false;
        if (!load_setup(file_, state_, error))
        {
            // A damaged file must not lock the player out: start over and say so.
            spdlog::warn("Setup: {}; starting with an empty setup", error);
            message = "setup.cfg was unreadable and has been reset: " + error;
            state_ = {};
            error.clear();
        }
        if (!state_.selected && !running_.empty())
            state_.selected = running_.front();
        rescan();
        initial_ = state_;
        return true;
    }

    void SetupScreen::rescan()
    {
        entries_ = scan_setup(state_, user_, running_);
        for (const auto& entry : entries_)
            add_mods(entry.installation);
        // The only playable installation is selected for the player.
        if (!state_.selected)
        {
            const SetupEntry* only = nullptr;
            for (const auto& entry : entries_)
                if (entry.error.empty())
                    only = only ? nullptr : &entry;
            if (only)
                state_.selected = only->installation;
        }
    }

    bool SetupScreen::save(std::string& error)
    {
        return save_setup(file_, state_, error);
    }

    void SetupScreen::add_mods(const Installation& installation)
    {
        const auto folder = mods_folder(installation.directory);
        if (folder.empty())
            return;
        std::error_code ec;
        for (auto it = fs::directory_iterator(folder, ec); !ec && it != fs::directory_iterator{}; it.increment(ec))
        {
            auto extension = it->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
            if (extension != ".rez" || !it->is_regular_file(ec) || state_.mods.size() >= max_setup_mods)
                continue;
            const auto archive = fs::absolute(it->path(), ec).lexically_normal();
            if (std::none_of(state_.mods.begin(), state_.mods.end(), [&](const SetupMod& mod)
                             { return mod.installation == installation && mod.archive == archive; }))
                state_.mods.push_back({installation, archive, false});
        }
    }

    void SetupScreen::open_dialog([[maybe_unused]] SDL_Window* window, Pick pick)
    {
#if BLOOD2_SETUP_DIALOGS
        {
            std::lock_guard lock(dialog_->mutex);
            if (dialog_->pending)
                return;
            dialog_->pending = true;
            dialog_->ready = false;
            dialog_->path.clear();
            dialog_->error.clear();
        }
        pick_ = pick;
        static const SDL_DialogFileFilter rez{"REZ archives", "rez"};
        static const SDL_DialogFileFilter zip{"ZIP archives", "zip"};
        if (pick == Pick::Folder)
            SDL_ShowOpenFolderDialog(Dialog::completed, dialog_.get(), window, nullptr, false);
        else
            SDL_ShowOpenFileDialog(Dialog::completed, dialog_.get(), window, pick == Pick::Mod ? &rez : &zip, 1,
                                   nullptr, false);
#else
        (void)pick;
#endif
    }

    void SetupScreen::poll()
    {
        std::string error;
        InstallResult installed;
        if (installer_.poll(installed))
        {
            if (!installed.success)
                message = installed.error;
            else
            {
                spdlog::info("Demo installed: {}", path_text(installed.directory));
                message = "The demo is installed.";
                state_.selected = Installation{Edition::Demo, installed.directory};
                if (!save(error))
                    message = error;
                rescan();
            }
        }
        std::string path;
        {
            std::lock_guard lock(dialog_->mutex);
            if (!dialog_->pending || !dialog_->ready)
                return;
            dialog_->pending = false;
            path = std::move(dialog_->path);
            error = std::move(dialog_->error);
        }
        if (!error.empty())
        {
            message = "The file dialog failed: " + error;
            return;
        }
        if (path.empty()) // Cancelled.
            return;
        const auto chosen = path_from_utf8(path);
        if (pick_ == Pick::Zip)
        {
            message.clear();
            if (!installer_.start(setup_content_folder(user_), chosen))
                message = "Cannot start the demo installation";
            return;
        }
        if (pick_ == Pick::Mod)
        {
            if (!state_.selected || state_.mods.size() >= max_setup_mods)
                return;
            state_.mods.push_back({*state_.selected, fs::absolute(chosen).lexically_normal(), true});
            if (!save(error))
            {
                state_.mods.pop_back();
                message = error;
            }
            return;
        }
        const auto found = folder_installations(chosen);
        if (found.empty())
        {
            message = "No BLOOD2.REZ or B2DEMO.REZ in " + path;
            return;
        }
        const auto previous = state_;
        for (const auto& item : found)
            if (std::find(state_.added.begin(), state_.added.end(), item) == state_.added.end())
                state_.added.push_back(item);
        state_.selected = found.front();
        if (!save(error))
        {
            state_ = previous;
            message = error;
            return;
        }
        message.clear();
        rescan();
    }

    SetupScreen::Result SetupScreen::draw(SDL_Window* window)
    {
        poll();
        Result result = Result::None;
        std::string error;
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::Begin("##setup", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        if (font)
            ImGui::PushFont(font, 0.0f);
        const float em = ImGui::GetFontSize();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {em * 0.7f, em * 0.4f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {em * 0.6f, em * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.07f, 0.06f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.10f, 0.08f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, accent);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.17f, 0.16f, 1));
        const float width = std::min(io.DisplaySize.x - 2 * em, 44 * em);
        ImGui::SetCursorPosX((io.DisplaySize.x - width) / 2);
        ImGui::BeginChild("##column", {width, 0});

        ImGui::PushFont(nullptr, em * 1.8f);
        ImGui::TextUnformatted("OpenBlood 2");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(dim, "%s", build::version);
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(dim, "%s", instructions());
        ImGui::PopTextWrapPos();

        section("Game data");
        const SetupEntry* chosen = nullptr;
        if (entries_.empty())
            ImGui::TextColored(dim, "No Blood II data found yet.");
        for (std::size_t i = 0; i < entries_.size(); ++i)
        {
            const auto& entry = entries_[i];
            ImGui::PushID(int(i));
            const bool playable = entry.error.empty();
            const bool selected = state_.selected && *state_.selected == entry.installation;
            if (selected && playable)
                chosen = &entry;
            ImGui::BeginDisabled(!playable);
            if (ImGui::RadioButton(edition_label(entry.installation.edition), selected && playable) && !selected)
            {
                state_.selected = entry.installation;
                if (!save(error))
                    message = error;
            }
            ImGui::EndDisabled();
            if (entry.added)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    std::erase(state_.added, entry.installation);
                    if (selected)
                        state_.selected.reset();
                    if (!save(error))
                        message = error;
                    ImGui::PopID();
                    rescan();
                    break;
                }
            }
            ImGui::Indent(em * 1.6f);
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(dim, "%s", path_text(entry.installation.directory).c_str());
            if (!playable)
                ImGui::TextColored(failure, "%s", entry.error.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Unindent(em * 1.6f);
            ImGui::PopID();
        }
#if BLOOD2_SETUP_DIALOGS
        if (ImGui::Button("Add folder..."))
            open_dialog(window, Pick::Folder);
        ImGui::SameLine();
#endif
        if (ImGui::Button("Rescan"))
            rescan();
        if (!storage_access())
        {
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(failure, "OpenBlood2 needs \"All files access\" to read /sdcard/openblood2.");
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Allow file access"))
                request_storage_access();
        }

        section("Free demo");
        if (installer_.pending())
        {
            const auto stage = installer_.progress.stage.load();
            const char* text = stage == InstallStage::Downloading  ? "Downloading the demo..."
                               : stage == InstallStage::Extracting ? "Installing the demo..."
                                                                   : "Checking the demo files...";
            const auto total = installer_.progress.total.load();
            ImGui::ProgressBar(total ? float(installer_.progress.bytes.load()) / float(total) : 0, {width - 7 * em, 0},
                               text);
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                installer_.cancel();
        }
        else if (std::any_of(entries_.begin(), entries_.end(), [](const SetupEntry& entry)
                             { return entry.installation.edition == Edition::Demo && entry.error.empty(); }))
            ImGui::TextColored(dim, "The demo is installed.");
        else
        {
            if (ImGui::Button("Download the demo"))
            {
                message.clear();
                if (!installer_.start(setup_content_folder(user_)))
                    message = "Cannot start the demo download";
            }
#if BLOOD2_SETUP_DIALOGS
            ImGui::SameLine();
            if (ImGui::Button("Install from ZIP..."))
                open_dialog(window, Pick::Zip);
#endif
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(dim, "The original 1998 demo, 37.7 MB from Archive.org, checked and installed in "
                                    "OpenBlood2's own folder.");
            ImGui::PopTextWrapPos();
        }

        section("Mods");
        if (!state_.selected)
            ImGui::TextColored(dim, "Choose the game data first.");
        else
        {
            unsigned rows = 0;
            for (std::size_t i = 0; i < state_.mods.size(); ++i)
            {
                auto& mod = state_.mods[i];
                if (mod.installation != *state_.selected)
                    continue;
                ++rows;
                ImGui::PushID(int(1000 + i));
                bool enabled = mod.enabled;
                if (ImGui::Checkbox(path_text(mod.archive.filename()).c_str(), &enabled))
                {
                    mod.enabled = enabled;
                    if (!save(error))
                        message = error;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", path_text(mod.archive).c_str());
                // Load order: later entries take priority.
                std::size_t swap = i;
                ImGui::SameLine();
                if (ImGui::SmallButton("Up"))
                    for (std::size_t j = i; j-- > 0;)
                        if (state_.mods[j].installation == mod.installation)
                        {
                            swap = j;
                            break;
                        }
                ImGui::SameLine();
                if (ImGui::SmallButton("Down"))
                    for (std::size_t j = i + 1; j < state_.mods.size(); ++j)
                        if (state_.mods[j].installation == mod.installation)
                        {
                            swap = j;
                            break;
                        }
                ImGui::SameLine();
                const bool remove = ImGui::SmallButton("Remove");
                ImGui::PopID();
                if (swap != i)
                {
                    std::swap(state_.mods[i], state_.mods[swap]);
                    if (!save(error))
                        message = error;
                    break;
                }
                if (remove)
                {
                    state_.mods.erase(state_.mods.begin() + std::ptrdiff_t(i));
                    if (!save(error))
                        message = error;
                    break;
                }
            }
            if (!rows)
                ImGui::TextColored(dim, "No mods for this installation.");
#if BLOOD2_SETUP_DIALOGS
            if (ImGui::Button("Add mod..."))
                open_dialog(window, Pick::Mod);
#endif
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(dim, "Data-only .rez archives; those in the game folder's \"mods\" folder are listed "
                                    "automatically. Later entries take priority.");
            ImGui::PopTextWrapPos();
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (!message.empty())
        {
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(failure, "%s", message.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::BeginDisabled(!chosen || installer_.pending());
        if (ImGui::Button("Play", {8 * em, 0}))
        {
            config_ = setup_config(state_, chosen->installation);
            // From the game, an unchanged setup returns to it instead of restarting.
            result = from_game_ && state_ == initial_ ? Result::Back : Result::Play;
            if (result == Result::Play && !save(error))
            {
                message = error;
                result = Result::None;
            }
        }
        ImGui::EndDisabled();
        if (from_game_)
        {
            ImGui::SameLine();
            // Esc and the gamepad's B also return, as the menus' Back does.
            if (ImGui::Button("Back", {8 * em, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))
                result = Result::Back;
        }
        if (quit_available)
        {
            ImGui::SameLine();
            if (ImGui::Button("Quit", {8 * em, 0}))
                result = Result::Quit;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        if (font)
            ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        return result;
    }
} // namespace OpenBlood2
