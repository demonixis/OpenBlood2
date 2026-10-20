// Selective original FlagObjects.cpp; typed checked engine services, no wire ABI.
#include "FlagObjects.h"
#include "engine/collision.h"
#include "game/game_settings.h"
using namespace OpenBlood2;
CTFWorldState g_ctf;
namespace
{
    bool properties(BaseClass& object, ObjectCreateStruct* c, int& team, bool stand, float reason)
    {
        auto* s = object.GetServerDE();
        if (!c || reason == PRECREATE_SAVEGAME)
            return false;
        GenericProp p;
        if ((reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP) &&
            s->GetPropGeneric("TeamID", &p) == DE_OK)
            team = p.m_Long;
        c->m_Flags |= FLAG_TOUCH_NOTIFY | FLAG_VISIBLE;
        c->m_Flags &= ~FLAG_GRAVITY;
        c->m_NextUpdate = .001f;
        if (team == 1 || team == 2)
            SAFE_STRCPY(c->m_Filename, stand       ? "Models_Multipatch\\flagbase.abc"
                                       : team == 1 ? "Models\\WorldObjects\\flagblue.abc"
                                                   : "Models\\WorldObjects\\flagred.abc");
        // Preserve the original skin-branch assignment AFTER model selection.
        if (team != 1)
            team = 2;
        SAFE_STRCPY(c->m_SkinName,
                    stand ? (team == 1 ? "Skins_Multipatch\\flagbaseblue.dtx" : "Skins_Multipatch\\flagbasered.dtx")
                          : (team == 1 ? "Skins\\WorldObjects\\flagblue.dtx" : "Skins\\WorldObjects\\flagred.dtx"));
        if (!c->m_Filename[0])
        {
            s->RejectPrecreate("Original flag TeamID selects no model");
            return false;
        }
        return true;
    }
    bool dimensions(HOBJECT h)
    {
        auto* s = BaseClass::GetServerDE();
        DVector d;
        if (s->GetModelAnimUserDims(h, &d, s->GetModelAnimation(h)) != DE_OK)
            return false;
        // These source objects are nonphysical. The later engine's
        // ChangeObjectDimensions assigns centered dimensions in this branch;
        // the character ResizeObject service intentionally preserves feet.
        if (!(s->GetObjectFlags(h) & (FLAG_SOLID | FLAG_GRAVITY)))
            return s->SetObjectDims(h, &d) == DE_OK;
        const auto result = s->ResizeObject(h, d);
        return result == ResizeResult::Resized || (result == ResizeResult::Blocked && s->SetObjectDims(h, &d) == DE_OK);
    }
    bool trigger(BaseClass& owner, HOBJECT player, HSTRING command)
    {
        auto* s = BaseClass::GetServerDE();
        if (!s->IsKindOf(s->GetObjectClass(player), s->GetClass("CPlayerObj")))
            return true;
        auto msg = s->StartMessageToObject(&owner, player, 1000);
        if (!msg)
            return false;
        s->WriteToMessageHString(msg, command);
        return s->EndMessage(msg) == DE_OK;
    }
} // namespace
FlagObject::~FlagObject()
{
    if (grab_string)
        GetServerDE()->FreeString(grab_string);
    for (auto& flag : g_ctf.flags)
        if (flag == this)
            flag = nullptr;
}
FlagStand::~FlagStand()
{
    if (give_string)
        GetServerDE()->FreeString(give_string);
    // The spawned flag is independently world-owned in the original.
}
DDWORD FlagObject::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (msg == MID_PRECREATE && !properties(*this, static_cast<ObjectCreateStruct*>(data), team, false, reason))
        return 0;
    if (msg == MID_INITIALUPDATE)
    {
        if (!dimensions(m_hObject) || s->GetObjectPos(m_hObject, &stand_position) != DE_OK ||
            s->GetObjectRotation(m_hObject, &stand_rotation) != DE_OK)
            error = "Flag model dimensions/pose unavailable";
        in_stand = true;
        on_ground = with_player = false;
        g_ctf.flags[team - 1] = this;
        grab_string = s->CreateString("flaggrab");
        if (!grab_string)
            error = "Flag trigger string unavailable";
    }
    else if (msg == MID_TOUCHNOTIFY && !with_player && !trigger(*this, static_cast<HOBJECT>(data), grab_string))
        error = "Flag grab trigger delivery failed";
    else if (msg == MID_UPDATE && on_ground)
        ReturnToFlagStand();
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}
DDWORD FlagStand::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    auto* s = GetServerDE();
    if (msg == MID_PRECREATE)
    {
        const auto accepted = B2BaseClass::EngineMessageFn(msg, data, reason);
        return accepted && properties(*this, static_cast<ObjectCreateStruct*>(data), team, true, reason);
    }
    if (msg == MID_INITIALUPDATE)
    {
        if (!dimensions(m_hObject) || s->MoveObjectToGround(m_hObject) != DE_OK)
            error = "Flag stand dimensions/ground unavailable";
        give_string = s->CreateString("flaggive");
        if (!give_string || !SpawnFlag())
            error = "Flag stand child/trigger creation failed";
    }
    else if (msg == MID_TOUCHNOTIFY && !trigger(*this, static_cast<HOBJECT>(data), give_string))
        error = "Flag capture trigger delivery failed";
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}
bool FlagStand::SpawnFlag()
{
    if (team < 1 || team > 2)
        return false;
    if (flag)
        return true;
    auto* s = GetServerDE();
    ObjectCreateStruct c;
    if (s->GetObjectPos(m_hObject, &c.m_Pos) != DE_OK || s->GetObjectRotation(m_hObject, &c.m_Rotation) != DE_OK)
        return false;
    c.m_Pos.x -= 16;
    c.m_Pos.y += 32;
    auto* child = s->CreateObjectProps(s->GetClass("FlagObject"), &c, team == 1 ? "TeamID 1" : "TeamID 2");
    flag = child ? child->m_hObject : nullptr;
    return flag != nullptr;
}
void FlagObject::GiveToPlayer(HOBJECT player)
{
    in_stand = on_ground = false;
    with_player = true;
    carrier = player;
    ++grabs;
}
bool FlagObject::ReturnToFlagStand()
{
    if (in_stand)
        return true;
    auto* s = GetServerDE();
    HATTACHMENT attached = 0;
    // Source SetObjectPos/rotation cannot override the next attachment pose.
    // Keep that observable result for an explicit RETURN while still carried.
    if (!carrier || s->FindAttachment(carrier, m_hObject, &attached) != DE_OK)
    {
        const DRotation temp = rotation_from_euler({1, 1, 1});
        auto rotation = temp;
        if (s->SetObjectRotation(m_hObject, &rotation) != DE_OK ||
            s->TeleportObject(m_hObject, &stand_position) != DE_OK ||
            s->SetObjectRotation(m_hObject, &stand_rotation) != DE_OK)
        {
            error = "Flag return pose unavailable";
            return false;
        }
    }
    SoundRequest request;
    request.resource = "sounds_multipatch\\CtfFlagRespawn.WAV";
    request.inner_radius = 200;
    request.outer_radius = 2000;
    request.priority = 1;
    request.reverb_requested = false;
    voice = s->PlayDetachedSound(m_hObject, request, stand_position, sound_error);
    in_stand = true;
    on_ground = with_player = false;
    carrier = nullptr;
    ++returns;
    return sound_error.empty();
}
bool FlagObject::DropToGround()
{
    auto* s = GetServerDE();
    if (s->MoveObjectToGround(m_hObject) != DE_OK)
    {
        error = "Dropped flag ground unavailable";
        return false;
    }
    in_stand = with_player = false;
    on_ground = true;
    carrier = nullptr;
    ++drops;
    s->SetNextUpdate(m_hObject, 30);
    return true;
}
bool FlagObject::IsOtherFlagInStand() const
{
    FlagObject* other = this == g_ctf.flags[0] ? g_ctf.flags[1] : this == g_ctf.flags[1] ? g_ctf.flags[0] : nullptr;
    return other && other != this && GetServerDE()->IsObjectAlive(other->m_hObject) && other->in_stand;
}
DDWORD FlagObject::ObjectMessageFn(HOBJECT sender, DDWORD msg, HMESSAGEREAD read)
{
    if (msg == 1000)
    {
        auto* s = GetServerDE();
        auto text = s->ReadFromMessageHString(read);
        const auto* bytes = s->GetStringData(text);
        const bool back = bytes && same_name(bytes, "return");
        s->FreeString(text);
        if (back)
            ReturnToFlagStand();
    }
    return B2BaseClass::ObjectMessageFn(sender, msg, read);
}
