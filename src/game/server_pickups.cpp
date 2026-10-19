#include "game/original/WeaponPickups.h"
#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    bool split_spawn(std::string_view input, std::string& type, std::string& props)
    {
        type.clear();
        props.clear();
        // Original Spawner copies the request into a MAX_GP_STRING_LEN buffer.
        if (input.empty() || input.size() > MAX_GP_STRING_LEN || input.find('\0') != input.npos)
            return false;
        auto first = input.find_first_not_of(' ');
        if (first == input.npos)
            return false;
        input.remove_prefix(first);
        auto end = input.find(' ');
        type = input.substr(0, end);
        if (end != input.npos)
            props = input.substr(end + 1);
        return true;
    }
    bool ServerAdapter::ValidateSpawnObject(std::string_view input)
    {
        trace_service("ValidateSpawnObject");
        std::string type, props;
        if (!split_spawn(input, type, props))
            return false;
        bool supported = false;
        // Fanatic/Prophet: original Spawner requests (fanatic_prophet_contracts.md). Item and weapon
        // leaves bounce on string creation; SniperRiflePU's weapon is not integrated.
        for (auto name : {"BulletAmmoPU",      "ShellAmmoPU",    "BMGAmmoPU",     "FlareAmmoPU",
                          "DieBugDieAmmoPU",   "HowitzerAmmoPU", "FuelAmmoPU",    "BatteryAmmoPU",
                          "HealthPU",          "MegaHealthPU",   "WardPU",        "NecroWardPU",
                          "InvulnerabilityPU", "MedKitPU",       "FlashlightPU",  "BinocularsPU",
                          "ProximitiesPU",     "RemotesPU",      "BerettaPU",     "SubMachineGunPU",
                          "FlareGunPU",        "AssaultRiflePU", "TeslaCannonPU", "VoodooDollPU",
                          "ShotgunPU",         "SingularityPU",  "Fanatic",       "Prophet"})
            supported |= same_name(type, name);
        Properties parsed;
        return supported && objects_.parse_properties(objects_.find_class(type), props, parsed);
    }
    void ServerAdapter::RejectPrecreate(std::string_view reason)
    {
        trace_service("RejectPrecreate");
        objects_.note_rejection(reason);
    }
    void ServerAdapter::RecordSpawn(const SpawnReport& report)
    {
        trace_service("RecordSpawn");
        if (spawn_reports.size() == 32)
            spawn_reports.erase(spawn_reports.begin());
        spawn_reports.push_back(report);
        if (auto* r = objects_.get(report.child); r && !r->model_error.empty())
            spawn_reports.back().outcome += ": " + r->model_error;
    }
    void ServerAdapter::RecordPickup(HOBJECT h, const PickupReport& report)
    {
        trace_service("RecordPickup");
        auto* r = objects_.get(h);
        if (!r)
            return;
        if (pickup_reports.size() == 32)
            pickup_reports.erase(pickup_reports.begin());
        pickup_reports.push_back(report);
        pickup_reports.back().name = r->state.m_Name;
        pickup_reports.back().class_name = r->type->name;
    }
    DRESULT ServerAdapter::WritePickupMessage(HMESSAGEWRITE h, const PickupMessage& value)
    {
        trace_service("WritePickupMessage");
        auto* m = writable(h);
        if (!m)
            return DE_INVALIDPARAMS;
        const auto limit = value.kind == PickupKind::Ammo     ? 8
                           : value.kind == PickupKind::Weapon ? 255
                           : value.kind == PickupKind::Item   ? 8
                                                              : 8; // POWERUP_HEALTH..POWERUP_REVENANT
        const auto request = value.kind == PickupKind::Ammo     ? add_ammo_message
                             : value.kind == PickupKind::Weapon ? add_weapon_message
                             : value.kind == PickupKind::Item   ? add_item_message
                                                                : add_powerup_message;
        if (!m->routed ||
            (value.kind != PickupKind::Ammo && value.kind != PickupKind::Powerup && value.kind != PickupKind::Weapon &&
             value.kind != PickupKind::Item) ||
            (m->id != request && m->id != picked_up_message) || !m->bytes.empty() || m->damage || m->key || m->pickup ||
            !IsObjectAlive(m->sender) || value.type < 1 || value.type > limit ||
            (value.kind == PickupKind::Weapon && !WeaponPickupClass(value.type)) || !std::isfinite(value.amount) ||
            value.amount < 0 || value.amount > 1e6f ||
            (value.kind == PickupKind::Item &&
             ((value.type != 1 && value.type != 2 && value.type != 5 && value.type != 7 && value.type != 8) ||
              value.amount > 255 || std::floor(value.amount) != value.amount)))
        {
            m->bad = true;
            return DE_INVALIDPARAMS;
        }
        m->pickup = value;
        return DE_OK;
    }
    bool ServerAdapter::ReadPickupMessage(HMESSAGEREAD h, PickupMessage& value)
    {
        trace_service("ReadPickupMessage");
        value = {};
        auto it = messages_.find(h);
        if (it == messages_.end())
            return false;
        auto& m = it->second;
        if (!m.borrowed || m.bad || !m.pickup || !IsObjectAlive(m.sender))
        {
            m.bad = true;
            return false;
        }
        value = *m.pickup;
        return true;
    }
} // namespace OpenBlood2
