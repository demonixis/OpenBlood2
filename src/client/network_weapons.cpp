#include "client/network_weapons.h"
#include <cmath>
namespace OpenBlood2
{
    bool NetworkWeapons::begin(const Net::Joined& owner)
    {
        if (!owner.world || !owner.player || !owner.object || !owner.object.generation || owner.object.slot >= 65536)
            return false;
        owner_ = owner;
        sequence_ = updates = sound_requests = 0;
        state_.reset();
        views_ = {};
        time_ = 0;
        return true;
    }
    NetworkWeapons::Reception NetworkWeapons::receive(const Net::PrivateEvent& event, ModelStore& models,
                                                      Resources& resources, std::string& error)
    {
        error.clear();
        if (!owner_.world || !Net::valid_private(event) || double(event.tick) / 60 > 1e12)
        {
            error = "Invalid private weapon state";
            return Reception::Refused;
        }
        const auto* next = std::get_if<Net::WeaponLoadout>(&event.value);
        if (!next || event.owner != owner_ || event.sequence <= sequence_ ||
            (state_ && next->force_revision < state_->force_revision))
            return Reception::Ignored;
        std::array<CViewWeapon, 2> prepared;
        std::array<bool, 2> changed{};
        for (unsigned hand = 0; hand < 2; ++hand)
        {
            const auto type = next->weapons[hand];
            // Original SyncWeapons: ammunition or same-type replacement does
            // not restart the draw/fire animation.
            changed[hand] = views_[hand].type != type || (type && views_[hand].character != next->character);
            if (!changed[hand] || !type)
                continue;
            const auto* definition = weapon_definition(type);
            std::shared_ptr<const Model> held;
            if (!definition || !models.load(definition->m_szHandModelFilename, held, error) ||
                !resources.find(definition->m_szHandModelSkin) ||
                !prepared[hand].Create(type, hand != 0, models, resources, error, next->character))
            {
                if (error.empty())
                    error = "Required network weapon resources are unavailable";
                return Reception::Refused;
            }
        }
        for (unsigned hand = 0; hand < 2; ++hand)
            if (changed[hand])
                views_[hand] = std::move(prepared[hand]);
        if (!state_)
            time_ = double(event.tick) / 60;
        state_ = *next;
        sequence_ = event.sequence;
        return Reception::Applied;
    }
    namespace
    {
        class PresentationWeapon final : public WeaponServices
        {
          public:
            PresentationWeapon(const Net::WeaponLoadout& state, unsigned hand, float time, std::uint64_t& sounds)
                : state_(state), hand_(hand), time_(time), sounds_(sounds)
            {
            }
            float time() const override
            {
                return time_;
            }
            bool has_player() const override
            {
                return true;
            }
            bool player_alive() const override
            {
                return state_.alive;
            }
            bool weapon_selected(DBYTE type, bool left) const override
            {
                return unsigned(left) == hand_ && state_.weapons[hand_] == type;
            }
            float ammunition(bool alternate) const override
            {
                return (alternate ? state_.alternate_ammunition : state_.ammunition)[hand_];
            }
            FireResult fire(const WeaponFire&) override
            {
                return FireResult::Blocked;
            }
            SoundId play_sound(const SoundRequest&, std::string& error) override
            {
                ++sounds_;
                error = "Remote weapon animation sound output is not connected";
                return 0;
            }

          private:
            const Net::WeaponLoadout& state_;
            unsigned hand_;
            float time_;
            std::uint64_t& sounds_;
        };
    } // namespace
    bool NetworkWeapons::update(std::uint64_t force_revision, double seconds, std::string& error)
    {
        error.clear();
        if (!std::isfinite(seconds) || seconds < 0 || seconds > .1 || time_ + seconds > 1e12)
        {
            error = "Invalid network weapon tick";
            return false;
        }
        if (!ready(force_revision))
            return true;
        time_ += seconds;
        for (unsigned hand = 0; hand < 2; ++hand)
        {
            PresentationWeapon services(*state_, hand, float(time_), sound_requests);
            views_[hand].binoculars_active = state_->binoculars;
            if (!views_[hand].Update(services, {}, {}, {0, 0, 1}, seconds, error))
                return false;
        }
        ++updates;
        return true;
    }
} // namespace OpenBlood2
