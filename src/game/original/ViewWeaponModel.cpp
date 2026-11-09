// Selected original ObjectDLL/ViewWeaponModel.cpp server model keys.
#include "ViewWeaponModel.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include <utility>
using namespace OpenBlood2;

CViewWeaponModel::~CViewWeaponModel()
{
    StopLoop();
}

bool CViewWeaponModel::Init(HOBJECT owner, bool left)
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !server->IsObjectAlive(owner))
        return false;
    const auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(owner));
    const auto* weapon = player ? player->m_InventoryMgr.CurrentWeapon(left) : nullptr;
    if (!weapon)
        return false;
    if (owner_ != owner && server->CreateInterObjectLink(m_hObject, owner) != DE_OK)
        return false;
    if (owner_ && owner_ != owner)
        server->BreakInterObjectLink(m_hObject, owner_);
    StopLoop();
    pending_fire_.reset();
    owner_ = owner;
    left_ = left;
    weapon_ = weapon->data.m_nType;
    return true;
}

CPlayerObj* CViewWeaponModel::CurrentOwner() const
{
    auto* server = GetServerDE();
    if (!server || !server->IsObjectAlive(m_hObject) || !server->IsObjectAlive(owner_))
        return nullptr;
    auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(owner_));
    const auto* weapon = player ? player->m_InventoryMgr.CurrentWeapon(left_) : nullptr;
    return weapon && weapon->data.m_nType == weapon_ ? player : nullptr;
}

void CViewWeaponModel::UseModelKeys(DBOOL enabled)
{
    auto* server = GetServerDE();
    if (server && server->IsObjectAlive(m_hObject))
    {
        const auto flags = server->GetObjectFlags(m_hObject);
        server->SetObjectFlags(m_hObject, enabled ? flags | FLAG_MODELKEYS : flags & ~FLAG_MODELKEYS);
    }
}

void CViewWeaponModel::SetVisible(DBOOL visible)
{
    auto* server = GetServerDE();
    if (server && server->IsObjectAlive(m_hObject))
    {
        const auto flags = server->GetObjectFlags(m_hObject);
        server->SetObjectFlags(m_hObject, visible ? flags | FLAG_VISIBLE : flags & ~FLAG_VISIBLE);
    }
}

bool CViewWeaponModel::QueueFire(const WeaponFire& shot)
{
    if (!CurrentOwner() || pending_fire_ || shot.weapon != weapon_ || shot.left != left_ ||
        !physics_vector(shot.position) || !physics_vector(shot.direction) ||
        vector_dot(shot.direction, shot.direction) < 1e-12f)
        return false;
    pending_fire_ = shot;
    return true;
}

void CViewWeaponModel::StopLoop()
{
    if (loop_)
        if (auto* server = GetServerDE())
            server->KillSound(std::exchange(loop_, 0));
}

void CViewWeaponModel::PlayKeySound(std::string_view filename, bool loop)
{
    if (!CurrentOwner() || filename.empty() || filename.size() > 240 || (loop && loop_))
        return;
    auto* server = GetServerDE();
    SoundRequest request;
    request.resource = "sounds/weapons/";
    for (char c : filename)
    {
        if (c == '\\')
            c = '/';
        if (c != '/' || request.resource.back() != '/')
            request.resource += c;
    }
    request.speaker = owner_;
    request.inner_radius = 100;
    request.outer_radius = 1000;
    request.priority = 6; // SOUNDPRIORITY_PLAYER_HIGH
    request.loop = loop;
    std::string error;
    if (loop)
        loop_ = server->PlayObjectLoopSound(owner_, request, error);
    else
        server->PlayCharacterSound(owner_, request, false, error);
}

void CViewWeaponModel::OnStringKey(const ArgList& args)
{
    if (!args.argv || args.argc < 1 || args.argc > 64)
        return;
    auto* server = GetServerDE();
    for (int i = 0; i < args.argc && args.argv[i] && server->IsObjectAlive(m_hObject); ++i)
    {
        const std::string_view key(args.argv[i]);
        if (same_name(key, "FIRE_KEY"))
        {
            auto shot = std::exchange(pending_fire_, std::nullopt);
            if (shot)
                if (auto* player = CurrentOwner())
                    player->HandleWeaponFireMessage(*shot);
        }
        else if (same_name(key, "SOUNDSTOP_KEY"))
            StopLoop();
        else if (same_name(key, "SOUND_KEY") || same_name(key, "SOUNDLOOP_KEY"))
        {
            if (++i < args.argc && args.argv[i])
                PlayKeySound(args.argv[i], same_name(key, "SOUNDLOOP_KEY"));
        }
        else if (same_name(key, "HIDE_KEY") || same_name(key, "SHOW_KEY"))
        {
            if (++i < args.argc && args.argv[i])
                server->SetModelNodeHideStatus(m_hObject, args.argv[i], same_name(key, "HIDE_KEY"));
        }
    }
}

DDWORD CViewWeaponModel::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (auto* server = GetServerDE())
    {
        if (message == MID_INITIALUPDATE)
            server->SetObjectFlags(m_hObject, FLAG_MODELGOURAUDSHADE | FLAG_REALLYCLOSE | FLAG_FORCECLIENTUPDATE);
        else if (message == MID_MODELSTRINGKEY && data)
            OnStringKey(*static_cast<const ArgList*>(data));
        else if (message == MID_LINKBROKEN && owner_ && static_cast<HOBJECT>(data) == owner_)
        {
            owner_ = nullptr;
            pending_fire_.reset();
            StopLoop();
            server->RemoveObject(m_hObject);
        }
    }
    return BaseClass::EngineMessageFn(message, data, reason);
}
