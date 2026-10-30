#include "game/game_settings.h"
#include "game/original/CameraObj.h"
#include "game/original/OrbProjectile.h"
#include "game/original/SeeingEye.h"
#include "server/session.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* path)
    {
        std::string error;
        Config config;
        config.data_dir = path;
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined joined;
        CHECK(session.join({}, joined, error) && session.tick(error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* owner = game.objects.get(joined.object);
        auto* player = dynamic_cast<CPlayerObj*>(owner->game.get());
        CHECK(player);
        MovementSetup setup;
        CameraState state;
        auto refresh = [&]()
        {
            CHECK(session.movement_setup(joined.player, setup, error));
            std::vector<PrivateEvent> events;
            CHECK(session.take_private_events(joined.player, events, error));
            for (const auto& event : events)
                if (const auto* view = std::get_if<CameraState>(&event.value))
                    state = *view;
            CHECK(state.force_revision == setup.owner.force_revision);
            return true;
        };
        CHECK(refresh() && setup.can_move && !state.scripted && !player->camera_movement_blocked);
        const auto create = [&](bool movement)
        {
            return game.objects.create(game.objects.find_class("CameraObj"), {}, Creation::World,
                                       {{"Pos", vector_add(owner->state.m_Pos, {50, 100, 50})},
                                        {"StartActive", true},
                                        {"AllowPlayerMovement", movement},
                                        {"IsListener", false},
                                        {"HidePlayer", true}});
        };
        const auto a = create(false), b = create(true);
        CHECK(a && b && refresh());
        CHECK(state.scripted == game.objects.get(a)->id && !setup.can_move && player->camera_movement_blocked);
        const auto pitch = player->m_fPitch, yaw = player->m_fYaw;
        MovementReport report;
        report.owner = setup.owner;
        report.sequence = 1;
        report.move.move_code = setup.code;
        report.move.position = owner->state.m_Pos;
        report.controls.movement.forward = true;
        report.controls.yaw = 1.8f;
        report.pitch = .6f;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(player->m_fPitch == pitch && player->m_fYaw == yaw);
        dynamic_cast<CameraObj*>(game.server.HandleToObject(a))->SetActive(false);
        CHECK(refresh() && state.scripted == game.objects.get(b)->id && setup.can_move);
        CHECK(state.settings.hide_player && !state.settings.listener && state.settings.player_movement);
        CHECK(session.tick(error) && player->m_fYaw == yaw); // Scripted movement permission is not look permission.
        CHECK(game.objects.remove(b) && game.objects.finish_frame() && refresh());
        CHECK(!state.scripted && setup.can_move && !player->camera_movement_blocked);
        ++report.sequence;
        report.move.position = owner->state.m_Pos;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(player->m_fPitch == report.pitch && player->m_fYaw == report.controls.yaw);
        CHECK(player->m_InventoryMgr.AddItem(4, 100) == InventoryResult::Ok);
        auto& eye = *player->m_InventoryMgr.eye;
        CHECK(eye.ActivateItem() && eye.eye && eye.SetActive(true) && refresh());
        const auto eye_id = game.objects.get(eye.eye)->id;
        CHECK(state.eye == eye_id && !setup.can_move && player->camera_movement_blocked);
        ++report.sequence;
        report.controls.yaw = .4f;
        report.pitch = -.25f;
        report.move.position = owner->state.m_Pos;
        CHECK(session.queue_movement(joined.player, report) == ServerSession::MoveResult::Queued &&
              session.tick(error));
        CHECK(player->m_fYaw == report.controls.yaw && player->m_fPitch == report.pitch);
        DRotation rotation;
        CHECK(game.server.GetObjectRotation(eye.eye, &rotation) == DE_OK);
        const auto expected = rotation_from_euler({report.pitch, report.controls.yaw, 0});
        CHECK(std::abs(rotation.m_Spin - expected.m_Spin) < .00001f);
        CHECK(eye.SetActive(false) && refresh() && !state.eye && setup.can_move);
        ObjectCreateStruct create_orb;
        create_orb.m_Pos = vector_add(owner->state.m_Pos, {0, 40, 0});
        auto* orb = dynamic_cast<COrbAltProjectile*>(
            game.server.CreateObject(game.server.GetClass("COrbAltProjectile"), &create_orb));
        CHECK(orb && orb->Setup({0, 0, 1}, WEAP_ORB, 12, 500, 0, owner->handle) && orb->StartCamera() && refresh());
        CHECK(state.orb == game.objects.get(orb->m_hObject)->id && !setup.can_move);
        CHECK(eye.SetActive(true) && refresh() && state.eye == eye_id && state.orb);
        const auto orb_handle = orb->m_hObject;
        CHECK(game.objects.remove(orb_handle) && game.objects.finish_frame() && refresh());
        CHECK(!state.orb && state.eye == eye_id);
        CHECK(eye.SetActive(false) && refresh() && setup.can_move);
        const auto force = state.force_revision;
        CHECK(player->TeleportClientToServerPos() && refresh() && state.force_revision > force && state.alive);
        CHECK(session.clear_players());
        std::cout << "Retail camera selection: creation order, movement/look gates, Eye/Orb ownership, retirement and "
                     "force state passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
