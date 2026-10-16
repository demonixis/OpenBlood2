#pragma once
#include "basedefs_de.h"
#include "engine/audio.h"
#include "game/brush_messages.h"
#include "game/camera_messages.h"
#include "game/container_messages.h"
#include "game/key_messages.h"
#include "game/light_messages.h"
#include "game/mover_messages.h"
#include "game/pickup_messages.h"
#include "game/player_messages.h"
#include "game/weapon_messages.h"

// Deliberately bounded source interface. Add reviewed declarations with their
// implementation/tests as original callers arrive. Absent services fail to
// compile; the explicitly deferred services below return DE_ERROR + diagnostics.
// Read-only input strings are const; fixed-width integers replace Windows long.
class CSBase
{
  public:
    virtual ~CSBase() = default;
    virtual HMESSAGEWRITE StartHMessageWrite() = 0;
    virtual DRESULT WriteToMessageDWord(HMESSAGEWRITE, DDWORD) = 0;
    virtual DRESULT WriteToMessageHString(HMESSAGEWRITE, HSTRING) = 0;
    virtual DDWORD ReadFromMessageDWord(HMESSAGEREAD) = 0;
    virtual HSTRING ReadFromMessageHString(HMESSAGEREAD) = 0;
    virtual void EndHMessageWrite(HMESSAGEWRITE) = 0;
    virtual void EndHMessageRead(HMESSAGEREAD) = 0;
    virtual void ResetRead(HMESSAGEREAD) = 0;
};

class ServerDE : public CSBase
{
  public:
    virtual OpenBlood2::SoundId PlayCharacterSound(HOBJECT, const OpenBlood2::SoundRequest&, bool retain,
                                                   std::string&) = 0;
    virtual OpenBlood2::SoundId PlayCinematicSound(HOBJECT, const OpenBlood2::SoundRequest&, std::string&) = 0;
    virtual OpenBlood2::SoundId PlayDetachedSound(HOBJECT, const OpenBlood2::SoundRequest&, DVector, std::string&) = 0;
    virtual OpenBlood2::SoundId PlaySoundAt(HOBJECT, const OpenBlood2::SoundRequest&, DVector, std::string&) = 0;
    virtual OpenBlood2::SoundId PlayWorldSound(HOBJECT, const OpenBlood2::SoundRequest&, bool file_stream,
                                               std::string&) = 0;
    virtual bool RandomInt(int, int, int&) = 0;
    virtual bool IsSoundDone(OpenBlood2::SoundId, bool&) = 0;
    virtual bool KillSound(OpenBlood2::SoundId) = 0;

