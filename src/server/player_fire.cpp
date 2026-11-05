#include "engine/collision.h"
#include "server/session.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    ServerSession::CommandResult ServerSession::queue_weapon_frame(std::uint64_t id, const Net::WeaponFrame& value)
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (found == players_.end() || !Net::valid_weapon_frame(value) || value.owner.player != id)
            return CommandResult::Invalid;
        if (value.owner.world != world_id_ || value.owner.object != found->object ||
            value.sequence <= found->weapon_received)
            return CommandResult::Ignored;
        const auto state = std::find_if(found->weapon_states.begin(), found->weapon_states.end(),
                                        [&](const auto& record) { return record.sequence == value.state; });
        if (value.sequence != found->weapon_received + 1 || value.state < found->weapon_received_state ||
            found->queued_weapon_frames >= Net::weapon_frame_queue_limit || state == found->weapon_states.end() ||
            state->value.force_revision != value.owner.force_revision)
            return CommandResult::Invalid;
        found->pending_controls.push_back(value);
        ++found->queued_weapon_frames;
        found->weapon_received = value.sequence;
        found->weapon_received_state = value.state;
        return CommandResult::Queued;
    }
    bool ServerSession::weapon_frame_ack(std::uint64_t id, std::optional<Net::WeaponFrameAck>& out) const
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (found == players_.end())
            return false;
        out.reset();
        if (found->weapon_processed)
            out = Net::WeaponFrameAck{found->setup.owner, found->weapon_processed, found->fired, found->weapon_blocked};
        return true;
    }
    namespace
    {
        // Reconstruct the client's animation from state that this server issued.
        // Cosmetic acceptance advances that proof exactly like the client; the
        // separate delivery callback checks current authoritative gameplay state.
        template <class Deliver, class Sound> class TimelineWeapon final : public WeaponServices
        {
          public:
            TimelineWeapon(const Net::WeaponLoadout& state, unsigned hand, float time, Deliver& deliver, Sound& sound)
                : state_(state), hand_(hand), time_(time), deliver_(deliver), sound_(sound)
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
            FireResult fire(const WeaponFire& request) override
            {
                if (!state_.fire_allowed)
                    return FireResult::Blocked;
                const auto* data = weapon_definition(request.weapon);
                if (!data ||
                    ammunition(request.alternate) < (request.alternate ? data->m_nAltAmmoUse : data->m_nAmmoUse))
                    return FireResult::Dry;
                deliver_(request);
                return FireResult::Fired;
            }
            void animation_sound(DBYTE weapon, WeaponSoundAction action, std::string_view resource,
                                 std::string& error) override
            {
                sound_(weapon, action, resource, error);
            }

          private:
            const Net::WeaponLoadout& state_;
            unsigned hand_;
            float time_;
            Deliver& deliver_;
            Sound& sound_;
        };
    } // namespace
    bool ServerSession::apply_weapon_frame(Player& p, const Net::WeaponFrame& frame, std::string& error)
    {
        const auto found = std::find_if(p.weapon_states.begin(), p.weapon_states.end(),
                                        [&](const auto& record) { return record.sequence == frame.state; });
        if (found == p.weapon_states.end() || !p.weapon_clock_started || p.weapon_time + ServerClock::step > 1e12 ||
            frame.sequence != p.weapon_processed + 1 || !refresh_movement(p, error))
        {
            if (error.empty())
                error = "Weapon frame lost its checked state or sequence";
            return false;
        }
        const auto state = found->value;
        // The reliable stream delivers every intervening loadout to the client,
        // even when no weapon tick runs between them. Reproduce those resource/
        // cancellation transitions before advancing time (never replay movement).
        for (const auto& record : p.weapon_states)
        {
            if (record.sequence <= p.weapon_applied_state)
                continue;
            if (record.sequence > frame.state)
                break;
            const auto& received = record.value;
            std::array<CViewWeapon, 2> prepared;
            std::array<bool, 2> changed{};
            for (unsigned hand = 0; hand < 2; ++hand)
            {
                changed[hand] = p.weapon_view_revision[hand] != received.view_revision[hand];
                if (changed[hand] && received.weapons[hand] &&
                    !prepared[hand].Create(received.weapons[hand], hand != 0, game_.models, game_.resources, error,
                                           received.character, CViewWeapon::ResourcesMode::AnimationOnly))
                    return false;
            }
            for (unsigned hand = 0; hand < 2; ++hand)
            {
                if (changed[hand])
                {
                    p.weapon_timeline[hand] = std::move(prepared[hand]);
                    p.weapon_view_revision[hand] = received.view_revision[hand];
                }
                if (!received.alive)
                    p.weapon_timeline[hand].CancelFiringState();
            }
            p.weapon_applied_state = record.sequence;
        }
        p.weapon_time += ServerClock::step;
        WeaponInput input{frame.primary, !frame.primary && frame.alternate, false, true};
        if (!state.input_enabled || changing_level())
            input.primary = input.alternate = false;
        if (state.weapons[0] && state.weapons[1] && input.alternate)
        {
            input.primary = true;
            input.alternate = false;
        }
        const float cp = std::cos(frame.aim.pitch);
        const DVector direction{std::sin(frame.aim.yaw) * cp, -std::sin(frame.aim.pitch), std::cos(frame.aim.yaw) * cp};
        for (unsigned hand = 0; hand < 2; ++hand)
        {
            bool delivered = true;
            const auto fire = [&](const WeaponFire& predicted)
            {
                if (!delivered)
                    return;
                auto* owner = game_.server_shell.Player(p.client);
                const auto* object = game_.objects.get(p.object);
                const auto* weapon = owner ? owner->m_InventoryMgr.CurrentWeapon(hand != 0) : nullptr;
                if (changing_level() || !owner || !object || object->life != Life::Alive ||
                    owner->m_hObject != object->handle || frame.owner != p.setup.owner ||
                    p.source_revision != owner->movement_revision || p.pose_revision != object->pose_revision ||
                    !p.weapons || p.weapons->view_revision[hand] != state.view_revision[hand] ||
                    owner->m_nCharacter != state.character || !weapon || weapon->data.m_nType != predicted.weapon ||
                    owner->m_bDead || owner->m_damage.IsDead() || !owner->attributes_received ||
                    owner->MovementBlocked() || owner->camera_movement_blocked ||
                    (owner->m_InventoryMgr.eye && owner->m_InventoryMgr.eye->active) ||
                    (owner->m_InventoryMgr.binoculars && owner->m_InventoryMgr.binoculars->active))
                {
                    ++p.weapon_blocked;
                    return;
                }
                auto actual = predicted;
                actual.position = object->state.m_Pos;
                actual.position.y += owner->eye_level;
                if (!physics_vector(actual.position) || !physics_vector(actual.direction))
                {
                    delivered = false;
                    error = "Authoritative weapon eye/direction is invalid";
                    return;
                }
                const auto result = game_.server_shell.OnMessage(p.client, actual);
                if (result == FireResult::Fired)
                    ++p.fired[hand];
                else if (result == FireResult::Dry || result == FireResult::Blocked)
                    ++p.weapon_blocked;
                else
                {
                    delivered = false;
                    error = "Authoritative network weapon fire failed";
                }
            };
            const auto sound =
                [&](DBYTE type, WeaponSoundAction action, std::string_view resource, std::string& sound_error)
            {
                sound_error.clear();
                auto* owner = game_.server_shell.Player(p.client);
                const auto* object = game_.objects.get(p.object);
                const auto* weapon = owner ? owner->m_InventoryMgr.CurrentWeapon(hand != 0) : nullptr;
                // Keys come only from the issued ABC timeline. Sounds attach to
                // the current body: a position correction does not change their
                // life/view or invalidate its delayed STOP. A death boundary does.
                if (changing_level() || !owner || !object || object->life != Life::Alive ||
                    owner->m_hObject != object->handle || frame.state < p.weapon_sound_life_sequence || !p.weapons ||
                    p.weapons->view_revision[hand] != state.view_revision[hand] ||
                    owner->m_nCharacter != state.character || !weapon || weapon->data.m_nType != type ||
                    owner->m_bDead || !owner->attributes_received)
                {
                    ++weapon_sounds_blocked;
                    return;
                }
                // Draw/reload sounds are not fire attempts: retain their source
                // camera/binocular/ammunition behavior. Failure is diagnostic and
                // never reverses a shot or advances the gameplay RNG.
                owner->HandleWeaponSound(action, resource);
                sound_error = owner->weapon_sound_error;
            };
            TimelineWeapon services(state, hand, float(p.weapon_time), fire, sound);
            auto& view = p.weapon_timeline[hand];
            view.binoculars_active = state.binoculars;
            if (!state.alive)
                view.CancelFiringState();
            if (!view.Update(services, input, {}, direction, ServerClock::step, error) || !delivered)
                return false;
        }
        p.weapon_processed = frame.sequence;
        while (p.weapon_states.front().sequence < frame.state)
            p.weapon_states.pop_front();
        return true;
    }
} // namespace OpenBlood2
