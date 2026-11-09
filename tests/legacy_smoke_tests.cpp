#include "brush_fixture.h"
#include "game/original/ClientSmokeTrail.h"
#include "game/server_adapter.h"
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
    HOBJECT create(GameSession& game, DVector at, DVector velocity, bool small)
    {
        ObjectCreateStruct state;
        state.m_Pos = at;
        const auto handle = game.objects.create(game.objects.find_class("CClientSmokeTrail"), state);
        auto* trail = dynamic_cast<CClientSmokeTrail*>(game.server.HandleToObject(handle));
        return trail && trail->Setup(velocity, small) ? handle : nullptr;
    }

    const EffectParticles* first_puff(const GameSession& game, float origin_x)
    {
        for (const auto& item : game.client_session.effects.frame().billboards)
            if (item.id.kind == EffectKind::LegacySmoke)
                if (const auto* puff = std::get_if<EffectParticles>(&item.value); puff && puff->origin.x == origin_x)
                    return puff;
        return nullptr;
    }

    bool smoke(const char* path)
    {
        Config config;
        config.data_dir = path;
        GameSession game;
        std::string error;
        CHECK(game.initialize() && game.resources.mount_game(config, error));
        game.world.models.push_back(test_brush::box("water", {450, 0, -50}, {550, 100, 50}));
        CHECK(game.objects.create(game.objects.find_class("Water"), {}, Creation::World,
                                  {{"Name", std::string("water")}, {"ShowSurface", false}}));
        ContainerCode code = CC_NOTHING;
        CHECK(game.server.GetPointContainerCode({500, 50, 0}, code, false) && code == CC_WATER);
        constexpr auto original_bubble = "spritetextures/particletextures/particlebubble.dtx";
        const auto bubble_texture = game.resources.find(original_bubble) ? original_bubble
                                                                        : "spritetextures/particles/particlebubble.dtx";
        CHECK(game.resources.find(bubble_texture));
        auto moving = create(game, {10, 100, 0}, {300, 0, 0}, false);
        auto stationary = create(game, {100, 100, 0}, {}, true);
        auto liquid = create(game, {500, 50, 0}, {300, 0, 0}, true);
        CHECK(moving && stationary && liquid);
        CHECK(game.publish_client_world(error) && game.client_session.effects.frame().billboards.empty());
        CHECK(game.objects.advance(.03) && game.publish_client_world(error));
        const auto* puff = first_puff(game, 10);
        const auto* bubbles = first_puff(game, 500);
        CHECK(puff && puff->texture == "spritetextures/smoke32_2.dtx" && puff->particles.size() == 10 &&
              puff->alpha == .5f && puff->half_extent == 1000.f / 640);
        CHECK(bubbles && bubbles->texture == bubble_texture &&
              bubbles->particles.size() == 30 && bubbles->half_extent == 125.f / 640);
        CHECK(!first_puff(game, 100));

        DVector position{42, 100, 0};
        CHECK(game.server.TeleportObject(moving, &position) == DE_OK);
        CHECK(game.objects.advance(.03) && game.publish_client_world(error));
        puff = first_puff(game, 10);
        CHECK(puff && puff->particles.size() == 90 && puff->particles.back().position.x > 20);
        CHECK(game.objects.advance(.06) && game.publish_client_world(error));
        const auto* slow = first_puff(game, 100);
        CHECK(slow && slow->particles.size() == 1 && slow->half_extent == 250.f / 640);

        for (auto handle : {moving, stationary, liquid})
            CHECK(game.objects.remove(handle));
        CHECK(game.objects.finish_frame() && game.objects.advance(.3) && game.publish_client_world(error));
        puff = first_puff(game, 10);
        CHECK(puff && puff->alpha < 1 && puff->alpha > 0); // Segments outlive the emitter.
        CHECK(game.objects.advance(1) && game.publish_client_world(error));
        CHECK(game.client_session.effects.frame().billboards.empty());
        return game.unload_world();
    }
}

int main(int argc, char** argv)
{
    return argc == 2 && smoke(argv[1]) ? 0 : 1;
}
