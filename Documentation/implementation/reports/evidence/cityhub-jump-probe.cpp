#include "game/server_adapter.h"
#include <iostream>
#include <cmath>
using namespace OpenBlood2;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 GameSession g;Config cfg;cfg.data_dir=argv[1];std::string error;
 if(!g.initialize()||!g.resources.mount_game(cfg,error)||!g.request_world("02_CityHub",error)||!g.enter_player(error)||!g.step(1./60)){
 std::cerr<<error;return 1;}
 auto&c=g.client_shell;auto*p=g.server_shell.Player(g.local_client);
 struct Case {const char*name;DVector pos;float yaw;bool moving;};
 for(auto test:{Case{"flat",{1100,682.002f,-2860},0,false},Case{"rim_to_dumpster96",{1000,714.002f,-2688},3.14159265f,true},Case{"dumpster_to_fence64",{1060,810.002f,-2750},3.14159265f,true}}){
 if(g.server.TeleportObject(c.player,&test.pos)!=DE_OK||!p->TeleportClientToServerPos()||!g.pump_player_messages(error))return 3;
 auto&mgr=c.move_mgr;
 MovementInput in{};
 for(int i=0;i<5;++i)if(!mgr.Update(g.server_shell,c.voice_manager,false,in,test.yaw,1./60,true)){auto*r=g.objects.get(c.player); std::cerr<<"settle failed "<<test.name<<" pos "<<r->state.m_Pos.x<<","<<r->state.m_Pos.y<<","<<r->state.m_Pos.z<<" contact "<<(r->contact_object?g.server.GetObjectName(r->contact_object):"world")<<" polygon "<<r->contact_polygon<<" locomotion "<<p->locomotion_error;if(auto*o=g.objects.get(r->contact_object))std::cerr<<" obstacle model "<<o->state.m_Filename<<" pos "<<o->state.m_Pos.x<<","<<o->state.m_Pos.y<<","<<o->state.m_Pos.z<<" dim "<<o->dimensions.x<<","<<o->dimensions.y<<","<<o->dimensions.z<<"\n";for(auto&d:g.objects.diagnostics())std::cerr<<d<<"\n";return 4;}
 auto start=mgr.body.position;float maxfeet=start.y-mgr.body.dimensions.y;
 auto jumps=mgr.jumps;
 in.jump=true;in.forward=test.moving;in.run=true;
 for(int i=0;i<45;++i){if(!mgr.Update(g.server_shell,c.voice_manager,false,in,test.yaw,1./60,true)){std::cerr<<"move failed "<<test.name;return 5;}
 maxfeet=std::max(maxfeet,mgr.body.position.y-mgr.body.dimensions.y); if(i%5==0)std::cout<<"FRAME "<<test.name<<' '<<i<<" pos "<<mgr.body.position.x<<','<<mgr.body.position.y<<','<<mgr.body.position.z<<" vy "<<mgr.body.velocity.y<<" ground "<<mgr.m_bOnGround<<'\n';}
 std::cout<<"CASE "<<test.name<<" start "<<start.x<<','<<start.y<<','<<start.z<<" max_feet_rise "<<maxfeet-(start.y-42)<<" end "<<mgr.body.position.x<<','<<mgr.body.position.y<<','<<mgr.body.position.z<<" jumps "<<mgr.jumps-jumps<<" crouched "<<p->crouched<<" gravity "<<mgr.physics.gravity.y<<'\n';
 }
}
