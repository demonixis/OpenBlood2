// Selected original SoccerObjects.cpp and CPlayerObj::SoccerGoal rules.
#include "SoccerObjects.h"
#include "PlayerObj.h"
#include "engine/collision.h"
#include "game/game_settings.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <cmath>

using namespace OpenBlood2;

namespace
{
    constexpr float update_delay = .001f, lifetime = 60.f, drag = .8f, min_speed = 20.f, max_speed = 1000.f;

    SoundRequest soccer_sound(std::string resource, float radius)
    {
        SoundRequest request;
        request.resource = std::move(resource);
        request.inner_radius = .1f * radius;
        request.outer_radius = radius;
        request.priority = 1;
        return request;
    }

    bool score_player(ServerDE& server, CPlayerObj& player, int goal_team, std::string& error)
    {
        if (server.net_game_type != GAMETYPE_SOCCER)
            return true; // Source has no NetGame to credit in an inspection or local custom map.
        const auto client = player.GetClient();
        const int team = server.GetClientTeam(client);
        if (team < 1 || team > 2 || server.GetClientUserData(client) != player.m_hObject ||
            server.soccer.goal_value < 0 || server.soccer.goal_value > 1000000)
            return false;
        const bool own = goal_team == team;
        unsigned multiplier = 1;
        if (server.soccer.use_team_size)
        {
            const auto counts = server.TeamPlayerCounts();
            multiplier = own ? counts[team - 1] : std::max(1u, counts[goal_team - 1]);
        }
        const auto value = std::int64_t(server.soccer.goal_value) * multiplier;
        const auto delta = own ? (server.negative_team_frags ? -value : 0) : value;
        const auto score = std::int64_t(player.m_Frags) + delta;
        if (delta < -1000000 || delta > 1000000 || score < -1000000 || score > 1000000 ||
            !server.AddClientTeamFrags(client, static_cast<int>(delta)))
            return false;
        player.m_Frags = static_cast<int>(score);
        auto request = soccer_sound("sounds_multipatch/ctfflagcapture.wav", 2000);
        request.speaker = player.m_hObject;
        server.PlayCharacterSound(player.m_hObject, request, false, error);
        return true;
    }
}

SoccerBall::~SoccerBall()
{
    auto* server = GetServerDE();
    if (!initialized || scored || !server)
        return;
    // The first remaining goal owns the request. During world clear there are no live goals.
    for (auto h = server->GetNextObject(nullptr); h; h = server->GetNextObject(h))
        if (auto* goal = dynamic_cast<SoccerGoal*>(server->HandleToObject(h)))
        {
            goal->ReplaceLostBall();
            break;
        }
}

DDWORD SoccerBall::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    auto* server = GetServerDE();
    if (msg == MID_PRECREATE)
    {
        auto* state = static_cast<ObjectCreateStruct*>(data);
        if (!state || !server->AddonBuild())
        {
            server->RejectPrecreate("SoccerBall art belongs to the Nightmares edition");
            return 0;
        }
        const bool zombie = !server->net_game_type || server->soccer.zombie_skin;
        SAFE_STRCPY(state->m_Filename, zombie ? "models_ao/worldobjects_ao/headball.abc"
                                              : "models_ao/worldobjects_ao/soccerball.abc");
        SAFE_STRCPY(state->m_SkinName, zombie ? "skins_ao/worldobjects_ao/headball.dtx"
                                              : "skins_ao/worldobjects_ao/soccerball.dtx");
        state->m_Flags = FLAG_TOUCH_NOTIFY | FLAG_GRAVITY | FLAG_SOLID | FLAG_VISIBLE | FLAG_REMOVEIFOUTSIDE;
    }
    else if (msg == MID_INITIALUPDATE)
    {
        DVector dims;
        if (server->GetModelAnimUserDims(m_hObject, &dims, 0) != DE_OK)
        {
            server->RemoveObject(m_hObject);
            return 0;
        }
        if (vector_dot(dims, dims) < 1)
            dims = {1, 1, 1};
        // Default LithTech object mass. Soccer applies its own drag in Update.
        if (server->SetObjectDims(m_hObject, &dims) != DE_OK || !server->ConfigureModelBody(m_hObject, {30, 0}) ||
            server->ResolveInitialWorldOverlap(m_hObject) != DE_OK)
        {
            server->RemoveObject(m_hObject);
            return 0;
        }
        radius = (dims.x + dims.y + dims.z) / 3.f;
        expires = server->GetTime() + lifetime;
        server->SetObjectUserFlags(m_hObject, server->GetObjectUserFlags(m_hObject) | (1u << 10));
        server->SetNextUpdate(m_hObject, update_delay);
        initialized = true;
    }
    else if (msg == MID_LINKBROKEN && static_cast<HOBJECT>(data) == last_player)
        last_player = nullptr;
    else if (msg == MID_TOUCHNOTIFY && initialized && !scored)
        Touch(static_cast<HOBJECT>(data), reason);
    else if (msg == MID_UPDATE && initialized && !scored)
        Update();
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}

