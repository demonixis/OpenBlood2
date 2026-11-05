#include "../resource_fixture.h"
#include "game/original/ViewWeapon.h"
#include <fstream>
#include <iostream>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Services final : WeaponServices
    {
        float clock = 0, ammo = 10000;
        bool present = true, alive = true, selected = true, left = false, remove_on_fire = false;
        DBYTE type = WEAP_BERETTA;
        FireResult result = FireResult::Fired;
        std::vector<WeaponFire> requests;
        std::vector<SoundRequest> sounds;
        struct SoundKey
        {
            WeaponSoundAction action;
            std::string resource;
        };
        std::vector<SoundKey> sound_keys;
        float time() const override
        {
            return clock;
        }
        bool has_player() const override
        {
            return present;
        }
        bool player_alive() const override
        {
            return present && alive;
        }
        bool weapon_selected(DBYTE value, bool hand) const override
        {
            return present && selected && type == value && left == hand;
        }
        float ammunition(bool) const override
        {
            return ammo;
        }
        FireResult fire(const WeaponFire& value) override
        {
            requests.push_back(value);
            present = !remove_on_fire;
            return result;
        }
        void animation_sound(DBYTE weapon, WeaponSoundAction action, std::string_view resource, std::string&) override
        {
            sound_keys.push_back({action, std::string(resource)});
            if (weapon == WEAP_SHOTGUN && action == WeaponSoundAction::OneShot)
            {
                SoundRequest request;
                request.resource = resource;
                request.space = SoundSpace::Local;
                sounds.push_back(std::move(request));
            }
        }
    };
    bool step(CViewWeapon& view, Services& services, WeaponInput input, unsigned count, std::string& error)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            services.clock += 1.f / 60;
            CHECK(view.Update(services, input, {1, 2, 3}, {0, 0, 1}, 1. / 60, error));
        }
        return true;
    }
    void add(test_resource::Directory& dir, std::string_view path, const std::vector<std::uint8_t>& bytes)
    {
        std::string key;
        if (!resource_path(path, key))
            return;
        auto* parent = &dir;
        std::size_t from = 0;
        while (true)
        {
            const auto slash = key.find('/', from);
            if (slash == key.npos)
            {
                parent->files[key.substr(from)] = bytes;
                return;
            }
            parent = &parent->children[key.substr(from, slash - from)];
            from = slash + 1;
        }
    }
    bool fixture(Resources& resources, const std::filesystem::path& root, std::string& error, bool presentation = true)
    {
        test_resource::Directory dir;
        for (auto type : {WEAP_BERETTA, WEAP_SHOTGUN, WEAP_MELEE})
        {
            const auto& data = *weapon_definition(type);
            add(dir, data.m_szViewModelFilename, test_model::abc(true).b);
            add(dir, data.m_szLeftViewModelFilename, test_model::abc(true).b);
            if (presentation)
                add(dir, data.m_szViewModelSkin, {1, 2, 3});
        }
        test_resource::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, offset);
        rez.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        CHECK(!ec);
        std::ofstream out(root / "weapons.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
        out.close();
        CHECK(out && resources.mount(root / "weapons.rez", error));
        return true;
    }
    bool synthetic(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(fixture(resources, root, error));
        ModelStore models(resources);
        Resources timing_resources;
        CHECK(fixture(timing_resources, root / "timing-only", error, false));
        ModelStore timing_models(timing_resources);
        CViewWeapon timing;
        CHECK(!timing.Create(WEAP_BERETTA, false, timing_models, timing_resources, error));
        CHECK(timing.Create(WEAP_BERETTA, false, timing_models, timing_resources, error, 0,
                            CViewWeapon::ResourcesMode::AnimationOnly));
        CHECK(timing.model && !timing.flashes[0] && !timing.flashes[1]);
        CViewWeapon view;
        Services services;
        CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
        CHECK(step(view, services, {}, 30, error) && view.state == WS_REST && !view.fire_keys);
        CHECK(step(view, services, {true}, 120, error));
        CHECK(view.shots && view.shots == services.requests.size() && view.fire_keys == view.shots);
        for (const auto& shot : services.requests)
            CHECK(shot.weapon == WEAP_BERETTA && !shot.alternate && !shot.left && shot.seed >= 2 &&
                  shot.position.x == 1 && shot.position.y == 2 && shot.position.z == 3 && shot.direction.z == 1);
        CHECK(step(view, services, {}, 30, error));
        const auto shots = view.shots;
        view.binoculars_active = true;
        CHECK(step(view, services, {true}, 120, error));
        CHECK(view.shots == shots && view.binocular_refusals);
        view.binoculars_active = false;
        services.alive = false;
        view.flash_visible = 1;
        const auto track = view.animation;
        CHECK(step(view, services, {true}, 10, error));
        CHECK(!view.flash_visible && view.animation.time == track.time && view.shots == shots);
        services.alive = true;
        services.selected = false;
        CHECK(!view.Update(services, {}, {}, {0, 0, 1}, 1. / 60, error));
        services = {};
        CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
        services.ammo = 0;
        CHECK(step(view, services, {true}, 120, error) && services.requests.empty());
        for (auto result : {FireResult::Dry, FireResult::Blocked})
        {
            services = {};
            services.result = result;
            CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
            CHECK(step(view, services, {true}, 120, error));
            CHECK(!services.requests.empty() && !view.shots && view.last_result == result);
        }
        for (auto result : {FireResult::Invalid, FireResult::Unavailable, FireResult::DeliveryFailed})
        {
            services = {};
            services.result = result;
            CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
            bool refused = false;
            for (unsigned frame = 0; frame < 120 && !refused; ++frame)
            {
                services.clock += 1.f / 60;
                refused = !view.Update(services, {true}, {}, {0, 0, 1}, 1. / 60, error);
            }
            CHECK(refused && services.requests.size() == 1 && !view.shots && view.last_result == result);
        }
        // Two keys in the same animation sample: removal at the first delivery
        // suppresses the second request without touching a stale owner.
        services = {};
        services.remove_on_fire = true;
        CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
        auto model = std::make_shared<Model>(*view.model);
        for (auto& clip : model->animations)
            if (clip.name == "fire")
                clip.keys[1].text = "FIRE_KEY; FIRE_KEY";
        view.model = model;
        CHECK(step(view, services, {true}, 120, error));
        CHECK(services.requests.size() == 1 && view.shots == 1);
        // Original shotgun animation sound stays local and collapses duplicate
        // separators. Other weapon model SOUND_KEY requests keep their old path.
        services = {};
        services.type = WEAP_SHOTGUN;
        CHECK(view.Create(WEAP_SHOTGUN, false, models, resources, error));
        model = std::make_shared<Model>(*view.model);
        model->animations[view.animation.animation].keys[1].text = "SOUND_KEY Shotgun\\\\reload.wav";
        view.model = model;
        CHECK(step(view, services, {}, 30, error));
        CHECK(services.sounds.size() == 1 && services.sounds[0].resource == "Sounds/Weapons/Shotgun/reload.wav" &&
              services.sounds[0].space == SoundSpace::Local);
        services = {};
        CHECK(view.Create(WEAP_BERETTA, false, models, resources, error));
        model = std::make_shared<Model>(*view.model);
        model->animations[view.animation.animation].keys[1].text =
            "sound_key Beretta\\\\reload.wav; SOUNDLOOP_KEY Tesla/charge.wav; SOUNDSTOP_KEY; SOUND_KEY";
        view.model = model;
        CHECK(step(view, services, {}, 30, error));
        CHECK(services.sounds.empty() && services.sound_keys.size() == 3);
        CHECK(services.sound_keys[0].action == WeaponSoundAction::OneShot &&
              services.sound_keys[0].resource == "Sounds/Weapons/Beretta/reload.wav");
        CHECK(services.sound_keys[1].action == WeaponSoundAction::Loop &&
              services.sound_keys[1].resource == "Sounds/Weapons/Tesla/charge.wav");
        CHECK(services.sound_keys[2].action == WeaponSoundAction::Stop && services.sound_keys[2].resource.empty());
        std::cout << "Independent view weapon: firing, ammunition, binocular/death/selection gates, removal and sound "
                     "passed\n";
        return true;
    }
    bool retail(const std::filesystem::path& root)
    {
        std::string error;
        Config config;
        config.data_dir = root;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ModelStore models(resources);
        unsigned fired = 0, modes = 0, sound_keys = 0, loop_keys = 0, stop_keys = 0;
        std::set<std::string> sound_assets;
        AudioTimeline audio;
        for (DBYTE type : {DBYTE(1),  DBYTE(2),  DBYTE(3),  DBYTE(4),  DBYTE(5),  DBYTE(6),  DBYTE(7),
                           DBYTE(8),  DBYTE(9),  DBYTE(10), DBYTE(11), DBYTE(12), DBYTE(13), DBYTE(14),
                           DBYTE(15), DBYTE(16), DBYTE(17), DBYTE(49), DBYTE(50), DBYTE(51)})
        {
            for (bool alternate : {false, true})
            {
                if (alternate && weapon_definition(type)->m_bAltFireZoom)
                    continue;
                CViewWeapon view, timing;
                Services services, logical;
                services.type = type;
                logical.type = type;
                CHECK(view.Create(type, false, models, resources, error));
                CHECK(
                    timing.Create(type, false, models, resources, error, 0, CViewWeapon::ResourcesMode::AnimationOnly));
                CHECK(!timing.flashes[0] && !timing.flashes[1]);
                for (unsigned frame = 0; frame < 720; ++frame)
                {
                    const auto input = frame >= 180 && frame < 540 ? WeaponInput{!alternate, alternate} : WeaponInput{};
                    CHECK(step(view, services, input, 1, error) && step(timing, logical, input, 1, error));
                    CHECK(view.state == timing.state && view.animation.animation == timing.animation.animation &&
                          view.animation.time == timing.animation.time &&
                          view.animation.revision == timing.animation.revision && view.fire_keys == timing.fire_keys &&
                          view.shots == timing.shots && view.second_barrel == timing.second_barrel &&
                          view.bomb_held == timing.bomb_held && view.bomb_speed == timing.bomb_speed &&
                          view.voodoo_hit == timing.voodoo_hit);
                    CHECK(services.requests.size() == logical.requests.size());
                    CHECK(services.sound_keys.size() == logical.sound_keys.size());
                    if (!services.sound_keys.empty())
                    {
                        CHECK(services.sound_keys.back().action == logical.sound_keys.back().action);
                        CHECK(services.sound_keys.back().resource == logical.sound_keys.back().resource);
                    }
                    if (!services.requests.empty())
                    {
                        const auto& a = services.requests.back();
                        const auto& b = logical.requests.back();
                        CHECK(a.seed == b.seed && a.weapon == b.weapon && a.left == b.left &&
                              a.alternate == b.alternate && a.projectile_speed == b.projectile_speed &&
                              a.voodoo_hit == b.voodoo_hit);
                    }
                }
                std::cout << "type=" << unsigned(type) << " alt=" << alternate << " shots=" << view.shots
                          << " keys=" << view.fire_keys << " state=" << view.state << '\n';
                CHECK(!services.requests.empty() && view.shots == services.requests.size() && view.fire_keys);
                for (const auto& request : services.requests)
                    CHECK(request.weapon == type && request.seed >= 2 && request.voodoo_hit <= 4 &&
                          request.projectile_speed >= 0 && request.projectile_speed <= 1750);
                fired += view.shots;
                ++modes;
                sound_keys += services.sound_keys.size();
                for (const auto& key : services.sound_keys)
                {
                    loop_keys += key.action == WeaponSoundAction::Loop;
                    stop_keys += key.action == WeaponSoundAction::Stop;
                    if (key.action != WeaponSoundAction::Stop && sound_assets.insert(key.resource).second)
                    {
                        SoundRequest request;
                        request.resource = key.resource;
                        request.loop = key.action == WeaponSoundAction::Loop;
                        AudioVoice voice;
                        if (!audio.prepare_voice(resources, request, {}, voice, error))
                        {
                            std::cerr << "Weapon key asset " << key.resource << ": " << error << '\n';
                            return false;
                        }
                        CHECK(voice.clip && voice.clip->frames());
                    }
                    if (key.action == WeaponSoundAction::Loop)
                        std::cout << "Loop key: type=" << unsigned(type) << " alt=" << alternate
                                  << " resource=" << key.resource << '\n';
                }
            }
        }
        for (DBYTE character = 0; character < 4; ++character)
            for (bool left : {false, true})
            {
                CViewWeapon view;
                Services services;
                services.left = left;
                CHECK(view.Create(WEAP_BERETTA, left, models, resources, error, character));
                CHECK(step(view, services, {true, false, left}, 240, error) && view.shots);
            }
        std::cout << "Retail independent view weapons: modes=" << modes << ", shots=" << fired
                  << ", sound_keys=" << sound_keys << ", loops=" << loop_keys << ", stops=" << stop_keys
                  << ", decoded_assets=" << sound_assets.size()
                  << ", identical animation-only timing/requests, four characters and both hands passed\n";
        CHECK(sound_keys);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "retail" ? retail(path_from_utf8(argv[2]))
                                                  : synthetic(path_from_utf8(argv[2])))
               ? 0
               : 1;
}
