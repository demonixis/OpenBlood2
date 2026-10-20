#include "app/movement_controls.h"
#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include <iostream>
using namespace OpenBlood2;
void dump(GameSession &g, const char *phase) {
  std::cout << "PHASE " << phase << " t=" << g.objects.time() << "\n";
  for (auto id : g.objects.snapshot()) {
    auto *r = g.objects.get(id);
    if (auto *a = dynamic_cast<AI_Mgr *>(r->game.get()))
      std::cout << r->state.m_Name << " @" << id.slot
                << " pos=" << r->state.m_Pos.x << "," << r->state.m_Pos.y << ","
                << r->state.m_Pos.z << " hp=" << a->m_damage.m_fHitPoints
                << " armor=" << a->m_damage.m_fArmorPoints
                << " state=" << a->m_nState << " phase=" << a->Metacmd
                << " active=" << r->activity
                << " target=" << (bool)a->GetTarget()
                << " pings=" << r->activation_pings
                << " visibility=" << int(r->activation_probe.visibility)
                << " updates=" << a->runtime_updates
                << " failed=" << a->runtime_error << "\n";
  }
}
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  auto g = std::make_unique<GameSession>();
  Config c;
  c.data_dir = argv[1];
  std::string e;
  if (!g->initialize() || !g->resources.mount_game(c, e) ||
      !g->request_world("01_SubwayA", e) || !g->enter_player(e))
    return 3;
  auto step = [&](int n) {
    for (int i = 0; i < n; ++i)
      if (!g->step(1. / 60)) {
        std::cerr << "STEP " << e << "\n";
        for (auto &v : g->trace)
          std::cerr << v << '\n';
        return false;
      }
    return true;
  };
  auto *p = g->server_shell.Player(g->local_client);
  p->m_damage.can_damage = false;
  if (!step(60 * 13))
    return 4;
  for (int level = 0; level < 2; ++level) {
    p = g->server_shell.Player(g->local_client);
    auto *w = p->m_InventoryMgr.CurrentWeapon();
    std::cout << "ENTRY " << g->world_path
              << " difficulty=" << int(p->GetGameDifficulty())
              << " hp=" << p->m_damage.m_fHitPoints
              << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
              << " movement_block=" << p->movement_blocked
              << " camera_block=" << p->camera_movement_blocked << "\n";
    g->client_shell.rotation.pitch = -1.4f;
    for (bool alt : {false, true}) {
      auto shots = w->shots;
      g->client_shell.weapon_input = {!alt, alt, false};
      if (!step(120))
        return 5;
      g->client_shell.weapon_input = {};
      if (!step(60))
        return 6;
      std::cout << "FIRE alt=" << alt << " shots=" << w->shots - shots
                << " ammo=" << p->m_InventoryMgr.GetAmmoCount(1)
                << " view=" << g->client_shell.weapons[0].state
                << " result=" << int(g->client_shell.weapons[0].last_result)
                << "\n";
    }
    dump(*g, "after_fire");
    if (!level) {
      WorldMessage m;
      if (!original_world_request("02_CityHub", 0, LOADTYPE_NEW_LEVEL, 0, m) ||
          !g->queue_level_change(m, "01_egressa", e) ||
          GameSession::process_level_change(g, e) !=
              LevelChangeResult::Committed) {
        std::cerr << e;
        return 7;
      }
      if (!step(60))
        return 8;
    }
  }
  p = g->server_shell.Player(g->local_client);
  p->m_damage.can_damage = false;
  struct Spot {
    DVector pos;
    std::string name;
  };
  std::vector<Spot> spots;
  for (auto id : g->objects.snapshot()) {
    auto *r = g->objects.get(id);
    if (dynamic_cast<AI_Mgr *>(r->game.get()))
      spots.push_back({r->state.m_Pos, r->state.m_Name});
  }
  for (auto &spot : spots) {
    auto pos = spot.pos;
    pos.z -= 160;
    pos.y += 64;
    CollisionHit hit;
    if (g->collision.sweep(pos, pos, {20, 42, 20}, hit) != CollisionQuery::Miss)
      continue;
    g->objects.get(p->m_hObject)->state.m_Pos = pos;
    if (!step(300))
      return 9;
    dump(*g, spot.name.c_str());
  }
}
