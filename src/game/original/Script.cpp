// Selected ObjectDLL/Script.cpp interpreter. Object scripts are separate from AI trigger scripts.
#include "Script.h"
#include "CameraObj.h"
#include "FireEffects.h"
#include "engine/collision.h"
#include "trigger_helpers.h"
#include <algorithm>
#include <charconv>
#include <cmath>

using namespace OpenBlood2;

namespace
{
    constexpr std::string_view separators = " ,\t\r";

    bool tokenize(std::string_view line, std::string_view delimiters, std::vector<std::string_view>& tokens)
    {
        while (!line.empty())
        {
            const auto first = line.find_first_not_of(delimiters);
            if (first == line.npos)
                break;
            line.remove_prefix(first);
            if (line.starts_with(';') || line.starts_with("//"))
                break;
            const bool quoted = line.starts_with('"');
            if (quoted)
                line.remove_prefix(1);
            const auto end = quoted ? line.find('"') : line.find_first_of(delimiters);
            if (quoted && (end == line.npos ||
                           (end + 1 < line.size() && delimiters.find(line[end + 1]) == delimiters.npos)))
                return false;
            const auto token = line.substr(0, end);
            if (token.size() > 50 || (!quoted && token.find('"') != token.npos))
                return false;
            tokens.push_back(token);
            if (end == line.npos)
                break;
            line.remove_prefix(end + 1);
        }
        return true;
    }

    bool value(std::string_view token, char kind, float& output)
    {
        if (token.starts_with('+'))
            token.remove_prefix(1);
        if (token.empty())
            return false;
        const auto end = token.data() + token.size();
        if (kind == 'f')
        {
            const auto parsed = std::from_chars(token.data(), end, output);
            return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(output) && std::abs(output) <= 1e7f;
        }
        int integer = 0;
        const auto parsed = std::from_chars(token.data(), end, integer);
        if (parsed.ec != std::errc{} || parsed.ptr != end || integer < -1000000 || integer > 1000000)
            return false;
        if (kind == 'i' && (integer < 0 || integer >= 30))
            return false;
        output = static_cast<float>(integer);
        return true;
    }
} // namespace

