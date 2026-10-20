#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <vector>
using namespace OpenBlood2;
using Clock=std::chrono::steady_clock;
struct StateDigest {
 std::uint64_t value=14695981039346656037ull;
 void bytes(const void*p,std::size_t n){auto b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i){value^=b[i];value*=1099511628211ull;}}
 template<class T>void add(const T&v){bytes(&v,sizeof(v));}
 void vec(DVector v){add(v.x);add(v.y);add(v.z);}
 void info(const GameSession&g,const ContainerInfo&i){auto*r=g.objects.get(i.object);add(r? r->id.slot: UINT32_MAX);add(r? r->id.generation: 0u);add(i.code);vec(i.current);add(i.gravity);add(i.viscosity);add(i.hidden);}
 void view(const ModelView&v){add(v.mesh.vertices.size());for(const auto&p:v.mesh.vertices){vec(p.position);vec(p.color);add(p.uv[0]);add(p.uv[1]);}add(v.mesh.ranges.size());for(const auto&r:v.mesh.ranges){add(r.first);add(r.count);for(const auto&b:r.batches){add(b.first);add(b.count);add(b.material);add(b.translucent);}}for(const auto* draws:{&v.world_draws,&v.close_draws}){add(draws->size());for(const auto&d:*draws){add(d.range);vec(d.position);vec(d.rotation.m_Vec);add(d.rotation.m_Spin);add(d.alpha);}}for(const auto&e:v.entries){add(e.object.slot);add(e.object.generation);add(e.visible);add(e.close);add(e.range);add(e.triangles);add(e.hidden_triangles);add(e.hidden_nodes);add(e.alpha);bytes(e.skin.data(),e.skin.size());bytes(e.error.data(),e.error.size());}}
 bool frame(GameSession&g){auto ids=g.objects.snapshot();add(ids.size());for(auto id:ids){auto*r=g.objects.get(id);add(id.slot);add(id.generation);bytes(r->type->name.data(),r->type->name.size());bytes(r->state.m_Name,std::char_traits<char>::length(r->state.m_Name));add(r->life);add(r->activity);add(r->state.m_Flags);add(r->user_flags);vec(r->state.m_Pos);vec(r->state.m_Rotation.m_Vec);add(r->state.m_Rotation.m_Spin);vec(r->velocity);vec(r->acceleration);vec(r->dimensions);add(r->updates);add(r->due);add(r->grounded);add(bool(r->container()));if(r->life==Life::Alive){ContainerState s;if(!g.server.GetContainers(r->handle,r->state.m_Pos,s))return false;add(s.total);add(s.count);info(g,s.head);for(std::size_t i=0;i<s.count;++i)info(g,s.body[i]);}}return true;}
};

bool run(GameSession&g){std::string error;ModelView models,effects;StateDigest digest;for(int i=0;i<360;++i){if(!g.step(1./60)||!update_model_scene(g,models,error)||!update_explosion_model_scene(g,effects,error)||!digest.frame(g)){std::cerr<<error;return false;}digest.view(models);digest.view(effects);}std::cout<<"DIGEST "<<g.world_path<<" "<<digest.value<<"\n";return true;}
int main(int argc,char**argv){if(argc!=3)return 2;auto g=std::make_unique<GameSession>();std::string e;Config c;c.data_dir=argv[1];if(!g->initialize()||!g->resources.mount_game(c,e)||!g->request_world(argv[2],e)||!g->enter_player(e)){std::cerr<<e;return 3;}g->server.audio.inspect_pcm=true;if(!run(*g))return 4;WorldMessage msg;if(!original_world_request("02_CityHub",0,LOADTYPE_NEW_LEVEL,0,msg)||!g->queue_level_change(msg,"01_egressa",e)||GameSession::process_level_change(g,e)!=LevelChangeResult::Committed){std::cerr<<e;return 5;}g->server.audio.inspect_pcm=true;if(!run(*g))return 6;return 0;}
