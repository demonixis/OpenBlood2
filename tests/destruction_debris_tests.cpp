#include "app/model_scene.h"
#include "blood_spurt_fixture.h"
#include "game/original/DestructableBrush.h"
#include "game/original/DestructableModel.h"
#include <limits>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        CHECK(test_player::setup(g, root, false));
        std::string error;
        std::vector<std::uint8_t> model, skin;
        CHECK(g.resources.read("models/characters/caleb.abc", model, error));
        CHECK(test_blood_splat::mount(g.resources, root, error));
        CHECK(g.resources.read("alpha.dtx", skin, error));
        test_player::Directory dir;
        for (const auto material : {"wood", "metal", "glass"})
            for (int i = 1; i <= 3; ++i)
            {
                const auto name = "gib" + std::to_string(i);
                dir.children["models"].children["gibs"].children[material].files[name + ".abc"] = model;
                dir.children["skins"].children["gibs"].children[material].files[name + ".dtx"] = skin;
                dir.children["sounds"].children["gibs"].children[material].files["gib_impact" + std::to_string(i) +
                                                                                 ".wav"] = test_audio::wave();
            }
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, dir);
        w.patch(131, off);
        w.patch(135, size);
        const auto path = root / "debris.rez";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        CHECK(g.resources.mount(path, error));
        CHECK(test_blood_spurt::floor(g, error));
        return true;
    }
    DebrisEffect request(unsigned surface, unsigned count = 10)
    {
        DebrisEffect e;
        e.source = "test box";
        e.flags = surface / 10 | 0x200;
        e.amount = DBYTE(count);
        e.position = {0, 100, 0};
        e.velocity = {0, 20, 0};
        e.dimensions = {10, 20, 10};
        e.scale = 2;
        return e;
    }
    bool recipes(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& fx = g.server.destruction_debris;
        for (const auto surface : {20u, 30u, 50u})
        {
            g.server.RecordDebrisEffect(request(surface));
            std::cerr << "Material " << surface << ": " << g.server.debris_effects.back().error << '\n';
            CHECK(g.server.debris_effects.back().event && g.server.debris_effects.back().error.empty());
            CHECK(fx.active.size() == 10 && fx.reports.back().created == 10);
            for (const auto& f : fx.active)
            {
                CHECK(f.surface == surface && !f.blood && f.bounces == 1 && f.spurt_event == 0 && f.splat_event == 0);
                CHECK(f.position.x >= -8 && f.position.x <= 8 && f.position.y >= (surface == 50 ? 84 : 92) &&
                      f.position.y <= 116);
                CHECK(f.velocity.y >= 0 && f.velocity.y <= (surface == 50 ? 40 : 80));
                if (surface == 50)
                    CHECK(f.alpha == .4f && f.scale.x >= 1 && f.scale.x <= 2.5f);
                else
                    CHECK(f.alpha == 1 && f.scale.x == 2 && f.scale.y == 2 && f.scale.z == 2);
            }
            for (bool gpu : {false, true})
            {
                ModelView scene;
                scene.gpu_skinning = gpu;
                std::string error;
                CHECK(update_explosion_model_scene(g, scene, error) && scene.entries.size() == 10);
                for (const auto& entry : scene.entries)
                    CHECK(entry.error.empty() && entry.triangles > 0 && entry.alpha == (surface == 50 ? .4f : 1.f));
                if (gpu)
                    for (const auto& instance : scene.instances)
                        CHECK(instance.translucent == (surface == 50));
            }
            fx.clear();
        }
        g.server.debris_detail = 0;
        g.server.RecordDebrisEffect(request(30, 11));
        CHECK(fx.active.size() == 5);
        fx.clear();
        g.server.debris_detail = 2;
        auto custom = request(30, 1);
        custom.flags |= 0x4000;
        custom.model = "models/gibs/wood/gib1.abc";
        custom.texture = "skins/gibs/wood/gib1.dtx";
        custom.sound = "sounds/gibs/wood/gib_impact1.wav";
        g.server.RecordDebrisEffect(custom);
        CHECK(fx.active.size() == 1 && fx.active[0].scale.x == 1);
        fx.clear();
        custom.model = "missing.abc";
        g.server.RecordDebrisEffect(custom);
        CHECK(fx.active.empty() && !g.server.debris_effects.back().error.empty());
        g.server.RecordDebrisEffect(request(10));
        CHECK(fx.active.empty() && !g.server.debris_effects.back().error.empty());
        auto invalid = request(30);
        invalid.scale = std::numeric_limits<float>::quiet_NaN();
        std::string error;
        CHECK(!fx.publish_debris(invalid, 2, g.server, g.models, g.resources, 0, error) && !error.empty());
        CHECK(g.server.blood_spurts.active.empty() && g.server.blood_splats.active.empty() &&
              g.server.flesh_gibs.active.empty());
        return true;
    }
    bool motion(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& fx = g.server.destruction_debris;
        g.server.RecordDebrisEffect(request(50, 1));
        CHECK(fx.active.size() == 1);
        auto& f = fx.active[0];
        f.position = {0, 1, 0};
        f.velocity = {0, -100, 0};
        CHECK(fx.advance(.02, g.server));
        CHECK(f.impacts == 1 && f.bounces == 0 && !f.resting && f.sound_error.empty());
        CHECK(g.server.audio.voices().size() == 1 && g.server.blood_splats.active.empty());
        for (int i = 3; i < 100; ++i)
        {
            CHECK(fx.advance(i * .01, g.server));
            CHECK(g.server.advance_audio(.01, {}, {}));
        }
        CHECK(f.resting && f.impacts == 2 && f.bounces == -1 && f.error.empty());
        const auto pos = f.position;
        CHECK(fx.advance(1, g.server) && f.position.y == pos.y);
        // Eviction replaces oldest live slots and keeps at most 50 terminating effects.
        fx.clear();
        g.server.RecordDebrisEffect(request(30, 255));
        CHECK(fx.active.size() == 130);
        CHECK(std::count_if(fx.active.begin(), fx.active.end(), [](auto& x) { return x.fading; }) == 50);
        auto id = fx.active.front().id;
        CHECK(fx.advance(.01, g.server));
        CHECK(fx.active.front().id == id && fx.active.front().alpha == 1);
        CHECK(fx.advance(.02, g.server));
        CHECK(fx.active.front().alpha < 1);
        for (int i = 3; i <= 152; ++i)
            CHECK(fx.advance(i * .01, g.server));
        CHECK(fx.active.size() == 80 &&
              std::none_of(fx.active.begin(), fx.active.end(), [](auto& x) { return x.fading; }));
        g.server.reset_world_time();
        CHECK(fx.active.empty() && fx.reports.empty());
        return true;
    }
    bool destruction(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = g.objects.create(g.objects.find_class("CDestructableModel"), {}, Creation::World,
                                  {{"InitFilename", std::string("models/characters/caleb.abc")},
                                   {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                   {"Pos", DVector{0, 100, 0}},
                                   {"Gravity", false},
                                   {"Wood", true},
                                   {"Metal", true},
                                   {"Glass", true},
                                   {"Amount", std::int32_t(4)}});
        CHECK(h);
        auto message = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(message && g.server.WriteDamage(message, {{1, 0, 0}, {}, 500, DamageType::Normal, h}) == DE_OK &&
              g.server.EndMessage(message) == DE_OK);
        CHECK(g.step(.02) && !g.server.IsObjectAlive(h));
        CHECK(g.server.debris_effects.size() == 3 && g.server.destruction_debris.active.size() == 12);
        const auto& reports = g.server.debris_effects;
        CHECK((reports[0].flags & 255) == 2 && (reports[1].flags & 255) == 3 && (reports[2].flags & 255) == 5);
        CHECK(reports[0].velocity.x == -reports[1].velocity.x && reports[1].velocity.x == -reports[2].velocity.x);
        for (int i = 0; i < 500; ++i)
            CHECK(g.step(.01));
        CHECK(std::all_of(g.server.destruction_debris.active.begin(), g.server.destruction_debris.active.end(),
                          [](const auto& f) { return f.error.empty() && f.sound_error.empty(); }));
        return true;
    }
    bool retail(const std::filesystem::path& root)
    {
        GameSession g;
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error) && g.request_world("02_cityhub", error));
        unsigned covered = 0;
        for (const auto material : {1u, 2u, 5u})
        {
            bool found = false;
            for (auto id : g.objects.snapshot())
            {
                auto* rec = g.objects.get(id);
                if (!rec)
                    continue;
                auto* model = dynamic_cast<CDestructableModel*>(rec->game.get());
                auto* brush = dynamic_cast<CDestructableBrush*>(rec->game.get());
                auto* debris = model ? &model->debris : brush ? &brush->debris : nullptr;
                if (!debris || !debris->materials[material] || !debris->amount || (model && !model->destructable) ||
                    (brush && !brush->destructable))
                    continue;
                const auto name = std::string(g.server.GetObjectName(rec->handle));
                const auto resource = std::string(rec->state.m_Filename);
                const auto before = g.server.destruction_debris.active.size();
                const auto first_report = g.server.debris_effects.size();
                auto message = g.server.StartMessageToObject(rec->game.get(), rec->handle, damage_message);
                CHECK(message &&
                      g.server.WriteDamage(
                          message, {{1, 0, 0}, rec->state.m_Pos, 100000, DamageType::Normal, rec->handle}) == DE_OK &&
                      g.server.EndMessage(message) == DE_OK);
                CHECK(g.step(.02));
                if (g.server.destruction_debris.active.size() == before)
                    continue;
                std::cout << "Destroyed " << name << " (" << resource << ") material " << CDebris::names[material]
                          << '\n';
                for (std::size_t i = first_report; i < g.server.debris_effects.size(); ++i)
                {
                    const auto& report = g.server.debris_effects[i];
                    const auto surface = (report.flags & 255) * 10;
                    std::cout << "  material " << surface << " / " << report.error << '\n';
                    if (surface == 20 || surface == 30 || surface == 50)
                        CHECK(report.error.empty() && report.event);
                    else
                        CHECK(!report.error.empty());
                }
                ++covered;
                found = true;
                break;
            }
            CHECK(found);
        }
        CHECK(covered == 3);
        for (int i = 0; i < 1000; ++i)
            CHECK(g.step(.01));
        for (const auto& f : g.server.destruction_debris.active)
            CHECK(f.error.empty() && f.sound_error.empty());
        CHECK(g.unload_world() && g.server.destruction_debris.active.empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    std::string name = argv[1];
    const std::filesystem::path root = argv[2];
    return (name == "recipes"       ? recipes(root)
            : name == "motion"      ? motion(root)
            : name == "destruction" ? destruction(root)
                                    : name == "retail" && retail(root))
               ? 0
               : 1;
}
