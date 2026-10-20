#pragma once
#include "app/menu_input.h"
#include "app/session_playback.h"
#include "game/save_slots.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

// Application half of slots, Quick and Restart (save_load_contracts lot 6): menu commands, the
// F5/F6 requests and the death Load menu, consumed at the owning frame boundary, never inside
// SDL or object callbacks. Headless, so menu traversal and key tests run without a GPU.
namespace OpenBlood2
{
    // A brief notice drawn over play (original DoMessageBox("Quicksave...")), on UI time.
    struct SaveNotice
    {
        std::string text;
        double remaining = 0;
        std::uint64_t shown = 0;
        void show(std::string message, double seconds = 1.5)
        {
            text = std::move(message);
            remaining = seconds;
            ++shown;
        }
        void advance(double seconds)
        {
            remaining = std::max(0.0, remaining - std::max(0.0, seconds));
            if (remaining == 0)
                text.clear();
        }
        bool visible() const
        {
            return remaining > 0 && !text.empty();
        }
    };

    // After a promoted NEW_LEVEL or saved-game load: clear UI selections, queued commands and
    // input ownership (level_change_contracts promotion step 6). The caller also excludes the
    // preparation time from the next simulation delta.
    inline void reset_after_promotion(Menu& menu, MenuInput& input, ObjectSelection& selection, ObjectId& controlled,
                                      CBloodClientShell& shell)
    {
        selection.id = {};
        controlled = {};
        neutralize_app_input(shell);
        shell.dead_use_menu = false;
        input.reset(menu, true);
    }

    struct SaveFlow
    {
        SaveSlots slots;
        SaveNotice notice;
        enum class Outcome : std::uint8_t
        {
            None,
            Saved,
            Loaded, // The session was replaced: reset UI and input, exclude preparation time.
            Empty,  // F6 with no quicksave: a notice, nothing else.
            Failed  // `error` holds the modal text.
        };
        // Keeps the menu's slot rows current; a visible Save/Load page is rebuilt.
        void sync(Menu& menu) const
        {
            if (!menu.demo())
                menu.set_saves(slots.menu_rows());
        }
        // A Save or Load menu row. Success closes the menu and resumes play (the original
        // SMSG_SAVEGAME_ACK and StartNewGame); a failure keeps the page for the modal.
        Outcome command(const MenuCommand& command, std::unique_ptr<GameSession>& game, Menu& menu,
                        std::string& error)
        {
            error.clear();
            if (command.action == MenuAction::SaveGame)
            {
                if (!slots.save(*game, command.slot, error))
                    return Outcome::Failed;
                menu.close();
                return Outcome::Saved;
            }
            if (command.action != MenuAction::LoadGame)
                return Outcome::None;
            switch (slots.load(game, command.slot, error))
            {
                case SaveSlots::LoadResult::Loaded:
                    menu.close();
                    return Outcome::Loaded;
                case SaveSlots::LoadResult::Empty:
                    error = "That slot is empty.";
                    return Outcome::Failed;
                case SaveSlots::LoadResult::Refused:
                    break;
            }
            return Outcome::Failed;
        }
        // F5: MenuSaveGame(SLOT_QUICK) without pausing, then "Quicksave...". A refusal reports
        // the reason instead (D3).
        Outcome quick_save(GameSession& game, std::string& error)
        {
            if (!slots.save(game, SLOT_QUICK, error))
                return Outcome::Failed;
            notice.show("Quicksave...");
            return Outcome::Saved;
        }
        // F6: MenuLoadGame(SLOT_QUICK), then "Quickload..."; an empty quick slot shows a notice.
        Outcome quick_load(std::unique_ptr<GameSession>& game, std::string& error)
        {
            switch (slots.load(game, SLOT_QUICK, error))
            {
                case SaveSlots::LoadResult::Loaded:
                    notice.show("Quickload...");
                    return Outcome::Loaded;
                case SaveSlots::LoadResult::Empty:
                    notice.show("No quicksave to load.");
                    return Outcome::Empty;
                case SaveSlots::LoadResult::Refused:
                    break;
            }
            return Outcome::Failed;
        }
        // A NEW_GAME (after MenuNewGame's ClearSaveSlot(SLOT_CURRENT) when `new_game`) or a
        // NEW_LEVEL entry: the level-entry snapshot. Refusals are silent, as the original
        // autosave was; `error` is for the log and the inspector.
        bool level_entered(GameSession& game, bool new_game, std::string& error)
        {
            if (new_game && !slots.clear_level_entry(error))
                return false;
            return slots.capture_level_entry(game, error);
        }
    };
} // namespace OpenBlood2
