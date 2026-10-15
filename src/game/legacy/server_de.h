#pragma once
#include "basedefs_de.h"

// Deliberately bounded source interface. Add reviewed declarations with their
// implementation/tests as original callers arrive. Absent services fail to
// compile; the explicitly deferred services below return DE_ERROR + diagnostics.
// Read-only input strings are const; fixed-width integers replace Windows long.
class CSBase {
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

class ServerDE : public CSBase {
public:
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
