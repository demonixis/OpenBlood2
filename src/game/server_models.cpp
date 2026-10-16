#include "engine/model_commands.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        DRESULT node_index(const ObjectRecord* r, const char* name, std::size_t& index)
        {
            if (!r || r->life != Life::Alive || r->state.m_ObjectType != OT_MODEL || !r->model || !name || !*name ||
                r->model->nodes.size() > 256 ||
                (!r->hidden_nodes.empty() && r->hidden_nodes.size() != r->model->nodes.size()))
                return DE_INVALIDPARAMS;
            index = r->model->nodes.size();
            for (std::size_t i = 0; i < r->model->nodes.size(); ++i)
                if (!CaseLess{}(name, r->model->nodes[i].name) && !CaseLess{}(r->model->nodes[i].name, name))
                {
                    if (index != r->model->nodes.size())
                        return DE_ERROR;
                    index = i;
                }
            return index == r->model->nodes.size() ? DE_NOTFOUND : DE_OK;
        }
    } // namespace
    DRESULT ServerAdapter::GetModelNodeHideStatus(HOBJECT h, const char* name, DBOOL* hidden)
    {
        trace_service("GetModelNodeHideStatus");
        if (!hidden)
            return DE_INVALIDPARAMS;
        const auto* r = objects_.get(h);
        std::size_t index;
        auto result = node_index(r, name, index);
        if (result == DE_OK)
            *hidden = !r->hidden_nodes.empty() && r->hidden_nodes[index] != 0;
        return result;
    }
    DRESULT ServerAdapter::SetModelNodeHideStatus(HOBJECT h, const char* name, DBOOL hidden)
    {
        trace_service("SetModelNodeHideStatus");
        auto* r = objects_.get(h);
        std::size_t index;
        auto result = node_index(r, name, index);
        if (result != DE_OK)
            return result;
        if (r->hidden_nodes.empty() && hidden)
            r->hidden_nodes.resize(r->model->nodes.size());
        if (!r->hidden_nodes.empty())
            r->hidden_nodes[index] = hidden != 0;
        return DE_OK; // Idempotent assignments still allow original _extra traversal.
    }
    void ServerAdapter::initialize_model_services()
    {
        GetAnimIndex = [](HOBJECT h, const char* name) -> HMODELANIM
        {
            auto& a = active();
            a.trace_service("GetAnimIndex");
            const auto* r = a.objects_.get(h);
            return r && r->model && name ? animation_index(*r->model, name) : no_animation;
        };
        SetModelAnimation = [](HOBJECT h, HMODELANIM animation)
        {
            auto& a = active();
            a.trace_service("SetModelAnimation");
            auto* r = a.objects_.get(h);
            if (!r || !r->model || animation >= r->model->animations.size())
            {
                a.objects_.fail(ObjectError::InvalidProperty, "SetModelAnimation: invalid model/animation");
                return;
            }
            if (r->animation.animation != animation)
            {
                r->animation.animation = animation;
                r->animation.reset();
            }
        };
        GetModelAnimation = [](HOBJECT h) -> HMODELANIM
        {
            auto& a = active();
            a.trace_service("GetModelAnimation");
            const auto* r = a.objects_.get(h);
            return r && r->model ? r->animation.animation : no_animation;
        };
        ResetModelAnimation = [](HOBJECT h) -> DRESULT
        {
            auto& a = active();
            a.trace_service("ResetModelAnimation");
            auto* r = a.objects_.get(h);
            if (!r || !r->model)
                return DE_INVALIDPARAMS;
            r->animation.reset();
            return DE_OK;
        };
        GetModelPlaybackState = [](HOBJECT h) -> DDWORD
        {
            auto& a = active();
            a.trace_service("GetModelPlaybackState");
            const auto* r = a.objects_.get(h);
            return r && r->model && r->animation.done ? MS_PLAYDONE : 0;
        };
        SetModelLooping = [](HOBJECT h, DBOOL looping)
        {
            auto& a = active();
            a.trace_service("SetModelLooping");
            auto* r = a.objects_.get(h);
            if (!r || !r->model)
            {
                a.objects_.fail(ObjectError::InvalidHandle, "SetModelLooping: invalid model");
                return;
            }
            r->animation.looping = looping != 0;
        };
        GetModelLooping = [](HOBJECT h) -> DBOOL
        {
            auto& a = active();
            a.trace_service("GetModelLooping");
            const auto* r = a.objects_.get(h);
            return r && r->model && r->animation.looping;
        };
    }
    DRESULT ServerAdapter::GetModelAnimUserDims(HOBJECT h, DVector* out, HMODELANIM animation)
    {
        trace_service("GetModelAnimUserDims");
        if (out)
            *out = {};
        auto* r = objects_.get(h);
        if (!out || !r || !r->model || animation >= r->model->animations.size())
            return DE_INVALIDPARAMS;
        *out = r->model->animations[animation].dimensions;
        return DE_OK;
    }
    DBOOL ServerAdapter::GetModelNodeTransform(HOBJECT object, const char* name, DVector* position, DRotation* rotation)
    {
        trace_service("GetModelNodeTransform");
        const auto* record = objects_.get(object);
        return record && objects_.refresh_attachment(object) &&
               object_node_transform(*record, name, position, rotation);
    }
    ServerDE::NodeQuery ServerAdapter::QueryModelNodeTransform(HOBJECT object, const char* name, DVector* position,
                                                               DRotation* rotation)
    {
        trace_service("QueryModelNodeTransform");
        const auto* r = objects_.get(object);
        if (!r || r->life != Life::Alive || !r->model || r->state.m_ObjectType != OT_MODEL || !name || !*name ||
            !position || !rotation)
            return NodeQuery::Unavailable;
        unsigned matches = 0;
        for (const auto& node : r->model->nodes)
            matches += !CaseLess{}(node.name, name) && !CaseLess{}(name, node.name);
        if (!matches)
            return NodeQuery::Missing;
        if (matches != 1 || !objects_.refresh_attachment(object) ||
            !object_node_transform(*r, name, position, rotation))
            return NodeQuery::Unavailable;
        return NodeQuery::Found;
    }
    bool ServerAdapter::GetModelAnimationName(HOBJECT object, HMODELANIM animation, std::string& name)
    {
        trace_service("GetModelAnimationName");
        const auto* record = objects_.get(object);
        if (!record || record->life != Life::Alive || !record->model || animation >= record->model->animations.size())
            return false;
        name = record->model->animations[animation].name;
        return true;
    }
    bool ServerAdapter::advance_models(double seconds)
    {
        if (models_advancing_ || objects_.busy())
            return objects_.fail(ObjectError::Busy, "Reentrant model step rejected");
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60)
            return objects_.fail(ObjectError::InvalidTime, "Invalid model delta");
        models_advancing_ = true;
        bool ok = true;
        for (auto id : objects_.snapshot())
        {
            auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || r->activity != OBJSTATE_ACTIVE)
                continue;
            if (r->sprite && !advance_sprite(*r->sprite, r->sprite_track, seconds))
                ok = objects_.fail(ObjectError::InvalidTime, "Invalid sprite clock");
            if (!r->model)
                continue;
            std::vector<std::uint32_t> events;
            std::string error;
            if (!advance_animation(*r->model, r->animation, seconds, events, error))
            {
                objects_.fail(ObjectError::InvalidTime, error);
                ok = false;
                continue;
            }
            const auto animation = r->animation.animation;
            const auto revision = r->animation.revision;
            for (auto key : events)
            {
                if (r->life != Life::Alive || r->animation.revision != revision)
                    break;
                if (!(r->state.m_Flags & FLAG_MODELKEYS))
                    continue;
                std::vector<std::vector<std::string>> parsed;
                if (!model_commands(r->model->animations[animation].keys[key].text, parsed))
                {
                    objects_.fail(ObjectError::InvalidProperty, "Malformed model key");
                    ok = false;
                    break;
                }
                for (auto& command : parsed)
                {
                    if (r->life != Life::Alive || r->animation.revision != revision)
                        break;
                    std::vector<char*> args;
                    for (auto& word : command)
                        args.push_back(word.data());
                    ArgList list{args.data(), static_cast<int>(args.size())};
                    trace_service("MID_MODELSTRINGKEY");
                    objects_.engine_message(r->handle, MID_MODELSTRINGKEY, &list);
                }
            }
        }
        const bool finished = objects_.finish_frame();
        models_advancing_ = false;
        return finished && ok;
    }
} // namespace OpenBlood2
