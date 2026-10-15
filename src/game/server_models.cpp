#include "game/server_adapter.h"
#include "engine/model_commands.h"
#include <cmath>

namespace blood2 {
void ServerAdapter::initialize_model_services() {
    GetAnimIndex=[](HOBJECT h,const char* name)->HMODELANIM { auto& a=active(); a.trace_service("GetAnimIndex");
        const auto* r=a.objects_.get(h); return r && r->model && name ? animation_index(*r->model,name) : no_animation;
    };
    SetModelAnimation=[](HOBJECT h,HMODELANIM animation) { auto& a=active(); a.trace_service("SetModelAnimation");
        auto* r=a.objects_.get(h);
        if (!r || !r->model || animation>=r->model->animations.size()) { a.objects_.fail(ObjectError::InvalidProperty,"SetModelAnimation: invalid model/animation"); return; }
        if (r->animation.animation!=animation) { r->animation.animation=animation; r->animation.reset(); }
    };
    GetModelAnimation=[](HOBJECT h)->HMODELANIM { auto& a=active(); a.trace_service("GetModelAnimation");
        const auto* r=a.objects_.get(h); return r && r->model ? r->animation.animation : no_animation;
    };
    ResetModelAnimation=[](HOBJECT h)->DRESULT { auto& a=active(); a.trace_service("ResetModelAnimation");
        auto* r=a.objects_.get(h); if (!r || !r->model) return DE_INVALIDPARAMS; r->animation.reset(); return DE_OK;
    };
    GetModelPlaybackState=[](HOBJECT h)->DDWORD { auto& a=active(); a.trace_service("GetModelPlaybackState");
        const auto* r=a.objects_.get(h); return r && r->model && r->animation.done ? MS_PLAYDONE : 0;
    };
    SetModelLooping=[](HOBJECT h,DBOOL looping) { auto& a=active(); a.trace_service("SetModelLooping");
        auto* r=a.objects_.get(h); if (!r || !r->model) { a.objects_.fail(ObjectError::InvalidHandle,"SetModelLooping: invalid model"); return; }
        r->animation.looping=looping!=0;
    };
    GetModelLooping=[](HOBJECT h)->DBOOL { auto& a=active(); a.trace_service("GetModelLooping");
        const auto* r=a.objects_.get(h); return r && r->model && r->animation.looping;
    };
}
DRESULT ServerAdapter::GetModelAnimUserDims(HOBJECT h,DVector* out,HMODELANIM animation) {
    trace_service("GetModelAnimUserDims"); if (out) *out={};
    auto* r=objects_.get(h); if (!out || !r || !r->model || animation>=r->model->animations.size()) return DE_INVALIDPARAMS;
    *out=r->model->animations[animation].dimensions; return DE_OK;
}
bool ServerAdapter::advance_models(double seconds) {
    if (models_advancing_ || objects_.busy()) return objects_.fail(ObjectError::Busy,"Reentrant model step rejected");
    if (!std::isfinite(seconds) || seconds<0 || seconds>60) return objects_.fail(ObjectError::InvalidTime,"Invalid model delta");
    models_advancing_ = true;
    bool ok=true;
    for (auto id:objects_.snapshot()) {
        auto* r=objects_.get(id);
        if (!r || r->life!=Life::Alive || r->activity!=OBJSTATE_ACTIVE || !r->model) continue;
        std::vector<std::uint32_t> events; std::string error;
        if (!advance_animation(*r->model,r->animation,seconds,events,error)) { objects_.fail(ObjectError::InvalidTime,error); ok=false; continue; }
        const auto animation=r->animation.animation; const auto revision=r->animation.revision;
        for (auto key:events) {
            if (r->life!=Life::Alive || r->animation.revision!=revision) break;
            if (!(r->state.m_Flags & FLAG_MODELKEYS)) continue;
            std::vector<std::vector<std::string>> parsed;
            if (!model_commands(r->model->animations[animation].keys[key].text,parsed)) { objects_.fail(ObjectError::InvalidProperty,"Malformed model key"); ok=false; break; }
            for (auto& command:parsed) {
                if (r->life!=Life::Alive || r->animation.revision!=revision) break;
                std::vector<char*> args; for (auto& word:command) args.push_back(word.data());
                ArgList list{args.data(),static_cast<int>(args.size())};
                trace_service("MID_MODELSTRINGKEY"); objects_.engine_message(r->handle,MID_MODELSTRINGKEY,&list);
            }
        }
    }
    const bool finished = objects_.finish_frame(); models_advancing_ = false;
    return finished && ok;
}
}
