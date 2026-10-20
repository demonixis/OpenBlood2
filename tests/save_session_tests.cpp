#include "engine/config.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original_globals.h"
#include "player_fixture.h"
#include <SDL3/SDL.h>
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Restore-mode world, save-support declarations and the candidate-session transaction of
// GameSession::save_game/load_game (save_load_contracts lot 4). Synthetic classes stand in for
// the class lots: every original class is AlwaysLoad, NotSaved or Unsupported.
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

    // Little-endian DAT writer, the layout world_tests uses for synthetic worlds.
    struct Writer
    {
        std::vector<std::uint8_t> data;
        void byte(unsigned v)
        {
            data.push_back(static_cast<std::uint8_t>(v));
        }
        void u16(unsigned v)
        {
            byte(v);
            byte(v >> 8);
        }
        void u32(std::uint32_t v)
        {
            u16(v);
            u16(v >> 16);
        }
        void real(float f)
        {
            u32(std::bit_cast<std::uint32_t>(f));
        }
        void vec(float x = 0, float y = 0, float z = 0)
        {
            real(x);
            real(y);
            real(z);
        }
        void str(std::string_view s)
        {
            u16(static_cast<unsigned>(s.size()));
            data.insert(data.end(), s.begin(), s.end());
        }
        void cstr(std::string_view s)
        {
            data.insert(data.end(), s.begin(), s.end());
            byte(0);
        }
        void zero(std::size_t n)
        {
            data.resize(data.size() + n);
        }
        void append(const Writer& w)
        {
            data.insert(data.end(), w.data.begin(), w.data.end());
        }
        void patch(std::size_t p, std::uint32_t n)
        {
            for (unsigned i = 0; i < 4; ++i)
                data[p + i] = static_cast<std::uint8_t>(n >> (i * 8));
        }
    };
    Writer prop(std::string_view name, unsigned type, const Writer& value)
    {
        Writer p;
        p.str(name);
        p.byte(type);
        p.u32(0);
        p.u16(static_cast<unsigned>(value.data.size()));
        p.append(value);
        return p;
    }
    Writer text(std::string_view name, std::string_view value)
    {
        Writer v;
        v.str(value);
        return prop(name, 0, v);
    }
    Writer vector(std::string_view name, DVector value)
    {
        Writer v;
        v.vec(value.x, value.y, value.z);
        return prop(name, 1, v);
    }
    Writer object(std::string_view cls, std::initializer_list<Writer> properties)
    {
        Writer v;
        v.str(cls);
        v.u32(static_cast<unsigned>(properties.size()));
        for (const auto& p : properties)
            v.append(p);
        Writer out;
        out.u16(static_cast<unsigned>(v.data.size()));
        out.append(v);
        return out;
    }
    // One floor triangle textured with `texture`, after the given object rows.
    std::vector<std::uint8_t> dat(const std::vector<Writer>& objects, std::string_view texture = "t.dtx")
    {
        Writer w;
        w.u32(56);
        w.u32(48);
        w.u32(0);
        w.u32(0);
        w.zero(32);
        w.u32(static_cast<unsigned>(objects.size()));
        for (const auto& o : objects)
            w.append(o);
        w.patch(8, static_cast<unsigned>(w.data.size()));
        const auto start = w.data.size();
        w.u32(0);
        w.zero(32);
        w.u32(0);
        w.str("SyntheticTriangle");
        w.u32(0);
        for (unsigned n : {3u, 1u, 1u, 0u, 1u, 0u, 3u, 0u, 0u, 0u})
            w.u32(n);
        w.u32(0);
        w.vec();
        w.vec(10, 10, 10);
        w.vec();
        w.u32(static_cast<unsigned>(texture.size() + 1));
        w.u32(1);
        w.cstr(texture);
        w.byte(3);
        w.byte(0); // Polygon vertex counts.
        w.vec(0, 1, 0);
        w.real(0); // Plane.
        w.vec();
        w.vec(1, 0, 0);
        w.vec(0, 0, 1);
        w.zero(24);
        w.u32(0xffffffff);
        w.zero(8);
        w.u16(0);
        w.u32(0);
        w.u32(1);
        w.u32(0);
        w.byte(0);
        w.u16(20);
        w.zero(12);
        w.u32(0);
        for (unsigned i = 0; i < 3; ++i)
        {
            w.u16(i);
            w.zero(3);
        }
        w.vec(-1000, 0, -1000);
        w.vec(1000, 0, -1000);
        w.vec(0, 0, 1000);
        w.patch(start, static_cast<unsigned>(w.data.size()));
        w.u32(0); // No additional models.
        w.vec(-2000, -2000, -2000);
        w.vec(2000, 2000, 2000);
        return w.data;
    }
    std::vector<std::uint8_t> dtx() // Smallest version -2 palette image.
    {
        test_model::Writer image;
        image.u32(0);
        image.u32(0xfffffffeu);
        image.u16(2);
        image.u16(2);
        image.u16(1);
        image.u16(0);
        image.u32(0);
        image.u32(0);
        image.b.resize(1068);
        image.b[44] = image.b[45] = image.b[46] = image.b[47] = 255;
        image.b.insert(image.b.end(), {0, 0, 0, 0});
        return image.b;
    }
    bool write_fixture()
    {
        std::vector<Writer> always{
            object("WorldProperties",
                   {text("Name", "WorldProperties0"), text("WorldName", "Authored"), vector("Gravity", {0, -1500, 0})}),
            object("GameStartPoint", {text("Name", "start"), vector("Pos", {0, 10, 0})}),
            object("PathPoint", {text("Name", "path0"), vector("Pos", {5, 10, 0})}),
            object("SmellHint", {text("Name", "scent"), vector("Pos", {6, 10, 0})}),
            object("ExitHint", {text("Name", "exit"), vector("Pos", {7, 10, 0})}),
            object("CameraSpot", {text("Name", "spot")}),
            object("SessionAlwaysLoad", {text("Name", "always")})};
        auto save1 = always;
        save1.push_back(object("SessionProbe", {text("Name", "probe-a"), vector("Pos", {1, 2, 3})}));
        save1.push_back(object("SessionProbe", {text("Name", "probe-b"), vector("Pos", {4, 5, 6})}));
        auto unported = save1;
        unported.push_back(object("NotPorted", {text("Name", "unknown")}));
        test_player::Directory dir;
        dir.children["worlds"].files["save1.dat"] = dat(save1);
        dir.children["worlds"].files["unported.dat"] = dat(unported);
        dir.children["worlds"].files["bare.dat"] = dat(save1, "absent.dtx");
        dir.children["worlds"].files["other.dat"] =
            dat({object("SessionAlwaysLoad", {text("Name", "always")}),
                 object("GameStartPoint", {text("Name", "start")}), object("SessionProbe", {text("Name", "probe-a")})});
        dir.files["t.dtx"] = dtx();
        dir.children["models"].files["path1.abc"] = test_model::abc(false, true).b; // PathPoint's model.
        dir.children["skins"].files["path1.dtx"] = dtx();
        // The player a save requires (lot 5): Caleb and the default weapons' view models.
        dir.children["models"].children["characters"].files["caleb.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].children["characters"].files["caleb1.dtx"] = dtx();
        for (auto path : {"beretta_pv.abc", "lh_beretta_pv.abc", "c_knife_pv.abc"})
            dir.children["models"].children["weapons"].files[path] = test_model::abc(true).b;
        for (auto path : {"c_beretta_pv_t.dtx", "c_knife_pv_t.dtx"})
            dir.children["skins"].children["weapons"].files[path] = dtx();
        for (auto name : {"beretta_pu", "knifec_pu"})
        {
            dir.children["models"].children["powerups"].files[std::string(name) + ".abc"] = test_model::abc(true).b;
            dir.children["skins"].children["powerups"].files[std::string(name) + ".dtx"] = dtx();
        }
        test_model::Writer archive;
        archive.b.resize(168);
        archive.b[0] = archive.b[62] = archive.b[124] = 13;
        archive.b[1] = archive.b[63] = archive.b[125] = 10;
        archive.b[126] = 26;
        archive.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(archive, dir);
        archive.patch(131, offset);
        archive.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        std::ofstream file(root / "session.rez", std::ios::binary);
        file.write(reinterpret_cast<const char*>(archive.b.data()), static_cast<std::streamsize>(archive.b.size()));
        return !ec && file.good();
    }

    // Synthetic classes: an Original saveable probe and an always-load observer.
    struct Hooks
    {
        std::function<bool(float)> precreate;            // false refuses.
        std::function<void(HOBJECT, HMESSAGEREAD)> load; // Replaces the default Load body.
        std::function<void(HOBJECT)> loaded;             // Runs after the default Load body.
    } hooks;
    struct Observation
    {
        float time = 0, world_time = 0, reason = 0;
        int draw = 0;
    };
    std::vector<Observation> observed;
    struct Probe : BaseClass
    {
        Probe() : BaseClass(OT_NORMAL) {}
        DDWORD value = 0;
        HOBJECT ref = nullptr;
        DDWORD EngineMessageFn(DDWORD id, void* data, float param) override
        {
            switch (id)
            {
                case MID_PRECREATE:
                    if (hooks.precreate && !hooks.precreate(param))
                        return 0;
                    break;
                case MID_INITIALUPDATE:
                    if (param != INITIALUPDATE_SAVEGAME) // The engine record restores user flags.
                        g_pServerDE->SetObjectUserFlags(m_hObject,
                                                        g_pServerDE->GetObjectUserFlags(m_hObject) | USRFLG_SAVEABLE);
                    break;
                case MID_SAVEOBJECT:
                {
                    const auto h = *static_cast<HMESSAGEWRITE*>(data);
                    g_pServerDE->WriteToMessageDWord(h, value);
                    g_pServerDE->WriteToLoadSaveMessageObject(h, ref);
                    break;
                }
                case MID_LOADOBJECT:
                {
                    const auto h = *static_cast<HMESSAGEREAD*>(data);
                    if (hooks.load)
                        hooks.load(m_hObject, h);
                    else
                    {
                        value = g_pServerDE->ReadFromMessageDWord(h);
                        g_pServerDE->ReadFromLoadSaveMessageObject(h, &ref);
                    }
                    if (hooks.loaded)
                        hooks.loaded(m_hObject);
                    break;
                }
                default:
                    break;
            }
            return BaseClass::EngineMessageFn(id, data, param);
        }
    };
    struct AlwaysProbe : BaseClass
    {
        AlwaysProbe() : BaseClass(OT_NORMAL) {}
        DDWORD EngineMessageFn(DDWORD id, void* data, float param) override
        {
            if (id == MID_INITIALUPDATE)
            {
                Observation o{g_pServerDE->GetTime(), g_pServerDE->GetWorldTime(), param};
                g_pServerDE->RandomInt(0, 1000000, o.draw);
                observed.push_back(o);
            }
            return BaseClass::EngineMessageFn(id, data, param);
        }
    };
    template <class T> std::unique_ptr<BaseClass> construct()
    {
        return std::make_unique<T>();
    }
    ClassInfo probe_type{"SessionProbe", nullptr, construct<Probe>, {}, SaveSupport::Original, 1};
    ClassInfo unsupported_type{"SessionUnsupported", nullptr, construct<Probe>, {}}; // Marks itself saveable.
    ClassInfo probe_v2{"SessionProbe", nullptr, construct<Probe>, {}, SaveSupport::Original, 2};
    ClassInfo always_type{"SessionAlwaysLoad", nullptr, construct<AlwaysProbe>, {}, SaveSupport::AlwaysLoad};
    const std::array<const ClassInfo*, 2> extra{&probe_type, &always_type};

    bool setup(GameSession& g)
    {
        hooks = {};
        observed.clear();
        std::string error;
        CHECK(g.initialize());
        probe_type.parent = probe_v2.parent = always_type.parent = g.objects.find_class("BaseClass");
        CHECK(g.objects.register_class(probe_type) && g.objects.register_class(always_type));
        CHECK(g.resources.mount(root / "session.rez", error));
        return true;
    }
    HOBJECT named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return r->handle;
        return nullptr;
    }
    Probe* probe(GameSession& g, std::string_view name)
    {
        const auto* r = g.objects.get(named(g, name));
        return r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
    }
    std::string var(GameSession& g, const char* name)
    {
        const auto* value = g.server.GetVarValueString(g.server.GetGameConVar(name));
        return value ? value : "";
    }
    bool contains(const std::string& text, std::string_view part)
    {
        if (text.find(part) != std::string::npos)
            return true;
        std::cerr << "missing \"" << part << "\" in: " << text << '\n';
        return false;
    }
    // A saved world with both probes changed, a pending update and runtime console values.
    bool saved_world(std::unique_ptr<GameSession>& game, GameSave& save, std::string_view world = "save1")
    {
        game = std::make_unique<GameSession>();
        CHECK(setup(*game));
        std::string error;
        CHECK(game->request_world(world, error));
        // Original entry without RunWorld's checks: the unported and bare worlds refuse those
        // only when loaded. A save requires the live local player (lot 5).
        if (!game->enter_player(error))
            std::cerr << "player entry: " << error << '\n';
        CHECK(game->local_client);
        for (int i = 0; i < 20; ++i)
            CHECK(game->step(.05));
        auto *a = probe(*game, "probe-a"), *b = probe(*game, "probe-b");
        CHECK(a);
        a->value = 7;
        if (b)
        {
            b->value = 11;
            a->ref = b->m_hObject;
            b->ref = named(*game, "start"); // Always-load: a reference to it reads NULL.
        }
        game->server.SetNextUpdate(a->m_hObject, 2.5f);
        int draw = 0;
        CHECK(game->server.RandomInt(0, 100, draw) && game->server.RandomInt(0, 100, draw));
        game->server.SetGameConVar("SessionVar", "saved");
        CHECK(game->save_game(save, error));
        return true;
    }

    bool load(std::unique_ptr<GameSession>& game, const GameSave& save, std::string& error,
              std::span<const ClassInfo* const> classes = extra)
    {
        if (GameSession::load_game(game, save, error, classes))
            return true;
        std::cerr << "load refused at " << game->game_load.stage << ": " << error << '\n';
        return false;
    }
    struct GlobalSnapshot
    {
        OriginalGlobals globals; // Only for its pointer tables.
        std::array<DDWORD, 7> counts{};
        std::vector<std::pair<DLink*, DLink>> links;
        HOBJECT camera = g_hActiveCamera;
        bool capture()
        {
            for (std::size_t i = 0; i < globals.heads.size(); ++i)
            {
                counts[i] = *globals.counts[i];
                links.emplace_back(globals.heads[i], *globals.heads[i]);
                auto* link = globals.heads[i]->m_pNext;
                unsigned walked = 0;
                while (link && link != globals.heads[i] && walked <= counts[i])
                {
                    links.emplace_back(link, *link);
                    ++walked;
                    link = link->m_pNext;
                }
                CHECK(walked == counts[i] && (!link || link == globals.heads[i]));
            }
            return true;
        }
        bool unchanged() const
        {
            CHECK(camera == g_hActiveCamera);
            for (std::size_t i = 0; i < globals.heads.size(); ++i)
                CHECK(*globals.counts[i] == counts[i]);
            for (const auto& [live, copy] : links)
                CHECK(live->m_pPrev == copy.m_pPrev && live->m_pNext == copy.m_pNext && live->m_pData == copy.m_pData);
            return true;
        }
    };
    // Everything a refused load must leave exactly as it was.
    struct SourceState
    {
        const GameSession* owner = nullptr;
        std::vector<ObjectId> ids;
        std::uint64_t revision = 0, store_revision = 0, rejected = 0;
        SaveClock clock;
        std::optional<SaveImage> current;
        std::size_t strings = 0, messages = 0, lists = 0, voices = 0, records = 0;
        std::string world, session_var;
        DDWORD probe_value = 0;
        GlobalSnapshot globals;
        bool capture(GameSession& g)
        {
            owner = &g;
            ids = g.objects.snapshot();
            revision = g.world_revision;
            store_revision = g.server.save_store.revision();
            rejected = g.game_load.rejected;
            clock = g.server.save_clock();
            if (const auto* image = g.server.save_store.find(SaveStoreSlot::Current))
                current = *image;
            strings = g.server.string_count();
            messages = g.server.message_count();
            lists = g.server.list_count();
            voices = g.server.audio.voices().size();
            records = g.objects.record_count();
            world = g.world_path;
            session_var = var(g, "SessionVar");
            probe_value = probe(g, "probe-a") ? probe(g, "probe-a")->value : 0;
            return globals.capture();
        }
        bool unchanged(GameSession& g, std::string_view stage) const
        {
            CHECK(&g == owner && g_pServerDE == &g.server);
            CHECK(g.game_load.stage == stage && g.game_load.rejected == rejected + 1);
            // A refusal before isolation built no candidate, so it has nothing to clean up.
            const bool isolated = stage != "admission" && stage != "fingerprint";
            CHECK(g.game_load.source_retained && g.game_load.cleanup_complete == isolated);
            CHECK(g.game_load.committed == 0 &&
                  g.game_load.world_loaded == (stage == "player" || stage == "validation" || stage == "audio"));
            CHECK(g.objects.snapshot() == ids && g.objects.record_count() == records && g.world_revision == revision);
            CHECK(g.server.save_clock() == clock && g.world_path == world);
            CHECK(g.server.save_store.revision() == store_revision);
            const auto* image = g.server.save_store.find(SaveStoreSlot::Current);
            CHECK(bool(image) == bool(current) && (!image || *image == *current));
            CHECK(g.server.string_count() == strings && g.server.message_count() == messages);
            CHECK(g.server.list_count() == lists && g.server.audio.voices().size() == voices);
            CHECK(var(g, "SessionVar") == session_var);
            CHECK(probe(g, "probe-a") && probe(g, "probe-a")->value == probe_value);
            return globals.unchanged();
        }
    };

    bool declarations()
    {
        GameSession g;
        CHECK(g.initialize());
        // CF_ALWAYSLOAD (BEGIN_CLASS ... END_CLASS_DEFAULT_FLAGS) among the registered original classes,
        // and the engine built-ins.
        const std::set<std::string> always{"CameraSpot",
                                           "Container",
                                           "DemoSkyWorldModel",
                                           "ExitHint",
                                           "GameStartPoint",
                                           "Model",
                                           "ParticleSystem",
                                           "PathPoint",
                                           "PolyGrid",
                                           "SkyPointer",
                                           "SmellHint",
                                           "Sound",
                                           "SparksObj",
                                           "Sprite",
                                           "StartPoint",
                                           "WorldProperties",
                                           "Key"};
        // Never marked USRFLG_SAVEABLE by the original and lost on its restore.
        const std::set<std::string> not_saved{"B2BaseClass",    "BaseClass", "CClientSFX",
                                              "CClientSplatFX", "CGib",      "CHandWeaponModel",
                                              "FlagObject",     "FlagStand", "NagaCeilingDebris"};
        // Lot 5: the player. Lot 8: movers, brushes, volumes, TripLaser and Tchernotronic. Lots 11
        // and 12: AI_Mgr's NPC subclasses and CCorpse. Lot 10: effect publishers and audio emitters.
        const std::set<std::string> original{
            "AncientOne", "AncientOneTentacle", "BoneLeech", "DrudgePriest", "EnhancedGideon", "Gideon", "Naga",
            "TheHandAI", "Thief", "UndeadGideon", "ZealotAI", "Behemoth", "CCorpse", "CalebAI", "CivilianAI",
            "CultistAI", "DeathShroud", "DrudgeLord", "Fanatic", "GabriellaAI", "GabriellaREV", "IshmaelAI",
            "IshmaelREV", "MadScientistAI", "OpheliaAI", "OpheliaREV", "Prophet", "ShikariAI", "SoulDrudge", "Acid",
            "Blood", "CClientExplosionSFX", "CClientLightningSFX", "CClientParticleStreamSFX", "CClientWarpGateSFX",
            "CDestructableBrush", "CDestructableModel", "CPlayerObj", "ClientLightFX", "DetailSprite",
            "Flicker1WaveLightFX", "Flicker2WaveLightFX", "Flicker3WaveLightFX", "Flicker4WaveLightFX", "FlickerLight",
            "GlowingLight", "RampDownWaveLightFX", "RampUpWaveLightFX", "RandomSoundFX", "SawWaveLightFX",
            "SearchWaveLightFX", "SineWaveLightFX", "SoundFX", "SquareWaveLightFX", "StrobeWaveLightFX", "Conveyor",
            "Damage", "Door", "FreeFall", "Ladder", "Minefield", "RotatingBrush", "RotatingDoor", "RotatingModel",
            "Tchernotronic", "TripLaser", "VolumeBrush", "Water", "WorldModelDebris",
            // Lot 7: world logic.
            "BigTrigger", "CameraObj", "CinematicTrigger", "ConversationTrigger", "ExitTrigger", "KeyFramer",
            "ObjectivesTrigger", "Spawner", "ToggleTrigger", "Trigger"};
        // Lot 9: every pickup, KeyPickup, the projectile family and SeeingEye.
        const std::set<std::string> lot9{"AmmoPickup",
                                         "AngerPU",
                                         "ArmorBase",
                                         "AssaultRiflePU",
                                         "BMGAmmoPU",
                                         "BatteryAmmoPU",
                                         "BerettaPU",
                                         "BinocularsPU",
                                         "BugSprayPU",
                                         "BulletAmmoPU",
                                         "CBehemothShockwaveProjectile",
                                         "CBugSprayAltProjectile",
                                         "CBugSprayProjectile",
                                         "CDeathRayProjectile",
                                         "CEnergyBlastProjectile",
                                         "CFlareAltProjectile",
                                         "CFlareBurstProjectile",
                                         "CFlareProjectile",
                                         "CGrenade",
                                         "CGroundStrikeProjectile",
                                         "CHowitzerAltFrag",
                                         "CHowitzerAltShell",
                                         "CHowitzerShell",
                                         "CLeechAltProjectile",
                                         "CLeechPrimeProjectile",
                                         "CNagaDebrisProjectile",
                                         "CNagaSpikeProjectile",
                                         "CNapalmAltProjectile",
                                         "CNapalmFireball",
                                         "CNapalmProjectile",
                                         "COrbAltProjectile",
                                         "COrbProjectile",
                                         "CProximityBomb",
                                         "CRemoteBomb",
                                         "CShikariLoogieProjectile",
                                         "CShockwaveProjectile",
                                         "CSingularityProjectile",
                                         "CSkullProjectile",
                                         "CTeslaBallProjectile",
                                         "CTeslaBoltProjectile",
                                         "CTeslaProjectile",
                                         "CTimeBomb",
                                         "CVomitProjectile",
                                         "DeathRayPU",
                                         "DieBugDieAmmoPU",
                                         "EnhancementBase",
                                         "FlareAmmoPU",
                                         "FlareGunPU",
                                         "FlashlightPU",
                                         "FuelAmmoPU",
                                         "HealthBase",
                                         "HealthPU",
                                         "HowitzerAmmoPU",
                                         "HowitzerPU",
                                         "InvulnerabilityPU",
                                         "ItemPickup",
                                         "ItemPickupCharged",
                                         "ItemPickupWeapon",
                                         "KeyPickup",
                                         "LifeLeechPU",
                                         "MedKitPU",
                                         "MegaHealthPU",
                                         "MiniGunPU",
                                         "NapalmCannonPU",
                                         "NecroWardPU",
                                         "NightGogglesPU",
                                         "OrbPU",
                                         "PickupObject",
                                         "PowerupPickup",
                                         "ProximitiesPU",
                                         "RemotesPU",
                                         "RevenantPU",
                                         "SeeingEye",
                                         "ShellAmmoPU",
                                         "ShotgunPU",
                                         "SingularityPU",
                                         "SniperRiflePU",
                                         "StealthPU",
                                         "SubMachineGunPU",
                                         "TeslaCannonPU",
                                         "TheEyePU",
                                         "TimeBombPU",
                                         "VoodooDollPU",
                                         "WardPU",
                                         "WeaponPickup"};
        const auto declared = [&]
        {
            auto all = original;
            all.insert(lot9.begin(), lot9.end());
            return all;
        }();
        std::size_t unsupported = 0;
        for (auto type : g.objects.classes())
        {
            const auto support = save_support(type);
            if (declared.contains(type->name))
                CHECK(support == SaveSupport::Original && type->payload_version == 1);
            else if (always.contains(type->name))
                CHECK(support == SaveSupport::AlwaysLoad);
            else if (not_saved.contains(type->name))
                CHECK(support == SaveSupport::NotSaved);
            else
            {
                if (support != SaveSupport::Unsupported)
                    std::cerr << type->name << " is " << save_support_name(support) << '\n';
                CHECK(support == SaveSupport::Unsupported && type->payload_version == 0);
                ++unsupported;
            }
        }
        for (const auto& name : always)
            CHECK(g.objects.find_class(name));
        for (const auto& name : not_saved)
            CHECK(g.objects.find_class(name));
        for (const auto& name : declared)
            CHECK(g.objects.find_class(name));
        // Original always-load classes OpenBlood2 does not register: none is placed in a shipped world.
        for (auto name : {"Script", "LightFX", "WreckingBall", "SoccerGoal", "WorldModel", "FogSphere"})
            CHECK(!g.objects.find_class(name));
        const auto build_schema = save_class_schema(g.objects);
        CHECK(unsupported > 0 && build_schema.size() == declared.size()); // Lot 12 remains.
        for (const auto& name : declared)
            CHECK(build_schema.contains(name) && build_schema.at(name) == 1);
        std::cout << g.objects.classes().size() << " classes: " << declared.size() << " Original, " << always.size()
                  << " AlwaysLoad, " << not_saved.size() << " NotSaved, " << unsupported << " Unsupported\n";
        // The schema follows the declaration, including inherited AlwaysLoad.
        Objects registry;
        CHECK(register_original_classes(registry));
        probe_type.parent = always_type.parent = registry.find_class("BaseClass");
        ClassInfo child{"SessionAlwaysChild", &always_type, construct<Probe>, {}, SaveSupport::Original, 4};
        CHECK(registry.register_class(probe_type) && registry.register_class(always_type) &&
              registry.register_class(child));
        const auto schema = save_class_schema(registry);
        CHECK(schema.size() == declared.size() + 1 && schema.at("SessionProbe") == 1);
        return true;
    }

    bool marking()
    {
        GameSession g;
        CHECK(setup(g));
        std::string error;
        CHECK(g.request_world("save1", error) && g.enter_player(error));
        const auto make = [&](const char* type, Properties p = {})
        { return g.objects.create(g.objects.find_class(type), {}, Creation::World, p); };
        unsupported_type.parent = g.objects.find_class("BaseClass");
        CHECK(g.objects.register_class(unsupported_type));
        struct Row
        {
            const char* type;
            Properties properties;
            bool saveable;
        };
        // Original USRFLG_SAVEABLE setters (subclasses inherit) and two decision-4 departures.
        const std::vector<Row> rows{
            {"Trigger", {}, true},
            {"BigTrigger", {}, true},
            {"SoundFX", {}, true},
            {"KeyFramer", {}, true},
            {"Spawner", {{"SpawnObject", std::string("HealthPU")}}, true},
            {"CinematicTrigger", {}, true},
            {"ClientLightFX", {}, true},
            {"ClientLightFX", {{"LightSwitch", false}}, true}, // Departure: the original left it unmarked.
            {"RandomSoundFX", {}, true},                       // Departure: the original never marks it.
            {"RandomSoundFX", {{"InitiallyOn", false}}, true},
            {"CClientExplosionSFX", {}, true},
            {"SessionUnsupported", {}, true},
            {"GameStartPoint", {}, false},
            {"PathPoint", {}, false},
            {"SmellHint", {}, false},
            {"ExitHint", {}, false},
            {"CameraSpot", {}, false},
            {"WorldProperties", {}, false},
            {"B2BaseClass", {}, false},
            {"BaseClass", {}, false}};
        for (const auto& row : rows)
        {
            const auto h = make(row.type, row.properties);
            if (!h || bool(g.server.GetObjectUserFlags(h) & USRFLG_SAVEABLE) != row.saveable)
                std::cerr << row.type << ": " << (h ? "unexpected marking" : "not created") << '\n';
            CHECK(h && bool(g.server.GetObjectUserFlags(h) & USRFLG_SAVEABLE) == row.saveable);
        }
        CHECK(g.objects.finish_frame());
        // The shell's list holds the marked objects; SaveObjects refuses their classes by name.
        GameSave save;
        const auto before = g.server.save_store.revision();
        CHECK(!g.save_game(save, error) && contains(error, "SaveObjects: listed classes cannot be saved"));
        // Lots 7 and 10 made every original row above Original; the synthetic class keeps an
        // Unsupported saveable object in the list.
        CHECK(contains(error, "SessionUnsupported (Unsupported)"));
        for (auto name : {"BigTrigger", "CinematicTrigger", "KeyFramer", "Spawner", "Trigger (", "CClientExplosionSFX",
                          "ClientLightFX", "RandomSoundFX", "SoundFX"})
            CHECK(error.find(name) == std::string::npos);
        CHECK(error.find("PathPoint") == std::string::npos && error.find("BaseClass") == std::string::npos);
        CHECK(g.server.save_store.revision() == before && !g.server.save_store.find(SaveStoreSlot::Current));
        // A saveable class's object without the marking is refused by name instead of being dropped.
        HOBJECT unmarked = nullptr;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->type->name == "Trigger")
                unmarked = r->handle;
        CHECK(unmarked);
        g.server.SetObjectUserFlags(unmarked, 0);
        CHECK(!g.save_game(save, error) && contains(error, "not marked USRFLG_SAVEABLE: Trigger") &&
              error.find("SaveObjects") == std::string::npos);
        g.server.SetObjectUserFlags(unmarked, USRFLG_SAVEABLE); // Trigger is Original since lot 7.
        // Without any Unsupported object, a marked NotSaved object still refuses by name. The
        // player (Original since lot 5) and its unsaved children stay.
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && save_support(r->type) == SaveSupport::Unsupported)
                g.objects.remove(r->handle);
        CHECK(g.objects.finish_frame());
        const auto base = make("B2BaseClass");
        CHECK(base && g.objects.finish_frame());
        g.server.SetObjectUserFlags(base, USRFLG_SAVEABLE);
        CHECK(!g.save_game(save, error) && contains(error, "B2BaseClass (NotSaved)"));
        g.server.SetObjectUserFlags(base, 0);
        return true;
    }

    bool refusals()
    {
        GameSession g;
        CHECK(test_player::setup(g, root));
        GameSave save;
        std::string error;
        // Original MenuSaveGame: no save without the live local player in the world (lot 5).
        CHECK(!g.save_game(save, error) && contains(error, "live local player"));
        CHECK(g.enter_player(error));
        auto* player = g.server_shell.Player(g.local_client);
        player->m_bDead = DTRUE;
        CHECK(!g.save_game(save, error) && contains(error, "live local player"));
        player->m_bDead = DFALSE;
        // Only the always-load start point and the player exist; the synthetic world is not a mounted DAT.
        CHECK(!g.save_game(save, error) && contains(error, "Fingerprint: World synthetic.dat"));
        CHECK(!player->HasClientSaveData() && g.server.message_count() == 0);
        CHECK(!g.server.save_store.find(SaveStoreSlot::Current)); // A failed save keeps the previous image.
        const auto revision = g.server.save_store.revision();
        g.server.SetGameConVar("GAME_GAMETYPE", "1"); // Bloodbath.
        CHECK(!g.save_game(save, error) && contains(error, "single-player"));
        g.server.SetGameConVar("GAME_GAMETYPE", "8"); // Action is single-player.
        CHECK(!g.save_game(save, error) && contains(error, "Fingerprint"));
        g.server.SetGameConVar("GAME_GAMETYPE", "0");
        const auto message = g.server.StartHMessageWrite();
        CHECK(!g.save_game(save, error) && contains(error, "frame boundary"));
        g.server.EndHMessageWrite(message);
        const auto doomed = g.objects.create(g.objects.find_class("PathPoint"), {}, Creation::World);
        CHECK(doomed && g.objects.remove(doomed));
        CHECK(!g.save_game(save, error) && contains(error, "frame boundary"));
        CHECK(g.objects.finish_frame());
        WorldMessage level;
        CHECK(original_world_request("save1", 0, LOADTYPE_NEW_LEVEL, 0, level));
        CHECK(g.queue_level_change(level, "start", error));
        CHECK(!g.save_game(save, error) && contains(error, "level change"));
        g.pending_level_change.reset();
        CHECK(g.unload_world() && !g.save_game(save, error) && contains(error, "No world"));
        CHECK(g.server.save_store.revision() == revision && g.game_saves == 0);
        // A foreign or nested call is refused before any work.
        GameSession other;
        CHECK(!other.initialize()); // Only one session can bind the original globals.
        CHECK(!other.save_game(save, error) && contains(error, "bound session"));
        return true;
    }

    bool promotion()
    {
        std::unique_ptr<GameSession> game;
        GameSave save;
        CHECK(saved_world(game, save));
        std::string error;
        CHECK(save.world_path == "worlds/save1.dat" && save.image.records == 3 && save.image.console_saved);
        const std::vector<SaveClass> schema{{"CPlayerObj", 1}, {"SessionProbe", 1}};
        CHECK(save.image.classes == schema && save.fingerprint.classes == save.image.classes);
        CHECK(game->game_saves == 1 && *game->server.save_store.find(SaveStoreSlot::Current) == save.image);
        CHECK(game->server_shell.SaveGame(SAVETYPE_AUTO, true, true)); // The original Auto.sav image.
        const auto autosave = *game->server.save_store.find(SaveStoreSlot::Auto);
        const auto clock = game->server.save_clock();
        const auto due = game->objects.get(named(*game, "probe-a"))->due;
        const auto revision = game->world_revision, store_revision = game->server.save_store.revision();
        const auto authored_rows = game->entries.size();
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            const auto* owner = game.get();
            observed.clear();
            CHECK(load(game, save, error));
            std::cout << "Load " << cycle << ": " << error << '\n';
            CHECK(error.empty() && game.get() != owner && g_pServerDE == &game->server);
            const auto& report = game->game_load;
            CHECK(report.committed == cycle + 1 && report.rejected == 0 && report.stage.empty());
            CHECK(report.cleanup_complete && !report.source_retained && report.restored && report.records == 3);
            CHECK(game->world_revision == revision + cycle + 1 && report.destination_revision == game->world_revision);
            // The candidate copied the store, then replaced Current: the revision only grows.
            CHECK(report.destination_store_revision == report.source_store_revision + 1);
            CHECK(game->server.save_store.revision() > store_revision + cycle);
            CHECK(*game->server.save_store.find(SaveStoreSlot::Current) == save.image);
            CHECK(*game->server.save_store.find(SaveStoreSlot::Auto) == autosave);
            // Restore mode: always-load rows created, the probes come from the save.
            CHECK(game->entries.size() == authored_rows && game->world_path == "worlds/save1.dat");
            std::size_t created = 0, saved = 0;
            for (std::size_t i = 0; i < game->entries.size(); ++i)
            {
                const auto& row = game->world.objects[i];
                const auto state = game->entries[i].state;
                created += state == EntryState::Created;
                saved += state == EntryState::SavedState;
                CHECK(state == (row.class_name == "SessionProbe" ? EntryState::SavedState : EntryState::Created));
            }
            CHECK(created == 7 && saved == 2 && report.unknown == 0 && report.failed == 0);
            // The saved local player is attached to a new client (lot 5); the session runs.
            CHECK(game->state == SessionState::Ready && game->local_client &&
                  game->server_shell.Player(game->local_client) && game->client_shell.restored_entry);
            // The clock, console and probe state, before any step. The always-load observer drew
            // once from the restored generator (D10), so only the generator moved on.
            auto expected = clock;
            expected.generator = game->server.save_clock().generator;
            CHECK(game->server.save_clock() == expected && expected.generator != clock.generator);
            CHECK(game->objects.get(named(*game, "probe-a"))->due == due);
            auto *a = probe(*game, "probe-a"), *b = probe(*game, "probe-b");
            CHECK(a && b && a->value == 7 && b->value == 11 && a->ref == b->m_hObject && !b->ref);
            CHECK(var(*game, "SessionVar") == "saved" && var(*game, "GAME_WORLDNAME") == "worlds/save1.dat");
            // LoadWorld forced all three to 0; RespawnPlayer then set GOTOSTARTPOINT back to 1.
            CHECK(var(*game, "GAME_GOTOSTARTPOINT") == "1" && var(*game, "GAME_REVISITING") == "0" &&
                  var(*game, "GAME_DOAUTOSAVE") == "0");
            CHECK(SmellHint::m_dwNumSmells == 1 && ExitHint::m_dwNumExits == 1 && !g_hActiveCamera);
            CHECK(observed.size() == 1 && observed[0].reason == INITIALUPDATE_WORLDFILE);
            // A save right after the load reproduces the loaded LINK and ATCH bytes and every
            // probe record; the player record carries its listed original rewrites (save_player
            // checks them); CLCK differs only in the generator the always-load draw advanced.
            GameSave again;
            CHECK(game->save_game(again, error) && again.fingerprint == save.fingerprint);
            CHECK(again.image.links == save.image.links && again.image.attachments == save.image.attachments &&
                  again.image.classes == save.image.classes && again.image.records == save.image.records);
            std::vector<SaveEngineRecord> first, second;
            SaveError records_error;
            CHECK(decode_save_records(save.image.objects, first, records_error) &&
                  decode_save_records(again.image.objects, second, records_error) && first.size() == second.size());
            for (std::size_t i = 0; i < first.size(); ++i)
                CHECK(first[i].class_name == "CPlayerObj"
                          ? second[i].class_name == "CPlayerObj" && second[i].local_client
                          : first[i] == second[i]);
            // The console's rewrites: LoadWorld forces the shell variables to 0, then RespawnPlayer
            // sets GOTOSTARTPOINT to 1 again.
            std::map<std::string, std::string> console, reloaded;
            for (const auto& variable : save.image.console)
                console[variable.name] = variable.value;
            console["GAME_GOTOSTARTPOINT"] = "1";
            console["GAME_REVISITING"] = console["GAME_DOAUTOSAVE"] = "0";
            for (const auto& variable : again.image.console)
                reloaded[variable.name] = variable.value;
            CHECK(reloaded == console);
            SaveClock saved_again;
            SaveError codec;
            CHECK(decode_save_clock(again.image.clock, saved_again, codec) && saved_again == expected);
            for (int i = 0; i < 4; ++i)
                CHECK(game->step(.05));
            CHECK(game->server.message_count() == 0);
        }
        // A load from the menu: the bound session has no world to retain.
        CHECK(game->unload_world() && game->world_path.empty() && game->objects.snapshot().empty());
        const auto empty_revision = game->world_revision;
        CHECK(load(game, save, error) && game->world_path == "worlds/save1.dat");
        CHECK(game->world_revision == empty_revision + 1 && probe(*game, "probe-a")->value == 7);
        return true;
    }

    bool order()
    {
        // A nonzero world-time origin: a first world, then the saved one in the same session.
        auto game = std::make_unique<GameSession>();
        CHECK(setup(*game));
        std::string error;
        CHECK(game->request_world("other", error));
        for (int i = 0; i < 10; ++i)
            CHECK(game->step(.5));
        observed.clear();
        CHECK(game->request_world("save1", error));
        CHECK(observed.size() == 1 && observed[0].world_time == 0); // The world-load reset.
        const auto fresh_draw = observed[0].draw;                   // The first draw after seed 1.
        CHECK(game->enter_player(error));
        for (int i = 0; i < 8; ++i)
            CHECK(game->step(.25));
        GameSave save;
        CHECK(game->save_game(save, error));
        const auto clock = game->server.save_clock();
        CHECK(clock.time == 7 && clock.world_origin == 5 && clock.generator != 1);
        int next_draw = 0; // What the saved generator yields first.
        CHECK(game->server.RandomInt(0, 1000000, next_draw) && next_draw != fresh_draw);
        observed.clear();
        double precreate_time = -1;
        hooks.precreate = [&](float reason)
        {
            if (reason == PRECREATE_SAVEGAME)
                precreate_time = g_pServerDE->GetTime();
            return true;
        };
        CHECK(load(game, save, error));
        // The candidate's world load reseeded and reset; the saved values were applied after
        // that and before the first always-load object.
        CHECK(observed.size() == 1 && observed[0].time == 7 && observed[0].world_time == 2);
        CHECK(observed[0].draw == next_draw && precreate_time == 7);
        auto restored = clock;
        int draw = 0;
        CHECK(game->server.RandomInt(0, 1000000, draw)); // The always-load draw advanced it once.
        restored.generator = game->server.save_clock().generator;
        CHECK(game->server.save_clock() == restored && game->server.world_time() == 2);
        return true;
    }

    bool console()
    {
        // Decision 3: the saved console wins over WorldProperties; the global force does not.
        std::unique_ptr<GameSession> game;
        GameSave save;
        game = std::make_unique<GameSession>();
        CHECK(setup(*game));
        std::string error;
        CHECK(game->request_world("save1", error) && game->enter_player(error));
        CHECK(var(*game, "WorldName") == "Authored" && game->server.gravity.y == -1500);
        game->server.SetGameConVar("WorldName", "Changed at runtime");
        game->server.SetGameConVar("GAME_DIFFICULTY", "2");
        DVector force{0, -100, 0};
        CHECK(game->server.SetGlobalForce(&force) == DE_OK);
        CHECK(game->save_game(save, error));
        CHECK(load(game, save, error));
        CHECK(var(*game, "WorldName") == "Changed at runtime" && var(*game, "GAME_DIFFICULTY") == "2");
        CHECK(game->server.gravity.y == -1500); // WorldProperties' authored value, as the original.
        // A world without WorldProperties keeps the saved global force.
        CHECK(game->request_world("other", error) && game->enter_player(error));
        CHECK(game->server.SetGlobalForce(&force) == DE_OK && game->save_game(save, error));
        CHECK(load(game, save, error) && game->server.gravity.y == -100);
        return true;
    }

    bool failures()
    {
        std::unique_ptr<GameSession> game;
        GameSave save;
        CHECK(saved_world(game, save));
        std::string error;
        const auto refused = [&](const GameSave& bad, std::string_view stage, std::string_view message,
                                 std::span<const ClassInfo* const> classes = extra)
        {
            SourceState state;
            CHECK(state.capture(*game));
            CHECK(!GameSession::load_game(game, bad, error, classes));
            std::cout << stage << ": " << error << '\n';
            CHECK(contains(error, message) && game->game_load.error == error);
            CHECK(state.unchanged(*game, stage));
            hooks = {};
            return true;
        };
        // Admission.
        WorldMessage level;
        CHECK(original_world_request("other", 0, LOADTYPE_NEW_LEVEL, 0, level));
        CHECK(game->queue_level_change(level, "start", error));
        CHECK(refused(save, "admission", "level change is pending"));
        game->pending_level_change.reset();
        auto bad = save;
        bad.world_path = "Worlds\\Save1.dat";
        CHECK(refused(bad, "admission", "normalized world DAT"));
        bad = save;
        bad.image.param = 1; // A keep-alive image is not a game save.
        CHECK(refused(bad, "admission", "full single-player save"));
        bad = save;
        bad.image.console_saved = false;
        CHECK(refused(bad, "admission", "full single-player save"));
        bad = save;
        for (auto& variable : bad.image.console)
            if (variable.name == "GAME_GAMETYPE")
                variable.value = "2"; // CTF: refused by name (lot 13).
        CHECK(refused(bad, "admission", "CTF saves cannot be loaded"));
        bad = save;
        for (auto& variable : bad.image.console)
            if (variable.name == "GAME_GAMETYPE")
                variable.value = "1"; // Bloodbath.
        CHECK(refused(bad, "admission", "full single-player save"));
        bad = save;
        bad.fingerprint.classes.clear();
        CHECK(refused(bad, "admission", "class schema differs"));
        // Fingerprint.
        bad = save;
        bad.fingerprint.world[0] ^= 1;
        CHECK(refused(bad, "fingerprint", "world DAT differs"));
        bad = save;
        bad.fingerprint.mounts.push_back({MountRole::Mod, "extra.rez", 1, {}});
        CHECK(refused(bad, "fingerprint", "archive extra.rez removed"));
        const std::array<const ClassInfo*, 2> newer{&probe_v2, &always_type};
        CHECK(refused(save, "fingerprint", "class SessionProbe payload version 1, build 2", newer));
        CHECK(refused(save, "fingerprint", "class SessionProbe is not registered", {}));
        const std::array<const ClassInfo*, 1> null_class{nullptr};
        CHECK(refused(save, "fingerprint", "class schema cannot be built", null_class));
        // The restore-mode world and the restore.
        bad = save;
        bad.image.clock.resize(3);
        CHECK(refused(bad, "world", "Saved console and clock"));
        hooks.precreate = [](float reason) { return reason != PRECREATE_SAVEGAME; };
        CHECK(refused(save, "restore", "was refused"));
        hooks.load = [](HOBJECT, HMESSAGEREAD) {}; // Under-read.
        CHECK(refused(save, "restore", "MID_LOADOBJECT failed"));
        hooks.load = [](HOBJECT, HMESSAGEREAD read) // Over-read.
        {
            for (int i = 0; i < 4; ++i)
                g_pServerDE->ReadFromMessageDWord(read);
        };
        CHECK(refused(save, "restore", "MID_LOADOBJECT failed"));
        bad = save;
        bad.image.objects.resize(bad.image.objects.size() - 1);
        CHECK(refused(bad, "restore", "RestoreObjects"));
        // Without the local-client flag the restored player is not the local one (D6).
        std::vector<SaveEngineRecord> records;
        SaveError codec;
        CHECK(decode_save_records(save.image.objects, records, codec));
        for (auto& record : records)
            record.local_client = false;
        bad = save;
        CHECK(encode_save_records(records, bad.image.objects, codec));
        CHECK(refused(bad, "player", "no local player"));
        // The admission checks of checked startup and NEW_LEVEL.
        hooks.loaded = [](HOBJECT) { g_pServerDE->StartHMessageWrite(); }; // Leaked by a Load.
        CHECK(refused(save, "validation", "Live messages remain"));
        hooks.loaded = [](HOBJECT) { static_cast<ServerAdapter*>(g_pServerDE)->record_unavailable("synthetic"); };
        CHECK(refused(save, "validation", "unavailable services"));
        // Promotion: the audio output transfer is the last step that can fail.
        CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
        CHECK(game->server.audio.open_output(error));
        hooks.loaded = [](HOBJECT)
        {
            std::string why;
            static_cast<ServerAdapter*>(g_pServerDE)->audio.open_output(why);
        };
        CHECK(refused(save, "audio", "separate offline destination"));
        CHECK(game->server.audio.has_output());
        // Worlds whose own checks refuse: an unported row and a missing world texture.
        for (auto [world, message] : {std::pair{"unported", "1 unsupported classes"}, std::pair{"bare", "absent.dtx"}})
        {
            CHECK(saved_world(game, save, world));
            CHECK(refused(save, "validation", message));
        }
        // Each refusal destroyed its candidate; a final load still commits and keeps the output.
        CHECK(saved_world(game, save));
        CHECK(game->server.audio.open_output(error));
        CHECK(load(game, save, error) && game->server.audio.has_output());
        CHECK(game->game_load.rejected == 0 && game->game_load.committed == 1);
        return true;
    }

    bool private_worlds(const std::filesystem::path& data)
    {
        // Real worlds restored from a player-only image: the DAT's CF_ALWAYSLOAD rows come back,
        // everything else is SavedState, the saved local player is reattached with its health,
        // armor, inventory, position and powers (lot 5), and the restored world saves and loads
        // again through the original shell. Only these single-player worlds have no SkyPointer;
        // every other one binds its sky slots to Door or CDestructableBrush objects, which must come
        // from the save (lot 8).
        for (const auto* name : {"01_SubwayA", "04_SteamTunnels", "08_SubwayB", "14_SubwayC", "20_PowerStation"})
        {
            auto game = std::make_unique<GameSession>();
            CHECK(game->initialize());
            Config config;
            config.data_dir = data;
            std::string error;
            CHECK(game->resources.mount_game(config, error));
            CHECK(game->request_world(name, error));
            if (!game->run_world(error))
                std::cerr << name << " entry: " << error << '\n';
            CHECK(game->local_client);
            std::size_t fresh_always = 0;
            for (std::size_t i = 0; i < game->entries.size(); ++i)
                if (const auto type = game->objects.find_class(game->world.objects[i].class_name);
                    type && save_support(type) == SaveSupport::AlwaysLoad)
                    ++fresh_always;
            for (int i = 0; i < 30; ++i)
                CHECK(game->step(.05));
            // A whole save is refused by name while a class lot is missing; once every class of the
            // world is supported it succeeds (08_SubwayB since lots 7, 10 and 11).
            GameSave whole;
            if (game->save_game(whole, error))
                std::cout << name << ": saved whole\n";
            else
            {
                CHECK(contains(error, "(Unsupported)"));
                std::cout << name << " refusal: " << error.substr(0, 160) << "...\n";
            }
            game->server.SetGameConVar("FarZ", "1234"); // WorldProperties sets it at creation.
            DVector force{0, -1, 0};
            DVector authored{};
            CHECK(game->server.GetGlobalForce(&authored) == DE_OK && game->server.SetGlobalForce(&force) == DE_OK);
            auto* player = game->server_shell.Player(game->local_client);
            CHECK(player);
            player->m_damage.SetHitPoints(61);
            player->m_damage.SetArmorPoints(17);
            player->m_InventoryMgr.SetAmmoCount(1, 77);
            CHECK(player->m_InventoryMgr.AddKey({"key1", "Key One", "key.pcx", "key_h.pcx", 1}) == InventoryResult::Ok);
            CHECK(player->AddPowerup(7, 30) && player->AddPowerup(5, 40)); // Anger and Willpower.
            auto position = game->objects.get(player->m_hObject)->state.m_Pos;
            float anger = player->m_fTripleDamageTime - game->server.GetTime();
            // Every other saveable class is Unsupported until its lot: the image holds the player.
            SaveObjectsReport saved;
            const std::array<HOBJECT, 1> list{player->m_hObject};
            CHECK(game->server.save_objects("Save\\Current\\Current.sav", list, 0,
                                            SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS, saved));
            GameSave save{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
            SaveError fingerprint;
            CHECK(
                build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint, fingerprint));
            auto clock = game->server.save_clock();
            for (unsigned cycle = 0; cycle < 2; ++cycle)
            {
                CHECK(load(game, save, error, {}));
                const auto& report = game->game_load;
                std::cout << name << " load " << cycle << ": " << report.entries.size() << " rows, "
                          << report.checked_textures << " textures, " << game->objects.snapshot().size() << " objects; "
                          << error << '\n';
                CHECK(error.empty() && report.unknown == 0 && report.failed == 0 && report.records == 1);
                std::size_t always = 0, saved_rows = 0;
                for (std::size_t i = 0; i < game->entries.size(); ++i)
                {
                    const auto state = game->entries[i].state;
                    const auto type = game->objects.find_class(game->world.objects[i].class_name);
                    if (type && save_support(type) == SaveSupport::AlwaysLoad)
                        ++always;
                    else
                        CHECK(state != EntryState::Created && state != EntryState::Failed);
                    saved_rows += state == EntryState::SavedState;
                }
                CHECK(always == fresh_always && saved_rows > 0);
                for (auto id : game->objects.snapshot())
                {
                    const auto support = save_support(game->objects.get(id)->type);
                    CHECK(support == SaveSupport::AlwaysLoad || support == SaveSupport::NotSaved ||
                          game->objects.get(id)->type->name == "CPlayerObj");
                }
                CHECK(game->objects.time() == clock.time &&
                      game->server.world_time() == clock.time - clock.world_origin);
                CHECK(var(*game, "FarZ") == "1234");
                DVector restored{};
                CHECK(game->server.GetGlobalForce(&restored) == DE_OK && restored.y == authored.y);
                // The reattached local player.
                player = game->server_shell.Player(game->local_client);
                CHECK(player && game->state == SessionState::Ready && game->client_shell.m_bInWorld);
                const auto& at = game->objects.get(player->m_hObject)->state.m_Pos;
                CHECK(at.x == position.x && at.y == position.y && at.z == position.z);
                CHECK(player->m_damage.m_fHitPoints == 61 && player->m_damage.m_fArmorPoints == 17 &&
                      player->m_InventoryMgr.GetAmmoCount(1) == 77 && player->m_InventoryMgr.keys.size() == 1);
                CHECK(player->IsPowerupActive(PU_TRIPLEDAMAGE) && player->IsPowerupActive(PU_INVULNERABLE) &&
                      player->m_InventoryMgr.damage_multiplier == 3 && player->m_damage.m_bNighInvulnerable);
                CHECK(std::abs(player->m_fTripleDamageTime - game->server.GetTime() - anger) < 1e-3f);
                CHECK(game->client_shell.client_data_loads == cycle); // The shell save attaches the blob.
                for (int i = 0; i < 30; ++i)
                    CHECK(game->step(.05));
                anger = player->m_fTripleDamageTime - game->server.GetTime();
                position = game->objects.get(player->m_hObject)->state.m_Pos; // The player may settle.
                CHECK(game->save_game(save, error) && save.image.records == 1);
                clock = game->server.save_clock();
                game->server.SetGameConVar("FarZ", "authored"); // The saved value must win again.
            }
        }
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    if (mode == "private")
        return private_worlds(argv[2]) ? 0 : 1;
    if (!write_fixture())
        return 1;
    const bool ok = mode == "declarations" ? declarations()
                    : mode == "marking"    ? marking()
                    : mode == "refusals"   ? refusals()
                    : mode == "promotion"  ? promotion()
                    : mode == "order"      ? order()
                    : mode == "console"    ? console()
                    : mode == "failures"   ? failures()
                                           : false;
    return ok ? 0 : 1;
}
