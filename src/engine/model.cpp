#include "engine/model.h"
#include "engine/formats/reader.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace blood2 {
namespace {
DVector add(DVector a, DVector b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
DVector mul(DVector a, float b) { return {a.x*b,a.y*b,a.z*b}; }
DVector cross(DVector a, DVector b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float dot(DVector a, DVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
DRotation normalize(DRotation q) {
    float length = std::sqrt(dot(q.m_Vec,q.m_Vec)+q.m_Spin*q.m_Spin);
    return {mul(q.m_Vec,1/length),q.m_Spin/length};
}
DRotation compose(DRotation a, DRotation b) {
    return normalize({add(add(mul(b.m_Vec,a.m_Spin),mul(a.m_Vec,b.m_Spin)),cross(a.m_Vec,b.m_Vec)),a.m_Spin*b.m_Spin-dot(a.m_Vec,b.m_Vec)});
}
bool coordinate(DVector v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x)<=1e7f && std::abs(v.y)<=1e7f && std::abs(v.z)<=1e7f; }
bool bounds(DVector a,DVector b) { return coordinate(a) && coordinate(b) && a.x<=b.x && a.y<=b.y && a.z<=b.z; }
DVector normal(Reader& r) {
    auto component = [&] { auto b=r.u8(); return static_cast<float>(b<128 ? b : int(b)-256)/127; };
    const float x=component(), y=component(), z=component(); return {x,y,z};
}
bool end(Reader& r) { return r.good() && r.remaining()==0; }
}
DVector rotate_vector(DRotation q,DVector v) {
    auto t=mul(cross(q.m_Vec,v),2); return add(v,add(mul(t,q.m_Spin),cross(q.m_Vec,t)));
}
bool read_model(std::span<const std::uint8_t> bytes,Model& out,std::string& error) {
    out={}; error.clear(); Model model;
    auto fail=[&](const char* detail) { error=std::string("ABC v6: ")+detail; return false; };
    if (bytes.empty() || bytes.size()>128*1024*1024) return fail("invalid size");
    std::map<std::string,std::span<const std::uint8_t>> sections;
    std::size_t offset=0;
    while (offset<bytes.size()) {
        Reader r(bytes.subspan(offset)); auto name=r.string(32); auto next=r.u32();
        auto start=offset+r.position(); std::size_t finish=next==UINT32_MAX ? bytes.size() : next;
        if (!r.good() || start>finish || finish>bytes.size() || sections.size()>=6 ||
            !sections.emplace(name,bytes.subspan(start,finish-start)).second) return fail("invalid section chain");
        if (next==UINT32_MAX) break;
        offset=finish;
        if (offset==bytes.size()) return fail("missing final section sentinel");
    }
    for (auto name : {"Header","Geometry","Nodes","Animation","AnimDims"}) if (!sections.contains(name)) return fail("missing section");
    for (const auto& [name,body] : sections) {
        (void)body;
        if (name!="Header" && name!="Geometry" && name!="Nodes" && name!="Animation" && name!="AnimDims" && name!="TransformInfo") return fail("unknown section");
    }
    Reader header(sections.at("Header"));
    if (header.string(64)!="MonolithExport Model File v6") return fail("unsupported version");
    model.command=header.string(); if (!end(header)) return fail("invalid header");
    if (sections.contains("TransformInfo")) {
        Reader r(sections.at("TransformInfo")); const auto geometry=r.u32(), animation=r.u32();
        if (!end(r) || geometry || animation) return fail("unsupported nonzero transform flags");
    }
    Reader g(sections.at("Geometry")); model.min=g.vec(); model.max=g.vec(); auto lods=g.u32();
    if (!g.good() || !bounds(model.min,model.max) || lods>65535 || lods+1>g.remaining()/2) return fail("invalid geometry bounds/LOD count");
    for (std::uint32_t i=0;i<=lods;++i) model.lod_starts.push_back(g.u16());
    auto faces=g.u32(); if (!g.good() || faces>200000 || faces>g.remaining()/33) return fail("invalid face count");
    for (std::uint32_t i=0;i<faces;++i) {
        ModelFace f;
        for (auto& uv:f.uv) { uv[0]=g.real(); uv[1]=g.real(); }
        for (auto& v:f.vertices) v=g.u16();
        f.normal=normal(g); model.faces.push_back(f);
    }
    auto vertices=g.u32(); model.main_vertices=g.u32();
    if (!g.good() || vertices>65536 || model.main_vertices>vertices || vertices>g.remaining()/20) return fail("invalid vertex count");
    for (std::uint32_t i=0;i<vertices;++i) {
        ModelVertex v; v.position=g.vec(); v.normal=normal(g); v.node=g.u8(); v.replacements={g.u16(),g.u16()};
        if (!coordinate(v.position)) return fail("invalid vertex position");
        model.vertices.push_back(v);
    }
    if (!end(g)) return fail("invalid geometry payload");
    for (const auto& f:model.faces) for (auto i:f.vertices) if (i>=vertices) return fail("invalid face vertex");
    for (auto i:model.lod_starts) if (i>vertices) return fail("invalid LOD start");
    Reader n(sections.at("Nodes"));
    struct Parent { int index; std::uint32_t remaining; };
    std::vector<Parent> parents{{-1,1}}; std::array<bool,256> indices{};
    while (!parents.empty()) {
        if (!parents.back().remaining) { parents.pop_back(); continue; }
        if (model.nodes.size()>=256 || parents.size()>64) return fail("node/depth limit");
        --parents.back().remaining;
        ModelNode node; node.parent=parents.back().index; node.min=n.vec(); node.max=n.vec(); node.name=n.string(255); node.index=n.u16(); node.flags=n.u8();
        auto deformed=n.u32();
        if (!n.good() || !bounds(node.min,node.max) || node.index>=256 || indices[node.index] || deformed>200000 || deformed>n.remaining()/2) return fail("invalid node");
        indices[node.index]=true;
        for (std::uint32_t i=0;i<deformed;++i) {
            auto v=n.u16();
            if (v>=vertices || model.vertices[v].node!=node.index) return fail("invalid deformation vertex");
            node.deformed_vertices.push_back(v);
        }
        auto children=n.u32(); if (!n.good() || children>255) return fail("invalid child count");
        model.nodes.push_back(std::move(node)); parents.push_back({static_cast<int>(model.nodes.size()-1),children});
    }
    if (!end(n)) return fail("invalid node tree");
    for (const auto& v:model.vertices) if (!indices[v.node]) return fail("missing vertex node");
    Reader a(sections.at("Animation")); auto count=a.u32();
    if (!a.good() || !count || count>4096) return fail("invalid animation count");
    std::size_t pose_count=0, deformation_bytes=0;
    for (std::uint32_t i=0;i<count;++i) {
        ModelAnimation anim; anim.name=a.string(255); anim.duration=a.u32(); anim.min=a.vec(); anim.max=a.vec(); auto frames=a.u32();
        pose_count+=static_cast<std::size_t>(frames)*model.nodes.size();
        if (!a.good() || anim.name.empty() || !bounds(anim.min,anim.max) || !frames || frames>65536 || frames>a.remaining()/30 || pose_count>3000000) return fail("invalid animation/key count");
        for (std::uint32_t j=0;j<frames;++j) {
            ModelKey key; key.time=a.u32(); key.min=a.vec(); key.max=a.vec(); key.text=a.string();
            if (!a.good() || !bounds(key.min,key.max) || key.time>anim.duration || (j && key.time<anim.keys.back().time)) return fail("invalid animation key");
            anim.keys.push_back(std::move(key));
        }
        for (const auto& node:model.nodes) {
            NodeAnimation na;
            if (frames>a.remaining()/28) return fail("truncated node poses");
            for (std::uint32_t j=0;j<frames;++j) {
                NodePose p; p.position=a.vec(); p.rotation.m_Vec=a.vec(); p.rotation.m_Spin=a.real();
                float norm=dot(p.rotation.m_Vec,p.rotation.m_Vec)+p.rotation.m_Spin*p.rotation.m_Spin;
                if (!a.good() || !coordinate(p.position) || norm<0.0001f || norm>10000) return fail("invalid node pose");
                p.rotation=normalize(p.rotation); na.frames.push_back(p);
            }
            auto size=static_cast<std::size_t>(frames)*node.deformed_vertices.size()*3; deformation_bytes+=size;
            if (deformation_bytes>32*1024*1024) return fail("deformation limit");
            auto raw=a.take(size); na.deformation.assign(raw.begin(),raw.end()); na.scale=a.vec(); na.translation=a.vec();
            if (!a.good() || !coordinate(na.scale) || !coordinate(na.translation)) return fail("invalid node animation");
            anim.nodes.push_back(std::move(na));
        }
        model.animations.push_back(std::move(anim));
    }
    if (!end(a)) return fail("invalid animation payload");
    Reader d(sections.at("AnimDims"));
    for (auto& anim:model.animations) { anim.dimensions=d.vec(); if (!bounds({},anim.dimensions)) return fail("negative animation dimensions"); }
    if (!end(d)) return fail("invalid animation dimensions");
    out=std::move(model); return true;
}
std::uint32_t animation_index(const Model& model,std::string_view name) {
    for (std::uint32_t i=0;i<model.animations.size();++i) if (model.animations[i].name==name) return i;
    return no_animation;
}
bool sample_pose(const Model& model,std::uint32_t animation,double milliseconds,std::vector<NodePose>& out) {
    out.clear(); if (animation>=model.animations.size() || !std::isfinite(milliseconds) || milliseconds<0) return false;
    const auto& a=model.animations[animation]; if (a.keys.empty()) return false;
    auto upper=std::upper_bound(a.keys.begin(),a.keys.end(),milliseconds,[](double time,const ModelKey& k) { return time<k.time; });
    std::size_t hi=static_cast<std::size_t>(upper-a.keys.begin()); if (hi==a.keys.size()) --hi;
    std::size_t lo=hi ? hi-1 : 0;
    float t=a.keys[hi].time>a.keys[lo].time ? static_cast<float>(std::clamp((milliseconds-a.keys[lo].time)/(a.keys[hi].time-a.keys[lo].time),0.0,1.0)) : 0;
    for (std::size_t i=0;i<model.nodes.size();++i) {
        const auto& x=a.nodes[i].frames[lo]; auto y=a.nodes[i].frames[hi];
        if (dot(x.rotation.m_Vec,y.rotation.m_Vec)+x.rotation.m_Spin*y.rotation.m_Spin<0) { y.rotation.m_Vec=mul(y.rotation.m_Vec,-1); y.rotation.m_Spin=-y.rotation.m_Spin; }
        NodePose p{add(mul(x.position,1-t),mul(y.position,t)),normalize({add(mul(x.rotation.m_Vec,1-t),mul(y.rotation.m_Vec,t)),x.rotation.m_Spin*(1-t)+y.rotation.m_Spin*t})};
        auto parent=model.nodes[i].parent;
        if (parent>=0) { p.position=add(out[parent].position,rotate_vector(out[parent].rotation,p.position)); p.rotation=compose(out[parent].rotation,p.rotation); }
        out.push_back(p);
    }
    return true;
}
bool sample_vertices(const Model& model,std::uint32_t animation,double milliseconds,std::vector<DVector>& out) {
    out.clear(); std::vector<NodePose> pose;
    if (!sample_pose(model,animation,milliseconds,pose)) return false;
    const auto& a=model.animations[animation];
    auto upper=std::upper_bound(a.keys.begin(),a.keys.end(),milliseconds,[](double time,const ModelKey& k) { return time<k.time; });
    std::size_t hi=static_cast<std::size_t>(upper-a.keys.begin()); if (hi==a.keys.size()) --hi;
    std::size_t lo=hi ? hi-1 : 0;
    float t=a.keys[hi].time>a.keys[lo].time ? static_cast<float>(std::clamp((milliseconds-a.keys[lo].time)/(a.keys[hi].time-a.keys[lo].time),0.0,1.0)) : 0;
    std::array<std::size_t,256> nodes{};
    for (std::size_t i=0;i<model.nodes.size();++i) nodes[model.nodes[i].index]=i;
    out.reserve(model.vertices.size());
    for (const auto& vertex:model.vertices) { const auto& bone=pose[nodes[vertex.node]]; out.push_back(add(bone.position,rotate_vector(bone.rotation,vertex.position))); }
    for (std::size_t i=0;i<model.nodes.size();++i) {
        const auto& list=model.nodes[i].deformed_vertices; const auto& data=a.nodes[i];
        // Repeated indices are present in retail data. Preserve the file's
        // write order instead of merging/deduplicating its animation samples.
        for (std::size_t v=0;v<list.size();++v) {
            auto at=(lo*list.size()+v)*3, next=(hi*list.size()+v)*3;
            auto component=[&](unsigned axis) { return data.deformation[at+axis]*(1-t)+data.deformation[next+axis]*t; };
            DVector local{component(0)*data.scale.x+data.translation.x,component(1)*data.scale.y+data.translation.y,component(2)*data.scale.z+data.translation.z};
            out[list[v]]=add(pose[i].position,rotate_vector(pose[i].rotation,local));
        }
    }
    return true;
}
bool advance_animation(const Model& model,AnimationTrack& track,double seconds,std::vector<std::uint32_t>& events,std::string& error) {
    error.clear();
    if (track.animation>=model.animations.size() || !std::isfinite(seconds) || seconds<0 || seconds>60) { error="Invalid animation/delta (maximum 60 seconds)"; return false; }
    const auto& a=model.animations[track.animation]; auto next=track; std::vector<std::uint32_t> crossed;
    if (!std::isfinite(track.time) || track.time<0 || track.time>a.duration) { error="Invalid animation time"; return false; }
    auto collect=[&](double from,double to,bool initial) {
        for (std::uint32_t i=0;i<a.keys.size();++i) if (!a.keys[i].text.empty() && (a.keys[i].time>from || (initial && a.keys[i].time==from)) && a.keys[i].time<=to) {
            if (crossed.size()==4096) return false;
            crossed.push_back(i);
        }
        return true;
    };
    double remaining=seconds*1000;
    unsigned loops=0;
    while (!next.done) {
        const double stop=std::min<double>(a.duration,next.time+remaining);
        if (++loops>4096 || !collect(next.time,stop,next.first)) { error="Animation event/loop limit"; return false; }
        next.first=false; remaining-=stop-next.time; next.time=stop;
        if (next.time<a.duration) break;
        if (!next.looping || a.duration==0) { next.done=true; break; }
        next.time=0; next.first=true;
        if (remaining==0) { if (!collect(0,0,true)) { error="Animation event limit"; return false; } next.first=false; break; }
    }
    track=next; events=std::move(crossed); return true;
}
}
