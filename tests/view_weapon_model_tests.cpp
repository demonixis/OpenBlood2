#include "audio_fixture.h"
#include "game/original/ViewWeaponModel.h"
#include "player_fixture.h"
#include <cstring>

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
    bool mount_audio(GameSession& game, const std::filesystem::path& root)
    {
        test_player::Directory dir;
        dir.children["sounds"].children["weapons"].children["beretta"].files["key.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        const auto path = root / "view-weapon-audio.rez";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return out.good() && game.resources.mount(path, error);
    }

    bool key(GameSession& game, HOBJECT model, std::string command, std::string argument = {})
    {
        char* argv[]{command.data(), argument.data()};
        ArgList args{argv, argument.empty() ? 1 : 2};
        return game.objects.engine_message(model, MID_MODELSTRINGKEY, &args) != 0;
    }

    bool view_weapon_model(const std::filesystem::path& root)
    {
        GameSession game;
        std::string error;
        CHECK(test_player::setup(game, root) && mount_audio(game, root) && game.enter_player(error));
        auto* player = game.server_shell.Player(game.local_client);
        CHECK(player && player->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA) == InventoryResult::Ok);
        auto* left = player->m_InventoryMgr.CurrentWeapon(true);
        auto* right = player->m_InventoryMgr.CurrentWeapon(false);
        CHECK(left && right && left != right);

        ObjectCreateStruct state;
        std::strcpy(state.m_Filename, "models/weapons/lh_beretta_pv.abc");
        std::strcpy(state.m_SkinName, "skins/weapons/c_beretta_pv_t.dtx");
        const auto* type = game.objects.find_class("CViewWeaponModel");
        const auto model = game.objects.create(type, state);
        auto* weapon_model = dynamic_cast<CViewWeaponModel*>(game.server.HandleToObject(model));
        const auto owner = player->m_hObject;
        const auto flags = FLAG_MODELGOURAUDSHADE | FLAG_REALLYCLOSE | FLAG_FORCECLIENTUPDATE;
        CHECK(weapon_model && game.server.GetObjectFlags(model) == flags && weapon_model->Init(owner, true));
        weapon_model->UseModelKeys(DTRUE);
        weapon_model->SetVisible(DTRUE);
        CHECK(game.server.GetObjectFlags(model) == (flags | FLAG_VISIBLE | FLAG_MODELKEYS));
        CHECK(key(game, model, "hide_key", "HAND"));
        DBOOL hidden = DFALSE;
        CHECK(game.server.GetModelNodeHideStatus(model, "hand", &hidden) == DE_OK && hidden);
        CHECK(key(game, model, "SHOW_KEY") && key(game, model, "HIDE_KEY", "missing"));
        CHECK(game.server.GetModelNodeHideStatus(model, "hand", &hidden) == DE_OK && hidden);
        CHECK(key(game, model, "SHOW_KEY", "hand"));
        CHECK(game.server.GetModelNodeHideStatus(model, "hand", &hidden) == DE_OK && !hidden);

        CHECK(key(game, model, "SOUND_KEY", "Beretta\\\\key.wav"));
        CHECK(game.server.audio.voices().size() == 1);
        const auto& one_shot = game.server.audio.voices().begin()->second;
        CHECK(one_shot.owner == owner && one_shot.request.speaker == owner && one_shot.request.priority == 6 &&
              one_shot.request.outer_radius == 1000 && !one_shot.request.loop);
        CHECK(key(game, model, "SOUNDLOOP_KEY", "Beretta\\key.wav") && key(game, model, "SOUNDLOOP_KEY", "Beretta\\key.wav"));
        CHECK(game.server.audio.voices().size() == 2 && game.server.audio.voices().rbegin()->second.request.loop);
        CHECK(key(game, model, "SOUNDSTOP_KEY") && game.server.audio.voices().size() == 1);

        WeaponFire shot;
        CHECK(game.server.GetObjectPos(owner, &shot.position) == DE_OK);
        shot.position.y += player->eye_level;
        shot.weapon = WEAP_BERETTA;
        CHECK(!weapon_model->QueueFire(shot));
        shot.left = true;
        CHECK(weapon_model->QueueFire(shot) && !weapon_model->QueueFire(shot));
        const auto animation = game.server.GetAnimIndex(model, "fire");
        CHECK(animation != no_animation);
        const auto ammo = player->m_InventoryMgr.GetAmmoCount(AMMO_BULLET);
        game.server.SetModelAnimation(model, animation);
        CHECK(game.server.advance_models(.06));
        CHECK(left->shots == 1 && right->shots == 0 && player->m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == ammo - 1);
        CHECK(key(game, model, "FIRE_KEY") && left->shots == 1);
        CHECK(weapon_model->QueueFire(shot) && player->m_InventoryMgr.ChangeWeaponSlot(0) == InventoryResult::Ok);
        CHECK(key(game, model, "FIRE_KEY") && left->shots == 1);
        CHECK(player->m_InventoryMgr.ChangeWeaponSlot(1) == InventoryResult::Ok);
        CHECK(key(game, model, "FIRE_KEY") && left->shots == 1);

        CHECK(key(game, model, "SOUNDLOOP_KEY", "Beretta\\key.wav"));
        const auto loop = game.server.audio.voices().rbegin()->first;
        CHECK(game.server.audio.get(loop)->request.loop);
        CHECK(game.objects.remove(model) && game.objects.finish_frame() && !game.server.audio.get(loop));

        const auto linked = game.objects.create(type, state);
        auto* linked_model = dynamic_cast<CViewWeaponModel*>(game.server.HandleToObject(linked));
        CHECK(linked_model && linked_model->Init(owner, true) && key(game, linked, "SOUNDLOOP_KEY", "Beretta\\key.wav"));
        const auto linked_loop = game.server.audio.voices().rbegin()->first;
        CHECK(game.objects.remove(owner) && game.objects.finish_frame());
        CHECK(!game.server.IsObjectAlive(linked) && !game.server.audio.get(linked_loop));
        return game.unload_world();
    }
}

int main(int argc, char** argv)
{
    return argc == 2 && view_weapon_model(argv[1]) ? 0 : 1;
}
