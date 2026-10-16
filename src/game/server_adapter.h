#pragma once
#include "engine/collision.h"
#include "engine/model_store.h"
#include "engine/objects.h"
#include "engine/string_resources.h"
#include "game/original/BloodShells.h"
#include "game/original/LightFX.h"
#include "game/session.h"
#include <array>
#include <optional>
#include <random>
#include <unordered_set>

struct StringHandle
{
};
struct LMessage
{
};
struct ConsoleVariable
{
    std::string text;
};
struct ClientHandle
{
};

namespace OpenBlood2
{

    class ServerAdapter final : public ServerDE
    {
      public:
        AudioMixer audio;
        SoundId PlayCinematicSound(HOBJECT, const SoundRequest&, std::string&) override;
        SoundId PlaySoundAt(HOBJECT, const SoundRequest&, DVector, std::string&) override;
        bool RandomInt(int, int, int&) override;
        bool IsSoundDone(SoundId, bool&) override;
        bool KillSound(SoundId) override;
        bool advance_audio(double, DVector, DRotation);
        explicit ServerAdapter(Objects& objects);
        ~ServerAdapter() override;
        ServerAdapter(const ServerAdapter&) = delete;
        ServerAdapter& operator=(const ServerAdapter&) = delete;
        bool bind();
        bool IsObjectAlive(HOBJECT) override;
        bool ValidateSpawnObject(std::string_view) override;
        void RecordSpawn(const SpawnReport&) override;
        void RecordPickup(HOBJECT, const PickupMessage&, std::string_view) override;
        DRESULT WritePickupMessage(HMESSAGEWRITE, const PickupMessage&) override;
        bool ReadPickupMessage(HMESSAGEREAD, PickupMessage&) override;
        std::vector<SpawnReport> spawn_reports;
        std::vector<PickupReport> pickup_reports;
        bool ConfigureWorldBody(HOBJECT, const WorldBodyProperties&) override;
        bool SetBlockingPriority(HOBJECT, DBYTE) override;
        void RecordBrushDestruction(HOBJECT, DVector, float, DBYTE) override;
        void RecordDebrisEffect(const DebrisEffect&) override;
        void RecordExplosionEffect(const ExplosionEffect&) override;
        std::vector<DebrisEffect> debris_effects;
        std::vector<ExplosionEffect> explosion_effects;
        std::vector<BrushDestruction> brush_destructions; // Latest 32, reset on unload.
        DRESULT WriteKeyMessage(HMESSAGEWRITE, const KeyMessage&) override;
        bool ReadKeyMessage(HMESSAGEREAD, KeyMessage&) override;
        MoverResult MoveWorldModel(HOBJECT, DVector destination) override;
        MoverResult RotateWorldModel(HOBJECT, const RotationMove&) override;
        bool GetContainers(HOBJECT, DVector head, ContainerState&) override;
        bool damage_containers();
        float GetWorldTime() override
        {
            trace_service("GetWorldTime");
            return static_cast<float>(objects_.time() - world_time_origin_);
        }
        void reset_world_time()
        {
            audio.clear();
            world_time_origin_ = objects_.time();
            brush_destructions.clear();
            debris_effects.clear();
            explosion_effects.clear();
            spawn_reports.clear();
            pickup_reports.clear();
            camera_effects.clear();
            light_effects.clear();
            random_.seed(1);
            variables_.erase("GAME_WORLDNAME");
            client_strings = {};
            client_strings_error.clear();
            client_strings_attempted = false;
        }
        ResizeResult ResizeObject(HOBJECT, DVector dimensions) override;
        bool GetMovementState(HOBJECT, MovementState&) override;
        SegmentResult IntersectSegment(DVector, DVector, HOBJECT, SegmentHit&, bool fire_through = false,
                                       SegmentObjects objects = SegmentObjects::SolidOnly) override;
        DRESULT WriteDamage(HMESSAGEWRITE, const Damage&) override;
        bool ReadDamage(HMESSAGEREAD, Damage&) override;
        HCLIENT connect_client();
        bool disconnect_client(HCLIENT);
        bool take_client_events(HCLIENT, std::vector<PlayerEvent>&);
        std::size_t client_count() const
        {
            return clients_.size();
        }
        HOBJECT GetClientUserData(HCLIENT) override;
        DRESULT SetClientUserData(HCLIENT, HOBJECT) override;
        const Resources* client_resources = nullptr;
        StringResources client_strings;
        std::string client_strings_error;
        bool client_strings_attempted = false;
        std::vector<CLightFX> light_effects;
        bool RandomFloat(float, float, float&) override;
        bool PublishLightEffect(HOBJECT, const LightSettings&) override;
        void RemoveLightEffect(HOBJECT) override;
        const CLightFX* GetLightEffect(HOBJECT) const;
        bool advance_lights();
        std::vector<CameraView> camera_effects;
        bool PublishCameraEffect(HOBJECT, const CameraSettings&) override;
        void RemoveCameraEffect(HOBJECT) override;
        CameraView GetCameraView() override;
        DRESULT InterpolateRotation(DRotation&, const DRotation&, const DRotation&, float) override;
        MoverResult MoveKeyframedObject(HOBJECT, DVector, const DRotation*) override;
        DRESULT AlignRotation(DRotation&, DVector) override;
        DDWORD GetObjectUserFlags(HOBJECT) override;
        DRESULT SendObjectives(const MissionObjectives&) override;
        bool GetClientResourceString(std::uint32_t, std::string&) override;
        DRESULT SendToClient(HCLIENT, const PlayerEvent&) override;
        DRESULT SetModelFilenames(HOBJECT, const char*, const char*) override;
        DRESULT GetObjectDims(HOBJECT, DVector*) override;
        DRESULT SetObjectRotation(HOBJECT, const DRotation*) override;
        DRESULT MoveObjectToGround(HOBJECT) override;
        DBOOL CompareStringsUpper(HSTRING, HSTRING) override;
        void unbind();
        void set_game_settings(unsigned difficulty, unsigned game_type);
        std::map<std::string, std::uint64_t> service_calls;
        bool trace_enabled = false;
        void trace_service(const char* name)
        {
            if (trace_enabled)
                ++service_calls[name];
        }
        std::size_t string_count() const
        {
            return strings_.size();
        }
        std::size_t message_count() const
        {
            return messages_.size();
        }
        std::size_t list_count() const
        {
            return lists_.size();
        }

