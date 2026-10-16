#include "audio_fixture.h"
#include "brush_fixture.h"
#include "camera_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/DestructableBrush.h"
#include "game/original/VolumeBrush.h"
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
    const std::vector<std::pair<SurfaceType, std::string>> materials{
        {SURFTYPE_STONE, "stone"},     {SURFTYPE_BUILDING, "stone"}, {SURFTYPE_METAL, "metal"},
        {SURFTYPE_WOOD, "wood"},       {SURFTYPE_ENERGY, "energy"},  {SURFTYPE_GLASS, "glass"},
        {SURFTYPE_TERRAIN, "terrain"}, {SURFTYPE_CLOTH, "cloth"},    {SURFTYPE_PLASTIC, "plastic"},
        {SURFTYPE_FLESH, "flesh"},     {SURFTYPE_LIQUID, "water"}};
    bool setup(GameSession& g, const std::filesystem::path& root, bool audio = true)
    {
        CHECK(test_player::setup(g, root));
        if (audio)
        {
            test_player::Directory dir;
            auto& weapons = dir.children["sounds"].children["weapons"];
            weapons.children["beretta"].files["fire.wav"] = test_audio::wave();
            weapons.children["knife"].files["fire.wav"] = test_audio::wave();
            for (auto& [type, material] : materials)
                for (unsigned i = 1; i <= 3; ++i)
                    weapons.children["impacts"].children[material].files["impact" + std::to_string(i) + ".wav"] =
                        test_audio::wave();
            test_model::Writer w;
            w.b.resize(168);
            w.b[0] = w.b[62] = w.b[124] = 13;
            w.b[1] = w.b[63] = w.b[125] = 10;
            w.b[126] = 26;
            w.patch(127, 1);
            auto [offset, size] = test_player::write_dir(w, dir);
            w.patch(131, offset);
            w.patch(135, size);
            std::ofstream out(root / "impact-audio.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
            out.close();
            std::string error;
            CHECK(g.resources.mount(root / "impact-audio.rez", error));
        }
        std::string error;
        CHECK(g.enter_player(error));
        g.server.audio.inspect_pcm = true;
        return true;
    }
    WeaponFire shot(GameSession& g, DBYTE type = WEAP_BERETTA)
    {
        auto* p = g.server_shell.Player(g.local_client);
        auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
        pos.y += p->eye_level;
        return {pos, {0, 0, 1}, 42, type, false, false};
    }
    CWeapon* weapon(GameSession& g, unsigned slot = 1)
    {
        return g.server_shell.Player(g.local_client)->m_InventoryMgr.GetWeapon(slot);
    }
    bool walls(GameSession& g, const std::vector<unsigned>& surfaces, float spacing = 30)
    {
        WorldModel model;
        auto pos = shot(g).position;
        float z = pos.z + 30;
        for (auto surface : surfaces)
        {
            test_brush::face(model,
                             {{pos.x - 200, pos.y - 200, z},
                              {pos.x + 200, pos.y - 200, z},
                              {pos.x + 200, pos.y + 200, z},
                              {pos.x - 200, pos.y + 200, z}},
                             {0, 0, -1});
            model.surfaces.back().flags = 1;
            model.surfaces.back().texture_flags = surface;
            z += spacing;
        }
        std::string error;
        CHECK(g.collision.build(model, error));
        return true;
    }
    HOBJECT brush(GameSession& g, const char* type, float z, Properties props = {})
    {
        const auto pos = vector_add(shot(g).position, {0, 0, z});
        const auto name = "impact" + std::to_string(g.world.models.size());
        auto m = test_brush::box(name, vector_add(pos, {-20, -20, -5}), vector_add(pos, {20, 20, 5}));
        m.translation = (std::string(type) == "Water" || std::string(type) == "VolumeBrush") ? DVector{} : pos;
        g.world.models.push_back(m);
        props["Name"] = name;
        if (std::string(type) == "CDestructableBrush")
            props.try_emplace("HitPoints", 100.0f);
        if (std::string(type) == "Door")
            props.try_emplace("MoveDir", DVector{1, 0, 0});
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    struct Listener : AI_Mgr
    {
        unsigned action = 0, impacts = 0;
        HOBJECT victim = nullptr;
        bool damage_seen = false;
        DDWORD ObjectMessageFn(HOBJECT sender, DDWORD message, HMESSAGEREAD read) override
        {
            auto result = AI_Mgr::ObjectMessageFn(sender, message, read);
            if (message == 1000 && heard_sound == 1)
            {
                ++impacts;
                if (auto* b = dynamic_cast<CDestructableBrush*>(g_pServerDE->HandleToObject(victim)))
                    damage_seen = b->m_damage.m_fHitPoints < 100;
                if (action == 1)
                    g_pServerDE->RemoveObject(sender);
                if (action == 2)
                    g_pServerDE->RemoveObject(victim);
            }
            return result;
        }
    };
    Listener* listener(GameSession& g)
    {
        static ClassInfo info{
            "ImpactListener", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Listener>(); }, {}};
        info.parent = g.objects.find_class("CBaseCharacter");
        if (!g.objects.find_class(info.name) && !g.objects.register_class(info))
            return nullptr;
        ObjectCreateStruct state;
        state.m_Pos = vector_add(shot(g).position, {80, 0, 40});
        auto h = g.objects.create(&info, state);
        auto* a = dynamic_cast<Listener*>(g.server.HandleToObject(h));
        if (!a || !a->InsertMe())
            return nullptr;
        a->m_dwFlags = 8;
        a->m_nState = AI_Mgr::StateAttackFar;
        a->m_nLastState = AI_Mgr::StateIdle;
        return a;
    }
    bool selection(const std::filesystem::path&)
    {
        CWeaponFX fx;
        std::minstd_rand random(1);
        for (auto& [surface, material] : materials)
        {
            fx.m_eSurfaceType = surface;
            for (unsigned i = 0; i < 50; ++i)
            {
                fx.m_nAmmoType = i % 4;
                auto resource = fx.GetImpactSound(random);
                CHECK(resource.starts_with("Sounds/Weapons/impacts/" + material + "/impact"));
                CHECK(resource.ends_with("1.wav") || resource.ends_with("2.wav") || resource.ends_with("3.wav"));
            }
        }
        for (auto surface : {SURFTYPE_UNKNOWN, SURFTYPE_SKY, SURFTYPE_FIRETHROUGH})
        {
            fx.m_eSurfaceType = surface;
            auto before = random;
            CHECK(fx.GetImpactSound(random).empty() && random == before);
        }
        return true;
    }
    bool world(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CHECK(walls(g, {SURFTYPE_GLASS, SURFTYPE_METAL}));
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
        auto* w = weapon(g);
        CHECK(w->impacts.size() == 2 && w->impacts[0].continued && !w->impacts[1].continued && w->impact_sounds == 2);
        CHECK(w->impacts[0].surface == SURFTYPE_GLASS && w->impacts[1].hit.texture_flags == SURFTYPE_METAL);
        CHECK(!w->last_shot.damage_delivered &&
              g.server_shell.Player(g.local_client)->m_InventoryMgr.GetAmmoCount(1) == 49);
        const auto& contact = w->impacts[0];
        const auto* voice = g.server.audio.get(contact.effect.sound);
        CHECK(voice && voice->auto_remove && voice->world_owned && voice->clip->frames() == 800);
        CHECK(voice->request.outer_radius == 500 && voice->request.inner_radius == 50 && voice->request.priority == 0);
        CHECK(std::abs(voice->position.z - (contact.hit.point.z - 1.25f)) < .001f);
        CHECK(g.server.advance_audio(.05, shot(g).position, {}) && g.server.audio.peak > 0);
        std::string error;
        CHECK(g.select_weapon(0, error));
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g, WEAP_MELEE)) == FireResult::Fired);
        CHECK(weapon(g, 0)->impacts.size() == 1 && !weapon(g, 0)->impacts[0].continued);
        CHECK(g.select_weapon(1, error));
        CHECK(walls(g, {SURFTYPE_FIRETHROUGH, SURFTYPE_METAL}));
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
        CHECK(w->impacts.size() == 1 && w->impacts[0].branch == WeaponImpactBranch::Other &&
              w->impacts[0].effect.resource.empty());
        CHECK(walls(g, {SURFTYPE_SKY}));
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired &&
              w->impacts[0].effect.resource.empty());
        return true;
    }
    bool brushes(const std::filesystem::path& root)
    {
        for (bool box : {false, true})
        {
            GameSession g;
            CHECK(setup(g, root));
            auto h = brush(g, "CDestructableBrush", 30,
                           {{"SurfaceType", std::int32_t(SURFTYPE_GLASS)}, {"BoxPhysics", box}});
            auto target = brush(g, "CDestructableBrush", 65, {{"SurfaceType", std::int32_t(SURFTYPE_METAL)}});
            CHECK(h && target);
            auto* b = dynamic_cast<CDestructableBrush*>(g.server.HandleToObject(h));
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
            auto* w = weapon(g);
            CHECK(w->impacts.size() == 2 && w->impacts[0].hit.object == h && w->impacts[1].hit.object == target);
            CHECK(w->impacts[0].damage_delivered &&
                  std::abs(b->m_damage.m_fHitPoints - (100 - w->last_shot.damage)) < .001f);
            b->fire_through = true;
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
            CHECK(w->impacts.size() == 2 && !w->impacts[0].effect_requested && !w->impacts[0].damage_delivered);
            // Original ext.nSurface is captured before the brush-specific surface fallback.
            b->fire_through = false;
            g.server.SetObjectUserFlags(h, 0);
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
            CHECK(w->impacts.size() == 2 && w->impacts[0].surface == 0 &&
                  w->impacts[0].stopping_surface == SURFTYPE_GLASS && w->impacts[0].effect.resource.empty());
            b->fire_through = true;
            std::string error;
            CHECK(g.select_weapon(0, error));
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g, WEAP_MELEE)) == FireResult::Fired);
            CHECK(weapon(g, 0)->impacts.size() == 1 && weapon(g, 0)->impacts[0].branch == WeaponImpactBranch::Other &&
                  weapon(g, 0)->impacts[0].damage_delivered);
        }
        GameSession g;
        CHECK(setup(g, root));
        auto door = brush(g, "Door", 30, {{"FireThrough", true}});
        auto target = brush(g, "CDestructableBrush", 60, {{"SurfaceType", std::int32_t(SURFTYPE_METAL)}});
        CHECK(door && target);
        std::string error;
        CHECK(g.select_weapon(0, error));
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g, WEAP_MELEE)) == FireResult::Fired);
        CHECK(weapon(g, 0)->impacts.size() == 2 && weapon(g, 0)->impacts[0].branch == WeaponImpactBranch::Door &&
              !weapon(g, 0)->impacts[0].damage_delivered);
        return true;
    }
    bool volumes(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto water = brush(g, "Water", 30);
        auto air = brush(g, "VolumeBrush", 60);
        auto target = brush(g, "CDestructableBrush", 90, {{"SurfaceType", std::int32_t(SURFTYPE_METAL)}});
        CHECK(water && air && target);
        for (auto h : {water, air})
            g.objects.get(h)->state.m_Flags = FLAG_RAYHIT;
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
        auto* w = weapon(g);
        CHECK(w->impacts.size() == 3);
        CHECK(w->impacts[0].branch == WeaponImpactBranch::Volume && w->impacts[0].effect_requested &&
              w->impacts[0].effect.resource.empty());
        CHECK(!w->impacts[0].damage_delivered && !w->impacts[0].alert.complete && !w->impacts[1].effect_requested);
        SegmentHit hit;
        auto from = shot(g).position;
        auto end = vector_add(from, {0, 0, 150});
        CHECK(g.server.IntersectSegment(from, end, g.client_shell.player, hit, false, SegmentObjects::SolidOrRayHit,
                                        SegmentFilter::IgnoreVolumes) == SegmentResult::Hit &&
              hit.object == target);
        g.server.SetObjectUserFlags(water, SURFTYPE_LIQUID << 24);
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired && w->impacts[0].effect.sound);
        from.z += 30;
        CHECK(g.server.IntersectSegment(from, end, g.client_shell.player, hit, false, SegmentObjects::SolidOrRayHit) ==
                  SegmentResult::Hit &&
              hit.object == water);
        CHECK(g.server.IntersectSegment(from, end, g.client_shell.player, hit, false, SegmentObjects::SolidOrRayHit,
                                        SegmentFilter::None, SegmentStart::SkipInterior) == SegmentResult::Hit &&
              hit.object == air);
        CHECK(g.server.IntersectSegment(from, end, nullptr, hit, false, SegmentObjects::SolidOnly, SegmentFilter::None,
                                        static_cast<SegmentStart>(99)) == SegmentResult::Invalid);
        return true;
    }
    bool hearing(const std::filesystem::path& root)
    {
        for (unsigned action : {0u, 1u, 2u, 3u})
        {
            GameSession g;
            CHECK(setup(g, root));
            auto h = brush(g, "CDestructableBrush", 40, {{"SurfaceType", std::int32_t(SURFTYPE_METAL)}});
            auto* a = listener(g);
            CHECK(h && a);
            a->action = action;
            a->victim = h;
            const auto strings = g.server.string_count();
            std::string error;
            if (action == 3)
                CHECK(g.select_weapon(0, error));
            auto* w = weapon(g, action == 3 ? 0 : 1);
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g, action == 3 ? WEAP_MELEE : WEAP_BERETTA)) ==
                  FireResult::Fired);
            CHECK(a->impacts == 1 && a->hearing_reactions == 1 && a->m_nState == AI_Mgr::StateDodge &&
                  a->damage_seen == (action == 3));
            CHECK(w->impacts[0].alert.complete && w->impacts[0].alert.delivered == 1);
            CHECK(w->last_shot.interrupted == (action == 1) && w->impacts[0].target_removed == (action == 2));
            CHECK(w->last_shot.damage_delivered == (action == 0 || action == 3));
            const auto sound = w->impacts[0].effect.sound;
            CHECK(sound && g.server.audio.get(sound));
            CHECK(g.server.message_count() == 0 && g.server.string_count() == strings);
            if (action != 1)
                CHECK(g.leave_player());
            CHECK(g.objects.finish_frame() && g.server.audio.get(sound));
            CHECK(g.server.advance_audio(.11, {}, {}) && !g.server.audio.get(sound));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.string_count());
        }
        // Character hits use flesh audio but never dispatch impact hearing.
        GameSession g;
        CHECK(setup(g, root));
        auto* a = listener(g);
        CHECK(a);
        auto pos = vector_add(shot(g).position, {0, 0, 40});
        DVector dims{10, 10, 10};
        CHECK(g.server.TeleportObject(a->m_hObject, &pos) == DE_OK &&
              g.server.SetObjectDims(a->m_hObject, &dims) == DE_OK);
        g.objects.get(a->m_hObject)->state.m_Flags |= FLAG_SOLID;
        CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) == FireResult::Fired);
        CHECK(weapon(g)->impacts[0].surface == SURFTYPE_FLESH && weapon(g)->impacts[0].effect.sound && !a->impacts);
        return true;
    }
    bool limits(const std::filesystem::path& root)
    {
        for (unsigned count : {64u, 65u})
        {
            GameSession g;
            CHECK(setup(g, root, false));
            CHECK(walls(g, std::vector<unsigned>(count, SURFTYPE_GLASS), 2));
            CHECK(g.server_shell.OnMessage(g.local_client, shot(g)) ==
                  (count == 64 ? FireResult::Fired : FireResult::Unavailable));
            auto* w = weapon(g);
            CHECK(w->impacts.size() == 64 && w->impact_sound_failures == 64 && w->impact_sounds == 0);
            CHECK(w->impact_error.empty() == (count == 64));
            CHECK(!w->impacts[0].effect.error.empty() &&
                  g.server_shell.Player(g.local_client)->m_InventoryMgr.GetAmmoCount(1) == 49);
            CHECK(g.server.message_count() == 0);
        }
        return true;
    }
    bool private_audio(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(g.request_world("01_subwaya", error) && g.enter_player(error));
            CHECK(test_camera::complete_subway_intro(g));
            auto fire = shot(g);
            for (auto direction : {DVector{0, 0, 1}, DVector{1, 0, 0}, DVector{0, 0, -1}, DVector{-1, 0, 0}})
            {
                fire.direction = direction;
                CHECK(g.server_shell.OnMessage(g.local_client, fire) == FireResult::Fired);
                auto* w = weapon(g);
                CHECK(!w->impacts.empty() && w->impact_error.empty() && !w->impact_sound_failures);
                std::cout << "cycle=" << cycle << " contacts=" << w->impacts.size() << " sounds=" << w->impact_sounds
                          << " failed=" << w->impact_sound_failures << '\n';
            }
            // Diagnostic material requests supplement actual world shots; they do not claim a rendered impact.
            CWeaponFX fx;
            fx.m_vDestPos = fire.position;
            std::minstd_rand random(1);
            SoundId tail = 0;
            for (auto& [surface, material] : materials)
            {
                fx.m_eSurfaceType = surface;
                CHECK(fx.PlayImpactSound(g.server, g.client_shell.player, random) && fx.sound);
                tail = fx.sound;
                auto* voice = g.server.audio.get(tail);
                CHECK(voice && voice->clip->frames() > 0);
                std::cout << fx.resource << " frames=" << voice->clip->frames() << " rate=" << voice->clip->rate
                          << '\n';
            }
            g.server.audio.inspect_pcm = true;
            CHECK(g.server.advance_audio(.05, fire.position, {}) && g.server.audio.peak > 0);
            CHECK(g.leave_player() && g.server.audio.get(tail));
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.objects.snapshot().empty() &&
                  !g.server.string_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string name = argv[1];
    const std::filesystem::path root = argv[2];
    if (name == "selection")
        return selection(root) ? 0 : 1;
    if (name == "world")
        return world(root) ? 0 : 1;
    if (name == "brushes")
        return brushes(root) ? 0 : 1;
    if (name == "volumes")
        return volumes(root) ? 0 : 1;
    if (name == "hearing")
        return hearing(root) ? 0 : 1;
    if (name == "limits")
        return limits(root) ? 0 : 1;
    if (name == "private")
        return private_audio(root) ? 0 : 1;
    return 2;
}