bool Script::Prepare()
{
    auto* server = GetServerDE();
    std::string contents;
    if (!server->ReadScriptText(filename_, contents))
    {
        error = "Script resource is missing, oversized or malformed: " + filename_;
        return false;
    }
    struct Spec
    {
        std::string_view name;
        Op op;
        std::string_view args; // i: slot, n: integer, f: finite scalar, s: text
    };
    static constexpr Spec specs[]{{"START", Op::Start, ""},
                                  {"END", Op::End, ""},
                                  {"WAIT", Op::Wait, "f"},
                                  {"OBJECTASSIGN", Op::Assign, "is"},
                                  {"OBJECTCREATE", Op::Create, "isfff"},
                                  {"OBJECTCREATENEAR", Op::CreateNear, "isi"},
                                  {"OBJECTCREATEMODEL", Op::CreateModel, "issfff"},
                                  {"OBJECTCREATEMODELNEAR", Op::CreateModelNear, "iiss"},
                                  {"OBJECTCREATEFIRE", Op::CreateFire, "iif"},
                                  {"OBJECTCREATE_EXPLOSIONAT", Op::ExplosionAt, "ffffffff"},
                                  {"OBJECTCREATE_EXPLOSIONNEAR", Op::ExplosionNear, "ifffff"},
                                  {"OBJECTMOVE", Op::Move, "ifff"},
                                  {"OBJECTMOVETOOBJ", Op::MoveTo, "ifi"},
                                  {"OBJECTTELEPORT", Op::Teleport, "ifff"},
                                  {"OBJECTFLAGS", Op::Flags, "is"},
                                  {"OBJECTFLAGVISIBLE", Op::Visible, "in"},
                                  {"OBJECTFLAGSOLID", Op::Solid, "in"},
                                  {"OBJECTFLAGGRAVITY", Op::Gravity, "in"},
                                  {"OBJECTDIMS", Op::Dims, "ifff"},
                                  {"OBJECTSCALE", Op::Scale, "ifff"},
                                  {"OBJECTDESTROY", Op::Destroy, "i"},
                                  {"OBJECTGIB", Op::Gib, "i"},
                                  {"PLAYANIM", Op::Animation, "in"},
                                  {"PLAYSOUNDPOS", Op::SoundAt, "ifffsfn"},
                                  {"PLAYSOUNDOBJ", Op::SoundOn, "iisfn"},
                                  {"KILLSOUND", Op::KillSound, "i"},
                                  {"SENDTRIGGER", Op::Trigger, "is"},
                                  {"SENDAISCRIPT", Op::Trigger, "is"},
                                  {"SENDTRIGGERNAMED", Op::TriggerNamed, "ss"}};
    std::vector<std::pair<std::string_view, std::string_view>> defines;
    std::vector<Command> prepared;
    std::string_view remaining(contents);
    unsigned line_number = 0;
    const auto reject = [&](std::string reason)
    {
        error = filename_ + " line " + std::to_string(line_number) + ": " + reason;
        return false;
    };
    while (!remaining.empty())
    {
        ++line_number;
        const auto next_line = remaining.find('\n');
        const auto line = remaining.substr(0, next_line);
        remaining.remove_prefix(next_line == remaining.npos ? remaining.size() : next_line + 1);
        const auto first = line.find_first_not_of(separators);
        const bool define = first != line.npos && line.substr(first).starts_with("define");
        std::vector<std::string_view> tokens;
        if (line.size() > 255 || !tokenize(line, define ? " =\t\r" : separators, tokens))
            return reject("malformed or oversized line");
        if (tokens.empty())
            continue;
        if (define)
        {
            if (tokens.size() != 3 || tokens[1].empty() || defines.size() == 50)
                return reject("invalid define");
            defines.emplace_back(tokens[1], tokens[2]);
            continue;
        }
        const auto* spec = std::find_if(std::begin(specs), std::end(specs),
                                        [&](const Spec& entry) { return same_name(entry.name, tokens[0]); });
        if (spec == std::end(specs))
            return reject("unsupported command " + std::string(tokens[0]));
        // The original move parser asks for six arguments. Its handler reads the first three.
        const bool padded_move = spec->op == Op::MoveTo && tokens.size() == 7;
        if ((!padded_move && tokens.size() != spec->args.size() + 1) || prepared.size() == 1024)
            return reject("wrong argument count or script command limit reached");
        Command command{spec->op, line_number, {}};
        for (unsigned index = 0; index < spec->args.size(); ++index)
        {
            auto token = tokens[index + 1];
            if (token.starts_with('#'))
                for (const auto& [key, replacement] : defines)
                    if (token.substr(1).starts_with(key))
                    {
                        token = replacement;
                        break;
                    }
            const auto kind = spec->args[index];
            if (kind == 's')
                command.text[index] = token;
            else if (!value(token, kind, command.number[index]))
                return reject("invalid argument " + std::to_string(index + 1));
        }
        if ((command.op == Op::Create || command.op == Op::CreateNear) && !server->GetClass(command.text[1].c_str()))
            return reject("unknown object class " + command.text[1]);
        prepared.push_back(std::move(command));
    }
    commands_ = std::move(prepared);
    return true;
}

bool Script::SetSlot(unsigned slot, HOBJECT object)
{
    auto* server = GetServerDE();
    if (!server->IsObjectAlive(object) || object == m_hObject ||
        server->CreateInterObjectLink(m_hObject, object) != DE_OK)
        return false;
    const auto previous = objects_[slot];
    objects_[slot] = object;
    for (auto& motion : motions_)
        if (motion.object && motion.slot == slot && motion.object != object)
            motion.object = nullptr;
    if (previous && std::find(objects_.begin(), objects_.end(), previous) == objects_.end())
        server->BreakInterObjectLink(m_hObject, previous);
    return true;
}

HOBJECT Script::Create(std::string_view class_name, ObjectCreateStruct& state)
{
    auto* server = GetServerDE();
    const std::string name(class_name);
    const auto type = server->GetClass(name.c_str());
    if (type)
        if (auto* object = server->CreateObject(type, &state))
            return object->m_hObject;
    return nullptr;
}

bool Script::NearPosition(HOBJECT object, DVector& position)
{
    auto* server = GetServerDE();
    DRotation rotation;
    DVector dims;
    float forward, up, right;
    if (!server->IsObjectAlive(object) || server->GetObjectPos(object, &position) != DE_OK ||
        server->GetObjectDims(object, &dims) != DE_OK || server->GetObjectRotation(object, &rotation) != DE_OK ||
        !server->RandomFloat(-dims.x, dims.x, forward) || !server->RandomFloat(-dims.y, dims.y / 2, up) ||
        !server->RandomFloat(-dims.z, dims.z, right))
        return false;
    position = vector_subtract(vector_add(position, vector_scale(rotate_vector({0, 0, 1}, rotation), forward)),
                               {right, up, forward});
    return true;
}

