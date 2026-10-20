#include "b2-cost-profile.h"
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
bool run(GameSession&g){std::string error;ModelView models,effects;SpriteView sprites;WorldMesh mesh;std::vector<WorldDraw>draws;WorldViewStats stats;auto start=Clock::now();if(!mesh.build(g.world,error,&g.resources)){std::cerr<<error;return false;}std::cout<<"WORLD "<<g.world_path<<" mesh_ms="<<ms(start)<<" objects="<<g.objects.snapshot().size()<<'\n';std::vector<double>step,model,sprite,world,fx; b2_costs={};
 for(int i=0;i<360;++i){auto t=Clock::now();if(!g.step(1./60)){std::cerr<<"step failed\n";return false;}auto s=ms(t);t=Clock::now();if(!world_draws(mesh,g.objects,draws,stats,error))return false;auto w=ms(t);t=Clock::now();if(!update_model_scene(g,models,error)){std::cerr<<error;return false;}auto m=ms(t);t=Clock::now();if(!update_explosion_model_scene(g,effects,error))return false;auto f=ms(t);t=Clock::now();if(!update_sprite_scene(g,sprites,error)){std::cerr<<error;return false;}auto sp=ms(t);if(i>=60){step.push_back(s);model.push_back(m);sprite.push_back(sp);world.push_back(w);fx.push_back(f);} }
 for(unsigned i=0;i<b2_costs.size();++i)std::cout<<"PROFILE "<<i<<" calls="<<b2_costs[i].calls<<" ms="<<b2_costs[i].ms<<"\n"; report("step",step);report("world",world);report("model",model);report("sprite",sprite);report("fx",fx);std::cout<<"COUNTS models="<<models.entries.size()<<" model_vertices="<<models.mesh.vertices.size()<<" sprites="<<sprites.mesh.vertices.size()<<" voices="<<g.server.audio.voices().size()<<'\n'<<std::flush;return true;}
int main(int argc,char**argv){if(argc!=3)return 2;auto g=std::make_unique<GameSession>();std::string e;Config c;c.data_dir=argv[1];if(!g->initialize()||!g->resources.mount_game(c,e)||!g->request_world(argv[2],e)||!g->enter_player(e)){std::cerr<<e;return 3;}g->server.audio.inspect_pcm=true;if(!run(*g))return 4;WorldMessage msg;if(!original_world_request("02_CityHub",0,LOADTYPE_NEW_LEVEL,0,msg)||!g->queue_level_change(msg,"01_egressa",e)||GameSession::process_level_change(g,e)!=LevelChangeResult::Committed){std::cerr<<e;return 5;}g->server.audio.inspect_pcm=true;if(!run(*g))return 6;return 0;}
