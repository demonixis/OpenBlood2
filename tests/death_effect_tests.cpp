#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Corpse.h"
#include "game/original/Gib.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(GameSession& g, const char* root, bool initialize = true)
    {
        if (initialize)
            CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    AI_Mgr* actor(GameSession& g, const char* type = "CultistAI", DVector position = {0, 100, 0})
    {
        auto h = g.objects.create(
            g.objects.find_class(type), {}, Creation::World,
            {{"Pos", position}, {"AIState", std::string("IDLE")}, {"AIWeapon1", std::string("BERETTA")}});
        auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
        if (!a || !a->RuntimeReady() || !a->m_MoveObj.Update())
            return nullptr;
        return a;
    }
    bool step(GameSession& g, double dt)
    {
        CHECK(g.server.advance_models(dt) && g.server.advance_physics(dt) && g.objects.advance(dt));
        CHECK(g.server.object_particles.advance(g.objects.time(), g.server));
        CHECK(g.server.flesh_gibs.advance(g.objects.time(), g.server));
        CHECK(g.server.advance_audio(dt, {}, {}));
        return true;
    }
    bool hit(GameSession& g, HOBJECT owner, Damage damage)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(owner), owner, damage_message);
        CHECK(m && g.server.WriteDamage(m, damage) == DE_OK && g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool burn(const char* root)
    {
        for (auto type : {"CultistAI", "CivilianAI"})
        {
            GameSession g;
            CHECK(setup(g, root));
            auto* a = actor(g, type);
            CHECK(a);
            const auto source = a->m_hObject;
            a->m_dwFlags |= 0x800; // NEVERGIB does not suppress burning.
            a->m_damage.SetArmorPoints(0);
            CHECK(hit(g, source, {{0, 0, 1}, {0, 100, 0}, 500, DamageType::Fire, source, true}));
            CHECK(a->m_damage.IsDead() && a->fire_source && !a->runtime_failed && !a->m_bRemoveMe);
            const auto flame = a->fire_source;
            CHECK(a->AI_Update() && a->AI_Update() && a->last_corpse);
            const auto h = a->last_corpse;
            auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(h));
            CHECK(c && c->fire_effect && c->burn_sound && c->burn_attachment && c->smoke_source && !a->fire_source);
            const auto smoke = c->smoke_source;
            const auto* record = g.objects.get(h);
            CHECK(std::string_view(record->state.m_SkinName) == "skins\\enemies\\burned.dtx");
            CHECK(!(record->state.m_Flags & FLAG_SOLID));
            CHECK(g.objects.remove(source) && g.objects.finish_frame() && g.server.IsObjectAlive(flame));
            std::size_t peak = 0;
            while (g.objects.time() < 4.95)
            {
                CHECK(step(g, .01));
                peak = std::max(peak, g.server.object_particles.particle_count());
            }
            CHECK(c->fire_source == flame && !c->smoke_start && c->burn_sound && peak > 0);
            while (g.objects.time() < 5.2)
                CHECK(step(g, .01));
            CHECK(!c->fire_source && c->smoke_start > 5 && c->smoke_effect && !c->burn_sound &&
                  !g.server.IsObjectAlive(flame));
            const auto smoke_start = c->smoke_start;
            while (g.objects.time() < smoke_start + 2.2)
                CHECK(step(g, .01));
            CHECK(g.server.object_particles.active.empty() && g.server.IsObjectAlive(smoke)); // FX lasts 2s, source 5s.
            while (g.objects.time() < smoke_start + 5.2)
                CHECK(step(g, .01));
            CHECK(!c->smoke_source && !c->burn_attachment && c->animation_done && c->error.empty() &&
                  c->burn_error.empty());
            CHECK(!g.server.IsObjectAlive(smoke));
            std::cout << type << " burned: particles=" << peak << " smoke start=" << smoke_start
                      << " independent corpse complete\n";
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.attachments().empty());
        }
        return true;
    }
    bool gibs(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* a = actor(g);
            CHECK(a);
            const auto h = a->m_hObject;
            a->m_damage.SetArmorPoints(0);
            CHECK(hit(g, h,
                      {{0, 0, 1}, {0, 100, 0}, 500, cycle == 0 ? DamageType::Explode : DamageType::Normal, h, true}));
            CHECK(a->m_damage.IsDead() && a->m_bRemoveMe && !a->runtime_failed && a->gib_events == 1 &&
                  a->gib_error.empty());
            const auto count = g.server.flesh_gibs.active.size();
            CHECK(count >= 7);
            const auto initial_position = g.server.flesh_gibs.active.front().position;
            CHECK(g.server.flesh_gibs.advance(g.objects.time(), g.server)); // Publication frame has zero elapsed time.
            CHECK(g.server.flesh_gibs.active.front().position.x == initial_position.x &&
                  g.server.flesh_gibs.active.front().position.y == initial_position.y);
            CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && g.objects.finish_frame() && !g.objects.get(h));
            CHECK(g.server.flesh_gibs.active.size() == count);
            for (int i = 0; i < 400; ++i)
                CHECK(step(g, .02));
            unsigned impacts = 0;
            for (const auto& f : g.server.flesh_gibs.active)
            {
                CHECK(f.error.empty() && f.sound_error.empty());
                impacts += f.impacts;
            }
            CHECK(impacts > 0);
            const auto id = g.server.flesh_gibs.active.back().id;
            std::string error;
            CHECK(!g.server.PublishFleshGibs(nullptr, {}, {}, {1, 1, 1}, 1, 2, error) && !error.empty());
            CHECK(g.unload_world() && g.server.flesh_gibs.active.empty() && g.server.object_particles.active.empty());
            std::cout << "gib cycle=" << cycle << " count=" << count << " impacts=" << impacts << " final id=" << id
                      << "\n";
            CHECK(setup(g, root, false));
        }
        return true;
    }
    bool limbs(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        std::string error;
        std::vector<HOBJECT> limbs;
        for (int node : {1, 4, 7, 10, 14})
        {
            auto h = CreateLimb(a->m_hObject, node, {0, 0, 100}, error);
            if (!h)
                std::cerr << error << "\n";
            CHECK(h);
            limbs.push_back(h);
            auto* limb = dynamic_cast<CGib*>(g.server.HandleToObject(h));
            CHECK(limb && limb->initialized && placement_succeeded(limb->placement));
            CHECK(limb->kickable == (node == 1) && !(g.server.GetObjectFlags(h) & FLAG_SOLID));
            const auto* r = g.objects.get(h);
            CHECK(r->model && std::count(r->hidden_nodes.begin(), r->hidden_nodes.end(), 1) > 0);
            DBOOL already = false;
            CHECK(g.server.GetModelNodeHideStatus(a->m_hObject, szNodes[node], &already) == DE_OK);
            CHECK(HideLimb(a->m_hObject, node, error) == !already); // Original arm +3 may already hide its neighbor.
            CHECK(!HideLimb(a->m_hObject, node, error));
        }
        a->m_damage.node_hit = 2;
        CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState() && a->DispatchState());
        auto corpse = a->last_corpse;
        auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(corpse));
        CHECK(c);
        for (int node : {1, 4, 7, 10, 14})
        {
            DBOOL hidden = false;
            CHECK(g.server.GetModelNodeHideStatus(corpse, szNodes[node], &hidden) == DE_OK && hidden);
        }
        const auto owner = a->m_hObject;
        CHECK(g.objects.remove(owner) && g.objects.finish_frame());
        CHECK(!g.server.IsObjectAlive(owner) && g.server.IsObjectAlive(corpse));
        for (auto limb : limbs)
            CHECK(g.server.IsObjectAlive(limb));
        CHECK(hit(g, corpse, {{0, 0, 1}, {0, 100, 0}, 100, DamageType::Explode, corpse, true}));
        CHECK(c->dead && c->gib_events == 1 &&
              c->limbs_created == 0); // Hidden limbs never duplicate on corpse gibbing.
        CHECK(step(g, .2) && !g.server.IsObjectAlive(corpse));
        CHECK(g.unload_world() && g.objects.attachments().empty());
        return true;
    }
    bool damage(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = actor(g);
        CHECK(a);
        a->m_damage.node_hit = 1;
        CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState() && a->DispatchState());
        const auto h = a->last_corpse;
        auto* c = dynamic_cast<CCorpse*>(g.server.HandleToObject(h));
        CHECK(c);
        for (int i = 0; i < 20 && !c->limbs_created; ++i)
        {
            DVector head;
            DRotation rotation;
            CHECK(g.server.GetModelNodeTransform(h, "head", &head, &rotation));
            head.z += 1;
            CHECK(hit(g, h, {{0, 0, 1}, head, 1, DamageType::Normal, h}));
        }
        CHECK(c->limbs_created == 1 && c->hit_limb == 1 && !c->dead && c->m_damage.m_fHitPoints < 50);
        CHECK(g.server.IsObjectAlive(c->last_limb));
        DBOOL hidden = false;
        CHECK(g.server.GetModelNodeHideStatus(h, "head", &hidden) == DE_OK && hidden);
        CHECK(hit(g, h, {{0, 0, 1}, {0, 100, 0}, 100, DamageType::Explode, h, true}));
        CHECK(c->dead && c->gib_events == 1 && c->gib_error.empty());
        CHECK(step(g, .2) && !g.server.IsObjectAlive(h));
        CHECK(g.unload_world());
        return true;
    }
    bool particles(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        std::minstd_rand rng(17);
        NodePose pose{{0, 100, 0}, {}};
        CObjectParticleFX fire, smoke;
        CHECK(fire.Init(24, 1, rng, error) && smoke.Init(21, 2, rng, error));
        CHECK(fire.authored_count == 15 && smoke.authored_count == 9 && fire.duration == 0 && smoke.duration == 2);
        CHECK(fire.CreateObject(g.resources, pose, 10, error) && smoke.CreateObject(g.resources, pose, 10, error));
        CHECK(fire.Update(10, &pose, rng) && smoke.Update(10, &pose, rng));
        CHECK(fire.Update(10.11, &pose, rng) && smoke.Update(10.11, &pose, rng));
        CHECK(fire.particles.size() == 15 && smoke.particles.empty() && fire.alpha == 1 && smoke.alpha == 1);
        pose.position.x += 26;
        CHECK(smoke.Update(10.22, &pose, rng) && smoke.particles.size() == 9 && smoke.origin.x == 0);
        CHECK(fire.Update(10.22, &pose, rng) && fire.origin.x == 0);
        const auto old = smoke.emitted;
        CHECK(smoke.Update(10.33, &pose, rng) && smoke.emitted == old);
        CHECK(smoke.Update(12, &pose, rng) && !smoke.Update(12.001, &pose, rng) && smoke.error.empty());
        CHECK(!fire.Update(12.001, nullptr, rng) && fire.error.empty());
        CHECK(g.unload_world());
        return true;
    }
    bool lifetime(const char* root)
    {
        for (int phase = 0; phase < 3; ++phase)
        {
            GameSession g;
            CHECK(setup(g, root));
            auto* a = actor(g);
            CHECK(a);
            const auto h = a->m_hObject;
            CHECK(a->StartBurnDeath());
            const auto fire = a->fire_source;
            if (phase)
            {
                a->m_damage.reaction_damage_type = DamageType::Fire;
                CHECK(a->SetNewState(AI_Mgr::StateDying) && a->DispatchState() && a->DispatchState());
                auto c = a->last_corpse;
                auto* body = dynamic_cast<CCorpse*>(g.server.HandleToObject(c));
                CHECK(body);
                if (phase == 2)
                    for (int i = 0; i < 270; ++i)
                        CHECK(step(g, .02));
                CHECK(g.objects.remove(c) && g.objects.finish_frame());
            }
            CHECK(g.objects.remove(h) || !g.server.IsObjectAlive(h));
            CHECK(g.objects.finish_frame());
            CHECK(!g.server.IsObjectAlive(fire) && g.objects.attachments().empty());
            CHECK(g.server.object_particles.advance(g.objects.time(), g.server) &&
                  g.server.object_particles.active.empty());
            CHECK(g.server.advance_audio(.01, {}, {}));
            CHECK(g.unload_world() && g.server.audio.voices().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "burn")
        return burn(argv[2]) ? 0 : 1;
    if (mode == "gibs")
        return gibs(argv[2]) ? 0 : 1;
    if (mode == "limbs")
        return limbs(argv[2]) ? 0 : 1;
    if (mode == "damage")
        return damage(argv[2]) ? 0 : 1;
    if (mode == "particles")
        return particles(argv[2]) ? 0 : 1;
    if (mode == "lifetime")
        return lifetime(argv[2]) ? 0 : 1;
    return 2;
}