bool Script::Send(HOBJECT target, const std::string& text)
{
    auto* server = GetServerDE();
    if (!server->IsObjectAlive(target))
        return false;
    const auto message = server->CreateString(text.c_str());
    if (!message)
        return false;
    const auto write = server->StartMessageToObject(this, target, 1000 /* MID_TRIGGER */);
    const bool written = write && server->WriteToMessageHString(write, message) == DE_OK;
    server->FreeString(message);
    const bool delivered = write && server->EndMessage(write) == DE_OK;
    return written && delivered;
}

bool Script::PlaySound(const Command& command)
{
    auto* server = GetServerDE();
    const bool attached = command.op == Op::SoundOn;
    const auto asset = attached ? 2 : 4;
    SoundRequest request;
    request.resource = command.text[asset];
    request.outer_radius = command.number[asset + 1];
    request.inner_radius = .1f * request.outer_radius;
    request.loop = command.number[asset + 2] == 1;
    request.priority = 2; // SOUNDPRIORITY_MISC_HIGH
    request.speaker = attached ? objects_[command.Slot(1)] : nullptr;
    if (request.resource.empty() || (attached && !server->IsObjectAlive(request.speaker)))
        return false;
    auto& prior = sounds_[command.Slot()];
    if (prior)
        server->KillSound(prior);
    prior = 0;
    std::string sound_error;
    if (request.loop)
    {
        prior = attached ? server->PlayObjectLoopSound(request.speaker, request, sound_error)
                         : server->PlaySoundAt(m_hObject, request, command.Vector(1), sound_error);
        return prior != 0;
    }
    return (attached ? server->PlayCharacterSound(request.speaker, request, false, sound_error)
                     : server->PlayDetachedSound(m_hObject, request, command.Vector(1), sound_error)) != 0;
}

bool Script::StartMotion(HOBJECT object, const Command& command)
{
    auto* server = GetServerDE();
    const auto target = objects_[command.Slot(2)];
    Motion motion;
    motion.object = object;
    motion.slot = command.Slot();
    motion.line = command.line;
    motion.start = server->GetTime();
    motion.duration = command.number[1];
    if (motion.duration < 0 || !server->IsObjectAlive(target) ||
        server->GetObjectPos(object, &motion.from) != DE_OK || server->GetObjectPos(target, &motion.to) != DE_OK ||
        server->GetObjectRotation(object, &motion.from_rotation) != DE_OK ||
        server->GetObjectRotation(target, &motion.to_rotation) != DE_OK)
        return false;
    auto entry = std::find_if(motions_.begin(), motions_.end(),
                              [&](const Motion& other) { return other.object == object; });
    if (motion.duration == 0)
    {
        if (entry != motions_.end())
            entry->object = nullptr;
        return MoveToward(motion, 1);
    }
    if (entry == motions_.end())
        entry = std::find_if(motions_.begin(), motions_.end(), [](const Motion& other) { return !other.object; });
    if (entry == motions_.end())
        return false;
    *entry = motion;
    return true;
}

bool Script::MoveToward(const Motion& motion, float time)
{
    auto* server = GetServerDE();
    auto position = vector_add(motion.from, vector_scale(vector_subtract(motion.to, motion.from), time));
    DRotation rotation;
    auto* camera = dynamic_cast<CameraObj*>(server->HandleToObject(motion.object));
    const bool rotate = !camera || !camera->GetLinkObject();
    if (rotate && server->InterpolateRotation(rotation, motion.from_rotation, motion.to_rotation, time) != DE_OK)
        return false;
    return server->MoveObject(motion.object, &position) == DE_OK &&
           (!rotate || server->SetObjectRotation(motion.object, &rotation) == DE_OK);
}

void Script::RecordFailure(unsigned line)
{
    ++failures;
    error = filename_ + " line " + std::to_string(line) + ": command failed";
}

