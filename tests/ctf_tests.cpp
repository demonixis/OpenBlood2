#include "game/original/FlagObjects.h"
#include "game/original/NewStatusBar.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(std::unique_ptr<GameSession>& g, const char* root, const char* world = "multi/ctf_cabana")
    {
        g = std::make_unique<GameSession>();
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g->initialize() && g->resources.mount_game(config, error));
        if (!GameSession::start_new_game(g, world, 1, 0, error, true))
        {
            std::cerr << error << '\n';
            return false;
        }
        CHECK(g->pump_player_messages(error));
        return true;
    }
    FlagStand* stand(GameSession& g, int team)
    {
        for (auto id : g.objects.snapshot())
            if (auto* f = dynamic_cast<FlagStand*>(g.objects.get(id)->game.get()); f && f->team == team)
                return f;
        return nullptr;
    }
    bool pump(GameSession& g)
    {
        std::string error;
        CHECK(g.pump_player_messages(error));
        if (!g.client_shell.ctf.error.empty())
            std::cerr << g.client_shell.ctf.error << '\n';
        CHECK(g.client_shell.ctf.error.empty());
        return true;
    }
    bool profiles(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        for (int i : {0, 1})
        {
            auto* f = g_ctf.flags[i];
            auto* b = stand(*g, i + 1);
            CHECK(f && b && b->flag == f->m_hObject && f->in_stand && !f->on_ground && !f->with_player);
            auto* r = g->objects.get(f->m_hObject);
            auto* br = g->objects.get(b->m_hObject);
            CHECK(r->model && br->model && r->state.m_Flags & FLAG_TOUCH_NOTIFY && !(r->state.m_Flags & FLAG_GRAVITY));
            std::cout << "team " << i + 1 << " stand=" << br->state.m_Pos.x << "," << br->state.m_Pos.y << ","
                      << br->state.m_Pos.z << " flag=" << r->state.m_Pos.x << "," << r->state.m_Pos.y << ","
                      << r->state.m_Pos.z << " dims=" << r->dimensions.y << "\n";
            CHECK(std::abs(r->state.m_Pos.x - br->state.m_Pos.x + 16) < .01 &&
                  std::abs(r->state.m_Pos.y - br->state.m_Pos.y - 32) < .01);
            CHECK(f->IsOtherFlagInStand() && f->error.empty() && b->error.empty());
        }
        ObjectCreateStruct c;
        CHECK(!g->server.CreateObjectProps(g->server.GetClass("FlagObject"), &c, "TeamID 0"));
        CHECK(g->unload_world() && !g_ctf.flags[0] && !g_ctf.flags[1] && g_ctf.team_scores[0] == 0);
        return true;
    }
    bool carry(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        auto* own = g_ctf.flags[0];
        p->GrabFlag(own->m_hObject);
        CHECK(!p->m_hFlagObject);
        p->m_bSpectatorMode = true;
        p->GrabFlag(f->m_hObject);
        CHECK(!p->m_hFlagObject);
        p->m_bSpectatorMode = false;
        // Exercise actual touch -> owned trigger -> original player command.
        f->EngineMessageFn(MID_TOUCHNOTIFY, p->m_hObject, 0);
        CHECK(p->m_hFlagObject == f->m_hObject && p->m_hFlagAttach && f->with_player && !f->in_stand);
        g->server.SetObjectFlags(f->m_hObject, g->server.GetObjectFlags(f->m_hObject) | FLAG_VISIBLE);
        CHECK(g->server.GetObjectFlags(f->m_hObject) & FLAG_TOUCH_NOTIFY);
        CHECK(g->objects.refresh_attachments());
        CHECK(std::abs(g->objects.get(f->m_hObject)->state.m_Pos.y - g->objects.get(p->m_hObject)->state.m_Pos.y - 75) <
              .01);
        p->GrabFlag(f->m_hObject);
        CHECK(f->grabs == 1 && pump(*g));
        CHECK(g->client_shell.ctf.flag_icon == 1 && g->client_shell.ctf.notice_resource == 7000 &&
              g->client_shell.ctf.voice);
        const auto* voice = g->server.audio.get(g->client_shell.ctf.voice);
        CHECK(voice && voice->request.priority == 1 && !voice->request.reverb_requested &&
              voice->request.space == SoundSpace::Local);
        CHECK(p->ctf_error.empty());
        p->DropFlag();
        CHECK(!p->m_hFlagObject && !p->m_hFlagAttach && f->on_ground && !f->with_player && pump(*g));
        CHECK(!g->client_shell.ctf.flag_icon && f->error.empty());
        const auto due = g->objects.get(f->m_hObject)->due;
        CHECK(due > g->objects.time());
        CHECK(g->objects.advance(29) && f->on_ground);
        CHECK(g->objects.advance(1.01) && f->in_stand && f->returns == 1 && f->voice && f->sound_error.empty());
        return g->unload_world();
    }
    bool capture(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        auto* own = g_ctf.flags[0];
        auto* base = stand(*g, 1);
        p->GrabFlag(f->m_hObject);
        CHECK(p->m_hFlagAttach);
        p->GiveFlag(stand(*g, 2)->m_hObject);
        CHECK(!p->ctf_score && p->m_hFlagAttach);
        own->GiveToPlayer(nullptr);
        CHECK(g->objects.advance(4) && pump(*g));
        const auto before = g->client_shell.ctf.messages;
        p->GiveFlag(base->m_hObject);
        CHECK(pump(*g) && g->client_shell.ctf.messages == before);
        CHECK(g->objects.advance(.01));
        p->GiveFlag(base->m_hObject);
        CHECK(pump(*g) && g->client_shell.ctf.messages == before + 1 && g->client_shell.ctf.notices == 1);
        CHECK(g->objects.advance(20));
        p->GiveFlag(base->m_hObject);
        CHECK(pump(*g) && g->client_shell.ctf.notice_resource == 7009);
        CHECK(own->ReturnToFlagStand());
        base->EngineMessageFn(MID_TOUCHNOTIFY, p->m_hObject, 0);
        CHECK(p->ctf_score == 5 && g_ctf.team_scores[0] == 5 && !p->m_hFlagObject && f->in_stand && pump(*g));
        CHECK(g->client_shell.ctf.score == 5 && g->client_shell.ctf.notice_resource == 7005 &&
              !g->client_shell.ctf.flag_icon && p->ctf_error.empty());
        // Source minimum multiplier remains one with no opposing connected player.
        g_ctf.use_team_size = true;
        g_ctf.flag_value = 7;
        p->GrabFlag(f->m_hObject);
        p->GiveFlag(base->m_hObject);
        CHECK(p->ctf_score == 12 && g_ctf.team_scores[0] == 12);
        return g->unload_world();
    }
    bool lifetime(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        auto* b = stand(*g, 2);
        CHECK(g->objects.remove(b->m_hObject) && g->objects.finish_frame() && g->server.IsObjectAlive(f->m_hObject));
        p->GrabFlag(f->m_hObject);
        CHECK(p->m_hFlagAttach);
        PlayerState save;
        std::string error;
        CHECK(!p->SaveFields(save, error) && error.find("CTF") != std::string::npos);
        CHECK(g->objects.remove(f->m_hObject) && g->objects.finish_frame() && !g_ctf.flags[1] && !p->m_hFlagObject &&
              !p->m_hFlagAttach && pump(*g));
        auto* own = g_ctf.flags[0];
        const auto pos = g->objects.get(own->m_hObject)->state.m_Pos;
        ObjectCreateStruct c;
        c.m_Pos = pos;
        auto* newer = g->server.CreateObjectProps(g->server.GetClass("FlagObject"), &c, "TeamID 1");
        CHECK(newer && g_ctf.flags[0] == newer);
        CHECK(g->objects.remove(newer->m_hObject) && g->objects.finish_frame() && !g_ctf.flags[0] &&
              g->server.IsObjectAlive(own->m_hObject));
        return g->unload_world();
    }
    bool owner(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        p->GrabFlag(f->m_hObject);
        CHECK(p->m_hFlagAttach);
        CHECK(g->objects.remove(p->m_hObject) && g->objects.finish_frame());
        CHECK(f->in_stand && !f->with_player && !g->objects.get(f->m_hObject)->attachment && f->error.empty());
        return g->unload_world();
    }
    bool death(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        p->GrabFlag(f->m_hObject);
        CHECK(p->m_hFlagAttach);
        // Original drops even when the null-attacker guard postpones death.
        p->m_damage.m_fHitPoints = 0;
        p->m_damage.m_bDead = true;
        p->HandleDeath();
        CHECK(!p->m_hFlagObject && f->on_ground && !p->m_bDead);
        CHECK(pump(*g) && !g->client_shell.ctf.flag_icon);
        return g->unload_world();
    }
    bool contact(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        auto* f = g_ctf.flags[1];
        auto start = f->stand_position;
        start.x -= 100;
        start.y += 35; // Cross above the authored solid pedestal, as during a jump.
        CHECK(g->server.TeleportObject(p->m_hObject, &start) == DE_OK);
        DVector end;
        CHECK(g->server.GetObjectPos(p->m_hObject, &end) == DE_OK);
        end.x += 200;
        CHECK(g->server.MoveObject(p->m_hObject, &end) == DE_OK);
        auto* pr = g->objects.get(p->m_hObject);
        auto* fr = g->objects.get(f->m_hObject);
        std::cout << "Contact: start " << start.x << ',' << start.y << ',' << start.z << " end " << end.x << ','
                  << end.y << ',' << end.z << " actual " << pr->state.m_Pos.x << ',' << pr->state.m_Pos.y << ','
                  << pr->state.m_Pos.z << " flag " << fr->state.m_Pos.x << ',' << fr->state.m_Pos.y << ','
                  << fr->state.m_Pos.z << " dims " << fr->dimensions.x << ',' << fr->dimensions.y << ','
                  << fr->dimensions.z << " grabs " << f->grabs << " trigger " << p->last_player_trigger << " error "
                  << p->ctf_error << '\n';
        CHECK(p->m_hFlagObject == f->m_hObject && f->with_player && f->grabs == 1 && pump(*g));
        CHECK(p->ctf_error.empty());
        return g->unload_world();
    }
    bool replacement(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        auto* p = g->server_shell.Player(g->local_client);
        CHECK(p && p->SetCTFTeam(1));
        p->GrabFlag(g_ctf.flags[1]->m_hObject);
        p->GiveFlag(stand(*g, 1)->m_hObject);
        CHECK(g_ctf.team_scores[0] == 5);
        p->GrabFlag(g_ctf.flags[1]->m_hObject);
        CHECK(p->m_hFlagAttach);
        std::string error;
        CHECK(GameSession::start_new_game(g, "multi/ctf_jam", 2, 0, error, true));
        CHECK(g->world_path == "worlds/multi/ctf_jam.dat" && g_ctf.team_scores[0] == 0 && g_ctf.last_need_flag == 0);
        for (const auto* f : g_ctf.flags)
            CHECK(f && g->server.IsObjectAlive(f->m_hObject) && f->in_stand);
        CHECK(g->step(.01) && g->run_world(error));
        return g->unload_world();
    }
    bool hud(const char* root)
    {
        std::unique_ptr<GameSession> g;
        CHECK(setup(g, root));
        CNewStatusBar bar(g->resources);
        CHECK(bar.Init() && bar.UpdateCTF(1, 1));
        CHECK(bar.team_icon && bar.flag_icon && bar.team_icon->width <= 128 && bar.flag_icon->height <= 128);
        auto first = bar.flag_icon;
        auto revision = bar.revision;
        CHECK(bar.UpdateCTF(1, 1) && bar.revision == revision);
        CHECK(bar.UpdateCTF(2, 2) && bar.flag_icon != first && bar.team_id == 2);
        CHECK(bar.UpdateCTF(0, 0) && !bar.flag_icon && !bar.team_icon);
        CHECK(!bar.UpdateCTF(3, 0));
        return g->unload_world();
    }
    bool worlds(const char* root)
    {
        std::unique_ptr<GameSession> g;
        for (auto name : {"multi/ctf_cabana", "multi/ctf_deadcities", "multi/ctf_dmx", "multi/ctf_jam"})
        {
            CHECK(setup(g, root, name));
            for (int i = 0; i < 180; ++i)
                CHECK(g->step(1. / 60));
            std::string error;
            CHECK(g->run_world(error));
            CHECK(g_ctf.flags[0] && g_ctf.flags[1] && stand(*g, 1) && stand(*g, 2));
            CHECK(g->unload_world() && !g->server.message_count() && !g->server.string_count());
            g.reset();
            std::cout << name << " opened, simulated and unloaded\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    bool ok = mode == "profiles"      ? profiles(argv[2])
              : mode == "carry"       ? carry(argv[2])
              : mode == "capture"     ? capture(argv[2])
              : mode == "lifetime"    ? lifetime(argv[2])
              : mode == "owner"       ? owner(argv[2])
              : mode == "death"       ? death(argv[2])
              : mode == "contact"     ? contact(argv[2])
              : mode == "replacement" ? replacement(argv[2])
              : mode == "hud"         ? hud(argv[2])
              : mode == "worlds"      ? worlds(argv[2])
                                      : false;
    return ok ? 0 : 1;
}