void SoccerBall::Touch(HOBJECT other, float force)
{
    auto* server = GetServerDE();
    DVector velocity;
    if (server->GetVelocity(m_hObject, &velocity) != DE_OK)
        return;
    if (!other)
    {
        DVector normal;
        const auto speed = std::sqrt(vector_dot(velocity, velocity));
        if (server->GetContactNormal(m_hObject, normal))
        {
            const auto multiplier = .2f + .3f * std::min(1.f, speed / max_speed);
            auto adjusted = vector_add(velocity, vector_scale(normal, speed * multiplier));
            if (std::abs(adjusted.y + velocity.y) < 100)
                adjusted.y = velocity.y;
            server->SetVelocity(m_hObject, &adjusted);
        }
        bounced |= std::abs(force) > 100;
        return;
    }
    DVector other_velocity;
    if (!(server->GetObjectFlags(other) & FLAG_SOLID) || server->GetVelocity(other, &other_velocity) != DE_OK)
        return;
    const bool player = dynamic_cast<CPlayerObj*>(server->HandleToObject(other));
    velocity = vector_add(velocity, other_velocity);
    if (standing && velocity.y < 0)
        velocity.y = 0;
    if (player)
    {
        if (last_player != other && server->CreateInterObjectLink(m_hObject, other) == DE_OK)
        {
            if (last_player)
                server->BreakInterObjectLink(m_hObject, last_player);
            last_player = other;
        }
        expires = server->GetTime() + lifetime;
        float lift = 0;
        if (!server->RandomFloat(150, 300, lift))
            return;
        velocity.y += lift;
    }
    const auto speed = std::sqrt(vector_dot(velocity, velocity));
    DRotation aim;
    DVector jitter;
    if (server->RandomFloat(-.1f, .1f, jitter.x) && server->RandomFloat(-.1f, .1f, jitter.y) &&
        server->RandomFloat(-.1f, .1f, jitter.z) && speed > 0 && server->AlignRotation(aim, velocity) == DE_OK)
    {
        velocity = vector_scale(rotate_vector(compose_rotation(aim, rotation_from_euler(jitter)), {0, 0, 1}), speed);
    }
    server->SetVelocity(m_hObject, &velocity);
    bounced |= std::abs(force) > 100 || player;
}

void SoccerBall::Update()
{
    auto* server = GetServerDE();
    const auto now = server->GetTime();
    if (now > expires)
    {
        server->RemoveObject(m_hObject);
        return;
    }
    MovementState movement;
    if (!server->GetMovementState(m_hObject, movement))
        return;
    standing = movement.standing;
    if (standing)
        last_normal = movement.ground_normal;
    const auto displacement = vector_subtract(movement.position, last_position);
    last_position = movement.position;
    const DVector axis{displacement.y * last_normal.z - displacement.z * last_normal.y,
                       displacement.z * last_normal.x - displacement.x * last_normal.z,
                       displacement.x * last_normal.y - displacement.y * last_normal.x};
    const auto distance = std::sqrt(vector_dot(axis, axis));
    DRotation rotation;
    if (distance > 0 && server->GetObjectRotation(m_hObject, &rotation) == DE_OK)
    {
        const float half_turn = distance / (2 * radius);
        rotation = compose_rotation({vector_scale(axis, std::sin(half_turn) / distance), std::cos(half_turn)}, rotation);
        server->SetObjectRotation(m_hObject, &rotation);
    }

    auto velocity = movement.velocity;
    const auto speed = std::sqrt(vector_dot(velocity, velocity));
    if (speed < min_speed)
        velocity = {};
    else if (speed > max_speed)
        velocity = vector_scale(velocity, max_speed / speed);
    else
    {
        const auto terminal = vector_scale(movement.acceleration, 1 / drag);
        velocity = vector_add(terminal, vector_scale(vector_subtract(velocity, terminal),
                                                      std::exp(-drag * server->GetFrameTime())));
    }
    server->SetVelocity(m_hObject, &velocity);
    if (standing && last_normal.y < .9f && std::abs(velocity.y) < 50)
    {
        DVector gravity;
        if (server->GetGlobalForce(&gravity) == DE_OK)
        {
            movement.acceleration.y += gravity.y * .5f;
            server->SetAcceleration(m_hObject, &movement.acceleration);
        }
    }
    // The source checks against a zero-initialized bounce time and never updates it.
    if (bounced && now > .1f)
        server->PlayDetachedSound(m_hObject, soccer_sound("sounds_ao/events/soccerball.wav", 750), movement.position,
                                  sound_error);
    bounced = false;
    server->SetNextUpdate(m_hObject, update_delay);
}

