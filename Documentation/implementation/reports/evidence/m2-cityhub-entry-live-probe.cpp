#include "game/server_adapter.h"
#include "engine/world_view.h"
#include <iostream>
using namespace OpenBlood2;
int main(){
    auto g=std::make_unique<GameSession>(); Config c; c.data_dir="/home/yann/.steam/steam/steamapps/common/Blood II"; std::string e;
    if(!g->initialize()||!g->resources.mount_game(c,e)||!g->request_world("01_SubwayA",e)||!g->enter_player(e)) return 1;
    if(g->server.SendWorldExit({"02_CityHub"},"01_egressa")!=DE_OK||!g->pump_player_messages(e)||GameSession::process_level_change(g,e)!=LevelChangeResult::Committed){std::cerr<<e;return 2;}
    auto* res=g->resources.find(g->world_path);
    std::cout<<"Resource "<<res->archive<<" offset="<<res->offset<<" size="<<res->size<<" overrides="<<res->overrides<<'\n';
    WorldMesh m;if(!m.build(g->world,e,&g->resources)){std::cerr<<e;return 3;}
    for(auto& mat:m.materials) if(!mat.frames.empty()) std::cout<<mat.path<<": "<<mat.frames.size()<<" frames at "<<mat.fps<<" fps, "<<mat.image.width<<"x"<<mat.image.height<<'\n';
    for(unsigned i=0;i<600;++i) if(!g->step(1./60)){std::cerr<<"Failed tick "<<i<<'\n';for(auto& line:g->trace)std::cerr<<line<<'\n';return 4;}
    std::cout<<"600 post-transition ticks, time="<<g->objects.time()<<"; HP="<<g->server_shell.Player(g->local_client)->m_damage.m_fHitPoints<<"; unavailable="<<g->server.unavailable_services.size()<<"\n";
    for(auto& [name,count]:g->server.unavailable_services)std::cout<<name<<": "<<count<<'\n';
    return g->unload_world()&&!g->server.string_count()&&!g->server.client_count()?0:5;
}
