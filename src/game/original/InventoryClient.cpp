// Original BloodClientShell and ViewCreature callbacks behind checked local messages.
#include "BloodShells.h"
#include "BoneLeech.h"
#include "SeeingEye.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;
bool CBloodServerShell::OnMessage(HCLIENT client, const CreatureResponse& response)
{
    auto* p = Player(client);
    auto* b = server_.IsObjectAlive(response.creature)
                  ? dynamic_cast<BoneLeech*>(server_.HandleToObject(response.creature))
                  : nullptr;
    return p && b && b->Respond(p->m_hObject, response.proceed);
}
bool CBloodServerShell::OnMessage(HCLIENT client, const PlayerRotation& rotation)
{
    auto* p = Player(client);
    if (!p || !std::isfinite(rotation.pitch) || !std::isfinite(rotation.yaw) || std::abs(rotation.pitch) > 1000000 ||
        std::abs(rotation.yaw) > 1000000)
        return false;
    p->m_fPitch = rotation.pitch;
    p->m_fYaw = rotation.yaw;
    if (p->m_InventoryMgr.eye && p->m_InventoryMgr.eye->active)
    {
        const auto eye = p->m_InventoryMgr.eye->eye;
        const auto pose = rotation_from_euler({rotation.pitch, rotation.yaw, 0});
        return server_.SetObjectRotation(eye, &pose) == DE_OK;
    }
    return true;
}
void CBloodClientShell::ClearInventoryEffects()
{
    if (goggles_loop)
        server_.KillSound(goggles_loop);
    if (eye_loop)
        server_.KillSound(eye_loop);
    if (server_.IsObjectAlive(creature))
        if (auto* b = dynamic_cast<BoneLeech*>(server_.HandleToObject(creature)); b && b->attached == player)
            b->Detach();
    goggles_loop = eye_loop = 0;
    seeing_eye = creature = nullptr;
    night_goggles = false;
    goggles_down = goggles_was_down = eye_down = eye_was_down = false;
    blind_until = creature_damage_time = last_use_hit = 0;
    creature_light = 1;
    use_hits = total_use_hits = 0;
}
bool CBloodClientShell::HandleInventoryView(const PlayerEvent& event, CBloodServerShell& server)
{
    if (const auto* v = std::get_if<SeeingEyeView>(&event))
    {
        if (eye_loop)
            server_.KillSound(eye_loop);
        eye_loop = 0;
        seeing_eye = nullptr;
        if (v->active)
        {
            auto* e =
                server_.IsObjectAlive(v->eye) ? dynamic_cast<SeeingEye*>(server_.HandleToObject(v->eye)) : nullptr;
            if (!e || e->m_hFiredFrom != player)
                return true; // Removed between enqueue and delivery: never retain a stale camera.
            seeing_eye = v->eye;
        }
        if (v->blind_seconds > 0)
            blind_until = server_.GetTime() + v->blind_seconds;
        return UpdateCameraSelection() && UpdateInventoryEffects();
    }
    const auto* v = std::get_if<CreatureAttach>(&event);
    if (!v || v->enemy != player)
        return false;
    if (!v->active)
    {
        if (creature == v->creature)
        {
            creature = nullptr;
            creature_light = 1;
        }
        return true;
    }
    auto* b =
        server_.IsObjectAlive(v->creature) ? dynamic_cast<BoneLeech*>(server_.HandleToObject(v->creature)) : nullptr;
    if (!b || b->attached != player)
        return true;
    if (creature && server_.IsObjectAlive(creature))
    {
        server.OnMessage(client_, CreatureResponse{v->creature, false});
        return true;
    }
    if (!server.OnMessage(client_, CreatureResponse{v->creature, true}))
        return false;
    creature = v->creature;
    creature_light = 1;
    creature_damage_time = last_use_hit = 0;
    use_hits = total_use_hits = 0;
    return UpdateInventoryEffects();
}
bool CBloodClientShell::UpdateInventoryEffects()
{
    if (seeing_eye && !server_.IsObjectAlive(seeing_eye))
        seeing_eye = nullptr;
    auto loop = [&](bool active, SoundId& voice, const char* resource, int priority)
    {
        if (!active)
        {
            if (voice)
                server_.KillSound(voice);
            voice = 0;
        }
        else if (!voice)
        {
            SoundRequest request;
            request.resource = resource;
            request.space = SoundSpace::Local;
            request.loop = true;
            request.priority = priority;
            request.reverb_requested = false;
            voice = server_.PlayCinematicSound(player, request, item_audio_error);
        }
    };
    loop(night_goggles, goggles_loop, "sounds\\powerups\\nightgogglesloop1.wav", 4);
    loop(seeing_eye != nullptr, eye_loop, "sounds\\powerups\\eyeloop1.wav", 2);
    if (creature)
    {
        auto* b =
            server_.IsObjectAlive(creature) ? dynamic_cast<BoneLeech*>(server_.HandleToObject(creature)) : nullptr;
        if (!b || b->attached != player)
        {
            creature = nullptr;
            creature_light = 1;
        }
        else
        {
            const auto pose = rotation_from_euler({rotation.pitch, rotation.yaw, 0});
            auto pos = vector_add(camera_position,
                                  rotate_vector(pose, b->npc_type == AI_Mgr::NPCType::Thief ? DVector{0, -.4f, 2}
                                                                                            : DVector{0, -2.5f, .6f}));
            if (server_.SetObjectRotation(creature, &pose) != DE_OK || server_.TeleportObject(creature, &pos) != DE_OK)
                return false;
            if (b->npc_type != AI_Mgr::NPCType::Thief && server_.GetTime() - creature_damage_time >= 1)
            {
                creature_damage_time = server_.GetTime();
                creature_light *= .9f;
            }
        }
    }
    return true;
}
DVector CBloodClientShell::SceneLightScale() const
{
    if (server_.GetTime() < blind_until)
        return {};
    if (seeing_eye)
        return {1, 0, .5f};
    if (creature)
        return {creature_light, creature_light, creature_light};
    return night_goggles ? DVector{.1f, 1, .1f} : DVector{1, 1, 1};
}
