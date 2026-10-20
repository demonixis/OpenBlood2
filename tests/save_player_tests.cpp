#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/CharacterProfiles.h"
#include "save_world_fixture.h"
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

// The player and client lot of save/load (save_load_contracts lot 5): the original CPlayerObj,
// CMovement, CInventoryMgr and CDestructable byte layouts, the client blob, the restored
// association and the shell console, through GameSession::save_game/load_game.
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
    std::filesystem::path root;
    using save_world::object;
    using save_world::text;
    using save_world::vector;

    bool write_fixture()
    {
        return save_world::write_archive(
            root / "player.rez",
            {{"player",
              save_world::dat({object("WorldProperties", {text("Name", "WorldProperties0")}),
                               object("GameStartPoint", {text("Name", "start"), vector("Pos", {0, 10, 0})})})},
             {"next",
              save_world::dat({object("GameStartPoint", {text("Name", "arrival"), vector("Pos", {40, 10, 40})})})}});
    }

    // An independent reader of the original layouts. Each field is recorded by name with its
    // offset, so a test checks values, order and exact consumption, and can patch a field.
    struct Field
    {
        std::string name;
        std::size_t offset = 0, size = 0;
        std::uint32_t u = 0;
        float f = 0;
        std::optional<std::string> text;
        std::vector<std::uint8_t> bytes;
    };
    struct Decoder
    {
        std::span<const std::uint8_t> data;
        std::size_t at = 0;
        bool ok = true;
        std::vector<Field> fields;
        std::uint32_t raw(std::size_t n)
        {
            if (!ok || data.size() - at < n)
            {
                ok = false;
                return 0;
            }
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < n; ++i)
                value |= std::uint32_t(data[at + i]) << (8 * i);
            at += n;
            return value;
        }
        Field& add(std::string name, std::size_t size)
        {
            Field field;
            field.name = std::move(name);
            field.offset = at;
            field.size = size;
            fields.push_back(std::move(field));
            return fields.back();
        }
        std::uint32_t integer(std::string name, std::size_t size)
        {
            auto& field = add(std::move(name), size);
            return field.u = raw(size);
        }
        std::uint32_t b(std::string name)
        {
            return integer(std::move(name), 1);
        }
        std::uint32_t w(std::string name)
        {
            return integer(std::move(name), 2);
        }
        std::uint32_t d(std::string name)
        {
            return integer(std::move(name), 4);
        }
        void f(std::string name)
        {
            auto& field = add(std::move(name), 4);
            field.u = raw(4);
            field.f = std::bit_cast<float>(field.u);
        }
        void v(const std::string& name)
        {
            for (auto axis : {".x", ".y", ".z"})
                f(name + axis);
        }
        void r(const std::string& name)
        {
            v(name);
            f(name + ".w");
        }
        void s(std::string name) // HString: u32 length, 0xFFFFFFFF for NULL.
        {
            auto& field = add(std::move(name), 4);
            const auto length = raw(4);
            if (length == 0xFFFFFFFF || !ok)
                return;
            if (data.size() - at < length)
            {
                ok = false;
                return;
            }
            field.text = std::string(data.begin() + static_cast<std::ptrdiff_t>(at),
                                     data.begin() + static_cast<std::ptrdiff_t>(at + length));
            field.size += length;
            at += length;
        }
        void n(std::string name) // Nested message: u32 length, then bytes.
        {
            auto& field = add(std::move(name), 4);
            const auto length = raw(4);
            if (!ok || data.size() - at < length)
            {
                ok = false;
                return;
            }
            field.bytes.assign(data.begin() + static_cast<std::ptrdiff_t>(at),
                               data.begin() + static_cast<std::ptrdiff_t>(at + length));
            field.size += length;
            at += length;
        }
        const Field* find(std::string_view name) const
        {
            for (const auto& field : fields)
                if (field.name == name)
                    return &field;
            return nullptr;
        }
        std::uint32_t u(std::string_view name) const
        {
            const auto* field = find(name);
            return field ? field->u : 0xDEADBEEF;
        }
        float real(std::string_view name) const
        {
            const auto* field = find(name);
            return field ? field->f : std::nanf("");
        }
        std::optional<std::string> string(std::string_view name) const
        {
            const auto* field = find(name);
            return field ? field->text : std::optional<std::string>("<missing>");
        }
        bool done() const
        {
            return ok && at == data.size();
        }
    };
    constexpr const char* power_names[]{"invulnerable", "invisible", "triple", "incorporeal", "silent"};
    // Original CPlayerObj::Save (PlayerObj.cpp:6541-6683).
    void player_block(Decoder& d)
    {
        d.b("demo_playback");
        d.d("control_flags");
        d.b("firing_weapon");
        d.b("falling");
        d.b("server_falling");
        for (auto name : {"fall_start", "idle_start", "min_fall", "max_fall", "binding_regen", "server_regen"})
            d.f(name);
        d.b("spectator");
        d.f("smell_time");
        d.f("smell_delay");
        d.b("dead");
        d.b("movement_blocked");
        for (auto power : power_names)
            d.b(std::string("power.") + power);
        for (auto power : power_names)
            d.f(std::string("expiry.") + power);
        const auto stack = d.b("sound_stack");
        for (unsigned i = 0; i < stack && d.ok; ++i)
            d.b("sound." + std::to_string(i));
        d.s("trigger");
        for (auto name : {"last_pos", "last_vel", "scale"})
            d.v(name);
        d.f("pitch");
        d.f("yaw");
        d.r("view");
        for (auto name : {"forward", "gun_muzzle", "lgun_muzzle"})
            d.v(name);
        d.s("name");
        d.b("client_flags");
        d.f("mouse0");
        d.f("mouse1");
        d.b("zoom");
        d.b("zoom_type");
        d.d("camera");
        d.b("last_crouch");
        d.b("last_jump");
        d.b("forced_crouch");
        d.d("frags");
        d.d("melee_kills");
        d.b("character");
        d.b("skin");
        for (auto name : {"strength", "speed", "resistance", "magic"})
            d.b(std::string("attr.") + name);
        for (auto name : {"strength", "speed", "resistance", "magic"})
            d.b(std::string("binding_attr.") + name);
        for (int i = 0; i < 8; ++i)
            d.b("binding." + std::to_string(i));
        d.b("move_state");
        d.w("injured_leg");
        d.w("node_hit");
        d.w("side_hit");
        d.f("death_timer");
        d.b("slow_death");
        d.f("old_air");
        d.f("air");
        d.b("swimming_jump");
        d.b("can_swim_jump");
        d.b("imprisoned");
        d.f("imprison_start");
        d.f("imprison_length");
        d.b("burning");
        d.f("burning_time");
        if (d.b("client_data"))
            d.n("blob");
    }
    // Original CMovement::Save (movement.cpp:584-606).
    void movement_block(Decoder& d)
    {
        d.d("move.owner");
        d.r("move.rotation");
        for (auto name : {"move.right", "move.up", "move.forward", "move.pos", "move.last_pos"})
            d.v(name);
        const auto points = d.d("move.points");
        for (unsigned i = 0; i < points && d.ok; ++i)
            d.v("move.point." + std::to_string(i));
    }
    // Original CInventoryMgr::Save (InventoryMgr.cpp:2843-2917) with CInvItem::Save (InvItem.h:193-207).
    void inventory_block(Decoder& d)
    {
        d.d("inv.open");
        d.b("inv.strength");
        d.b("inv.magic");
        d.b("inv.god");
        d.b("inv.infinite");
        for (int i = 0; i < 10; ++i)
        {
            d.b("inv.weapon." + std::to_string(i));
            d.b("inv.lweapon." + std::to_string(i));
        }
        for (int i = 0; i < 3; ++i)
            d.b("inv.bomb." + std::to_string(i));
        for (int i = 0; i < 13; ++i)
            d.f("inv.ammo." + std::to_string(i));
        const auto items = d.d("inv.items");
        for (unsigned i = 0; i < items && d.ok; ++i)
        {
            const auto prefix = "inv.item." + std::to_string(i) + ".";
            const auto type = d.b(prefix + "list_type");
            d.d(prefix + "owner");
            for (auto name : {"name", "display", "pic", "pic_h"})
                d.s(prefix + name);
            for (auto name : {"initialized", "dropped", "type", "active", "count"})
                d.b(prefix + name);
            if (type == 2 || type == 3)
                d.f(prefix + "discharge");
            if (type == 4)
                d.d(prefix + "seeing");
        }
        for (auto name : {"inv.cur_slot", "inv.cur_weapon", "inv.cur_item", "inv.weap_count"})
            d.d(name);
        for (auto name : {"inv.damage", "inv.fire_rate", "inv.melee"})
            d.f(name);
        d.b("inv.drop_eye");
        d.d("inv.close");
    }
    // Original CDestructable::Save (Destructable.cpp:1134-1176).
    void destructable_block(Decoder& d)
    {
        d.d("dmg.open");
        d.d("dmg.self");
        d.b("dmg.physics");
        for (auto name : {"dmg.mass", "dmg.max_hp", "dmg.max_mega", "dmg.hp", "dmg.death_hp", "dmg.max_armor",
                          "dmg.max_necro", "dmg.armor", "dmg.resistance"})
            d.f(name);
        for (auto name : {"dmg.dead", "dmg.nigh", "dmg.type", "dmg.location"})
            d.b(name);
        d.v("dmg.direction");
        d.f("dmg.amount");
        d.f("dmg.percent");
        for (auto name :
             {"dmg.damage_target", "dmg.damage_message", "dmg.death_target", "dmg.death_message", "dmg.spawn"})
            d.s(name);
        d.v("dmg.spawn_vel");
        d.b("dmg.god");
        d.b("dmg.trigger_only");
        d.f("dmg.delay");
        d.d("dmg.close");
    }
    // Original order: the most derived class, CBaseCharacter (0 bytes), then the aggregates in
    // reverse order of addition: CMovement, CInventoryMgr, CDestructable.
    bool decode_player(std::span<const std::uint8_t> payload, Decoder& d)
    {
        d = {};
        d.data = payload;
        player_block(d);
        movement_block(d);
        inventory_block(d);
        destructable_block(d);
        return d.done();
    }
    // Original SaveClientData (BloodClientShell.cpp:6351-6381).
    bool decode_blob(std::span<const std::uint8_t> blob, Decoder& d)
    {
        d = {};
        d.data = blob;
        d.b("night");
        d.f("wonky_time");
        d.b("wonky");
        d.b("wonky_no_move");
        d.f("blind_time");
        d.b("blind");
        d.b("fade_in");
        d.b("fade_out");
        d.f("fade");
        d.b("music");
        d.f("load_type");
        d.f("game_type");
        d.s("title");
        d.s("text");
        return d.done();
    }

    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    bool contains(const std::string& text, std::string_view part)
    {
        if (text.find(part) != std::string::npos)
            return true;
        std::cerr << "missing \"" << part << "\" in: " << text << '\n';
        return false;
    }
    bool near(float a, float b, float tolerance = 1e-3f)
    {
        return std::abs(a - b) <= tolerance;
    }
    std::string var(GameSession& g, const char* name)
    {
        const auto* value = g.server.GetVarValueString(g.server.GetGameConVar(name));
        return value ? value : "";
    }
    bool setup(std::unique_ptr<GameSession>& game, bool enter = true)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "player.rez", error));
        CHECK(game->request_world("player", error));
        if (enter && !game->run_world(error))
        {
            std::cerr << "run_world: " << error << '\n';
            return false;
        }
        return true;
    }
    bool load(std::unique_ptr<GameSession>& game, const GameSave& save)
    {
        std::string error;
        if (GameSession::load_game(game, save, error))
            return true;
        std::cerr << "load refused at " << game->game_load.stage << ": " << error << '\n';
        return false;
    }
    // The single saved record of the synthetic world: the local player.
    bool player_record(const GameSave& save, SaveEngineRecord& out, std::vector<SaveEngineRecord>* all = nullptr)
    {
        std::vector<SaveEngineRecord> records;
        SaveError error;
        CHECK(decode_save_records(save.image.objects, records, error));
        std::size_t players = 0;
        for (const auto& record : records)
            if (record.class_name == "CPlayerObj")
            {
                out = record;
                ++players;
            }
        CHECK(players == 1);
        if (all)
            *all = std::move(records);
        return true;
    }
    // Replaces the player's payload after `edit` rewrote it, as a corrupt or crafted file would.
    bool edit_player(const GameSave& save, GameSave& out,
                     const std::function<void(std::vector<std::uint8_t>&, const Decoder&)>& edit)
    {
        std::vector<SaveEngineRecord> records;
        SaveError error;
        CHECK(decode_save_records(save.image.objects, records, error));
        for (auto& record : records)
            if (record.class_name == "CPlayerObj")
            {
                Decoder d;
                CHECK(decode_player(record.payload, d));
                edit(record.payload, d);
            }
        out = save;
        CHECK(encode_save_records(records, out.image.objects, error));
        return true;
    }
    void set_byte(std::vector<std::uint8_t>& payload, const Decoder& d, std::string_view name, std::uint8_t value)
    {
        payload[d.find(name)->offset] = value;
    }

    // Nondefault state on every saved part of the player and the client.
    bool configure(GameSession& g, CPlayerObj& p)
    {
        auto& inv = p.m_InventoryMgr;
        p.m_damage.SetHitPoints(37);
        p.m_damage.SetArmorPoints(23);
        p.m_damage.m_fMaxHitPoints = 150; // ValidateAttributes would make it 200 for strength 5.
        p.m_damage.m_fResistance = .5f;
        p.m_damage.damage_target = "hurt";
        p.m_damage.death_message = "gone";
        p.m_nAttribStrength = 5;
        p.m_nAttribSpeed = 4;
        p.m_nAttribResistance = 3;
        p.m_nAttribMagic = 2;
        p.m_fAirLevel = 42;
        p.m_fYaw = 1.25f;
        p.m_fPitch = -.25f;
        inv.SetAmmoCount(1, 123);
        CHECK(inv.ObtainWeapon(WEAP_BERETTA, 1) == InventoryResult::Ok && inv.GetWeapon(1, true)); // Dual.
        CHECK(inv.AddKey({"key1", "Key One", "key.pcx", "key_h.pcx", 2}) == InventoryResult::Ok);
        CHECK(inv.AddItem(1, 50) == InventoryResult::Ok && inv.AddItem(2, 100) == InventoryResult::Ok &&
              inv.AddItem(5, 1) == InventoryResult::Ok);
        CHECK(p.AddPowerup(5, 30) && p.AddPowerup(7, 10)); // Willpower and Anger.
        CHECK(p.Imprison(5) && p.m_bImprisoned);
        p.original_values.frags = 7;
        p.original_values.client_flags = 0;
        p.original_values.view_rotation = {0, .6f, 0, .8f};
        p.SetZoomMode(true, 1);
        g.client_shell.objectives = {0, "Find", "the exit"};
        g.client_shell.objectives_resolved = true;
        g.client_shell.fade.fade_in = true;
        g.client_shell.fade.value = .25f;
        g.client_shell.blind_until = g.server.GetTime() + 3;
        g.client_shell.music.logical_level = 2;
        std::string error;
        CHECK(g.pump_player_messages(error) && g.server.message_count() == 0);
        return true;
    }

    bool layout()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto* p = player(g);
        CHECK(p && configure(g, *p));
        const float now = g.server.GetTime();
        GameSave save;
        std::string error;
        CHECK(g.save_game(save, error));
        CHECK(!p->HasClientSaveData() && g.server.message_count() == 0); // The Save consumed the blob.
        SaveEngineRecord record;
        CHECK(player_record(save, record) && record.local_client && save.image.records == 1);
        CHECK(save.image.classes == (std::vector<SaveClass>{{"CPlayerObj", 1}}));
        Decoder d;
        if (!decode_player(record.payload, d))
            std::cerr << "decoded " << d.at << " of " << record.payload.size() << " bytes\n";
        CHECK(d.done());
        // CPlayerObj: relative clocks, powers and their sound stack, carried original fields.
        CHECK(d.u("falling") == 0 && near(d.real("fall_start"), -1 - now));
        CHECK(d.u("power.invulnerable") == 1 && d.u("power.invisible") == 0 && d.u("power.triple") == 1);
        CHECK(near(d.real("expiry.invulnerable"), 30) && near(d.real("expiry.triple"), 10));
        CHECK(d.u("sound_stack") == 2 && d.u("sound.0") == PU_INVULNERABLE && d.u("sound.1") == PU_TRIPLEDAMAGE);
        CHECK(d.string("trigger") == "TRIGGER" && d.string("name") == "");
        CHECK(d.real("pitch") == -.25f && d.real("yaw") == 1.25f);
        CHECK(d.real("view.y") == .6f && d.real("view.w") == .8f && d.u("client_flags") == 0 && d.u("frags") == 7);
        CHECK(d.real("scale.x") == 1 && d.real("old_air") == 100 && d.real("air") == 42);
        CHECK(d.u("zoom") == 1 && d.u("zoom_type") == 1 && d.u("camera") == save_null_reference);
        CHECK(d.u("attr.strength") == 5 && d.u("attr.speed") == 4 && d.u("attr.resistance") == 3 &&
              d.u("attr.magic") == 2 && d.u("character") == 0 && d.u("skin") == 0);
        CHECK(d.u("imprisoned") == 1 && near(d.real("imprison_start"), 0) && d.real("imprison_length") == 5);
        CHECK(d.u("client_data") == 1);
        Decoder blob;
        CHECK(decode_blob(d.find("blob")->bytes, blob));
        CHECK(blob.u("night") == 0 && blob.real("wonky_time") == 0 && blob.u("wonky") == 0);
        CHECK(near(blob.real("blind_time"), now + 3) && blob.u("blind") == 1);
        CHECK(blob.u("fade_in") == 1 && blob.u("fade_out") == 0 && blob.real("fade") == .25f && blob.u("music") == 2);
        CHECK(blob.real("load_type") == LOADTYPE_NEW_GAME &&
              blob.real("game_type") == static_cast<float>(GAMETYPE_SINGLE));
        CHECK(blob.string("title") == "Find" && blob.string("text") == "the exit");
        // CMovement: the player never initializes it, so its owner is NULL.
        CHECK(d.u("move.owner") == save_null_reference && d.real("move.rotation.w") == 1 &&
              d.real("move.right.x") == 1 && d.real("move.up.y") == 1 && d.real("move.forward.z") == 1 &&
              d.u("move.points") == 0);
        // CInventoryMgr: weapon types, raw pools, items in type order, the cumulative multiplier.
        CHECK(d.u("inv.open") == 0xCECECECE && d.u("inv.close") == 0xCECECECE && d.u("inv.god") == 0);
        CHECK(d.u("inv.weapon.0") == WEAP_MELEE && d.u("inv.weapon.1") == WEAP_BERETTA &&
              d.u("inv.lweapon.1") == WEAP_BERETTA && d.u("inv.bomb.0") == 0);
        CHECK(d.real("inv.ammo.1") == 123 && d.u("inv.items") == 4);
        CHECK(d.u("inv.item.0.list_type") == 1 && d.u("inv.item.0.count") == 100 &&
              d.string("inv.item.0.pic") == "medkit.pcx" && !d.string("inv.item.0.name"));
        CHECK(d.u("inv.item.1.list_type") == 2 && d.real("inv.item.1.discharge") == 120);
        CHECK(d.u("inv.item.2.list_type") == 5 && d.u("inv.item.3.list_type") == 6);
        CHECK(d.string("inv.item.3.name") == "key1" && d.string("inv.item.3.display") == "Key One" &&
              d.string("inv.item.3.pic_h") == "key_h.pcx" && d.u("inv.item.3.count") == 2);
        for (int i = 0; i < 4; ++i)
            CHECK(d.u("inv.item." + std::to_string(i) + ".owner") == 0 && // The player is record 0.
                  d.u("inv.item." + std::to_string(i) + ".type") ==
                      d.u("inv.item." + std::to_string(i) + ".list_type"));
        CHECK(d.u("inv.cur_slot") == 1 && d.u("inv.cur_weapon") == WEAP_BERETTA && d.u("inv.cur_item") == 0);
        CHECK(d.real("inv.damage") == 3 && d.real("inv.fire_rate") == 1); // Anger tripled the multiplier.
        // CDestructable: its own LoadSave reference, raw values and the trigger strings.
        CHECK(d.u("dmg.open") == 0xCECECECE && d.u("dmg.close") == 0xCECECECE && d.u("dmg.self") == 0);
        CHECK(d.real("dmg.hp") == 37 && d.real("dmg.armor") == 23 && d.real("dmg.max_hp") == 150 &&
              d.real("dmg.resistance") == .5f && d.real("dmg.mass") == 250 && d.u("dmg.nigh") == 1);
        CHECK(d.string("dmg.damage_target") == "hurt" && d.string("dmg.death_message") == "gone" &&
              !d.string("dmg.death_target") && !d.string("dmg.spawn"));
        // The CanDamage sentinel replaces the real health of a non-destructible owner.
        p->m_damage.can_damage = false;
        CHECK(g.save_game(save, error) && player_record(save, record) && decode_player(record.payload, d));
        CHECK(d.real("dmg.hp") == 1.0e6f);
        return true;
    }

    bool roundtrip()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto* p = player(*game);
        CHECK(p && configure(*game, *p));
        const auto* r = game->objects.get(p->m_hObject);
        const auto position = r->state.m_Pos;
        const auto old_handle = p->m_hObject;
        GameSave save;
        std::string error;
        CHECK(game->save_game(save, error));
        const auto clock = game->server.save_clock();
        CHECK(load(game, save));
        auto& g = *game;
        p = player(g);
        r = p ? g.objects.get(p->m_hObject) : nullptr;
        CHECK(p && r && g.state == SessionState::Ready && g.client_shell.m_bInWorld && g.client_shell.restored_entry);
        CHECK(g.objects.time() == clock.time && g.client_shell.client_data_loads == 1);
        // Engine record and the original Load tail: no SHADOW, zero velocity, zoom off.
        CHECK(r->state.m_Pos.x == position.x && r->state.m_Pos.y == position.y && r->state.m_Pos.z == position.z);
        CHECK(!(r->state.m_Flags & FLAG_SHADOW) && (r->state.m_Flags & FLAG_SOLID) && r->velocity.y == 0);
        CHECK(!p->m_bZoomView && p->m_nZoomType == 0 && r->client_movement && p->m_hObject != old_handle);
        // Raw attributes and maxima: the first update derives speeds without ValidateAttributes.
        CHECK(p->m_nAttribStrength == 5 && p->m_nAttribSpeed == 4 && p->m_damage.m_fMaxHitPoints == 150);
        CHECK(p->m_fMoveVel == 310 && p->m_fJumpVel == 600 && !p->m_bFirstUpdate && p->respawns == 0);
        CHECK(p->m_damage.m_fHitPoints == 37 && p->m_damage.m_fArmorPoints == 23 && p->m_damage.m_fResistance == .5f);
        CHECK(p->m_damage.damage_target == "hurt" && p->m_damage.death_message == "gone" && p->m_damage.can_damage);
        CHECK(p->m_fAirLevel == 42 && p->m_fYaw == 1.25f && p->m_fPitch == -.25f);
        CHECK(g.client_shell.rotation.yaw == 1.25f && g.client_shell.rotation.pitch == -.25f);
        // Inventory: recreated weapons, raw ammo, keys and items (inactive), the multiplier.
        auto& inv = p->m_InventoryMgr;
        CHECK(inv.GetWeapon(1) && inv.GetWeapon(1, true) && inv.GetWeapon(0) && inv.m_nCurWeaponSlot == 1);
        CHECK(inv.GetAmmoCount(1) == 123 && inv.damage_multiplier == 3 && inv.keys.size() == 1 &&
              inv.keys[0].uses == 2 && inv.keys[0].display_name == "Key One");
        CHECK(inv.medkit && inv.medkit->count == 100 && inv.flashlight && !inv.flashlight->active && inv.binoculars &&
              !inv.binoculars->active && !inv.goggles && !inv.eye);
        // Powers and imprisonment keep their remaining time (the full restore keeps the clock).
        const float now = g.server.GetTime();
        CHECK(p->IsPowerupActive(PU_INVULNERABLE) && p->IsPowerupActive(PU_TRIPLEDAMAGE) &&
              !p->IsPowerupActive(PU_INVISIBLE) && p->m_damage.m_bNighInvulnerable);
        CHECK(near(p->m_fNighInvulnerableTime - now, 30) && near(p->m_fTripleDamageTime - now, 10));
        CHECK(p->m_nPowerupSoundStackPos == 2 && p->m_bImprisoned && near(p->m_fImprisonStart, now) &&
              p->m_fImprisonLength == 5);
        CHECK(p->original_values.frags == 7 && p->original_values.view_rotation[1] == .6f &&
              p->original_values.client_flags == 0);
        // The client blob: objectives, fade, blind clock and music level; goggles stay off.
        CHECK(g.client_shell.objectives.title == "Find" && g.client_shell.objectives.text == "the exit" &&
              g.client_shell.objectives_resolved && !g.client_shell.night_goggles);
        CHECK(g.client_shell.fade.fade_in && g.client_shell.fade.value == .25f &&
              g.client_shell.music.logical_level == 2);
        CHECK(near(g.client_shell.blind_until, now + 3) && g.client_shell.m_nLastLoadType == LOADTYPE_RESTORESAVE);
        // The shell console: LoadWorld forced the three variables to 0, RespawnPlayer reset GOTOSTARTPOINT.
        CHECK(var(g, "GAME_GOTOSTARTPOINT") == "1" && var(g, "GAME_REVISITING") == "0" &&
              var(g, "GAME_DOAUTOSAVE") == "0");
        // A save right after the load reproduces the bytes apart from the listed original rewrites.
        GameSave again;
        CHECK(g.save_game(again, error));
        SaveEngineRecord before, after;
        CHECK(player_record(save, before) && player_record(again, after));
        CHECK(before.position.x == after.position.x && before.dimensions.y == after.dimensions.y &&
              before.user_flags == after.user_flags && before.animation == after.animation && after.local_client);
        CHECK((before.flags & FLAG_SHADOW) && after.flags == (before.flags & ~FLAG_SHADOW)); // Load's flag set.
        Decoder first, second;
        CHECK(decode_player(before.payload, first) && decode_player(after.payload, second));
        CHECK(first.fields.size() == second.fields.size());
        const std::set<std::string> rewritten{"zoom", "zoom_type", "blob"}; // Load forces zoom off.
        for (std::size_t i = 0; i < first.fields.size(); ++i)
        {
            const auto &a = first.fields[i], &b = second.fields[i];
            CHECK(a.name == b.name);
            // Relative clocks included: Load picks the float that saves the same offset again.
            if (!rewritten.contains(a.name) && (a.u != b.u || a.text != b.text))
                std::cerr << a.name << " differs: " << a.u << " / " << b.u << '\n';
            CHECK(rewritten.contains(a.name) || (a.u == b.u && a.text == b.text));
        }
        CHECK(second.u("zoom") == 0 && second.u("zoom_type") == 0);
        Decoder blob_before, blob_after;
        CHECK(decode_blob(first.find("blob")->bytes, blob_before) &&
              decode_blob(second.find("blob")->bytes, blob_after));
        for (std::size_t i = 0; i < blob_before.fields.size(); ++i)
            if (blob_before.fields[i].name != "load_type") // The client's m_nLastLoadType, discarded on load.
                CHECK(blob_before.fields[i].u == blob_after.fields[i].u &&
                      blob_before.fields[i].text == blob_after.fields[i].text);
        CHECK(blob_after.real("load_type") == LOADTYPE_RESTORESAVE);
        // The restored session runs.
        for (int i = 0; i < 10; ++i)
            CHECK(g.step(.05));
        CHECK(p->updates >= 10 && g.server.message_count() == 0 && g.server.client_save_data == std::nullopt);
        return true;
    }

    bool powers()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto* p = player(*game);
        CHECK(p && p->AddPowerup(5, 30) && p->AddPowerup(6, 20) && p->AddPowerup(7, 10) && p->Imprison(3));
        CHECK(p->m_InventoryMgr.damage_multiplier == 3);
        for (int i = 0; i < 10; ++i)
            CHECK(game->step(.1));
        const float saved_at = game->server.GetTime();
        const float willpower = p->m_fNighInvulnerableTime - saved_at, stealth = p->m_fInvisibleTime - saved_at,
                    anger = p->m_fTripleDamageTime - saved_at, imprisoned = p->m_fImprisonStart + 3 - saved_at;
        GameSave save;
        std::string error;
        CHECK(game->save_game(save, error) && load(game, save));
        p = player(*game);
        auto* r = game->objects.get(p->m_hObject);
        float now = game->server.GetTime();
        CHECK(now == saved_at);
        CHECK(p->IsPowerupActive(PU_INVULNERABLE) && p->IsPowerupActive(PU_INVISIBLE) &&
              p->IsPowerupActive(PU_TRIPLEDAMAGE) && p->m_damage.m_bNighInvulnerable);
        CHECK(near(p->m_fNighInvulnerableTime - now, willpower) && near(p->m_fInvisibleTime - now, stealth) &&
              near(p->m_fTripleDamageTime - now, anger) && near(p->m_fImprisonStart + 3 - now, imprisoned));
        CHECK(p->m_InventoryMgr.damage_multiplier == 3 && near(r->alpha, .08f) && p->m_nPowerupSoundStackPos == 3);
        // Each power and the imprisonment expire at their saved deadlines.
        const double start = game->objects.time();
        while (game->objects.time() < start + imprisoned + .2)
            CHECK(game->step(.1));
        CHECK(!p->m_bImprisoned && p->IsPowerupActive(PU_TRIPLEDAMAGE));
        while (game->objects.time() < start + anger + .2)
            CHECK(game->step(.1));
        CHECK(!p->IsPowerupActive(PU_TRIPLEDAMAGE) && p->m_InventoryMgr.damage_multiplier == 1 &&
              p->IsPowerupActive(PU_INVISIBLE));
        // A second save/load mid-way keeps the rest.
        CHECK(game->save_game(save, error) && load(game, save));
        p = player(*game);
        r = game->objects.get(p->m_hObject);
        while (game->objects.time() < start + stealth + .2)
            CHECK(game->step(.1));
        CHECK(!p->IsPowerupActive(PU_INVISIBLE) && r->alpha == 1 && p->IsPowerupActive(PU_INVULNERABLE));
        while (game->objects.time() < start + willpower + .2)
            CHECK(game->step(.1));
        CHECK(!p->IsPowerupActive(PU_INVULNERABLE) && !p->m_damage.m_bNighInvulnerable);
        return true;
    }

    bool refusals()
    {
        std::string error;
        {
            // Original MenuSaveGame: no save without the live local player.
            std::unique_ptr<GameSession> empty;
            CHECK(setup(empty, false));
            GameSave none;
            CHECK(!empty->save_game(none, error) && contains(error, "live local player"));
        }
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        GameSave save;
        CHECK(game->save_game(save, error));
        const auto source = player(*game)->m_hObject;
        const auto refused = [&](const GameSave& bad, std::string_view stage, std::string_view message)
        {
            const auto revision = game->world_revision;
            CHECK(!GameSession::load_game(game, bad, error));
            std::cout << stage << ": " << error << '\n';
            CHECK(game->game_load.stage == stage && contains(error, message));
            CHECK(game->world_revision == revision && player(*game) && player(*game)->m_hObject == source);
            CHECK(game->server.message_count() == 0 && !game->server.client_save_data);
            return true;
        };
        GameSave bad;
        // Two players: a second CPlayerObj record without the local-client flag or a blob (D6).
        {
            std::vector<SaveEngineRecord> records;
            SaveEngineRecord record;
            CHECK(player_record(save, record, &records));
            Decoder d;
            CHECK(decode_player(record.payload, d));
            auto second = record;
            second.local_client = false;
            const auto* flag = d.find("client_data");
            const auto* blob = d.find("blob");
            second.payload[flag->offset] = 0;
            second.payload.erase(second.payload.begin() + static_cast<std::ptrdiff_t>(blob->offset),
                                 second.payload.begin() + static_cast<std::ptrdiff_t>(blob->offset + blob->size));
            records.push_back(second);
            bad = save;
            SaveError codec;
            CHECK(encode_save_records(records, bad.image.objects, codec));
            bad.image.records = static_cast<std::uint32_t>(records.size());
            CHECK(refused(bad, "player", "exactly one local player"));
        }
        // No record carries the local-client flag.
        {
            std::vector<SaveEngineRecord> records;
            SaveEngineRecord record;
            CHECK(player_record(save, record, &records));
            records[0].local_client = false;
            bad = save;
            SaveError codec;
            CHECK(encode_save_records(records, bad.image.objects, codec));
            CHECK(refused(bad, "player", "no local player"));
        }
        // A corrupt blob: an oversized nested length fails the player's Load; a truncated, wonky or
        // foreign blob reaches LoadClientData, which refuses it.
        CHECK(edit_player(save, bad,
                          [](auto& payload, const Decoder& d) { payload[d.find("blob")->offset + 3] = 0x7F; }));
        CHECK(refused(bad, "restore", "nested message"));
        CHECK(edit_player(save, bad,
                          [](auto& payload, const Decoder& d)
                          {
                              const auto* blob = d.find("blob");
                              payload.erase(payload.begin() +
                                            static_cast<std::ptrdiff_t>(blob->offset + blob->size - 1));
                              --payload[blob->offset];
                          }));
        CHECK(refused(bad, "player", "client blob is truncated"));
        const auto blob_byte = [&](std::string_view field, std::uint8_t value)
        {
            return edit_player(save, bad,
                               [&](auto& payload, const Decoder& d)
                               {
                                   Decoder blob;
                                   decode_blob(d.find("blob")->bytes, blob);
                                   payload[d.find("blob")->offset + 4 + blob.find(field)->offset] = value;
                               });
        };
        CHECK(blob_byte("wonky", 1) && refused(bad, "player", "wonky vision"));
        CHECK(blob_byte("blind", 2) && refused(bad, "player", "invalid boolean"));
        CHECK(blob_byte("game_type", 0x80) && refused(bad, "player", "game type differs"));
        // Typed validators refuse a corrupt player block, inventory or damage aggregate.
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "attr.speed", 7); }));
        CHECK(refused(bad, "restore", "Invalid saved player attribute"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "falling", 2); }));
        CHECK(refused(bad, "restore", "out of range"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "sound_stack", 16); }));
        CHECK(refused(bad, "restore", "PU_MAXSTACK"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "inv.weapon.4", 200); }));
        CHECK(refused(bad, "restore", "Unsupported weapon"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "inv.close", 0); }));
        CHECK(refused(bad, "restore", "closing block marker"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "dmg.open", 0); }));
        CHECK(refused(bad, "restore", "damage block framing"));
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "dmg.type", 2); }));
        CHECK(refused(bad, "restore", "Unsupported saved damage type"));
        // A dead player is restored by the original Load but never attached (no corpse restoration).
        CHECK(edit_player(save, bad, [](auto& p, const Decoder& d) { set_byte(p, d, "dmg.dead", 1); }));
        CHECK(refused(bad, "player", "dead player"));
        // The intact save still loads.
        CHECK(load(game, save) && player(*game) && player(*game)->m_hObject != source);
        return true;
    }

    bool character()
    {
        // D13: the record's model replaces precreate's forced Caleb.abc before the animation track
        // is applied, so another character keeps its track through SetCharacter's same-model call.
        auto game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "player.rez", error));
        CHECK(game->request_world("player", error));
        game->client_shell.entry_profile.character = 1; // Ophelia.
        if (!game->run_world(error))
            std::cerr << "run_world: " << error << '\n';
        CHECK(game->local_client);
        auto* p = player(*game);
        auto* r = game->objects.get(p->m_hObject);
        CHECK(p->m_nCharacter == 1 && std::string(r->state.m_Filename) == "Models\\Characters\\Ophelia.abc");
        r->animation.first = r->animation.looping = false;
        GameSave save;
        CHECK(game->save_game(save, error) && load(game, save));
        p = player(*game);
        r = game->objects.get(p->m_hObject);
        CHECK(p->m_nCharacter == 1 && std::string(r->state.m_Filename) == "Models\\Characters\\Ophelia.abc" &&
              std::string(r->state.m_SkinName) == "Skins\\Characters\\Ophelia.dtx");
        CHECK(!r->animation.first && !r->animation.looping && p->eye_level == OpenBlood2::character_profiles[1].eye);
        return true;
    }

    bool level_change()
    {
        // The typed NEW_LEVEL transfer keeps working and now carries powers with their expiry.
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto* p = player(*game);
        CHECK(p && p->AddPowerup(7, 10) && p->Imprison(4));
        CHECK(p->m_InventoryMgr.AddKey({"key1", "Key One", "key.pcx", "key_h.pcx", 1}) == InventoryResult::Ok);
        for (int i = 0; i < 5; ++i)
            CHECK(game->step(.1));
        const float now = game->server.GetTime(), anger = p->m_fTripleDamageTime - now;
        std::string error;
        WorldMessage level;
        CHECK(original_world_request("next", 0, LOADTYPE_NEW_LEVEL, 0, level));
        CHECK(game->queue_level_change(level, "arrival", error));
        if (GameSession::process_level_change(game, error) != LevelChangeResult::Committed)
        {
            std::cerr << "level change: " << error << '\n';
            return false;
        }
        p = player(*game);
        CHECK(p && game->world_path == "worlds/next.dat" && game->level_change.selected_start == "arrival");
        const float arrived = game->server.GetTime();
        CHECK(p->IsPowerupActive(PU_TRIPLEDAMAGE) && near(p->m_fTripleDamageTime - arrived, anger) &&
              p->m_InventoryMgr.damage_multiplier == 3 && p->m_bImprisoned && p->m_InventoryMgr.keys.empty());
        CHECK(game->client_shell.m_nLastLoadType == LOADTYPE_NEW_LEVEL && var(*game, "GAME_DOAUTOSAVE") == "1" &&
              var(*game, "GAME_GOTOSTARTPOINT") == "1");
        // The destination saves and loads, with the transferred player.
        GameSave save;
        CHECK(game->save_game(save, error) && load(game, save));
        p = player(*game);
        CHECK(p && p->IsPowerupActive(PU_TRIPLEDAMAGE) && game->world_path == "worlds/next.dat");
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    if (!write_fixture())
        return 1;
    const bool ok = mode == "layout"         ? layout()
                    : mode == "roundtrip"    ? roundtrip()
                    : mode == "powers"       ? powers()
                    : mode == "refusals"     ? refusals()
                    : mode == "character"    ? character()
                    : mode == "level_change" ? level_change()
                                             : false;
    return ok ? 0 : 1;
}
