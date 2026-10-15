#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace blood2 {
bool ServerAdapter::GetMovementState(HOBJECT h,MovementState& state) {
    trace_service("GetMovementState"); state={};
    const auto* r=objects_.get(h);
    if (!r || r->life!=Life::Alive || !collision || !collision_ready) return false;
    state.position=r->state.m_Pos; state.velocity=r->velocity;
    state.acceleration=r->acceleration; state.dimensions=r->dimensions;
    if (!physics_vector(state.position) || !physics_vector(state.velocity) || !physics_vector(state.acceleration) ||
        !physics_vector(state.dimensions) || state.dimensions.x<0 || state.dimensions.y<0 || state.dimensions.z<0) return false;
    // A fresh short support sweep avoids a stale last-contact normal after
    // sliding along a wall or walking off a ledge. World support has a polygon
    // identity, not a fabricated HOBJECT. Motion away from the support plane
    // is airborne; positive Y alone can also mean walking uphill.
    auto to=state.position; to.y-=0.02f;
    CollisionHit nearest;
    const auto query=collision->sweep(state.position,to,state.dimensions,nearest);
    if (query==CollisionQuery::Invalid) return false;
    bool found=query==CollisionQuery::Hit;
    for (auto id:objects_.snapshot()) {
        const auto* other=objects_.get(id);
        if (!other || other->life!=Life::Alive || other->handle==h || !(other->state.m_Flags&FLAG_SOLID)) continue;
        CollisionHit hit;
        const auto result=sweep_box(state.position,to,state.dimensions,other->state.m_Pos,other->dimensions,hit);
        if (result==CollisionQuery::Invalid) return false;
        if (result==CollisionQuery::Hit && (!found || hit.fraction<nearest.fraction)) {
            nearest=hit; nearest.object=other->handle; found=true;
        }
    }
    if (found && !nearest.start_solid && nearest.normal.y>0 && vector_dot(state.velocity,nearest.normal)<=0.01f) {
        state.standing=true; state.ground_normal=nearest.normal;
        state.ground_object=nearest.object; state.ground_polygon=nearest.polygon;
    }
    return true;
}
void ServerAdapter::initialize_physics_services() {
    TeleportObject=[](HOBJECT h,DVector* position)->DRESULT { auto& a=active(); a.trace_service("TeleportObject");
        auto* r=a.objects_.get(h);
        if (!r || r->life!=Life::Alive || !position || !physics_vector(*position)) return DE_INVALIDPARAMS;
        r->state.m_Pos=*position; r->grounded=false; r->contact_normal={}; r->contact_object=nullptr; r->contact_polygon=UINT32_MAX; return DE_OK;
    };
}
DRESULT ServerAdapter::GetVelocity(HOBJECT h,DVector* v) {
    trace_service("GetVelocity"); if (v) *v={}; auto* r=objects_.get(h); if (!r || !v) return DE_INVALIDPARAMS; *v=r->velocity; return DE_OK;
}
DRESULT ServerAdapter::SetVelocity(HOBJECT h,DVector* v) {
    trace_service("SetVelocity"); auto* r=objects_.get(h); if (!r || !v || !physics_vector(*v)) return DE_INVALIDPARAMS; r->velocity=*v; return DE_OK;
}
DRESULT ServerAdapter::GetAcceleration(HOBJECT h,DVector* v) {
    trace_service("GetAcceleration"); if (v) *v={}; auto* r=objects_.get(h); if (!r || !v) return DE_INVALIDPARAMS; *v=r->acceleration; return DE_OK;
}
DRESULT ServerAdapter::SetAcceleration(HOBJECT h,DVector* v) {
    trace_service("SetAcceleration"); auto* r=objects_.get(h); if (!r || !v || !physics_vector(*v)) return DE_INVALIDPARAMS; r->acceleration=*v; return DE_OK;
}
DRESULT ServerAdapter::MoveObject(HOBJECT h,DVector* destination) {
    trace_service("MoveObject"); auto* r=objects_.get(h);
    if (!r || r->life!=Life::Alive || !destination || !physics_vector(*destination) || !physics_vector(r->state.m_Pos) ||
        !physics_vector(r->dimensions) || r->dimensions.x<0 || r->dimensions.y<0 || r->dimensions.z<0) return DE_INVALIDPARAMS;
    if (!collision || !collision_ready) return unsupported("MoveObject without loaded collision world");
    if (moving_) { objects_.fail(ObjectError::Busy,"Recursive MoveObject during contact dispatch"); return DE_ERROR; }
    if (r->state.m_ObjectType!=OT_NORMAL && r->state.m_ObjectType!=OT_MODEL) return unsupported("Moving world-model collision");
    moving_=true;
    const auto ids=objects_.snapshot();
    std::map<std::uint32_t,HOBJECT> touched;
    auto touch=[&](HOBJECT target) { if (const auto* o=objects_.get(target)) touched.emplace(o->id.slot,target); };
    auto position=r->state.m_Pos,remaining=vector_subtract(*destination,position);
    r->grounded=false; r->contact_normal={}; r->contact_object=nullptr; r->contact_polygon=UINT32_MAX;
    bool blocked_start=false;
    for (unsigned iteration=0;iteration<4;++iteration) {
        auto end=vector_add(position,remaining); CollisionHit nearest; bool found=false;
        if (r->state.m_Flags & FLAG_SOLID) found=collision->sweep(position,end,r->dimensions,nearest)==CollisionQuery::Hit;
        for (auto id:ids) {
            const auto* other=objects_.get(id);
            if (!other || other->handle==h || other->life!=Life::Alive || !(other->state.m_Flags & FLAG_SOLID) || !(r->state.m_Flags & FLAG_SOLID)) continue;
            CollisionHit hit;
            const auto query=sweep_box(position,end,r->dimensions,other->state.m_Pos,other->dimensions,hit);
            if (query==CollisionQuery::Invalid) { moving_=false; objects_.fail(ObjectError::InvalidProperty,"Invalid blocking object collision bounds"); return DE_ERROR; }
            if (query==CollisionQuery::Hit && (!found || hit.fraction<nearest.fraction)) {
                nearest=hit; nearest.object=other->handle; found=true;
            }
        }
        auto reached=found ? nearest.point : end;
        for (auto id:ids) {
            const auto* other=objects_.get(id);
            if (!other || other->handle==h || other->life!=Life::Alive || !((other->state.m_Flags | r->state.m_Flags) & FLAG_TOUCH_NOTIFY)) continue;
            CollisionHit hit;
            if (sweep_box(position,reached,r->dimensions,other->state.m_Pos,other->dimensions,hit)==CollisionQuery::Hit) touch(other->handle);
        }
        if (!found) { position=end; break; }
        if (nearest.object) touch(nearest.object);
        r->contact_normal=nearest.normal; r->contact_polygon=nearest.polygon; r->contact_object=nearest.object;
        r->grounded |= nearest.normal.y>0.7f;
        if (nearest.start_solid) { blocked_start=true; break; }
        // A small separation avoids repeatedly reporting the same touching face.
        position=vector_add(reached,vector_scale(nearest.normal,0.001f));
        remaining=vector_scale(remaining,static_cast<float>(1-nearest.fraction));
        auto into=vector_dot(remaining,nearest.normal);
        if (into<0) remaining=vector_subtract(remaining,vector_scale(nearest.normal,into));
        auto velocity_into=vector_dot(r->velocity,nearest.normal);
        if (velocity_into<0) r->velocity=vector_subtract(r->velocity,vector_scale(nearest.normal,velocity_into));
        if (vector_dot(remaining,remaining)<1e-10f) break;
    }
    r->state.m_Pos=position;
    for (const auto& contact:touched) {
        auto other=contact.second;
        auto* target=objects_.get(other);
        if (r->life!=Life::Alive) break;
        if (!target || target->life!=Life::Alive) continue;
        // Force is not invented: a mass/impulse solver has not been connected.
        if (r->state.m_Flags & FLAG_TOUCH_NOTIFY) objects_.engine_message(h,MID_TOUCHNOTIFY,other);
        if (r->life==Life::Alive && target->life==Life::Alive && (target->state.m_Flags & FLAG_TOUCH_NOTIFY)) objects_.engine_message(other,MID_TOUCHNOTIFY,h);
    }
    moving_=false;
    if (blocked_start) { objects_.fail(ObjectError::Unsupported,"MoveObject starts penetrating geometry; depenetration is not implemented"); return DE_ERROR; }
    return DE_OK;
}
bool ServerAdapter::advance_physics(double seconds) {
    if (physics_advancing_ || objects_.busy()) return objects_.fail(ObjectError::Busy,"Reentrant physics step rejected");
    if (!std::isfinite(seconds) || seconds<0 || seconds>60) return objects_.fail(ObjectError::InvalidTime,"Invalid physics delta");
    if (seconds==0) return true;
    physics_advancing_ = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{physics_advancing_};
    const auto ids=objects_.snapshot();
    for (auto id:ids) {
        auto* r=objects_.get(id); if (!r || r->life!=Life::Alive || r->activity!=OBJSTATE_ACTIVE || r->client_movement) continue;
        if (!(r->state.m_Flags & FLAG_GRAVITY) && vector_dot(r->velocity,r->velocity)==0 && vector_dot(r->acceleration,r->acceleration)==0) continue;
        if (seconds>0.5) return objects_.fail(ObjectError::InvalidTime,"Physics catch-up exceeds 0.5 seconds");
        const auto steps=static_cast<unsigned>(std::max(1.0,std::ceil(seconds*120))); const auto dt=static_cast<float>(seconds/steps);
        for (unsigned i=0;i<steps && r->life==Life::Alive;++i) {
            auto acceleration=r->acceleration;
            if (r->state.m_Flags & FLAG_GRAVITY) acceleration=vector_add(acceleration,gravity);
            r->velocity=vector_add(r->velocity,vector_scale(acceleration,dt));
            auto destination=vector_add(r->state.m_Pos,vector_scale(r->velocity,dt));
            if (MoveObject(r->handle,&destination)!=DE_OK) return false;
        }
    }
    return objects_.finish_frame();
}
}
