#pragma once
// Source-level subset of Blood II AppHeaders/basedefs_de.h, basetypes_de.h
// and de_codes.h. No retail DLL ABI is exposed. See Documentation/implementation/provenance.md.
#include <cstddef>
#include <cstdint>
#include <cstring>

using DBYTE = std::uint8_t;
using D_WORD = std::uint16_t;
using DDWORD = std::uint32_t;
using DBOOL = std::int32_t;
using DRESULT = std::int32_t;
using DFLOAT = float;
inline constexpr DBOOL DTRUE = 1, DFALSE = 0;
inline constexpr DRESULT DE_OK = 0, DE_ERROR = 1, DE_INVALIDPARAMS = 60, DE_NOTFOUND = 61;
#define DNULL nullptr
class BaseClass;
class Aggregate;
class ServerDE;
struct DObject;
struct StringHandle;
struct ConsoleVariable;
struct ClientHandle;
namespace OpenBlood2
{
    struct ClassInfo;
    // Source-level identity, not a pointer or a retail packet/DLL ABI. A released
    // value is never reused, even by another adapter; only live payloads remain.
    struct MessageHandle
    {
        std::uint64_t value = 0;
        constexpr MessageHandle(std::nullptr_t = nullptr) {}
        explicit constexpr MessageHandle(std::uint64_t identity) : value(identity) {}
        explicit constexpr operator bool() const
        {
            return value != 0;
        }
        bool operator==(const MessageHandle&) const = default;
    };
    struct MessageHandleHash
    {
        std::size_t operator()(MessageHandle handle) const noexcept
        {
            return static_cast<std::size_t>(handle.value ^ (handle.value >> 32));
        }
    };
} // namespace OpenBlood2
using LPBASECLASS = BaseClass*;
using LPAGGREGATE = Aggregate*;
using HOBJECT = DObject*;
using HSTRING = StringHandle*;
using HMESSAGEREAD = OpenBlood2::MessageHandle;
using HMESSAGEWRITE = OpenBlood2::MessageHandle;
using HCLASS = const OpenBlood2::ClassInfo*;
using HCONVAR = ConsoleVariable*;
using HCLIENT = ClientHandle*;
using HMODELANIM = DDWORD;
using HATTACHMENT = std::uint64_t; // Checked, non-reused session identity; no DLL ABI.
inline constexpr DRESULT DE_NODENOTFOUND = 59;
inline constexpr DDWORD MS_PLAYDONE = 1;
inline constexpr DDWORD FLAG_SKYOBJECT = 1u << 12, FLAG_FORCEOPTIMIZEOBJECT = 1u << 28;
struct ArgList
{
    char** argv;
    int argc;
};
using CServerDE = ServerDE;
struct DVector
{
    float x = 0, y = 0, z = 0;
};
struct DRotation
{
    DVector m_Vec{};
    float m_Spin = 1;
};
#define VEC_INIT(v) ((v) = DVector{})
#define ROT_INIT(v) ((v) = DRotation{})
#define VEC_COPY(dest, src) ((dest) = (src))
#define VEC_SET(v, a, b, c) ((v) = DVector{a, b, c})
#define ROT_COPY(dest, src) ((dest) = (src))
template <std::size_t N> void legacy_copy(char (&to)[N], const char* from)
{
    std::size_t size = 0;
    while (size + 1 < N && from[size])
    {
        to[size] = from[size];
        ++size;
    }
    to[size] = '\0';
}
#define SAFE_STRCPY(dest, src) legacy_copy(dest, src)
inline constexpr int MAX_CS_FILENAME_LEN = 100;
inline constexpr int MAX_GP_STRING_LEN = 200;
inline constexpr DBYTE OT_NORMAL = 0, OT_MODEL = 1, OT_WORLDMODEL = 2, OT_SPRITE = 3, OT_LIGHT = 4, OT_CONTAINER = 9;
inline constexpr DDWORD FLAG_FULLPOSITIONRES = 1u << 8, FLAG_GOTHRUWORLD = 1u << 21, FLAG_FORCECLIENTUPDATE = 1u << 24;
inline constexpr DDWORD FLAG_REALLYCLOSE = 1u << 6;
inline constexpr DDWORD FLAG_NOLIGHT = 1u << 9;
inline constexpr DDWORD FLAG_MODELGOURAUDSHADE = 1u << 3, FLAG_ANIMTRANSITION = 1u << 7, FLAG_YROTATION = 1u << 11,
                        FLAG_STAIRSTEP = 1u << 18;
inline constexpr DDWORD FLAG_VISIBLE = 1u << 0, FLAG_SHADOW = 1u << 1, FLAG_ROTATEABLESPRITE = 1u << 3,
                        FLAG_SPRITECHROMAKEY = 1u << 4, FLAG_SOLID = 1u << 13, FLAG_BOXPHYSICS = 1u << 14,
                        FLAG_TOUCH_NOTIFY = 1u << 16, FLAG_GRAVITY = 1u << 17, FLAG_MODELKEYS = 1u << 19,
                        FLAG_RAYHIT = 1u << 22, FLAG_DONTFOLLOWSTANDING = 1u << 23, FLAG_NOSLIDING = 1u << 25,
                        FLAG_REMOVEIFOUTSIDE = 1u << 27, FLAG_POINTCOLLIDE = 1u << 26;
struct ObjectCreateStruct
{
    D_WORD m_ObjectType = OT_NORMAL;
    DDWORD m_Flags = 0;
    DVector m_Pos{}, m_Scale{1, 1, 1};
    DRotation m_Rotation{};
    D_WORD m_ContainerCode = 0;
    DDWORD m_UserData = 0;
    char m_Filename[MAX_CS_FILENAME_LEN + 1]{};
    char m_SkinName[MAX_CS_FILENAME_LEN + 1]{};
    char m_Name[MAX_CS_FILENAME_LEN + 1]{};
    float m_NextUpdate = 0, m_fDeactivationTime = 0;
};
#define INIT_OBJECTCREATESTRUCT(s) ((s) = ObjectCreateStruct{})
struct GenericProp
{
    DVector m_Vec{}, m_Color{};
    char m_String[MAX_GP_STRING_LEN + 1]{};
    DRotation m_Rotation{};
    std::int32_t m_Long = 0;
    float m_Float = 0;
    DBOOL m_Bool = DFALSE;
};
struct ObjectLink
{
    HOBJECT m_hObject = nullptr;
    ObjectLink* m_pNext = nullptr;
};
struct ObjectList
{
    ObjectLink* m_pFirstLink = nullptr;
    int m_nInList = 0;
};
inline constexpr DDWORD OBJSTATE_ACTIVE = 0, OBJSTATE_INACTIVE = 1, OBJSTATE_INACTIVE_TOUCH = 2,
                        OBJSTATE_AUTODEACTIVATE_NOW = 3;
static_assert(sizeof(DDWORD) == 4 && sizeof(float) == 4);
