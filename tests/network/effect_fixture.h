#pragma once
#include "net/effect_events.h"
namespace effect_fixture
{
    inline OpenBlood2::Net::EffectEvent sample(unsigned kind)
    {
        using namespace OpenBlood2;
        Net::EffectEvent e{7, 2, kind + 1, {15, 3}, "detached impact", {}};
        if (kind == 0)
        {
            auto& s = e.recipe.emplace<ShotgunEffectSettings>();
            s.source = {0, 80, 0};
            s.destination = {0, 1, 0};
            s.forward = {0, 0, 1};
            s.normal = {0, 1, 0};
            s.ammo = AMMO_BULLET;
            s.eject = true;
            s.surface = SURFTYPE_METAL;
        }
        else if (kind == 1)
        {
            auto& s = e.recipe.emplace<WeaponSpriteSettings>();
            auto& f = s.flash.emplace();
            f.position = {0, 80, 0};
            f.scale1 = f.scale2 = {.1f, .1f, 0};
            f.resource = "Sprites/impact01.spr";
            f.duration = .15f;
            auto& smoke = s.smoke.emplace();
            smoke.position = {0, 80, 0};
            smoke.normal = {0, 1, 0};
            smoke.scale1 = {.1f, .1f, 0};
            smoke.scale2 = {.35f, .35f, 0};
            smoke.alpha = .65f;
            smoke.duration = 1;
            smoke.fade = smoke.move = true;
            smoke.resource = "Sprites/smokepuff1.spr";
            smoke.gravity.y = .07f;
        }
        else if (kind == 2)
        {
            auto& s = e.recipe.emplace<SparkSettings>();
            s.position = {0, 80, 0};
            s.direction = {0, 75, 0};
            s.color1 = {255, 255, 0};
            s.color2 = {255, 255, 255};
            s.count = 30;
            s.duration = .5f;
            s.emission_radius = .3f;
            s.radius = 175;
            s.gravity = -150;
            s.texture = "spritetextures/particles/particle2.dtx";
        }
        else if (kind == 3)
        {
            auto& r = e.recipe.emplace<BloodSpurtRecipe>();
            r.immediate = true;
            r.settings.position = {0, 80, 0};
            r.settings.min_velocity = r.settings.max_velocity = {0, -100, 0};
            r.settings.lifetime = 1.2f;
            r.settings.fade_time = .8f;
            e.source = {}; // Original direct client publication has no source object.
        }
        else
        {
            auto& s = e.recipe.emplace<BloodSplatSettings>();
            s.position = {100, .1f, 0};
            s.scale = .1f;
            s.grow_scale = .02f;
            e.source = {};
        }
        return e;
    }
} // namespace effect_fixture
