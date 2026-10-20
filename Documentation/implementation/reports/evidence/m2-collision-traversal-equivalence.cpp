#include "game/server_adapter.h"
#include <bit>
#include <iostream>
#include <iomanip>
using namespace OpenBlood2;
std::uint64_t digest=14695981039346656037ull, queries=0;
void hash(std::uint64_t v){for(int i=0;i<8;++i){digest^=(v>>(i*8))&255;digest*=1099511628211ull;}}
void test(const CollisionWorld&w,DVector a,DVector b,DVector half){CollisionHit h;auto r=w.sweep(a,b,half,h);hash(static_cast<unsigned>(r));hash(std::bit_cast<std::uint64_t>(h.fraction));hash(std::bit_cast<std::uint64_t>(h.penetration));for(auto v:{h.point.x,h.point.y,h.point.z,h.normal.x,h.normal.y,h.normal.z})hash(std::bit_cast<std::uint32_t>(v));hash(h.polygon);hash(h.start_solid);hash(h.texture_flags);++queries;}

int main(int argc,char**argv){if(argc!=2)return 2;GameSession g;Config c;c.data_dir=argv[1];std::string e;if(!g.initialize()||!g.resources.mount_game(c,e))return 3;
std::uint32_t seed=12345;auto next=[&](){seed=seed*1664525+1013904223;return seed;};
for(auto name:{"01_SubwayA","02_CityHub"}){if(!g.request_world(name,e)){std::cerr<<e;return 4;}const auto&m=g.world.models.front();
for(unsigned i=0;i<24000;++i){auto a=m.points[next()%m.points.size()],b=m.points[next()%m.points.size()];DVector h{};if(i%4==1)h={16,32,16};else if(i%4==2)h={.000001f,.000001f,.000001f};else if(i%4==3)h={64,12,48};if(i%6==0)b=a;if(i%6==1)b=vector_add(a,{.000001f,0,0});if(i%6==2){a=vector_add(a,{1,37,1});b=vector_add(b,{-1,-17,-1});}test(g.collision,a,b,h);}
std::cout<<name<<" queries="<<queries<<" digest="<<std::hex<<digest<<std::dec<<'\n';}
}
