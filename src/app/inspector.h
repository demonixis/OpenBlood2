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
        bool select_containers = false;
        bool select_triggers = false;
        ObjectId trigger_selection{}, trigger_request{};
        std::string trigger_command;
        bool grant_key_request = false;
        bool select_sprites = false;
        ObjectId sprite_selection{};
        void draw_sprites(GameSession&, ObjectSelection&);
        bool select_world_sounds = false;
        SoundId world_sound_selection = 0;
        void draw_world_sounds(GameSession&);
        bool select_random_sounds = false;
        ObjectId random_sound_selection{}, random_sound_request{};
        std::string random_sound_command;
        void draw_random_sounds(GameSession&, ObjectSelection&);
        bool select_cinematics = false;
        ObjectId cinematic_selection{}, cinematic_request{};
        void draw_cinematics(GameSession&, ObjectSelection&);
        bool select_lights = false;
        ObjectId light_selection{}, light_request{};
        std::string light_command;
        void draw_lights(GameSession&, ObjectSelection&);
        bool select_keyframes = false;
        ObjectId keyframe_selection{}, keyframe_request{};
        std::string keyframe_command;
        void draw_keyframes(GameSession&, ObjectSelection&);
        bool select_cameras = false;
        ObjectId camera_selection{}, camera_request{};
        char camera_link_name[96]{};
        std::string camera_command;
        void draw_cameras(GameSession&, ObjectSelection&);
        bool select_objectives = false;
        ObjectId objective_selection{};
        void draw_objectives(GameSession&, ObjectSelection&);
        bool select_pickups = false;
        ObjectId pickup_selection{};
        void draw_pickups(GameSession&, ObjectSelection&);
        bool select_brushes = false;
        ObjectId brush_selection{}, brush_request{};
        std::string brush_command;
        void draw_brushes(GameSession&, ObjectSelection&);
        bool select_doors = false;
        ObjectId door_selection{}, door_trigger_request{};
        bool door_close_request = false;
        ObjectId container_selection{}, container_toggle_request{};
        std::optional<DVector> player_position_request;
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
        void draw_containers(GameSession&, ObjectSelection&);
        void draw_triggers(GameSession&, ObjectSelection&);
        void draw_doors(GameSession&, ObjectSelection&);

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
