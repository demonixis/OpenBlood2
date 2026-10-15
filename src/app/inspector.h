#pragma once

#include "engine/config.h"
#include "engine/runtime.h"
#include "game/server_adapter.h"

namespace OpenBlood2
{

    struct Inspector
    {
        Runtime& runtime;
        bool context = false;
        bool platform = false;
        bool renderer = false;
        bool select_models = false;
        bool select_session = false;
        bool select_movement = false, keyboard_movement = false;
        std::optional<MovementInput> movement_input_request;
        bool select_weapons = false, add_beretta_request = false, add_ammo_request = false;
        int weapon_slot_request = -1;
        bool weapon_left = false;
        std::optional<WeaponInput> weapon_input_request;
        std::optional<PlayerRotation> weapon_aim_request;
        bool enter_player_request = false, leave_player_request = false, respawn_request = false;
        ObjectId remove_request{};
        std::string spawn_request;
        std::string world_error, reload_world_name = "01_SubwayA";
        bool reload_request = false, unload_request = false, run_request = false;
        int source_selection = 0, model_selection = 0;
        char world_filter[100]{}, resource_filter[128]{};
        void draw_world(GameSession&, ObjectSelection&);
        void draw_resources(const GameSession&);
        void draw_models(GameSession&);
        void draw_services(const GameSession&);
        void draw_session(GameSession&, ObjectSelection&);
        void draw_weapons(GameSession&, ObjectSelection&);
        void draw_movement(GameSession&, ObjectSelection&);

        explicit Inspector(Runtime& value) : runtime(value) {}
        ~Inspector();
        Inspector(const Inspector&) = delete;
        Inspector& operator=(const Inspector&) = delete;
        bool initialize(std::string& error);
        void apply_commands(GameSession& game, ObjectSelection& selection);
        void draw(const Config& config, const DataStatus& data, bool& visible, std::uint64_t frames, GameSession& game,
                  ObjectSelection& selection);
    };

} // namespace OpenBlood2