    virtual DRESULT CreateAttachment(HOBJECT, HOBJECT, const char*, const DVector*, const DRotation*, HATTACHMENT*) = 0;
    virtual DRESULT RemoveAttachment(HATTACHMENT) = 0;
    virtual DRESULT FindAttachment(HOBJECT, HOBJECT, HATTACHMENT*) = 0;
    virtual DRESULT GetAttachedModelNodeTransform(HATTACHMENT, const char*, DVector*, DRotation*) = 0;
    virtual bool IsObjectAlive(HOBJECT) = 0;
    virtual bool ValidateSpawnObject(std::string_view) = 0;
    virtual void RecordSpawn(const OpenBlood2::SpawnReport&) = 0;
    virtual void RecordPickup(HOBJECT, const OpenBlood2::PickupMessage&, std::string_view sound) = 0;
    virtual DRESULT WritePickupMessage(HMESSAGEWRITE, const OpenBlood2::PickupMessage&) = 0;
    virtual bool ReadPickupMessage(HMESSAGEREAD, OpenBlood2::PickupMessage&) = 0;
    virtual bool ConfigureWorldBody(HOBJECT, const OpenBlood2::WorldBodyProperties&) = 0;
    virtual bool SetBlockingPriority(HOBJECT, DBYTE) = 0;
    virtual void RecordBrushDestruction(HOBJECT, DVector direction, float damage, DBYTE amount) = 0;
    virtual void RecordDebrisEffect(const OpenBlood2::DebrisEffect&) = 0;
    virtual void RecordSceneExplosion(const OpenBlood2::SceneExplosionReport&) = 0;
    virtual void RecordExplosionEffect(const OpenBlood2::ExplosionEffect&) = 0;
    virtual DRESULT WriteKeyMessage(HMESSAGEWRITE, const OpenBlood2::KeyMessage&) = 0;
    virtual bool ReadKeyMessage(HMESSAGEREAD, OpenBlood2::KeyMessage&) = 0;
    virtual OpenBlood2::MoverResult MoveWorldModel(HOBJECT, DVector destination) = 0;
    virtual OpenBlood2::MoverResult RotateWorldModel(HOBJECT, const OpenBlood2::RotationMove&) = 0;
    virtual bool GetPointContainerCode(DVector, ContainerCode&) = 0;
    virtual bool GetContainers(HOBJECT, DVector head, OpenBlood2::ContainerState&) = 0;
    virtual OpenBlood2::ResizeResult ResizeObject(HOBJECT, DVector dimensions) = 0;
    virtual bool GetMovementState(HOBJECT, OpenBlood2::MovementState&) = 0;
    virtual float GetWorldTime() = 0;
    virtual OpenBlood2::SegmentResult CastWorldRay(DVector, DVector, OpenBlood2::SegmentHit&) = 0;
    virtual OpenBlood2::SegmentResult IntersectSegment(
        DVector from, DVector to, HOBJECT ignore, OpenBlood2::SegmentHit&, bool fire_through = false,
        OpenBlood2::SegmentObjects objects = OpenBlood2::SegmentObjects::SolidOnly,
        OpenBlood2::SegmentFilter filter = OpenBlood2::SegmentFilter::None) = 0;
    virtual DRESULT WriteDamage(HMESSAGEWRITE, const OpenBlood2::Damage&) = 0;
    virtual bool ReadDamage(HMESSAGEREAD, OpenBlood2::Damage&) = 0;
    // Source adaptation: client association is a checked object handle, never a
    // raw C++ pointer whose lifetime can outlive its object.
    virtual HOBJECT GetClientUserData(HCLIENT) = 0;
    virtual DRESULT SetClientUserData(HCLIENT, HOBJECT) = 0;
    virtual bool RandomFloat(float, float, float&) = 0;
    virtual bool PublishLightEffect(HOBJECT, const OpenBlood2::LightSettings&) = 0;
    virtual void RemoveLightEffect(HOBJECT) = 0;
    virtual bool PublishCameraEffect(HOBJECT, const OpenBlood2::CameraSettings&) = 0;
    virtual void RemoveCameraEffect(HOBJECT) = 0;
    virtual OpenBlood2::CameraView GetCameraView() = 0;
    virtual DRESULT AlignRotation(DRotation&, DVector forward) = 0; // Selected world-Y-up caller.
    virtual DRESULT InterpolateRotation(DRotation&, const DRotation&, const DRotation&, float) = 0;
    virtual OpenBlood2::MoverResult MoveKeyframedObject(HOBJECT, DVector, const DRotation*) = 0;
    virtual DDWORD GetObjectUserFlags(HOBJECT) = 0;
    virtual DRESULT SendObjectives(const OpenBlood2::MissionObjectives&) = 0;
    virtual bool GetClientResourceString(std::uint32_t, std::string&) = 0;
    virtual DRESULT SendToClient(HCLIENT, const OpenBlood2::PlayerEvent&) = 0;
    virtual DRESULT SetModelFilenames(HOBJECT, const char*, const char*) = 0;
    virtual DRESULT GetObjectDims(HOBJECT, DVector*) = 0;
    virtual DRESULT SetObjectRotation(HOBJECT, const DRotation*) = 0;
    virtual DRESULT MoveObjectToGround(HOBJECT) = 0;
    virtual DBOOL CompareStringsUpper(HSTRING, HSTRING) = 0;
    HCLASS (*GetClass)(const char*) = nullptr;
    HCLASS (*GetObjectClass)(HOBJECT) = nullptr;
    DBOOL (*IsKindOf)(HCLASS, HCLASS) = nullptr;
    LPBASECLASS (*CreateObject)(HCLASS, ObjectCreateStruct*) = nullptr;
    LPBASECLASS (*CreateObjectProps)(HCLASS, ObjectCreateStruct*, const char*) = nullptr;
    void (*RemoveObject)(HOBJECT) = nullptr;
    void (*SetNextUpdate)(HOBJECT, DFLOAT) = nullptr;
    DFLOAT (*GetTime)() = nullptr;
    DFLOAT (*GetFrameTime)() = nullptr;
    LPBASECLASS (*HandleToObject)(HOBJECT) = nullptr;
    HOBJECT (*ObjectToHandle)(LPBASECLASS) = nullptr;
    DDWORD (*GetObjectFlags)(HOBJECT) = nullptr;
    void (*SetObjectFlags)(HOBJECT, DDWORD) = nullptr;
    DRESULT (*SetObjectDims)(HOBJECT, DVector*) = nullptr;
    void (*SetObjectUserFlags)(HOBJECT, DDWORD) = nullptr;
    HOBJECT (*GetNextObject)(HOBJECT) = nullptr;
    char* (*GetObjectName)(HOBJECT) = nullptr;
    void (*GetObjectPos)(HOBJECT, DVector*) = nullptr;
    DRESULT (*GetObjectRotation)(HOBJECT, DRotation*) = nullptr;
    int (*GetObjectState)(HOBJECT) = nullptr;
    void (*SetObjectState)(HOBJECT, int) = nullptr;
    DRESULT (*GetPropGeneric)(const char*, GenericProp*) = nullptr;
    DRESULT (*GetPropString)(const char*, char*, int) = nullptr;
    DRESULT (*GetPropReal)(const char*, float*) = nullptr;
    DRESULT (*GetPropBool)(const char*, DBOOL*) = nullptr;
    HCONVAR (*GetGameConVar)(const char*) = nullptr;
    float (*GetVarValueFloat)(HCONVAR) = nullptr;
    char* (*GetVarValueString)(HCONVAR) = nullptr;
    void (*SetGameConVar)(const char*, const char*) = nullptr;
    DRESULT (*GetPropVector)(const char*, DVector*) = nullptr;
    HMODELANIM (*GetAnimIndex)(HOBJECT, const char*) = nullptr;
    void (*SetModelAnimation)(HOBJECT, HMODELANIM) = nullptr;
    HMODELANIM (*GetModelAnimation)(HOBJECT) = nullptr;
    DRESULT (*ResetModelAnimation)(HOBJECT) = nullptr;
    DDWORD (*GetModelPlaybackState)(HOBJECT) = nullptr;
    void (*SetModelLooping)(HOBJECT, DBOOL) = nullptr;
    DBOOL (*GetModelLooping)(HOBJECT) = nullptr;
    virtual DRESULT GetModelAnimUserDims(HOBJECT, DVector*, HMODELANIM) = 0;
    virtual DBOOL GetModelNodeTransform(HOBJECT, const char*, DVector*, DRotation*) = 0;
    virtual bool GetModelAnimationName(HOBJECT, HMODELANIM, std::string&) = 0;
    virtual DRESULT SetGlobalForce(DVector*) = 0;
    virtual DRESULT GetGlobalForce(DVector*) = 0;
    virtual DRESULT GetVelocity(HOBJECT, DVector*) = 0;
    virtual DRESULT SetVelocity(HOBJECT, DVector*) = 0;
    virtual DRESULT GetAcceleration(HOBJECT, DVector*) = 0;
    virtual DRESULT SetAcceleration(HOBJECT, DVector*) = 0;
    DRESULT (*TeleportObject)(HOBJECT, DVector*) = nullptr;
    HSTRING (*CreateString)(const char*) = nullptr;
    HSTRING (*CopyString)(HSTRING) = nullptr;
    void (*FreeString)(HSTRING) = nullptr;
    char* (*GetStringData)(HSTRING) = nullptr;
    ObjectList* (*FindNamedObjects)(const char*) = nullptr;
    ObjectList* (*CreateObjectList)() = nullptr;
    ObjectLink* (*AddObjectToList)(ObjectList*, HOBJECT) = nullptr;
    void (*RemoveObjectFromList)(ObjectList*, HOBJECT) = nullptr;
    void (*RelinquishList)(ObjectList*) = nullptr;
    DRESULT (*CreateInterObjectLink)(HOBJECT, HOBJECT) = nullptr;
    void (*BreakInterObjectLink)(HOBJECT, HOBJECT) = nullptr;
    DRESULT (*SaveObjects)(const char*, ObjectList*, DDWORD, DDWORD) = nullptr;
    DRESULT (*RestoreObjects)(const char*, DDWORD, DDWORD) = nullptr;
    virtual HMESSAGEWRITE StartMessageToObject(LPBASECLASS, HOBJECT, DDWORD) = 0;
    virtual DRESULT EndMessage(HMESSAGEWRITE) = 0;
    virtual DRESULT MoveObject(HOBJECT, DVector*) = 0;
};