        HMESSAGEWRITE StartHMessageWrite() override;
        DRESULT WriteToMessageDWord(HMESSAGEWRITE, DDWORD) override;
        DRESULT WriteToMessageHString(HMESSAGEWRITE, HSTRING) override;
        DDWORD ReadFromMessageDWord(HMESSAGEREAD) override;
        HSTRING ReadFromMessageHString(HMESSAGEREAD) override;
        void EndHMessageWrite(HMESSAGEWRITE) override;
        void EndHMessageRead(HMESSAGEREAD) override;
        void ResetRead(HMESSAGEREAD) override;
        HMESSAGEWRITE StartMessageToObject(LPBASECLASS, HOBJECT, DDWORD) override;
        DRESULT EndMessage(HMESSAGEWRITE) override;
        DRESULT MoveObject(HOBJECT, DVector*) override;
        DRESULT GetModelAnimUserDims(HOBJECT, DVector*, HMODELANIM) override;
        DRESULT SetGlobalForce(DVector*) override;
        DRESULT GetGlobalForce(DVector*) override;
        DRESULT GetVelocity(HOBJECT, DVector*) override;
        DRESULT SetVelocity(HOBJECT, DVector*) override;
        DRESULT GetAcceleration(HOBJECT, DVector*) override;
        DRESULT SetAcceleration(HOBJECT, DVector*) override;
        bool advance_physics(double seconds);
        CollisionWorld* collision = nullptr;
        bool collision_ready = false;
        bool advance_models(double seconds);
        const auto& variables() const
        {
            return variables_;
        }
        DVector gravity{0, -2000, 0};

