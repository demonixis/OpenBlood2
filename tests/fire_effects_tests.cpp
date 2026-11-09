#include "engine/collision.h"
#include "game/original/FireEffects.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << '\n';                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    bool mount(GameSession& game, const char* path)
    {
        Config config;
        config.data_dir = path;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        return true;
    }

    bool trigger(GameSession& game, BaseClass& receiver, const char* command)
    {
        auto message = game.server.StartMessageToObject(&receiver, receiver.m_hObject, 1000);
        auto text = game.server.CreateString(command);
        const bool written = message && text && game.server.WriteToMessageHString(message, text) == DE_OK;
        if (text)
            game.server.FreeString(text);
        return written && game.server.EndMessage(message) == DE_OK;
    }

    bool explosion(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path));
        const auto handle = game.objects.create(game.objects.find_class("Explosion"), {}, Creation::World,
                                                {{"Delay", .1f},
                                                 {"CreateShockwave", true},
                                                 {"ShockwaveScaleMin", DVector{.25f, .25f, 1}},
                                                 {"ShockwaveScaleMax", DVector{1, 1, 1}},
                                                 {"ShockwaveDuration", 3.f}});
        auto* effect = dynamic_cast<Explosion*>(game.server.HandleToObject(handle));
        CHECK(effect && game.objects.advance(.2) && !effect->model);
        CHECK(trigger(game, *effect, "trigger") && game.objects.advance(.09) && !effect->model);
        CHECK(game.objects.advance(.02));
        if (!effect->model || !effect->shockwave)
            for (const auto& error : game.objects.diagnostics())
                std::cerr << error << '\n';
        CHECK(effect->model && effect->light && effect->shockwave);
        if (game.resources.find("Sounds\\exp_tnt.wav"))
            CHECK(effect->voice && effect->sound_error.empty());
        else
            CHECK(!effect->voice && !effect->sound_error.empty());
        CHECK(game.server.GetObjectFlags(effect->model) & FLAG_NOLIGHT);
        CHECK(game.server.GetObjectFlags(effect->shockwave) & FLAG_ROTATEABLESPRITE);
        const auto model = effect->model, light = effect->light, shockwave = effect->shockwave;
        CHECK(game.objects.advance(.5));
        const auto* visible = game.objects.get(model);
        CHECK(visible && visible->state.m_Scale.x > .1f && visible->state.m_Scale.x < 1 && visible->alpha == .4f);
        CHECK(game.server.light_effects.size() == 1 && game.server.light_effects[0].settings.radius.max > 100);
        CHECK(game.objects.advance(1.1) && !game.server.IsObjectAlive(handle) && !game.server.IsObjectAlive(model) &&
              !game.server.IsObjectAlive(light) && game.server.IsObjectAlive(shockwave));
        CHECK(game.objects.advance(2) && !game.server.IsObjectAlive(shockwave));
        // Runtime Setup (the original Script call site) emits both independent scorch lights.
        ObjectCreateStruct state;
        auto* scripted = dynamic_cast<Explosion*>(game.server.CreateObject(game.server.GetClass("Explosion"), &state));
        CHECK(scripted && scripted->Setup("Sounds\\Weapons\\c4\\explosion_1.wav", 750, .5f, nullptr,
                                         100, 50, .1f, 2.f, true) && scripted->Explode());
        CHECK(game.objects.advance(.02) && scripted->model && scripted->voice && game.server.light_effects.size() == 3);
        return true;
    }

    unsigned flame_sprites(GameSession& game)
    {
        unsigned count = 0;
        for (auto id : game.objects.snapshot())
            if (const auto* record = game.objects.get(id); record && record->life == Life::Alive &&
                                                          same_name(record->state.m_Filename, "Sprites\\gibflame.spr"))
                ++count;
        return count;
    }

    bool fire(const char* path)
    {
        GameSession game;
        CHECK(mount(game, path));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(game.collision.build(floor, error));
        game.server.collision_ready = true;
        const auto owner = game.objects.create(game.objects.find_class("CultistAI"), {}, Creation::World,
                                               {{"Name", std::string("burning")}, {"Pos", DVector{0, 100, 0}}});
        auto* actor = dynamic_cast<CBaseCharacter*>(game.server.HandleToObject(owner));
        CHECK(actor);
        game.server.SetObjectState(owner, OBJSTATE_INACTIVE);
        CHECK(game.server.SetObjectColor(owner, {.1f, .2f, .3f}, 1));
        const auto effect = game.objects.create(game.objects.find_class("FireFX"), {}, Creation::World,
                                                {{"PlaceFireOn", std::string("burning")}, {"BurnTime", .05f}});
        auto* fire = dynamic_cast<FireFX*>(game.server.HandleToObject(effect));
        CHECK(fire && game.objects.advance(.11) && fire->target == owner && fire->light);
        const auto light = fire->light;
        DVector position{5, 100, 10};
        CHECK(game.server.TeleportObject(owner, &position) == DE_OK);
        for (unsigned frame = 0; frame < 100; ++frame)
            CHECK(game.objects.advance(.01));
        CHECK(flame_sprites(game) == 20 && game.objects.get(light)->state.m_Pos.x == 5);
        DVector color;
        float alpha = 0;
        CHECK(game.server.GetObjectColor(owner, color, alpha) && color.x == 1 && color.y == .2f && color.z == .3f);
        actor->m_damage.m_bDead = true;
        CHECK(game.objects.advance(.02) && flame_sprites(game) == 0 && game.server.IsObjectAlive(light));
        CHECK(game.objects.advance(4.95) && game.server.IsObjectAlive(effect));
        CHECK(game.objects.advance(.1) && !game.server.IsObjectAlive(effect) && !game.server.IsObjectAlive(light));
        // A runtime target link owns its effects even when the target is removed outright.
        actor->m_damage.m_bDead = false;
        ObjectCreateStruct state;
        auto* scripted = dynamic_cast<FireFX*>(game.server.CreateObject(game.server.GetClass("FireFX"), &state));
        CHECK(scripted && scripted->Setup(owner, 0) && scripted->light);
        const auto runtime = scripted->m_hObject, lamp = scripted->light;
        CHECK(game.objects.remove(owner) && game.objects.finish_frame() && game.objects.advance(.2) &&
              !game.server.IsObjectAlive(runtime) && !game.server.IsObjectAlive(lamp));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "explosion" ? explosion(argv[2]) : mode == "fire" && fire(argv[2])) ? 0 : 1;
}
