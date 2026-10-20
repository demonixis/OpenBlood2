#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <vector>
using namespace OpenBlood2;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();}
void report(const char*label,std::vector<double>v){std::sort(v.begin(),v.end());double sum=0;for(auto t:v)sum+=t;std::cout<<label<<" mean="<<sum/v.size()<<" p50="<<v[v.size()/2]<<" p95="<<v[v.size()*95/100]<<" max="<<v.back()<<'\n';}

struct StateDigest {
 std::uint64_t value=14695981039346656037ull;
 void bytes(const void*p,std::size_t n){auto b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i){value^=b[i];value*=1099511628211ull;}}
 template<class T>void add(const T&v){bytes(&v,sizeof(v));}
 void vec(DVector v){add(v.x);add(v.y);add(v.z);}
 void info(const GameSession&g,const ContainerInfo&i){auto*r=g.objects.get(i.object);add(r? r->id.slot: UINT32_MAX);add(r? r->id.generation: 0u);add(i.code);vec(i.current);add(i.gravity);add(i.viscosity);add(i.hidden);}
 bool frame(GameSession&g){auto ids=g.objects.snapshot();add(ids.size());for(auto id:ids){auto*r=g.objects.get(id);add(id.slot);add(id.generation);bytes(r->type->name.data(),r->type->name.size());bytes(r->state.m_Name,std::char_traits<char>::length(r->state.m_Name));add(r->life);add(r->activity);add(r->state.m_Flags);add(r->user_flags);vec(r->state.m_Pos);vec(r->state.m_Rotation.m_Vec);add(r->state.m_Rotation.m_Spin);vec(r->velocity);vec(r->acceleration);vec(r->dimensions);add(r->updates);add(r->due);add(r->grounded);add(bool(r->container()));if(r->life==Life::Alive){ContainerState s;if(!g.server.GetContainers(r->handle,r->state.m_Pos,s))return false;add(s.total);add(s.count);info(g,s.head);for(std::size_t i=0;i<s.count;++i)info(g,s.body[i]);}}return true;}
};

bool run(GameSession&g){std::string error;ModelView models,effects;SpriteView sprites;WorldMesh mesh;std::vector<WorldDraw>draws;WorldViewStats stats;StateDigest digest;auto start=Clock::now();if(!mesh.build(g.world,error,&g.resources)){std::cerr<<error;return false;}std::cout<<"WORLD "<<g.world_path<<" mesh_ms="<<ms(start)<<" objects="<<g.objects.snapshot().size()<<'\n';std::vector<double>step,model,sprite,world,fx;
 for(int i=0;i<360;++i){auto t=Clock::now();if(!g.step(1./60)){std::cerr<<"step failed\n";return false;}auto s=ms(t);t=Clock::now();if(!world_draws(mesh,g.objects,draws,stats,error))return false;auto w=ms(t);t=Clock::now();if(!update_model_scene(g,models,error)){std::cerr<<error;return false;}auto m=ms(t);t=Clock::now();if(!update_explosion_model_scene(g,effects,error))return false;auto f=ms(t);t=Clock::now();if(!update_sprite_scene(g,sprites,error)){std::cerr<<error;return false;}auto sp=ms(t);if(!digest.frame(g))return false;if(i>=60){step.push_back(s);model.push_back(m);sprite.push_back(sp);world.push_back(w);fx.push_back(f);} }
 std::cout<<"DIGEST "<<g.world_path<<" "<<digest.value<<"\n";report("step",step);report("world",world);report("model",model);report("sprite",sprite);report("fx",fx);std::cout<<"COUNTS models="<<models.entries.size()<<" model_vertices="<<models.mesh.vertices.size()<<" sprites="<<sprites.mesh.vertices.size()<<" voices="<<g.server.audio.voices().size()<<'\n'<<std::flush;return true;}
int main(int argc,char**argv){if(argc!=3)return 2;auto g=std::make_unique<GameSession>();std::string e;Config c;c.data_dir=argv[1];if(!g->initialize()||!g->resources.mount_game(c,e)||!g->request_world(argv[2],e)||!g->enter_player(e)){std::cerr<<e;return 3;}g->server.audio.inspect_pcm=true;if(!run(*g))return 4;WorldMessage msg;if(!original_world_request("02_CityHub",0,LOADTYPE_NEW_LEVEL,0,msg)||!g->queue_level_change(msg,"01_egressa",e)||GameSession::process_level_change(g,e)!=LevelChangeResult::Committed){std::cerr<<e;return 5;}g->server.audio.inspect_pcm=true;if(!run(*g))return 6;return 0;}