      private:
        std::minstd_rand random_{1};
        double world_time_origin_ = 0;
        struct String
        {
            std::string text;
            unsigned references = 1;
        };
        struct Message
        {
            std::vector<std::uint8_t> bytes;
            std::size_t cursor = 0;
            bool bad = false, routed = false, borrowed = false;
            HOBJECT sender = nullptr, receiver = nullptr;
            DDWORD id = 0;
            std::optional<Damage> damage;
            std::optional<KeyMessage> key;
            std::optional<PickupMessage> pickup;
        };
        Objects& objects_;
        bool bound_ = false;
        struct Client
        {
            HOBJECT player = nullptr;
            std::vector<PlayerEvent> events;
        };
        std::vector<std::unique_ptr<ClientHandle>> client_tokens_;
        std::unordered_map<HCLIENT, Client> clients_;
        std::map<std::string, ConsoleVariable, CaseLess> variables_{{"GAME_DIFFICULTY", {"1"}},
                                                                    {"GAME_GAMETYPE", {"0"}}};
        void initialize_model_services();
        void initialize_physics_services();
        CollisionQuery sweep_scene(HOBJECT, DVector, DVector, DVector, CollisionHit&, const std::vector<ObjectId>&,
                                   HOBJECT ignore_extra = nullptr);
        bool affect_containers(HOBJECT, ContainerPhysics&);
        struct BodyPath
        {
            DVector from{}, to{};
        };
        struct BodyRider
        {
            ObjectRecord* body;
            DVector origin{};
        };
        struct BodyPlan
        {
            ObjectRecord* body = nullptr;
            HOBJECT initiator = nullptr;
            DVector origin{}, destination{}, velocity{};
            std::vector<BodyPath> paths;
            std::vector<BodyRider> riders;
            CollisionHit contact;
            bool blocked = false;
        };
        MoverResult plan_world_body(ObjectRecord&, DVector, const std::vector<ObjectId>&, HOBJECT, BodyPlan&);
        void commit_world_body(const BodyPlan&);
        void dispatch_world_body(const BodyPlan&, const std::vector<ObjectId>&);
        DRESULT move_world_body(ObjectRecord&, DVector);
        bool moving_ = false;
        bool models_advancing_ = false, physics_advancing_ = false;
        std::vector<std::unique_ptr<StringHandle>> string_tokens_;
        std::unordered_map<HSTRING, String> strings_;
        std::vector<std::unique_ptr<LMessage>> message_tokens_;
        std::unordered_map<HMESSAGEWRITE, Message> messages_;
        std::vector<std::unique_ptr<ObjectList>> list_tokens_;
        std::unordered_set<ObjectList*> lists_;
        static ServerAdapter* active_;
        static ServerAdapter& active();
        HSTRING make_string(const char* text);
        HSTRING copy_string(HSTRING);
        void free_string(HSTRING);
        char* string_data(HSTRING);
        ObjectList* make_list();
        ObjectLink* add_to_list(ObjectList*, HOBJECT);
        void remove_from_list(ObjectList*, HOBJECT);
        void release_list(ObjectList*);
        Message* writable(HMESSAGEWRITE);
        Message* readable(HMESSAGEREAD, std::size_t bytes);
        DRESULT unsupported(const char* service);
    };

    bool register_original_classes(Objects& objects);
    class GameSession
    {
      public:
        Resources resources;
        ModelStore models{resources};
        CollisionWorld collision;
        Objects objects;
        ServerAdapter server{objects};
        CBloodServerShell server_shell{server};
        CBloodClientShell client_shell{server};
        HCLIENT local_client = nullptr;
        World world;
        std::vector<WorldEntry> entries;
        std::vector<ResourceUse> resource_uses;
        std::vector<std::string> trace;
        std::string world_path;
        // Inspection resource, not a CPlayerObj or a replacement player controller.
        std::shared_ptr<const Model> preview_model;
        AnimationTrack preview_animation;
        std::string preview_error;
        std::vector<std::uint32_t> preview_events;
        bool preview_playing = true;
        SessionState state = SessionState::Empty;
        ~GameSession()
        {
            unload_world();
        }
        bool load_world(const WorldMessage& message, std::string& error);
        bool request_world(std::string_view name, std::string& error);
        bool run_world(std::string& error);
        bool enter_player(std::string& error);
        bool select_weapon(DBYTE slot, std::string& error);
        bool leave_player();
        bool pump_player_messages(std::string& error);
        bool unload_world();
        bool step(double seconds);
        bool initialize()
        {
            objects.models = &models;
            objects.world = &world;
            server.collision = &collision;
            server.client_resources = &resources;
            return server.bind() && register_original_classes(objects);
        }
        HOBJECT create_example(std::string_view class_name);
    };

} // namespace OpenBlood2