bool Script::Execute(const Command& command)
{
    auto* server = GetServerDE();
    const auto& args = command.text;
    const auto& number = command.number;
    switch (command.op)
    {
        case Op::Start:
            return true;
        case Op::Assign:
        {
            HOBJECT target = nullptr;
            if (auto* matches = server->FindNamedObjects(args[1].c_str()))
            {
                for (auto* link = matches->m_pFirstLink; link; link = link->m_pNext)
                    target = link->m_hObject; // The last named match wins in the original loop.
                server->RelinquishList(matches);
            }
            return SetSlot(command.Slot(), target);
        }
        case Op::Create:
        case Op::CreateNear:
        case Op::CreateModel:
        case Op::CreateModelNear:
        case Op::CreateFire:
        case Op::ExplosionAt:
        case Op::ExplosionNear:
        {
            ObjectCreateStruct state;
            state.m_NextUpdate = .1f;
            const bool explosion = command.op == Op::ExplosionAt || command.op == Op::ExplosionNear;
            const bool model = command.op == Op::CreateModel || command.op == Op::CreateModelNear;
            const auto slot = explosion ? 0 : command.Slot();
            const auto name = explosion ? "ScriptObjExp" : "ScriptObj" + std::to_string(slot);
            legacy_copy(state.m_Name, name.c_str());
            std::string_view type;
            switch (command.op)
            {
                case Op::Create:
                    type = args[1];
                    state.m_Pos = command.Vector(2);
                    break;
                case Op::CreateNear:
                case Op::CreateFire:
                {
                    const bool fire = command.op == Op::CreateFire;
                    const auto target = objects_[command.Slot(fire ? 1 : 2)];
                    const auto origin = !fire && server->IsObjectAlive(target) ? target : m_hObject;
                    if (server->GetObjectPos(origin, &state.m_Pos) != DE_OK || (fire && !server->IsObjectAlive(target)))
                        return false;
                    type = fire ? std::string_view("FireFX") : args[1];
                    break;
                }
                case Op::CreateModel:
                case Op::CreateModelNear:
                {
                    const bool near = command.op == Op::CreateModelNear;
                    type = "BaseClass";
                    state.m_ObjectType = OT_MODEL;
                    if (near)
                    {
                        if (!NearPosition(objects_[command.Slot(1)], state.m_Pos))
                            return false;
                    }
                    else
                        state.m_Pos = command.Vector(3);
                    legacy_copy(state.m_Filename, args[near ? 2 : 1].c_str());
                    legacy_copy(state.m_SkinName, args[near ? 3 : 2].c_str());
                    break;
                }
                case Op::ExplosionAt:
                case Op::ExplosionNear:
                    type = "Explosion";
                    if (command.op == Op::ExplosionNear)
                    {
                        if (!NearPosition(objects_[command.Slot()], state.m_Pos))
                            return false;
                    }
                    else
                        state.m_Pos = command.Vector(0);
                    state.m_NextUpdate = .01f;
                    break;
                default:
                    return false;
            }
            const auto object = Create(type, state);
            if (!server->IsObjectAlive(object))
                return false;
            bool ready = true;
            if (explosion)
            {
                auto* effect = dynamic_cast<Explosion*>(server->HandleToObject(object));
                const auto* settings = number.data() + (command.op == Op::ExplosionNear ? 1 : 3);
                ready = effect && effect->Setup("Sounds\\exp_tnt.wav", settings[1], settings[0], nullptr, settings[2], 50,
                                                settings[3], settings[4], true) &&
                        effect->SetupLight(true, {1, 1, 1}, settings[3] * 10, settings[4] * 10) && effect->Explode();
            }
            else if (command.op == Op::CreateFire)
            {
                auto* effect = dynamic_cast<FireFX*>(server->HandleToObject(object));
                ready = effect && effect->Setup(objects_[command.Slot(1)], number[2]);
            }
            else if (model)
            {
                DVector dims;
                ready = server->GetModelAnimUserDims(object, &dims, server->GetModelAnimation(object)) == DE_OK &&
                        server->SetObjectDims(object, &dims) == DE_OK;
            }
            if (!ready || (!explosion && !SetSlot(slot, object)))
            {
                server->RemoveObject(object);
                return false;
            }
            return true;
        }
        case Op::SoundAt:
        case Op::SoundOn:
            return PlaySound(command);
        case Op::KillSound:
        {
            auto& voice = sounds_[command.Slot()];
            const bool killed = voice && server->KillSound(voice);
            voice = 0;
            return killed;
        }
        case Op::TriggerNamed:
        {
            const auto target = server->CreateString(args[0].c_str());
            const auto message = server->CreateString(args[1].c_str());
            if (target && message)
                SendTriggerMsgToObjects(this, target, message);
            if (target)
                server->FreeString(target);
            if (message)
                server->FreeString(message);
            return target && message;
        }
        case Op::End:
        case Op::Wait:
            return false; // Handled by the scheduler.
        default:
            break;
    }
    const auto object = objects_[command.Slot()];
    if (!server->IsObjectAlive(object))
        return false;
    switch (command.op)
    {
        case Op::MoveTo:
            return StartMotion(object, command);
        case Op::Move:
        case Op::Teleport:
        {
            DVector position;
            if (server->GetObjectPos(object, &position) != DE_OK)
                return false;
            position = vector_add(position, command.Vector(1)); // Both original commands use a relative offset.
            return (command.op == Op::Move ? server->MoveObject(object, &position)
                                          : server->TeleportObject(object, &position)) == DE_OK;
        }
        case Op::Flags:
            server->SetObjectFlags(object, FLAG_VISIBLE | FLAG_GOTHRUWORLD); // The original ignores its numeric argument.
            return true;
        case Op::Visible:
        case Op::Solid:
        case Op::Gravity:
        {
            const auto mask = command.op == Op::Visible ? FLAG_VISIBLE : command.op == Op::Solid ? FLAG_SOLID : FLAG_GRAVITY;
            const auto flags = server->GetObjectFlags(object);
            server->SetObjectFlags(object, number[1] != 0 ? flags | mask : flags & ~mask);
            return true;
        }
        case Op::Scale:
            return server->ScaleObject(object, command.Vector(1));
        case Op::Dims:
        {
            auto dims = command.Vector(1);
            return server->SetObjectDims(object, &dims) == DE_OK;
        }
        case Op::Destroy:
            server->RemoveObject(object);
            return true;
        case Op::Gib:
        {
            DVector position, dims;
            float speed = 0;
            int count = 0;
            std::string gib_error;
            const bool emitted = server->GetObjectPos(object, &position) == DE_OK &&
                                 server->GetObjectDims(object, &dims) == DE_OK &&
                                 server->RandomInt(3, 8, count) && server->RandomFloat(200, 500, speed) &&
                                 server->PublishFleshGibs(object, position, {0, speed, 0}, dims, 1, count, gib_error);
            server->RemoveObject(object); // The original removes the target even if debris creation fails.
            return emitted;
        }
        case Op::Animation:
        {
            std::string clip;
            const auto index = static_cast<DDWORD>(number[1]);
            if (!server->GetModelAnimationName(object, index, clip))
                return false;
            server->SetModelAnimation(object, index);
            return true;
        }
        case Op::Trigger:
            return Send(object, args[1]);
        default:
            return false;
    }
}