bool SoccerGoal::ReadProperties(ObjectCreateStruct& state, float reason)
{
    auto* server = GetServerDE();
    if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
    {
        GenericProp property;
        if (server->GetPropGeneric("TeamID", &property) == DE_OK)
            team = property.m_Long;
        if (server->GetPropGeneric("Directional", &property) == DE_OK)
            directional = property.m_Bool;
        if (server->GetPropGeneric("GoalDirection", &property) == DE_OK)
            direction = rotate_vector(property.m_Rotation, {0, 0, 1});
        if (server->GetPropGeneric("BoxPhysics", &property) == DE_OK)
            box_physics = property.m_Bool;
        if (server->GetPropGeneric("ScoreSoundRadius", &property) == DE_OK)
            sound_radius = property.m_Float;
        if (server->GetPropGeneric("ScoreSound", &property) == DE_OK)
            sound = property.m_String;
        if (server->GetPropGeneric("ScoreTarget", &property) == DE_OK)
            score_target = property.m_String;
        if (server->GetPropGeneric("ScoreMsg", &property) == DE_OK)
            score_message = property.m_String;
    }
    if (team < 1 || team > 2 || !std::isfinite(sound_radius) || sound_radius < 0 || sound_radius > 1e6f)
    {
        server->RejectPrecreate("Invalid SoccerGoal team or sound radius");
        return false;
    }
    SAFE_STRCPY(state.m_Filename, state.m_Name);
    state.m_Flags = FLAG_TOUCH_NOTIFY | (box_physics ? FLAG_BOXPHYSICS : 0);
    return true;
}

DDWORD SoccerGoal::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    if (msg == MID_PRECREATE)
        return B2BaseClass::EngineMessageFn(msg, data, reason) && data &&
               ReadProperties(*static_cast<ObjectCreateStruct*>(data), reason);
    if (msg == MID_TOUCHNOTIFY)
        Touch(static_cast<HOBJECT>(data));
    else if (msg == MID_UPDATE)
        Update();
    return B2BaseClass::EngineMessageFn(msg, data, reason);
}

void SoccerGoal::Touch(HOBJECT h)
{
    auto* server = GetServerDE();
    auto* ball = h ? dynamic_cast<SoccerBall*>(server->HandleToObject(h)) : nullptr;
    if (!ball || ball->scored || !ball->last_player)
        return;
    auto* player = dynamic_cast<CPlayerObj*>(server->HandleToObject(ball->last_player));
    DVector position, velocity;
    if (!player || server->GetObjectPos(h, &position) != DE_OK || server->GetVelocity(h, &velocity) != DE_OK ||
        (directional && vector_dot(velocity, direction) <= 0))
        return;

    ball->scored = true; // Claim the ball before nested trigger delivery.
    server->RemoveObject(h);
    ++scores;
    score_error.clear();
    if (!score_player(*server, *player, team, score_error))
        score_error = "Soccer goal requires valid team membership and score bounds";
    server->PublishFleshGibs(m_hObject, position, vector_scale(velocity, -1), {25, 25, 25}, 1, 5, effect_error);
    if (!sound.empty() && server->GetObjectPos(m_hObject, &position) == DE_OK)
        server->PlayDetachedSound(m_hObject, soccer_sound(sound, sound_radius), position, sound_error);
    SendScoreTrigger();
}

void SoccerGoal::SendScoreTrigger()
{
    if (score_target.empty())
        return;
    auto* server = GetServerDE();
    auto target = server->CreateString(score_target.c_str());
    auto message = server->CreateString(score_message.c_str());
    SendTriggerMsgToObjects(this, target, message);
    if (message)
        server->FreeString(message);
    if (target)
        server->FreeString(target);
}

void SoccerGoal::ReplaceLostBall()
{
    if (replacements < 255)
        ++replacements;
    wait_one_frame = true;
    GetServerDE()->SetNextUpdate(m_hObject, update_delay);
}

void SoccerGoal::Update()
{
    auto* server = GetServerDE();
    if (wait_one_frame)
    {
        wait_one_frame = false;
        server->SetNextUpdate(m_hObject, update_delay);
        return;
    }
    while (replacements && server->IsObjectAlive(m_hObject))
    {
        --replacements;
        std::vector<HOBJECT> goals;
        for (auto h = server->GetNextObject(nullptr); h; h = server->GetNextObject(h))
            if (dynamic_cast<SoccerGoal*>(server->HandleToObject(h)))
                goals.push_back(h);
        int index = 0;
        if (goals.empty() || !server->RandomInt(0, static_cast<int>(goals.size() - 1), index))
            break;
        if (auto* goal = dynamic_cast<SoccerGoal*>(server->HandleToObject(goals[index])))
            goal->SendScoreTrigger();
    }
    if (!wait_one_frame && server->IsObjectAlive(m_hObject))
        server->SetNextUpdate(m_hObject, 0);
}
