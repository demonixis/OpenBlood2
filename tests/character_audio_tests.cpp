#include "audio_fixture.h"
#include "brush_fixture.h"
#include "camera_fixture.h"
#include "engine/container.h"
#include "engine/model_commands.h"
#include <functional>
#include <limits>
#include <set>
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
        CHECK(test_audio::setup(g, root));
        test_player::Directory d;
        auto& chosen = d.children["sounds"].children["chosen"].children["caleb"];
        chosen.files["cue.wav"] = test_audio::wave();
        chosen.files["long.wav"] = test_audio::wave(1, 16, 8000, 8000);
        for (auto material : {"wood", "flesh", "glass", "metal", "plastic", "stone", "terrain", "water", "carpet"})
            for (auto side : {"leftm.wav", "rightm.wav"})
                d.children["sounds"].children["player"].children["footsteps"].children[material].files[side] =
                    test_audio::wave();
        auto w = test_model::Writer{};
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "character.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        CHECK(g.resources.mount(root / "character.rez", error));
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("start")}, {"Pos", DVector{32, 100, 64}}}));
        return true;
    }
    HOBJECT owner(GameSession& g)
    {
        return g.objects.create(g.objects.find_class("BaseClass"), {});
    }
    bool key(GameSession& g, HOBJECT h, const char* name, const char* sound)
    {
        char* args[]{const_cast<char*>(name), const_cast<char*>(sound)};
        ArgList list{args, 2};
        return g.objects.engine_message(h, MID_MODELSTRINGKEY, &list);
    }
    bool paths(const std::filesystem::path&)
    {
        CAnim_Sound a;
        CHECK(a.SetSoundRoot("Sounds\\Chosen\\Caleb"));
        std::string p = "pain";
        CHECK(a.GetSoundPath(p, 3) && p == "Sounds\\Chosen\\Caleb\\pain3.wav");
        p = "pain.wav";
        CHECK(a.GetSoundPath(p) && p == "Sounds\\Chosen\\Caleb\\pain.wav");
        p = "pain";
        CHECK(a.GetSoundPath(p, -1) && p == "Sounds\\Chosen\\Caleb\\pain-1.wav");
        CHECK(!a.SetSoundRoot(std::string(256, 'x')) && a.GetSoundRoot() == "Sounds\\Chosen\\Caleb");
        CHECK(!a.SetSoundRoot(std::string("x\0y", 3)));
        p = std::string(255, 'x');
        const auto old = p;
        CHECK(!a.GetSoundPath(p) && p == old);
        p = std::string("x\0y", 3);
        CHECK(!a.GetSoundPath(p) && p.size() == 3);
        return true;
    }
    bool voices(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const auto h = owner(g);
        CAnim_Sound a;
        CHECK(a.SetSoundRoot("sounds/chosen/caleb"));
        CHECK(a.PlayVoice(h, "cue.wav", 500, 70));
        const auto first = a.m_hVoxSound;
        const auto* v = g.server.audio.get(first);
        CHECK(v && v->owner == h && v->request.speaker == h && !v->auto_remove && !v->world_owned);
        CHECK(v->request.inner_radius == 50 && v->request.outer_radius == 500 && v->request.volume == .7f &&
              v->request.priority == 3 && v->request.reverb_requested);
        CHECK(!a.PlayVoice(h, "missing.wav", 500, 100) && a.m_hVoxSound == first && a.voice_busy == 1 &&
              a.sound_attempts == 1);
        CHECK(g.server.advance_audio(.1, {}, {}));
        bool done = false;
        CHECK(g.server.IsSoundDone(first, done) && done);
        a.SetSilent(DTRUE);
        CHECK(a.PlayVoice(h, "long.wav", 500, 100)); // Source flag is not checked here.
        const auto second = a.m_hVoxSound;
        CHECK(second != first && !g.server.audio.get(first));
        CHECK(g.server.KillSound(second));
        CHECK(a.PlayVoice(h, "cue.wav", 500, 100)); // Failed completion lookup permits retry.
        CHECK(g.server.advance_audio(.1, {}, {}));
        CHECK(!a.PlayVoice(h, "missing.wav", 500, 100) && !a.m_hVoxSound && a.sound_starts == 3 &&
              a.sound_failures == 1 && !a.sound_error.empty());
        CHECK(a.PlayVoice(h, "cue.wav", 500, 100));
        return true; // Destructor kills retained voice before session teardown.
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = owner(g);
        SoundId saved = 0;
        {
            CAnim_Sound a;
            CHECK(a.SetSoundRoot("sounds/chosen/caleb"));
            CHECK(a.PlayVoice(h, "long.wav", 500, 100));
            saved = a.m_hVoxSound;
            CHECK(a.PlaySound(h, "cue.wav", 500, 100) == 0 && a.sound_starts == 2);
            CHECK(g.server.audio.voices().size() == 2);
            g.server.audio.inspect_pcm = true;
            auto* r = g.objects.get(h);
            r->state.m_Pos = {25, 0, 0};
            r->activity = OBJSTATE_INACTIVE;
            CHECK(g.server.advance_audio(.05, {}, {}));
            const auto* v = g.server.audio.get(saved);
            CHECK(v && v->position.x == 25 && v->right > v->left && g.server.audio.peak > 0);
            CHECK(g.server.advance_audio(.05, {}, {}));
            CHECK(g.server.audio.voices().size() == 1);
        }
        CHECK(!g.server.audio.get(saved));
        CAnim_Sound a;
        CHECK(a.SetSoundRoot("sounds/chosen/caleb") && a.PlayVoice(h, "long.wav", 500, 100));
        saved = a.m_hVoxSound;
        g.server.RemoveObject(h);
        CHECK(!a.PlayVoice(h, "cue.wav", 500, 100));
        CHECK(g.server.advance_audio(0, {}, {}));
        CHECK(!g.server.audio.get(saved));
        CHECK(g.objects.finish_frame());
        CHECK(!a.PlayVoice(h, "cue.wav", 500, 100));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.audio.cached_clips() == 0);
        return true;
    }
    bool rejection(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = owner(g), other = owner(g);
        std::string error;
        SoundRequest r;
        r.resource = "cue.wav";
        r.speaker = h;
        CHECK(!g.server.PlayCharacterSound(nullptr, r, true, error));
        CHECK(!g.server.PlayCharacterSound(other, r, true, error));
        r.speaker = nullptr;
        CHECK(!g.server.PlayCharacterSound(h, r, true, error));
        r.speaker = h;
        r.loop = true;
        CHECK(!g.server.PlayCharacterSound(h, r, true, error));
        r.loop = false;
        r.space = SoundSpace::Local;
        CHECK(!g.server.PlayCharacterSound(h, r, true, error));
        r.space = SoundSpace::Spatial;
        CAnim_Sound a;
        CHECK(a.SetSoundRoot("sounds/chosen/caleb"));
        for (float radius : {0.f, -1.f, std::numeric_limits<float>::infinity(), 1.1e6f})
            CHECK(!a.PlayVoice(h, "cue.wav", radius, 100));
        CHECK(!a.PlayVoice(h, "cue.wav", 500, -1) && !a.PlayVoice(h, "cue.wav", 500, 101));
        CHECK(!a.PlayVoice(h, "../cue.wav", 500, 100));
        for (int i = 0; i < 128; ++i)
            CHECK(g.server.PlayCharacterSound(h, r, false, error));
        CHECK(!a.PlayVoice(h, "cue.wav", 500, 100));
        CHECK(g.server.advance_audio(.1, {}, {}));
        CHECK(g.server.audio.voices().empty());
        CHECK(a.PlayVoice(h, "cue.wav", 500, 100));
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        CHECK(p && p->m_Anim_Sound.GetSoundRoot() == "sounds\\chosen\\caleb");
        auto h = p->m_hObject;
        CHECK(key(g, h, "PLAY_SOUND", "long"));
        const auto old = p->m_hCurSound;
        CHECK(old && p->sound_starts == 1 && p->pending_model_commands.empty());
        CHECK(key(g, h, "play_sound", "missing"));
        CHECK(!g.server.audio.get(old) && !p->m_hCurSound && p->sound_failures == 1 &&
              p->unavailable_sound_alerts == 2);
        CHECK(key(g, h, "play_sound", "cue.wav") && p->sound_resource == "sounds\\chosen\\caleb\\cue.wav.wav" &&
              !p->m_hCurSound);
        CHECK(p->PlayPlayerSound("sounds/chosen/caleb/cue.wav", 1000, 255, false, 0));
        auto id = p->m_hCurSound;
        const auto* v = g.server.audio.get(id);
        CHECK(v && v->request.volume == 1 && v->request.inner_radius == 100 && v->request.priority == 4);
        CHECK(g.server.advance_audio(.1, {}, {}));
        p->UpdateSound();
        CHECK(!p->m_hCurSound && !g.server.audio.get(id));
        DVector velocity{0, 50, 0};
        CHECK(g.server.SetVelocity(h, &velocity) == DE_OK);
        p->UpdateSound();
        CHECK(!p->m_bDisableFootsteps);
        CHECK(key(g, h, "play_sound", "FOOTSTEP") && p->unresolved_footsteps == 1 && p->pending_model_commands.empty());
        velocity.y = 49;
        CHECK(g.server.SetVelocity(h, &velocity) == DE_OK);
        p->UpdateSound();
        CHECK(key(g, h, "play_sound", "footstep") && p->suppressed_footsteps == 1 && p->pending_model_commands.empty());
        CHECK(key(g, h, "RotY", "1") &&
              p->pending_model_commands.size() == 1); // Player does not call BaseCharacter motion-key branch.
        CHECK(p->PlayPlayerSound("sounds/chosen/caleb/long.wav", 1000, 100, false));
        id = p->m_hCurSound;
        CHECK(!p->PlayPlayerSound("sounds/chosen/caleb/cue.wav", 1000, 100, true) && !g.server.audio.get(id) &&
              !p->m_hCurSound);
        CHECK(key(g, h, "play_sound", "long"));
        id = p->m_hCurSound;
        CHECK(g.leave_player() && !g.server.audio.get(id));
        CHECK(!key(g, h, "play_sound", "cue"));
        return true;
    }
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        auto model = std::make_shared<Model>(*r->model);
        auto& clip = model->animations.front();
        for (auto& k : clip.keys)
            k.text.clear();
        CHECK(clip.keys.size() >= 2);
        clip.keys[0].text = "play_sound long; play_sound cue";
        r->model = model;
        g.server.SetModelAnimation(r->handle, 0);
        CHECK(g.server.ResetModelAnimation(r->handle) == DE_OK);
        CHECK(g.server.advance_models(.05));
        CHECK(p->sound_keys == 2 && p->sound_starts == 2 && g.server.audio.voices().size() == 1 &&
              p->sound_resource.ends_with("cue.wav"));
        const auto count = p->sound_starts;
        CHECK(g.server.advance_models(.01) && p->sound_starts == count);
        r->activity = OBJSTATE_INACTIVE;
        CHECK(g.server.ResetModelAnimation(r->handle) == DE_OK);
        CHECK(g.server.advance_models(.05) && p->sound_starts == count);
        r->activity = OBJSTATE_ACTIVE;
        r->state.m_Flags &= ~FLAG_MODELKEYS;
        CHECK(g.server.advance_models(.05) && p->sound_starts == count);
        return true;
    }
    bool footsteps(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        auto* r = g.objects.get(p->m_hObject);
        std::string path = "unchanged";
        CHECK(!p->GetFootstepSound(path) && path == "unchanged" && p->m_bNextFootstepLeft);
        for (auto [surface, material] : std::map<SurfaceType, std::string>{{SURFTYPE_WOOD, "wood"},
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
            CHECK(p->GetFootstepSound(path) && path == "sounds/player/footsteps/" + material + "/leftm.wav");
            CHECK(p->GetFootstepSound(path) && path == "sounds/player/footsteps/" + material + "/rightm.wav");
        }
        for (auto type : {SURFTYPE_UNKNOWN, SURFTYPE_ENERGY, SURFTYPE_BUILDING, SURFTYPE_SKY, SURFTYPE_FIRETHROUGH})
        {
            p->m_eLastSurface = type;
            CHECK(!p->GetFootstepSound(path) && p->m_bNextFootstepLeft);
        }
        auto point = r->state.m_Pos;
        point.y -= r->dimensions.y - 1;
        g.world.models.push_back(test_brush::box("wet", {point.x - 20, point.y - 2, point.z - 20},
                                                 {point.x + 20, point.y + 2, point.z + 20}));
        auto water = g.objects.create(g.objects.find_class("Water"), {}, Creation::World,
                                      {{"Name", std::string("wet")}, {"Hidden", true}});
        CHECK(water);
        p->m_eLastSurface = SURFTYPE_METAL;
        CHECK(p->GetFootstepSound(path) && path == "sounds/player/footsteps/water/leftm.wav" &&
              p->footstep_container == CC_WATER);
        g.server.RemoveObject(water);
        CHECK(p->GetFootstepSound(path) && path == "sounds/player/footsteps/metal/rightm.wav");
        ContainerCode code = CC_LADDER;
        CHECK(!g.server.GetPointContainerCode({NAN, 0, 0}, code) && code == CC_LADDER);
        CHECK(g.objects.finish_frame());
        p->m_bDisableFootsteps = false;
        CHECK(key(g, r->handle, "play_sound", "footstep") && p->m_hCurSound && !p->m_bNextFootstepLeft);
        // Unknown material does not cancel an existing sound or advance the foot.
        auto id = p->m_hCurSound;
        p->m_eLastSurface = SURFTYPE_UNKNOWN;
        CHECK(key(g, r->handle, "play_sound", "footstep") && p->m_hCurSound == id && !p->m_bNextFootstepLeft);
        return true;
    }
    bool surfaces(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        CHECK(g.enter_player(error));
        auto* p = g.server_shell.Player(g.local_client);
        const auto h = p->m_hObject;
        WorldModel floor;
        floor.points = {{-1000, 0, -1000}, {1000, 0, -1000}, {1000, 0, 1000}, {-1000, 0, 1000}};
        floor.surfaces.push_back({});
        floor.surfaces[0].flags = 1;
        floor.surfaces[0].texture_flags = 20;
        floor.polygons.push_back({0, {0, 1, 2, 3}});
        CHECK(g.collision.build(floor, error));
        CollisionHit hit;
        CHECK(g.collision.transform(rotation_from_euler({0, .5f, 0}), {}, {0, 2, 0}));
        CHECK(g.collision.sweep({0, 10, 0}, {0, -10, 0}, {1, 1, 1}, hit) == CollisionQuery::Hit &&
              hit.texture_flags == 20);
        CHECK(g.collision.build(floor, error));
        auto mesh = test_brush::box("material", {-20, -2, -20}, {20, 2, 20});
        for (auto& surface : mesh.surfaces)
            surface.texture_flags = 30;
        ContainerShape shape;
        CHECK(shape.build(mesh, {}, error));
        CHECK(shape.transform(rotation_from_euler({0, .3f, 0}), {0, 3, 0}));
        CHECK(shape.sweep({0, 12, 0}, {0, -10, 0}, {1, 1, 1}, hit) == CollisionQuery::Hit && hit.texture_flags == 30);
        MovementState state;
        CHECK(g.server.GetMovementState(h, state) && state.standing && state.ground_surface == 20);
        g.client_shell.movement_input.forward = true;
        CHECK(g.step(.05));
        CHECK(g.client_shell.move_mgr.m_eLastSurface == SURFTYPE_METAL && p->m_eLastSurface == SURFTYPE_METAL);
        auto request = g.client_shell.move_mgr.WritePositionInfo(state.position, {});
        request.move_code++;
        request.surface = 30;
        CHECK(!p->HandlePlayerPositionMessage(request) && p->m_eLastSurface == SURFTYPE_METAL);
        floor.surfaces[0].texture_flags = UINT16_MAX;
        CHECK(g.collision.build(floor, error));
        CHECK(g.step(.05));
        CHECK(g.client_shell.move_mgr.m_eLastSurface == static_cast<SurfaceType>(UINT16_MAX) &&
              p->m_eLastSurface == static_cast<SurfaceType>(UINT8_MAX)); // Original byte transport, no material alias.
        // UserFlags fallback applies to ordinary body support, independent of primary polygon identity.
        const auto box = owner(g);
        auto* b = g.objects.get(box);
        b->state.m_Pos = {32, 10, 64};
        b->dimensions = {50, 10, 50};
        b->state.m_Flags = FLAG_SOLID;
        b->user_flags = 100u << 24;
        DVector position{32, 20 + g.objects.get(h)->dimensions.y + .001f, 64};
        CHECK(g.server.TeleportObject(h, &position) == DE_OK);
        DVector zero{};
        CHECK(g.server.SetVelocity(h, &zero) == DE_OK);
        CHECK(g.server.GetMovementState(h, state) && state.standing && state.ground_surface == 100);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        std::shared_ptr<const Model> model;
        CHECK(g.models.load("models/characters/caleb.abc", model, error));
        CAnim_Sound a;
        CHECK(a.SetSoundRoot("sounds/chosen/caleb"));
        size_t keys = 0, footsteps = 0, decoded = 0, missing = 0;
        std::set<std::string> paths;
        for (const auto& clip : model->animations)
            for (const auto& k : clip.keys)
            {
                std::vector<std::vector<std::string>> commands;
                CHECK(model_commands(k.text, commands));
                for (const auto& command : commands)
                    if (command.size() > 1 && command[0] == "play_sound")
                    {
                        ++keys;
                        if (command[1] == "footstep")
                        {
                            ++footsteps;
                            continue;
                        }
                        auto path = command[1] + ".wav";
                        CHECK(a.GetSoundPath(path));
                        paths.insert(path);
                    }
            }
        // Caleb's authored keys are footsteps only; validate their original mapping resources.
        for (auto material : {"wood", "flesh", "glass", "metal", "plastic", "stone", "terrain", "water", "carpet"})
            for (auto side : {"leftm.wav", "rightm.wav", "leftf.wav", "rightf.wav"})
                paths.insert(std::string("sounds/player/footsteps/") + material + "/" + side);
        for (const auto& path : paths)
        {
            std::shared_ptr<const AudioClip> clip;
            if (g.server.audio.load(g.resources, path, clip, error))
            {
                ++decoded;
                std::cout << path << " frames=" << clip->frames() << " rate=" << clip->rate << '\n';
            }
            else
            {
                CHECK(!g.resources.find(path));
                ++missing;
                std::cout << path << " unavailable=" << error << '\n';
            }
        }
        std::cout << "Caleb sound keys=" << keys << " footsteps=" << footsteps << " unique_resources=" << paths.size()
                  << " decoded=" << decoded << " missing=" << missing << '\n';
        CHECK(keys == 46 && footsteps == 46 && decoded > 0);
        CHECK(g.request_world("01_SubwayA", error));
        std::map<unsigned, unsigned> surfaces;
        unsigned compared = 0, matched = 0;
        std::map<std::string, unsigned> disagreements;
        std::map<std::string, unsigned> header_flags;
        for (const auto& world : g.world.models)
            for (const auto& s : world.surfaces)
            {
                ++surfaces[s.texture_flags];
                const auto& path = world.textures[s.texture];
                if (!header_flags.contains(path))
                {
                    std::vector<std::uint8_t> bytes;
                    CHECK(g.resources.read(path, bytes, error) && bytes.size() >= 164);
                    unsigned value = 0;
                    for (unsigned b = 0; b < 4; ++b)
                        value |= unsigned(bytes[20 + b]) << (8 * b);
                    header_flags[path] = value;
                }
                const auto value = header_flags[path];
                ++compared;
                matched += value == s.texture_flags;
                if (value != s.texture_flags)
                    ++disagreements[path + " baked=" + std::to_string(s.texture_flags) +
                                    " dtx=" + std::to_string(value)];
            }
        for (auto [type, count] : surfaces)
            std::cout << "Subway surface=" << type << " count=" << count << '\n';
        std::cout << "DAT material / DTX user flags comparisons=" << compared << " matched=" << matched << '\n';
        for (const auto& [difference, count] : disagreements)
            std::cout << difference << " surfaces=" << count << '\n';
        // The engine API uses baked flags: live DTX metadata is a comparison, not an override.
        CHECK(compared == 2723 && matched == 2163 && surfaces[20] == 1438 && surfaces[80] == 244);
        CHECK(decoded == 34 && missing == 2);
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        SoundId old = 0;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(test_camera::complete_subway_intro(g));
            g.server.audio.inspect_pcm = true;
            g.client_shell.movement_input.forward = true;
            bool heard = false;
            for (unsigned frame = 0; frame < 120; ++frame)
            {
                CHECK(g.step(1. / 60));
                const auto* voice = g.server.audio.get(p->m_hCurSound);
                if (voice && voice->cursor > 0 && g.server.audio.peak > 0)
                {
                    heard = true;
                    old = p->m_hCurSound;
                    CHECK(voice->request.resource.starts_with("sounds/player/footsteps/"));
                    CHECK(voice->request.inner_radius == 40 && voice->request.outer_radius == 400 &&
                          voice->request.volume == .7f);
                }
            }
            g.client_shell.movement_input = {};
            std::cout << "cycle=" << cycle << " keys=" << p->sound_keys << " starts=" << p->sound_starts
                      << " material=" << p->m_eLastSurface << " last=" << p->sound_resource << '\n';
            CHECK(heard && p->sound_starts > 0 && p->sound_failures == 0 &&
                  p->unavailable_sound_alerts == p->sound_attempts);
            CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.audio.cached_clips() == 0 &&
                  !g.server.audio.get(old));
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"paths", paths},     {"voices", voices},       {"lifetime", lifetime},   {"rejection", rejection},
        {"player", player},   {"callbacks", callbacks}, {"footsteps", footsteps}, {"surfaces", surfaces},
        {"catalog", catalog}, {"subway", subway}};
    auto i = cases.find(argv[1]);
    return i == cases.end() ? 2 : i->second(argv[2]) ? 0 : 1;
}