void Script::Update()
{
    auto* server = GetServerDE();
    const auto now = server->GetTime();
    for (auto& motion : motions_)
    {
        if (!server->IsObjectAlive(motion.object))
        {
            motion.object = nullptr;
            continue;
        }
        const auto elapsed = std::clamp((now - motion.start) / motion.duration, 0.f, 1.f);
        const bool moved = MoveToward(motion, elapsed);
        if (!moved)
            RecordFailure(motion.line);
        if (!moved || elapsed == 1)
            motion.object = nullptr;
    }
    while (next_ < commands_.size() && now >= next_time_ && server->IsObjectAlive(m_hObject))
    {
        const auto& command = commands_[next_++];
        ++executed;
        if (command.op == Op::End)
        {
            next_ = commands_.size();
            next_time_ = 0;
            break;
        }
        if (command.op == Op::Wait)
        {
            if (command.number[0] > 0)
            {
                next_time_ = now + command.number[0];
                break;
            }
            continue;
        }
        if (!Execute(command))
            RecordFailure(command.line);
    }
    if (!server->IsObjectAlive(m_hObject))
        return;
    auto delay = std::max(next_time_ - now, 0.f);
    if (next_ < commands_.size() || delay > 0)
        if (std::any_of(motions_.begin(), motions_.end(), [](const Motion& motion) { return motion.object; }))
            delay = std::min(delay, .01f);
    server->SetNextUpdate(m_hObject, delay);
}

DDWORD Script::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    const auto accepted = B2BaseClass::EngineMessageFn(message, data, reason);
    auto* server = GetServerDE();
    if (message == MID_PRECREATE && accepted)
    {
        GenericProp property;
        if (reason == PRECREATE_WORLDFILE && server->GetPropGeneric("ScriptName", &property) == DE_OK)
            filename_ = property.m_String;
        server->GetPropReal("StartTime", &delay_);
        if (filename_.empty() || !std::isfinite(delay_) || delay_ > 1e6f || !Prepare())
        {
            server->RejectPrecreate(error.empty() ? "Script needs an authored ScriptName and valid StartTime" : error);
            return 0;
        }
        delay_ = delay_ <= 0 ? .001f : delay_;
    }
    else if (message == MID_INITIALUPDATE)
        server->SetNextUpdate(m_hObject, delay_);
    else if (message == MID_UPDATE)
        Update();
    else if (message == MID_LINKBROKEN)
        std::replace(objects_.begin(), objects_.end(), static_cast<HOBJECT>(data), HOBJECT{});
    return accepted;
}
