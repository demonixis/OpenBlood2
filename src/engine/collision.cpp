#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace blood2 {
DVector vector_add(DVector a,DVector b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
DVector vector_subtract(DVector a,DVector b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
DVector vector_scale(DVector a,float s) { return {a.x*s,a.y*s,a.z*s}; }
float vector_dot(DVector a,DVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
bool physics_vector(DVector v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x)<=1e7f && std::abs(v.y)<=1e7f && std::abs(v.z)<=1e7f; }
namespace {
DVector cross(DVector a,DVector b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
DVector minv(DVector a,DVector b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
DVector maxv(DVector a,DVector b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }
bool extents(DVector v) { return physics_vector(v) && v.x>=0 && v.y>=0 && v.z>=0; }
constexpr std::array<DVector,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
// Swept separating-axis intervals. An already touching face permits movement
// parallel to or away from it; genuine initial penetration is reported.
struct Sweep {
    DVector from,delta,half,normal{},penetration_normal{};
    double enter=-std::numeric_limits<double>::infinity(),leave=1,penetration=std::numeric_limits<double>::infinity();
    bool axis(DVector axis,double low,double high) {
        const double length=std::sqrt(vector_dot(axis,axis)); if (length<1e-8) return true;
        axis=vector_scale(axis,static_cast<float>(1/length)); low/=length; high/=length;
        const double radius=std::abs(axis.x)*half.x+std::abs(axis.y)*half.y+std::abs(axis.z)*half.z;
        low-=radius; high+=radius;
        const double origin=vector_dot(from,axis), speed=vector_dot(delta,axis);
        const double depth=std::min(origin-low,high-origin);
        if (depth<penetration) { penetration=depth; penetration_normal=vector_scale(axis,origin<(low+high)/2 ? -1.0f : 1.0f); }
        if (std::abs(speed)<1e-10) {
            if (radius==0 && high-low>1e-6) return origin>=low-1e-6 && origin<=high+1e-6;
            return origin>low+1e-6 && origin<high-1e-6;
        }
        double first=(low-origin)/speed,last=(high-origin)/speed;
        DVector n=vector_scale(axis,speed>0 ? -1.0f : 1.0f);
        if (first>last) std::swap(first,last);
        if (first>enter) { enter=first; normal=n; }
        leave=std::min(leave,last);
        return enter<=leave && leave>1e-8 && enter<=1;
    }
    bool finish(CollisionHit& hit) const {
        if (enter>leave || leave<=1e-8 || enter>1) return false;
        hit.start_solid=enter < -1e-6;
        hit.fraction=std::max(0.0,enter); hit.normal=hit.start_solid ? penetration_normal : normal;
        hit.point=vector_add(from,vector_scale(delta,static_cast<float>(hit.fraction))); return true;
    }
};
bool triangle_hit(const std::array<DVector,3>& p,DVector from,DVector to,DVector half,CollisionHit& hit) {
    Sweep s{from,vector_subtract(to,from),half};
    const std::array<DVector,3> edges{{vector_subtract(p[1],p[0]),vector_subtract(p[2],p[1]),vector_subtract(p[0],p[2])}};
    auto test=[&](DVector axis) {
        const double a=vector_dot(axis,p[0]),b=vector_dot(axis,p[1]),c=vector_dot(axis,p[2]);
        return s.axis(axis,std::min({a,b,c}),std::max({a,b,c}));
    };
    for (auto axis:axes) if (!test(axis)) return false;
    if (!test(cross(edges[0],edges[1]))) return false;
    for (auto edge:edges) for (auto axis:axes) if (!test(cross(edge,axis))) return false;
    return s.finish(hit);
}
bool broadphase(DVector from,DVector to,DVector half,DVector min,DVector max) {
    auto lo=vector_subtract(minv(from,to),half),hi=vector_add(maxv(from,to),half);
    return lo.x<=max.x && hi.x>=min.x && lo.y<=max.y && hi.y>=min.y && lo.z<=max.z && hi.z>=min.z;
}
}
CollisionQuery sweep_box(DVector from,DVector to,DVector half,DVector center,DVector other_half,CollisionHit& hit) {
    hit={};
    if (!physics_vector(from) || !physics_vector(to) || !extents(half) || !physics_vector(center) || !extents(other_half)) return CollisionQuery::Invalid;
    Sweep s{from,vector_subtract(to,from),half};
    auto low=vector_subtract(center,other_half),high=vector_add(center,other_half);
    for (auto axis:axes) if (!s.axis(axis,vector_dot(low,axis),vector_dot(high,axis))) return CollisionQuery::Miss;
    return s.finish(hit) ? CollisionQuery::Hit : CollisionQuery::Miss;
}
bool CollisionWorld::build(const WorldModel& model,std::string& error) {
    clear(); error.clear();
    auto fail=[&] { clear(); error="Invalid or excessive primary collision geometry"; return false; };
    if (model.polygons.size()>2000000) return fail();
    for (std::uint32_t i=0;i<model.polygons.size();++i) {
        const auto& polygon=model.polygons[i];
        if (polygon.surface>=model.surfaces.size() || polygon.points.size()<3) return fail();
        // SURF_SOLID = bit 0 in the donor and LithTech world sources.
        if (!(model.surfaces[polygon.surface].flags & 1)) continue;
        for (auto index:polygon.points) if (index>=model.points.size() || !physics_vector(model.points[index])) return fail();
        for (std::size_t j=1;j+1<polygon.points.size();++j) {
            if (triangles_.size()>=2000000) return fail();
            Triangle t{{model.points[polygon.points[0]],model.points[polygon.points[j]],model.points[polygon.points[j+1]]},{},{},i};
            auto normal=cross(vector_subtract(t.points[1],t.points[0]),vector_subtract(t.points[2],t.points[0]));
            if (vector_dot(normal,normal)<1e-12f) continue;
            t.min=minv(t.points[0],minv(t.points[1],t.points[2])); t.max=maxv(t.points[0],maxv(t.points[1],t.points[2])); triangles_.push_back(t);
        }
    }
    if (!triangles_.empty()) partition(0,static_cast<std::uint32_t>(triangles_.size()));
    return true;
}
std::uint32_t CollisionWorld::partition(std::uint32_t begin,std::uint32_t end) {
    Node n; n.begin=begin; n.end=end; n.min=triangles_[begin].min; n.max=triangles_[begin].max;
    for (auto i=begin+1;i<end;++i) { n.min=minv(n.min,triangles_[i].min); n.max=maxv(n.max,triangles_[i].max); }
    auto index=static_cast<std::uint32_t>(nodes_.size()); nodes_.push_back(n);
    if (end-begin>8) {
        auto size=vector_subtract(n.max,n.min); auto axis=size.x>=size.y && size.x>=size.z ? axes[0] : size.y>=size.z ? axes[1] : axes[2];
        auto mid=begin+(end-begin)/2;
        std::nth_element(triangles_.begin()+begin,triangles_.begin()+mid,triangles_.begin()+end,[axis](const auto& a,const auto& b) {
            return vector_dot(vector_add(a.min,a.max),axis)<vector_dot(vector_add(b.min,b.max),axis);
        });
        auto left=partition(begin,mid),right=partition(mid,end); nodes_[index].left=left; nodes_[index].right=right;
    }
    return index;
}
void CollisionWorld::query(std::uint32_t index,DVector from,DVector to,DVector half,CollisionHit& hit,bool& found) const {
    const auto& n=nodes_[index]; if (!broadphase(from,to,half,n.min,n.max)) return;
    if (n.left) { query(n.left,from,to,half,hit,found); query(n.right,from,to,half,hit,found); return; }
    for (auto i=n.begin;i<n.end;++i) {
        CollisionHit candidate;
        if (triangle_hit(triangles_[i].points,from,to,half,candidate) && (!found || candidate.fraction<hit.fraction ||
                (candidate.fraction==hit.fraction && triangles_[i].polygon<hit.polygon))) {
            hit=candidate; hit.polygon=triangles_[i].polygon; found=true;
        }
    }
}
CollisionQuery CollisionWorld::sweep(DVector from,DVector to,DVector half,CollisionHit& hit) const {
    hit={}; if (!physics_vector(from) || !physics_vector(to) || !extents(half)) return CollisionQuery::Invalid;
    if (nodes_.empty()) return CollisionQuery::Miss;
    bool found=false; query(0,from,to,half,hit,found); return found ? CollisionQuery::Hit : CollisionQuery::Miss;
}
}
