#include "player_fixture.h"
#include "app/movement_controls.h"
#include <cmath>
#include <functional>
#include <limits>

using namespace blood2;
#define CHECK(x) do { if (!(x)) { std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n"; return false; } } while(false)
namespace {
bool enter(GameSession& game,const std::filesystem::path& root) {
    std::string error; CHECK(test_player::setup(game,root)); CHECK(game.enter_player(error));
    game.client_shell.rotation={}; return true;
}
bool steps(GameSession& game,int count,double delta=1.0/60) {
    for (int i=0;i<count;++i) CHECK(game.step(delta));
    return true;
}
bool walking(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
    CHECK(client.move_mgr.ready()); auto start=client.position;
    client.movement_input.forward=true;
    CHECK(steps(game,60)); const auto& body=client.move_mgr.body;
    CHECK(std::abs(body.position.x-start.x)<.01 && body.position.z>start.z+250 && body.position.z<start.z+285);
    CHECK(std::abs(client.move_mgr.m_fVelMagnitude-280)<.1 && client.move_mgr.m_bOnGround);
    CHECK(std::abs(client.camera_position.y-body.position.y-36)<.01);
    CHECK(game.objects.get(client.player)->client_movement);
    // Run and diagonals preserve original speed factors, including sqrt(2).
    client.movement_input.run=true; CHECK(steps(game,30)); CHECK(std::abs(client.move_mgr.m_fVelMagnitude-420)<1);
    client.movement_input.right=true; CHECK(steps(game,20)); CHECK(client.move_mgr.m_fVelMagnitude>580);
    client.movement_input={}; CHECK(steps(game,60)); CHECK(client.move_mgr.m_fVelMagnitude<.01);
    const auto before=client.position;
    client.rotation.yaw=1.57079632679f; client.movement_input.forward=true;
    CHECK(steps(game,20)); CHECK(client.position.x>before.x+60 && std::abs(client.position.z-before.z)<.1);
    client.movement_input.backward=true; CHECK(steps(game,60)); CHECK(client.move_mgr.m_fVelMagnitude<.01);
    return true;
}
bool jumping(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
    const float floor=client.position.y; client.movement_input.jump=true;
    float peak=floor; for (int i=0;i<90;++i) { CHECK(game.step(1.0/60)); peak=std::max(peak,client.position.y); }
    CHECK(peak>floor+85 && peak<floor+95); CHECK(client.move_mgr.jumps==1 && client.move_mgr.landings==1);
    CHECK(client.move_mgr.m_bOnGround && std::abs(client.position.y-floor)<.01 && client.move_mgr.body.velocity.y==0);
    client.movement_input.jump=false; CHECK(game.step(1.0/60)); client.movement_input.jump=true;
    CHECK(game.step(1.0/60)); CHECK(client.move_mgr.jumps==2 && !client.move_mgr.m_bOnGround);
    // A new jump press in midair cannot add another impulse.
    client.movement_input.jump=false; CHECK(game.step(1.0/60)); client.movement_input.jump=true;
    CHECK(game.step(1.0/60)); CHECK(client.move_mgr.jumps==2);
    return true;
}
HOBJECT box(GameSession& game,DVector position,DVector dimensions,DDWORD flags=FLAG_SOLID) {
    ObjectCreateStruct state; state.m_Pos=position; state.m_Flags=flags;
    auto h=game.objects.create(game.objects.find_class("BaseClass"),state);
    game.server.SetObjectDims(h,&dimensions); return h;
}
bool collisions(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
    auto wall=box(game,{32,100,200},{200,100,5}); CHECK(wall);
    client.movement_input.forward=true; CHECK(steps(game,60)); CHECK(client.position.z>182.9 && client.position.z<183.01);
    CHECK(std::abs(client.move_mgr.body.velocity.z)<.01 && client.move_mgr.m_bOnGround);
    const auto before=client.position; client.movement_input.right=true;
    CHECK(steps(game,20)); CHECK(client.position.x>before.x+60 && client.position.z<183.01);
    CHECK(client.move_mgr.body.ground_polygon!=UINT32_MAX && !client.move_mgr.body.ground_object);
    // A low ceiling clips upward velocity; gravity subsequently returns to floor.
    client.movement_input={}; CHECK(steps(game,30));
    const auto p=client.position; CHECK(box(game,{p.x,110,p.z},{100,2,100}));
    client.movement_input.jump=true; float peak=p.y;
    for (int i=0;i<60;++i) { CHECK(game.step(1.0/60)); peak=std::max(peak,client.position.y); }
    CHECK(peak<=67.01f && peak>60 && client.move_mgr.m_bOnGround && client.move_mgr.jumps==1);
    return true;
}
bool support(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
    CHECK(game.step(1.0/60)); // Consume the original first-update force-position.
    const auto platform=box(game,{0,50,0},{100,5,100}); CHECK(platform);
    DVector position{0,96.001f,0}; CHECK(game.server.TeleportObject(client.player,&position)==DE_OK);
    CHECK(steps(game,2)); CHECK(client.move_mgr.m_bOnGround && client.move_mgr.body.ground_object==platform);
    CHECK(game.objects.remove(platform) && game.objects.finish_frame());
    CHECK(steps(game,2)); CHECK(!client.move_mgr.m_bOnGround && client.move_mgr.body.velocity.y<0);
    CHECK(steps(game,60)); CHECK(client.move_mgr.m_bOnGround && client.position.y<41.01);
    // Walk beyond the finite world floor: cached ground must not persist.
    position={0,41.001f,980}; CHECK(game.server.TeleportObject(client.player,&position)==DE_OK);
    client.movement_input.forward=true; CHECK(steps(game,30));
    CHECK(!client.move_mgr.m_bOnGround && client.position.y<30 && client.move_mgr.body.velocity.y<0);
    return true;
}
bool world_collision(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell; std::string error;
    CHECK(game.step(1.0/60));
    WorldModel world; world.points={{-1000,0,-1000},{1000,0,-1000},{1000,0,1000},{-1000,0,1000},
        {-1000,0,200},{1000,0,200},{1000,500,200},{-1000,500,200}};
    world.surfaces.resize(1); world.surfaces[0].flags=1; world.polygons={{0,{0,1,2,3}},{0,{4,5,6,7}}};
    CHECK(game.collision.build(world,error)); client.movement_input.forward=true;
    CHECK(steps(game,60)); CHECK(client.position.z>187.9 && client.position.z<188.01 && client.move_mgr.m_bOnGround);
    client.movement_input.right=true; CHECK(steps(game,20)); CHECK(client.position.x>90 && client.position.z<188.01);
    // Replace the fixture by a broad 1:4 ramp; support normal and projected
    // acceleration must permit walking uphill without entering the triangles.
    world.points={{-1000,-250,-1000},{1000,-250,-1000},{1000,250,1000},{-1000,250,1000}};
    world.polygons={{0,{0,1,2,3}}}; CHECK(game.collision.build(world,error));
    client.movement_input={}; DVector position{0,200,0},zero;
    CHECK(game.server.TeleportObject(client.player,&position)==DE_OK && game.server.SetVelocity(client.player,&zero)==DE_OK);
    CHECK(steps(game,60)); CHECK(client.move_mgr.m_bOnGround && client.move_mgr.body.ground_normal.y>.95f);
    const auto start=client.position; client.movement_input.forward=true; CHECK(steps(game,30));
    CHECK(client.position.z>start.z+90 && client.position.y>start.y+20 && client.move_mgr.m_bOnGround);
    client.movement_input={}; CHECK(steps(game,120));
    CHECK(client.move_mgr.m_fVelMagnitude<.1 && client.move_mgr.m_bOnGround);
    const auto stopped=client.position; CHECK(steps(game,60));
    CHECK(std::abs(client.position.z-stopped.z)<.05 && std::abs(client.position.y-stopped.y)<.05);
    return true;
}
bool boundaries(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
    CHECK(game.step(1.0/60)); auto* player=game.server_shell.Player(game.local_client);
    const auto start=client.position; PlayerMove move{static_cast<DBYTE>(player->m_byClientMoveCode+1),{500,100,0},{},{},false};
    CHECK(!game.server_shell.OnMessage(game.local_client,move));
    CHECK(client.position.x==start.x && client.position.y==start.y);
    move.move_code=player->m_byClientMoveCode; move.velocity.x=std::numeric_limits<float>::quiet_NaN();
    CHECK(!game.server_shell.OnMessage(game.local_client,move));
    CHECK(!client.move_mgr.Update(game.server_shell,{},0,-1,true));
    CHECK(!client.move_mgr.Update(game.server_shell,{},std::numeric_limits<float>::infinity(),.01,true));
    CHECK(!game.step(std::numeric_limits<double>::infinity()));
    client.movement_input.forward=true; CHECK(game.step(.5));
    CHECK(std::abs(client.move_mgr.discarded_time-.4)<1e-6 && client.position.z<start.z+20);
    CHECK(player->Respawn()); CHECK(game.pump_player_messages(player->entry_error));
    CHECK(client.move_mgr.body.velocity.z==0 && client.move_mgr.body.acceleration.z==0);
    CHECK(client.move_mgr.m_ClientMoveCode==player->m_byClientMoveCode);
    const auto code=player->m_byClientMoveCode;
    for (unsigned i=0;i<260;++i) { CHECK(player->TeleportClientToServerPos()); CHECK(game.pump_player_messages(player->entry_error)); }
    CHECK(client.move_mgr.m_ClientMoveCode==static_cast<DBYTE>(code+260));
    player->m_damage.m_bDead=DTRUE; CHECK(steps(game,10)); CHECK(client.move_mgr.m_dwControlFlags==0);
    return true;
}
bool timing(const std::filesystem::path& root) {
    DVector baseline;
    for (int fps : {120,60,30}) {
        GameSession game; CHECK(enter(game,root)); auto& client=game.client_shell;
        client.movement_input.forward=true; CHECK(steps(game,fps,1.0/fps));
        CHECK(client.move_mgr.substeps==120 && client.move_mgr.discarded_time==0);
        if (fps==120) baseline=client.position;
        else CHECK(std::abs(client.position.z-baseline.z)<.001 && std::abs(client.position.y-baseline.y)<.001);
        const auto position=client.position;
        CHECK(game.server.advance_physics(.1));
        DVector actual; game.server.GetObjectPos(client.player,&actual);
        CHECK(actual.x==position.x && actual.y==position.y && actual.z==position.z);
        // Frame zero is observational: no duplicated integration/messages.
        const auto sent=game.server_shell.Player(game.local_client)->movement_messages;
        CHECK(game.step(0)); CHECK(game.server_shell.Player(game.local_client)->movement_messages==sent);
    }
    return true;
}
std::function<void(BaseClass&,DDWORD,void*)> callback;
struct Contact : BaseClass {
    DDWORD EngineMessageFn(DDWORD msg,void* data,float reason) override {
        if (callback) callback(*this,msg,data);
        return BaseClass::EngineMessageFn(msg,data,reason);
    }
};
std::unique_ptr<BaseClass> make_contact() { return std::make_unique<Contact>(); }
ClassInfo contact_class{"MovementContact",nullptr,make_contact,{}};
bool lifetime(const std::filesystem::path& root) {
    GameSession game; CHECK(enter(game,root)); CHECK(game.objects.register_class(contact_class));
    const auto player=game.client_shell.player;
    ObjectCreateStruct state; state.m_Pos={32,41,100}; state.m_Flags=FLAG_TOUCH_NOTIFY;
    const auto sensor=game.objects.create(&contact_class,state); DVector dims{30,50,1}; game.server.SetObjectDims(sensor,&dims);
    bool touched=false, guarded=false;
    callback=[&](BaseClass&,DDWORD msg,void* data) { if (msg==MID_TOUCHNOTIFY && data==player) {
        touched=true; guarded=!game.step(.01) && !game.unload_world(); game.objects.remove(player);
    } };
    game.client_shell.movement_input.forward=true; CHECK(steps(game,30)); callback={};
    CHECK(touched && guarded && !game.objects.get(player) && !game.local_client && !game.client_shell.move_mgr.ready());
    CHECK(!game.client_shell.movement_input.forward && CDestructable::m_dwNumDestructables==0);
    std::string error; CHECK(game.objects.remove(sensor) && game.objects.finish_frame()); CHECK(game.enter_player(error));
    CHECK(game.client_shell.move_mgr.substeps==0 && game.client_shell.move_mgr.jumps==0);
    CHECK(game.unload_world() && game.objects.snapshot().empty() && game.server.string_count()==0);
    return true;
}
bool controls(const std::filesystem::path&) {
    MovementControls input; SDL_Event event{}; constexpr SDL_WindowID window=4;
    event.type=SDL_EVENT_WINDOW_FOCUS_GAINED; event.window.windowID=window; input.event(event,window);
    auto key=[&](SDL_Scancode code,bool down,SDL_WindowID id=4) {
        event={}; event.type=down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP; event.key.scancode=code; event.key.windowID=id; input.event(event,window);
    };
    key(SDL_SCANCODE_F2,true); key(SDL_SCANCODE_W,true); key(SDL_SCANCODE_SPACE,true); key(SDL_SCANCODE_LSHIFT,true);
    CHECK(input.sample(false).forward && input.sample(false).jump && input.sample(false).run);
    CHECK(input.consumes(event,false) && !input.consumes(event,true));
    CHECK(!input.sample(true).forward); key(SDL_SCANCODE_W,false,99); CHECK(input.sample(false).forward);
    PlayerRotation rotation; key(SDL_SCANCODE_RIGHT,true); input.look(rotation,.1,false); CHECK(rotation.yaw>.19f);
    event={}; event.type=SDL_EVENT_WINDOW_FOCUS_LOST; event.window.windowID=window; input.event(event,window);
    CHECK(!input.sample(false).jump && !input.keys[SDL_SCANCODE_W]);
    event.type=SDL_EVENT_WINDOW_FOCUS_GAINED; input.event(event,window); CHECK(!input.sample(false).forward);
    return true;
}
bool private_world(const std::filesystem::path& root) {
    GameSession game; CHECK(game.initialize()); Config config; config.data_dir=root; std::string error;
    CHECK(game.resources.mount_game(config,error));
    for (int cycle=0;cycle<3;++cycle) {
        CHECK(game.request_world("01_SubwayA",error) && game.enter_player(error)); auto& client=game.client_shell;
        CHECK(steps(game,20)); const auto initial=client.position;
        client.movement_input={true,false,false,false,false,true}; float peak=initial.y;
        for(int i=0;i<40;++i) { CHECK(game.step(1.0/60)); peak=std::max(peak,client.position.y); }
        client.movement_input={}; CHECK(steps(game,100));
        std::cout << "cycle " << cycle << ": distance " << std::hypot(client.position.x-initial.x,client.position.z-initial.z)
            << "; rise " << peak-initial.y << "; jumps " << client.move_mgr.jumps << "; landings " << client.move_mgr.landings
            << "; ground " << client.move_mgr.m_bOnGround << "; messages " << game.server_shell.Player(game.local_client)->movement_messages << '\n';
        CHECK(std::hypot(client.position.x-initial.x,client.position.z-initial.z)>1);
        CHECK(peak>initial.y+20 && client.move_mgr.jumps==1 && client.move_mgr.landings>=1 && client.move_mgr.m_bOnGround);
        CHECK(game.unload_world() && !client.move_mgr.ready() && game.objects.snapshot().empty() && game.server.string_count()==0);
    }
    return true;
}
}
int main(int argc,char** argv) {
    if (argc!=3) return 2;
    const std::string_view name=argv[1]; const auto root=path_from_utf8(argv[2]);
    bool ok=false;
    if(name=="walking") ok=walking(root); else if(name=="jumping") ok=jumping(root); else if(name=="collisions") ok=collisions(root);
    else if(name=="support") ok=support(root); else if(name=="boundaries") ok=boundaries(root); else if(name=="lifetime") ok=lifetime(root);
    else if(name=="world_collision") ok=world_collision(root);
    else if(name=="timing") ok=timing(root);
    else if(name=="controls") ok=controls(root); else if(name=="private") ok=private_world(root);
    return ok ? 0 : 1;
}
