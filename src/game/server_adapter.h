#pragma once
#include "engine/collision.h"
#include "engine/model_store.h"
#include "engine/objects.h"
#include "engine/sky.h"
#include "engine/string_resources.h"
#include "game/blood_splats.h"
#include "game/blood_spurts.h"
#include "game/explosion_flames.h"
#include "game/explosion_fragments.h"
#include "game/explosion_lights.h"
#include "game/explosion_models.h"
#include "game/explosion_rings.h"
#include "game/explosion_sprites.h"
#include "game/flesh_gibs.h"
#include "game/laser_beams.h"
#include "game/lightning.h"
#include "game/music_catalog.h"
#include "game/object_particles.h"
#include "game/original/BloodShells.h"
#include "game/original/LightFX.h"
#include "game/particle_streams.h"
#include "game/player_restore.h"
#include "game/polygrids.h"
#include "game/projectile_report.h"
#include "game/service_coverage.h"
#include "game/session.h"
#include "game/sparks.h"
#include "game/warp_gates.h"
#include "game/weapon_sprites.h"
#include "game/world_preparation.h"
#include <array>
#include <optional>
#include <random>
#include <unordered_set>

struct StringHandle
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
        SoundId PlayCharacterSound(HOBJECT, const SoundRequest&, bool retain, std::string&) override;
        SoundId PlayCinematicSound(HOBJECT, const SoundRequest&, std::string&) override;
        SoundId PlayLocalSound(HOBJECT, const OpenBlood2::SoundRequest&, std::string&) override;
        SoundId PlayDetachedSound(HOBJECT, const OpenBlood2::SoundRequest&, DVector, std::string&) override;
        SoundId PlayClientSound(const SoundRequest&, DVector, std::string&) override;
        SoundId PlaySoundAt(HOBJECT, const SoundRequest&, DVector, std::string&) override;
        SoundId PlayWorldSound(HOBJECT, const SoundRequest&, bool file_stream, std::string&) override;
        bool RandomInt(int, int, int&) override;
        bool IsSoundDone(SoundId, bool&) override;
        bool KillSound(SoundId) override;
        bool GetSoundDuration(SoundId, float&) override;
        bool advance_audio(double, DVector, DRotation);
        explicit ServerAdapter(Objects& objects);
        ~ServerAdapter() override;
        ServerAdapter(const ServerAdapter&) = delete;
        ServerAdapter& operator=(const ServerAdapter&) = delete;
        bool bind();
        DRESULT CreateAttachment(HOBJECT, HOBJECT, const char*, const DVector*, const DRotation*,
                                 HATTACHMENT*) override;
        DRESULT RemoveAttachment(HATTACHMENT) override;
        DRESULT FindAttachment(HOBJECT, HOBJECT, HATTACHMENT*) override;
        DRESULT GetAttachedModelNodeTransform(HATTACHMENT, const char*, DVector*, DRotation*) override;
        bool IsObjectAlive(HOBJECT) override;
        bool SetDeactivationTime(HOBJECT, float) override;
        bool PingObjects(HOBJECT) override;
        bool ConfigureCharacterBody(HOBJECT, float mass, float friction) override;
        bool ScaleObject(HOBJECT, DVector) override;
        PathMgr* GetPathManager() override
        {
            trace_service("GetPathManager");
            return ai_paths;
        }
        PathMgr* ai_paths = nullptr;
        bool ValidateSpawnObject(std::string_view) override;
        void RejectPrecreate(std::string_view reason) override;
        void RecordSpawn(const SpawnReport&) override;
        void RecordPickup(HOBJECT, const PickupReport&) override;
        DRESULT WritePickupMessage(HMESSAGEWRITE, const PickupMessage&) override;
        bool ReadPickupMessage(HMESSAGEREAD, PickupMessage&) override;
        std::vector<SpawnReport> spawn_reports;
        std::vector<PickupReport> pickup_reports;
        bool ConfigureWorldBody(HOBJECT, const WorldBodyProperties&) override;
        bool ConfigureModelBody(HOBJECT, const WorldBodyProperties&) override;
        SoundId PlayObjectLoopSound(HOBJECT, const SoundRequest&, std::string&) override;
        bool SetBlockingPriority(HOBJECT, DBYTE) override;
        void RecordBrushDestruction(HOBJECT, DVector, float, DBYTE, unsigned = 0, unsigned = 0) override;
        void RecordDebrisEffect(const DebrisEffect&) override;
        std::uint64_t PublishExplosionSprites(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        ExplosionSprites explosion_sprites;
        Sparks sparks;
        LightningBolts lightning;
        WarpGates warp_gates;
        std::uint64_t PublishWarpGate(HOBJECT, const WarpGateSettings&, std::string&) override;
        std::uint64_t PublishLightning(HOBJECT, const LightningSettings&, std::string&) override;
        std::uint64_t PublishSparks(HOBJECT, const SparkSettings&, std::string&) override;
        BloodSpurts blood_spurts;
        std::uint64_t PublishClientBloodSplat(const BloodSplatSettings&, std::string_view, std::string&) override;
        std::uint64_t PublishClientBloodSpurt(const BloodSpurtSettings&, std::string_view, bool, std::string&) override;
        std::uint64_t PublishBloodSpurt(HOBJECT, const BloodSpurtSettings&, std::string&) override;
        BloodSplats blood_splats;
        std::uint64_t PublishBloodSplat(HOBJECT, const BloodSplatSettings&, std::string&) override;
        WeaponSprites weapon_sprites;
        std::uint64_t PublishWeaponSprites(HOBJECT, const WeaponSpriteSettings&, std::string&) override;
        ExplosionModels explosion_models;
        LaserBeams laser_beams;
        ObjectParticles object_particles;
        SkyState sky;
        bool SetSkyDefinition(const SkyDefinition&) override;
        bool AddSkyObject(HOBJECT, unsigned) override;
        void ReportSkyFailure(std::string_view) override;
        PolyGrids polygrids;
        std::uint64_t PublishPolyGrid(HOBJECT, const PolyGridSettings&, std::string&) override;
        ParticleStreams particle_streams;
        std::uint64_t PublishParticleStream(HOBJECT, const ParticleStreamSettings&, std::string&) override;
        std::uint64_t PublishObjectParticles(HOBJECT, unsigned, std::string&, float lifetime_scale = 1) override;
        std::uint64_t PublishLaserBeam(HOBJECT, DVector, DVector, std::string&, DBYTE type = 0) override;
        ExplosionLights explosion_lights;
        ExplosionRings explosion_rings;
        ExplosionFlames explosion_flames;
        std::uint64_t PublishExplosionFlames(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        FleshGibs flesh_gibs;
        std::uint64_t PublishFleshGibs(HOBJECT, DVector, DVector, DVector, float, unsigned, std::string&) override;
        ExplosionFragments explosion_fragments;
        std::uint64_t PublishExplosionFragments(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        std::uint64_t PublishExplosionRings(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        std::uint64_t PublishExplosionLights(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        std::uint64_t PublishExplosionModels(HOBJECT, DDWORD, DVector, DVector, std::string&) override;
        void RecordSceneExplosion(const OpenBlood2::SceneExplosionReport&) override;
        void RecordExplosionEffect(const ExplosionEffect&) override;
        std::vector<DebrisEffect> debris_effects;
        std::vector<ExplosionEffect> explosion_effects;
        std::vector<SceneExplosionReport> scene_explosions;
        std::vector<BrushDestruction> brush_destructions; // Latest 32, reset on unload.
        DRESULT WriteKeyMessage(HMESSAGEWRITE, const KeyMessage&) override;
        bool ReadKeyMessage(HMESSAGEREAD, KeyMessage&) override;
        MoverResult MoveWorldModel(HOBJECT, DVector destination) override;
        MoverResult RotateWorldModel(HOBJECT, const RotationMove&) override;
        bool GetPointContainerCode(DVector, ContainerCode&, bool visible_only = false) override;
        bool GetContainers(HOBJECT, DVector head, ContainerState&) override;
        bool damage_containers();
        double world_time() const
        {
            return objects_.time() - world_time_origin_;
        }
        float GetWorldTime() override
        {
            trace_service("GetWorldTime");
            return static_cast<float>(world_time());
        }
        bool DemoBuild() override
        {
            trace_service("DemoBuild");
            return client_resources && client_resources->demo();
        }
        void reset_world_time()
        {
            audio.clear();
            unavailable_services.clear();
            world_time_origin_ = objects_.time();
            brush_destructions.clear();
            debris_effects.clear();
            explosion_effects.clear();
            scene_explosions.clear();
            projectile_reports.clear();
            explosion_sprites.clear();
            sparks.clear();
            lightning.clear();
            warp_gates.clear();
            blood_spurts.clear();
            blood_splats.clear();
            weapon_sprites.clear();
            explosion_models.clear();
            laser_beams.clear();
            object_particles.clear();
            particle_streams.clear();
            polygrids.clear();
            sky = {};
            explosion_lights.clear();
            explosion_rings.clear();
            explosion_flames.clear();
            explosion_fragments.clear();
            flesh_gibs.clear();
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
        DRESULT ResolveInitialWorldOverlap(HOBJECT) override;
        bool PlaceNonSolidModel(HOBJECT, DVector requested, DVector anchor, DVector anchor_half,
                                BodyPlacement&) override;
        bool GetMovementState(HOBJECT, MovementState&) override;
        SegmentResult CastRay(DVector, DVector, SegmentHit&, HOBJECT ignore = nullptr) override;
        SegmentResult CastWorldRay(DVector from, DVector direction, SegmentHit&) override;
        SegmentResult IntersectWorldSegment(DVector from, DVector to, SegmentHit&) override;
        SegmentResult IntersectSegment(DVector, DVector, HOBJECT, SegmentHit&, bool fire_through = false,
                                       SegmentObjects objects = SegmentObjects::SolidOnly,
                                       SegmentFilter filter = SegmentFilter::None,
                                       SegmentStart start = SegmentStart::IncludeInterior) override;
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
        DBYTE GetObjectType(HOBJECT) override;
        CBloodServerShell* session_shell = nullptr;
        DRESULT SendWorldExit(const WorldExit&, const std::string&) override;
        DRESULT SendBossHealth(const BossHealth&) override;
        DRESULT SendConversation(const Conversation&) override;
        DRESULT SendObjectives(const MissionObjectives&) override;
        bool GetClientResourceString(std::uint32_t, std::string&) override;
        DRESULT SendToClient(HCLIENT, const PlayerEvent&) override;
        bool GetModelFilenames(HOBJECT, std::string&, std::string&) override;
        DRESULT SetModelFilenames(HOBJECT, const char*, const char*) override;
        DRESULT GetModelNodeHideStatus(HOBJECT, const char*, DBOOL*) override;
        DRESULT SetModelNodeHideStatus(HOBJECT, const char*, DBOOL) override;
        DRESULT GetObjectDims(HOBJECT, DVector*) override;
        bool CollectObjectsTouchingSphere(DVector, float, std::vector<HOBJECT>&) override;
        DRESULT SetObjectRotation(HOBJECT, const DRotation*) override;
        DRESULT MoveObjectToGround(HOBJECT) override;
        DBOOL CompareStringsUpper(HSTRING, HSTRING) override;
        void unbind();
        void set_game_settings(unsigned difficulty, unsigned game_type);
        ServiceCalls service_calls;
        std::map<std::string, std::uint64_t> unavailable_services;
        // Retained through teardown; cleared alongside service_calls on the next world load.
        std::map<std::string, std::uint64_t> unavailable_history;
        void record_unavailable(const std::string& reason)
        {
            ++unavailable_services[reason];
            ++unavailable_history[reason];
        }
        bool trace_enabled = false;
        void trace_service(const char* name)
        {
            if (trace_enabled)
            {
                const auto found = service_calls.find(std::string_view(name));
                if (found != service_calls.end())
                    ++found->second;
                else
                    ++service_calls[name];
            }
        }
        std::size_t string_count() const
        {
            return strings_.size();
        }
        std::size_t message_count() const
        {
            return messages_.size();
        }
        static constexpr std::size_t max_live_messages = 65536;
        struct MessageStats
        {
            std::uint64_t created = 0, released = 0, refused = 0;
            std::size_t peak_live = 0;
        } message_stats; // Adapter lifetime; inspecting or unloading does not reset history.
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
        bool GetObjectColor(HOBJECT, DVector&, float&) override;
        bool SetObjectColor(HOBJECT, DVector, float) override;
        DRESULT MoveObject(HOBJECT, DVector*) override;
        DRESULT GetModelAnimUserDims(HOBJECT, DVector*, HMODELANIM) override;
        NodeQuery QueryModelNodeTransform(HOBJECT, const char*, DVector*, DRotation*) override;
        DBOOL GetModelNodeTransform(HOBJECT, const char*, DVector*, DRotation*) override;
        bool GetModelAnimationName(HOBJECT, HMODELANIM, std::string&) override;
        DRESULT SetGlobalForce(DVector*) override;
        DRESULT GetGlobalForce(DVector*) override;
        DRESULT GetVelocity(HOBJECT, DVector*) override;
        DRESULT SetVelocity(HOBJECT, DVector*) override;
        DRESULT GetAcceleration(HOBJECT, DVector*) override;
        DRESULT SetAcceleration(HOBJECT, DVector*) override;
        bool advance_physics(double seconds);
        bool advance_projectiles(double seconds);
        bool AdvanceModelProjectile(HOBJECT, double seconds) override;
        void RecordProjectile(const ProjectileReport&) override;
        std::vector<ProjectileReport> projectile_reports;
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
                                   HOBJECT ignore_extra = nullptr, double allowed_penetration = 0,
                                   StartOverlap start = StartOverlap::Block);
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
        std::unordered_map<HMESSAGEWRITE, Message, MessageHandleHash> messages_;
        std::vector<std::unique_ptr<ObjectList>> list_tokens_;
        std::unordered_set<ObjectList*> lists_;
        // Only synchronous destination preparation/commit may temporarily suspend this binding.
        friend class GameSession;
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
        SpriteStore sprites{resources};
        CollisionWorld collision;
        Objects objects;
        ServerAdapter server{objects};
        CBloodServerShell server_shell{server};
        MusicCatalog music_catalog;
        CBloodClientShell client_shell{server, resources, music_catalog, server.audio};
        HCLIENT local_client = nullptr;
        World world;
        std::vector<WorldEntry> entries;
        std::vector<ResourceUse> resource_uses;
        std::vector<std::string> trace;
        std::string world_path;
        std::uint64_t world_revision = 0; // Changes on teardown; diagnostic geometry edits must also increment it.
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
        DestinationInspection destination_inspection;
        DestinationTrial destination_trial;
        LevelChangeReport level_change;
        SessionStartReport startup;
        std::optional<LevelChangeRequest> pending_level_change;
        bool queue_level_change(const WorldMessage&, std::string_view start, std::string&);
        // Called only at an owning frame boundary, after all callbacks/step references have returned.
        static LevelChangeResult process_level_change(std::unique_ptr<GameSession>&, std::string&);
        static bool start_new_game(std::unique_ptr<GameSession>&, std::string_view world, DBYTE difficulty,
                                   DBYTE character, std::string& error, bool inspection_start = false);
        bool rehearse_destination(const WorldMessage&, std::string_view start, std::string& error);
        bool inspect_destination(const WorldMessage&, std::string_view start, std::string& error);
        bool load_world(const WorldMessage& message, std::string& error);
        bool request_world(std::string_view name, std::string& error);
        bool run_world(std::string& error);
        bool enter_player(std::string& error);
        bool capture_player(PlayerRestoreState&, std::string&);
        bool restore_player(const PlayerRestoreState&, std::string&,
                            PlayerRestoreMode = PlayerRestoreMode::SavedPosition, std::string_view start_name = {});
        struct Restoration
        {
            std::optional<PlayerRestoreState> saved;
            std::string error;
            std::uint64_t captures = 0, attempts = 0, restored = 0;
            ObjectId last_player{}; // Inspection only, never a restoration input.
            PlayerRestoreMode last_mode = PlayerRestoreMode::SavedPosition;
            std::string destination_world, requested_start, selected_start;
            bool start_fallback = false;
        } restoration;

        bool select_weapon(DBYTE slot, std::string& error);
        bool command_weapon(DBYTE slot, std::string& error);
        bool leave_player();
        bool pump_player_messages(std::string& error);
        bool unload_world();
        bool step(double seconds);
        // Labelled durable subsystem errors for logging a failed step; many
        // update paths report through these fields, not object diagnostics.
        std::vector<std::string> step_errors();
        bool initialize()
        {
            objects.models = &models;
            objects.sprites = &sprites;
            objects.world = &world;
            server.collision = &collision;
            server.client_resources = &resources;
            server.ai_paths = &server_shell.paths;
            server.session_shell = &server_shell;
            return server.bind() && register_original_classes(objects);
        }
        HOBJECT create_example(std::string_view class_name);
    };

} // namespace OpenBlood2
