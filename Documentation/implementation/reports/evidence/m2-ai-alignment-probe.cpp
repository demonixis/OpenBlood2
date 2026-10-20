#include "game/server_adapter.h"
#include "game/original/AI_Mgr.h"
#include <iostream>
using namespace OpenBlood2;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 auto g=std::make_unique<GameSession>(); Config c;c.data_dir=argv[1];std::string error;
 if(!g->initialize()||!g->resources.mount_game(c,error)||!g->request_world("02_CityHub",error)||!g->enter_player(error)){std::cerr<<error;return 1;}
 auto*p=g->server_shell.Player(g->local_client);p->m_damage.can_damage=false;
 struct Spot{DVector pos; std::string name;}; std::vector<Spot> spots;
 for(auto id:g->objects.snapshot()){auto*r=g->objects.get(id);if(auto*a=dynamic_cast<AI_Mgr*>(r->game.get())){spots.push_back({r->state.m_Pos,r->state.m_Name});std::cout<<"NPC "<<r->state.m_Name<<" state="<<a->m_nState<<" smell="<<a->m_fSmellingDist<<" pos="<<r->state.m_Pos.x<<","<<r->state.m_Pos.y<<","<<r->state.m_Pos.z<<'\n';}}
 for(auto& spot:spots){auto pos=spot.pos;pos.z-=160;pos.y+=64;CollisionHit hit;if(g->collision.sweep(pos,pos,{20,42,20},hit)!=CollisionQuery::Miss){std::cout<<"SKIP blocked "<<spot.name<<"\n";continue;}g->objects.get(p->m_hObject)->state.m_Pos=pos;for(unsigned i=0;i<300;++i)if(!g->step(1./60)){std::cerr<<"step failed "<<spot.name<<" at "<<i<<'\n';for(auto&x:g->trace)std::cerr<<x<<'\n';for(auto&x:g->objects.diagnostics())std::cerr<<x<<'\n';return 3;}std::cout<<"SPOT "<<spot.name<<'\n';for(auto id:g->objects.snapshot()){auto*r=g->objects.get(id);if(auto*a=dynamic_cast<AI_Mgr*>(r->game.get());a&&a->runtime_failed)std::cout<<r->state.m_Name<<" state="<<a->m_nState<<" phase="<<a->Metacmd<<" dims="<<r->dimensions.x<<","<<r->dimensions.y<<","<<r->dimensions.z<<" cached="<<a->m_MoveObj.GetPos().x<<","<<a->m_MoveObj.GetPos().y<<","<<a->m_MoveObj.GetPos().z<<" dest="<<a->m_MoveObj.requested_destination.x<<","<<a->m_MoveObj.requested_destination.y<<","<<a->m_MoveObj.requested_destination.z<<" failure="<<a->runtime_error<<'\n';}}
 return 0;
}
