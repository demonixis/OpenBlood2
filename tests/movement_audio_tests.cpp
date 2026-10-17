#include "audio_fixture.h"
#include "brush_fixture.h"
#include "engine/collision.h"
#include <cmath>
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
    bool floor(GameSession& g)
    {
        WorldModel m;
        m.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        m.surfaces.resize(1);
        m.surfaces[0].flags = 1;
        m.surfaces[0].texture_flags = SURFTYPE_STONE;
        m.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(m, error));
        g.server.collision_ready = true;
        g.world_path = "diagnostic-movement-audio.dat";
        g.state = SessionState::Inspection;
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(g.enter_player(error));
        for (int i = 0; i < 90; ++i)
            CHECK(g.step(1. / 60));
        return true;
    }
    bool setup(GameSession& g, const std::filesystem::path& root, bool landing_files = true)
    {
        CHECK(test_audio::setup(g, root));
        test_player::Directory d;
        d.children["sounds"].children["chosen"].children["caleb"].children["jump"].files["jump.wav"] =
            test_audio::wave();
        if (landing_files)
            for (auto material : {"wood", "flesh", "glass", "metal", "plastic", "stone", "terrain", "water", "carpet"})
                d.children["sounds"].children["player"].children["footsteps"].children[material].files["landm.wav"] =
                    test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "movement-audio.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        CHECK(g.resources.mount(root / "movement-audio.rez", error));
        return floor(g);
    }
    bool rules(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        const auto start = p->landing_requests;
        auto arm = [&](float distance)
        {
            p->m_bFalling = true;
            p->m_startFall = 0;
            p->m_fMinFallPos = 0;
            p->m_fMaxFallPos = distance;
        };
        arm(56);
        CHECK(p->UpdateLandingSound() && p->landing_requests == start && !p->m_bFalling && p->m_startFall == -1);
        arm(56.01f);
        CHECK(p->UpdateLandingSound() && p->landing_requests == start + 1);
        auto* voice = g.server.audio.get(p->m_hCurSound);
        CHECK(voice && voice->request.resource == "sounds/player/footsteps/stone/landm.wav" &&
              voice->request.outer_radius == 200 && voice->request.inner_radius == 20 && voice->request.volume == .7f &&
              voice->request.priority == 4 && !voice->request.streaming && voice->request.speaker == p->m_hObject);
        const auto handle = p->m_hCurSound, alerts = p->sound_alerts;
        CHECK(p->UpdateLandingSound() && p->m_hCurSound == handle && p->sound_alerts == alerts);
        arm(100);
        p->m_bPowerupActivated[PU_INCORPOREAL] = true;
        CHECK(p->UpdateLandingSound() && p->landing_requests == start + 1);
        p->m_bPowerupActivated[PU_INCORPOREAL] = false;
        arm(100);
        p->movement_blocked = true;
        CHECK(p->UpdateLandingSound() && p->m_bFalling && p->m_startFall == 0);
        p->movement_blocked = false;
        p->m_damage.m_bDead = true;
        CHECK(p->UpdateLandingSound() && p->m_bFalling && p->landing_requests == start + 1);
        p->m_damage.m_bDead = false;
        // Unmapped surface never interrupts an already playing character sound or flips a foot.
        p->m_eLastSurface = SURFTYPE_UNKNOWN;
        CHECK(p->UpdateLandingSound() && p->landing_requests == start + 2 && p->m_hCurSound == handle &&
              !p->landing_error.empty());
        std::string path = "unchanged";
        const bool left = p->m_bNextFootstepLeft;
        for (auto [surface, material] : std::vector<std::pair<SurfaceType, std::string>>{{SURFTYPE_WOOD, "wood"},
                                                                                         {SURFTYPE_FLESH, "flesh"},
                                                                                         {SURFTYPE_GLASS, "glass"},
                                                                                         {SURFTYPE_METAL, "metal"},
                                                                                         {SURFTYPE_PLASTIC, "plastic"},
                                                                                         {SURFTYPE_STONE, "stone"},
                                                                                         {SURFTYPE_TERRAIN, "terrain"},
                                                                                         {SURFTYPE_LIQUID, "water"},
                                                                                         {SURFTYPE_CLOTH, "carpet"}})
        {
            p->m_eLastSurface = surface;
            CHECK(p->GetLandingSound(path) && path == "sounds/player/footsteps/" + material + "/landm.wav" &&
                  p->m_bNextFootstepLeft == left);
        }
        const auto* r = g.objects.get(p->m_hObject);
        auto foot = r->state.m_Pos;
        foot.y -= r->dimensions.y - 1;
        g.world.models.push_back(
            test_brush::box("wet", {foot.x - 20, foot.y - 2, foot.z - 20}, {foot.x + 20, foot.y + 2, foot.z + 20}));
        auto water = g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                                      {{"Name", std::string("wet")}, {"Hidden", true}});
        CHECK(water);
        p->m_eLastSurface = SURFTYPE_METAL;
        CHECK(p->GetLandingSound(path) && path == "sounds/player/footsteps/water/landm.wav");
        g.server.RemoveObject(water);
        CHECK(!g.server.IsObjectAlive(water));
        CHECK(p->GetLandingSound(path) && path == "sounds/player/footsteps/metal/landm.wav");
        CHECK(g.objects.finish_frame());
        // A real head-in-water volume suppresses landing and resets the source tracker.
        g.world.models.push_back(test_brush::box("deep", {-100, -100, -100}, {100, 300, 100}));
        water = g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                                 {{"Name", std::string("deep")}, {"ShowSurface", false}});
        CHECK(water);
        arm(100);
        const auto contacts = p->landing_contacts;
        CHECK(p->UpdateLandingSound() && p->landing_contacts == contacts && p->m_startFall == -1 && !p->m_bFalling);
        g.server.RemoveObject(water);
        CHECK(g.objects.finish_frame());
        CHECK(g.leave_player());
        CHECK(!g.server.audio.get(handle));
        return true;
    }
    bool jumps(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& c = g.client_shell;
        c.movement_input.jump = true;
        CHECK(g.step(1. / 60));
        CHECK(c.move_mgr.jump_voice_requests == 1 && c.move_mgr.jump_voice_starts == 1);
        auto id = c.voice_manager.voice;
        auto* voice = g.server.audio.get(id);
        CHECK(voice && !voice->request.streaming && !voice->clip->stream && voice->request.priority == 5);
        for (int i = 0; i < 100; ++i)
            CHECK(g.step(1. / 60));
        CHECK(c.move_mgr.jumps == 1 && c.move_mgr.jump_voice_requests == 1);
        c.movement_input.jump = false;
        CHECK(g.step(1. / 60));
        // Camera gate refuses audio while a permitted movement branch can still jump.
        CHECK(c.move_mgr.Update(g.server_shell, c.voice_manager, true, {.jump = true}, 0, 1. / 60, true));
        CHECK(c.move_mgr.jump_voice_requests == 2 && c.move_mgr.jump_voice_starts == 1 &&
              c.voice_manager.reason == "external camera");
        const auto requests = c.move_mgr.jump_voice_requests;
        for (int i = 0; i < 5; ++i)
            CHECK(g.step(1. / 60));
        c.movement_input.jump = true;
        CHECK(g.step(1. / 60));
        CHECK(c.move_mgr.jump_voice_requests == requests); // no midair impulse or voice
        CHECK(g.leave_player() && c.move_mgr.jump_voice_requests == 0 && !g.server.audio.get(id));
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool contexts(const std::filesystem::path& root)
    {
        for (int mode = 0; mode < 4; ++mode)
        {
            GameSession g;
            CHECK(setup(g, root / std::to_string(mode)));
            auto& c = g.client_shell;
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            const auto base = p->landing_requests;
            if (mode == 3)
            {
                ObjectCreateStruct state;
                state.m_Pos = {0, 110, 0};
                state.m_Flags = FLAG_SOLID;
                auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
                DVector dims{100, 1, 100};
                CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
            }
            else
            {
                const float top = 300;
                g.world.models.push_back(test_brush::box("medium", {-100, -100, -100}, {100, top, 100}));
                auto h = g.objects.create(g.objects.find_class(mode == 1 ? "Ladder" : "Water"), {}, Creation::World,
                                          {{"Name", std::string("medium")}, {"ShowSurface", false}});
                CHECK(h);
            }
            if (mode == 0)
            {
                DVector position{0, 250, 0}, zero{};
                CHECK(g.server.TeleportObject(c.player, &position) == DE_OK &&
                      g.server.SetVelocity(c.player, &zero) == DE_OK);
                CHECK(g.step(1. / 60) && c.move_mgr.m_bCanSwimJump);
                position.y = 280;
                CHECK(g.server.TeleportObject(c.player, &position) == DE_OK &&
                      g.server.SetVelocity(c.player, &zero) == DE_OK);
                CHECK(g.step(1. / 60) && c.move_mgr.m_bSwimmingOnSurface);
            }
            c.movement_input.jump = true;
            for (int i = 0; i < 90; ++i)
                CHECK(g.step(1. / 60));
            std::cout << "context=" << mode << " dry voices=" << c.move_mgr.jump_voice_requests
                      << " jumps=" << c.move_mgr.jumps << " liquid=" << c.move_mgr.m_bBodyInLiquid
                      << " ladder=" << c.move_mgr.m_bBodyOnLadder << std::endl;
            CHECK(c.move_mgr.jump_voice_requests == (mode == 3 ? 1 : 0));
            CHECK(p->landing_requests == base);
            if (mode == 0)
                CHECK(c.move_mgr.jumps == 1); // Actual swimming jump, no dry-ground voice.
            if (mode == 3)
                CHECK(c.move_mgr.landings >= 1 && p->landing_distance < 56);
            CHECK(g.unload_world() && g.server.audio.voices().empty());
        }
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p);
        CHECK(p->PlayPlayerSound("long.wav", 200, 70, false, 0));
        const auto old = p->m_hCurSound, alerts = p->sound_alerts, starts = p->landing_starts;
        p->m_bFalling = true;
        p->m_startFall = 0;
        p->m_fMinFallPos = 0;
        p->m_fMaxFallPos = 100;
        CHECK(p->UpdateLandingSound() && !p->m_hCurSound && !g.server.audio.get(old));
        CHECK(p->landing_starts == starts && !p->landing_error.empty() && p->sound_alerts == alerts + 1 &&
              p->sound_alert.complete);
        CHECK(p->UpdateLandingSound() && p->sound_alerts == alerts + 1);
        auto& c = g.client_shell;
        c.voice_manager.groups[0][11].sounds = {"missing-jump.wav"};
        c.movement_input.jump = true;
        CHECK(g.step(1. / 60));
        CHECK(c.move_mgr.jumps == 1 && c.move_mgr.jump_voice_requests == 1 && !c.move_mgr.jump_voice_starts &&
              !c.voice_manager.error.empty());
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        for (int hz : {30, 60, 120})
        {
            GameSession g;
            std::string error;
            Config config;
            config.data_dir = root;
            CHECK(g.initialize() && g.resources.mount_game(config, error) && floor(g));
            auto& c = g.client_shell;
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p);
            const auto base_landings = p->landing_starts;
            c.movement_input.jump = true;
            bool heard_landing = false, jump_advanced = false, landing_advanced = false;
            SoundId jump_id = 0, land_id = 0;
            for (int i = 0; i < 2 * hz; ++i)
            {
                CHECK(g.step(1. / hz));
                if (!jump_id && c.voice_manager.voice)
                {
                    jump_id = c.voice_manager.voice;
                    const auto* v = g.server.audio.get(jump_id);
                    CHECK(v && v->clip && !v->clip->stream && v->clip->frames() > 0);
                }
                if (const auto* v = g.server.audio.get(jump_id))
                    jump_advanced |= v->cursor > 0;
                if (p->landing_starts > base_landings)
                {
                    heard_landing = true;
                    if (p->m_hCurSound)
                        land_id = p->m_hCurSound;
                    if (const auto* v = g.server.audio.get(land_id))
                    {
                        CHECK(v->request.resource == "sounds/player/footsteps/stone/landm.wav" && v->clip &&
                              v->clip->frames() > 0);
                        landing_advanced |= v->cursor > 0;
                    }
                }
            }
            CHECK(jump_id && land_id && jump_advanced && landing_advanced && heard_landing && c.move_mgr.jumps == 1 &&
                  c.move_mgr.jump_voice_requests == 1 && c.move_mgr.jump_voice_starts == 1);
            CHECK(p->landing_starts == base_landings + 1 && p->landing_distance > 56 && p->landing_error.empty());
            CHECK(p->sound_alert.complete && !p->unavailable_sound_alerts);
            std::cout << hz << " Hz: one original jump voice / one landing; measured fall span=" << p->landing_distance
                      << "; resource=" << p->landing_resource << '\n';
            CHECK(g.leave_player() && !g.server.audio.get(jump_id) && !g.server.audio.get(land_id));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.record_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    return (mode == "rules"      ? rules(argv[2])
            : mode == "jumps"    ? jumps(argv[2])
            : mode == "contexts" ? contexts(argv[2])
            : mode == "failures" ? failures(argv[2])
                                 : mode == "private" && private_data(argv[2]))
               ? 0
               : 1;
}
