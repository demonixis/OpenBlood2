#pragma once

#include "engine/config.h"
#include "engine/model_view.h"
#include "engine/runtime.h"
#include "engine/sprite_view.h"
#include "engine/world_view.h"
#include "game/server_adapter.h"

namespace OpenBlood2
{

    struct Inspector
    {
        Runtime& runtime;
        const WorldMesh* world_mesh = nullptr;
        const ModelView* model_view = nullptr;
        const ModelView* explosion_model_view = nullptr;
        const SpriteView* sprite_view = nullptr;
        const WorldViewStats* sprite_render = nullptr;
        const WorldViewStats* world_render = nullptr;
        bool context = false;
        bool platform = false;
        bool renderer = false;
        bool select_models = false;
        bool select_skeleton = false;
        ObjectId skeleton_selection{};
        void draw_skeleton(GameSession&, ObjectSelection&);
        bool select_session = false;
        bool select_movement = false, keyboard_movement = true, mouse_captured = false;
        std::string mouse_error;
        bool collapsed = false, waiting_for_player = false;
        std::uint64_t limited_frames = 0;
        double discarded_wall_time = 0;
        bool select_containers = false;
        bool select_triggers = false;
        ObjectId trigger_selection{}, trigger_request{};
        std::string trigger_command;
        bool grant_key_request = false;
        bool select_scene_explosions = false;
        ObjectId scene_explosion_selection{}, scene_explosion_request{};
        void draw_scene_explosions(GameSession&, ObjectSelection&);
        bool select_sprites = false;
        ObjectId sprite_selection{};
        void draw_sprites(GameSession&, ObjectSelection&);
        void draw_blood_spurts(const GameSession&);
        void draw_blood_splats(const GameSession&);
        void draw_weapon_sprites(const GameSession&);
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
        bool select_weapons = false, add_beretta_request = false, add_mac10_request = false, add_flare_request = false,
             add_flares_request = false, add_ammo_request = false, add_assault_request = false,
             add_charges_request = false;
        int weapon_slot_request = -1;
        bool detonate_request = false;
        bool weapon_left = false;
        std::optional<WeaponInput> weapon_input_request;
        std::optional<PlayerRotation> weapon_aim_request;
        bool enter_player_request = false, leave_player_request = false, respawn_request = false;
        bool play_request = false;
        bool capture_player_request = false, restore_player_request = false, keep_player_request = false;
        char keep_start_name[101]{};
        char keep_world_name[256]{"02_cityhub"};
        bool keep_world_request = false, inspect_destination_request = false, trial_destination_request = false,
             level_change_request = false;
        void draw_destination(const GameSession&);
        void draw_destination_trial(const GameSession&);
        void draw_level_change(const GameSession&);
        void forget_world();
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
        char path_prefix[101]{};
        void draw_paths(GameSession&, ObjectSelection&);
        void draw_session(GameSession&, ObjectSelection&);
        void draw_weapon_sound(const CWeapon&, const GameSession&);
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
