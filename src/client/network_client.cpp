#include "client/network_client.h"
#include "game/game_settings.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    using namespace std::chrono_literals;
    bool NetworkClient::fail(std::string& error, const char* fallback)
    {
        if (error.empty())
            error = fallback;
        reason_ = error;
        transport_.disconnect(peer_, std::uint32_t(Net::Reject::Malformed));
        transport_.flush();
        transport_.close();
        speech_.clear();
        audio_.clear();
        effects_.clear();
        phase_ = Phase::Failed;
        return false;
    }
    bool NetworkClient::send(const Net::AdmissionMessage& value, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        return Net::encode_admission(value, bytes) && transport_.send(peer_, Net::Channel::Control, bytes, error);
    }
    bool NetworkClient::prepare(const Config& config, std::string& error)
    {
        error.clear();
        if (phase_ != Phase::Idle)
        {
            error = "Client content preparation requires a fresh endpoint";
            return false;
        }
        if (!resources_.mount_game(config, error) || !Net::content_fingerprint(resources_, digest_, error))
            return fail(error, "Client content initialization failed");
        phase_ = Phase::Prepared;
        return true;
    }
    bool NetworkClient::connect(std::string_view host, std::uint16_t port, const Net::JoinProfile& profile,
                                Clock::time_point now, std::string& error)
    {
        error.clear();
        if (phase_ != Phase::Prepared || !Net::valid_profile(profile))
        {
            error = "Client connection requires prepared content and a valid profile";
            return false;
        }
        if (!transport_.open_client(error))
            return fail(error, "Client transport initialization failed");
        peer_ = transport_.connect(host, port, error);
        if (!peer_)
            return fail(error, "Cannot connect to server");
        profile_ = profile;
        origin_ = last_ = now;
        deadline_ = now + 5s;
        phase_ = Phase::Connecting;
        return true;
    }
    bool NetworkClient::observe(double server, Clock::time_point now)
    {
        Net::PeerStats stats;
        const double one_way = transport_.stats(peer_, stats) ? double(stats.round_trip_ms) / 2000 : 0;
        return clock_.observe(server, std::chrono::duration<double>(now - origin_).count(), one_way);
    }
    bool NetworkClient::complete_baseline(Clock::time_point now, std::string& error)
    {
        if (!initial_ || !joined_.player)
            return true;
        if (initial_->player != joined_.object || !receiver_.receive(*initial_, geometry_, models_, sprites_, error) ||
            !observe(initial_->time, now) || !send(Net::BaselineReady{offer_.world, initial_->sequence}, error))
            return false;
        initial_.reset();
        phase_ = Phase::Active;
        deadline_ = now + 15s;
        input_ = {};
        return true;
    }
    bool NetworkClient::receive(const Net::Event& event, Clock::time_point now, std::string& error)
    {
        if (event.kind == Net::EventKind::Disconnected)
        {
            reason_ = event.reason ? Net::reject_text(event.reason) : "Disconnected";
            speech_.clear();
            audio_.clear();
            effects_.clear();
            phase_ = Phase::Closed;
            transport_.close();
            return true;
        }
        if (phase_ == Phase::Closing)
            return true;
        if (event.channel == Net::Channel::Effects || Net::is_effect_packet(event.bytes) ||
            Net::is_effect_batch(event.bytes))
        {
            // Only batches, only on their own channel; a stray single effect is malformed.
            std::vector<Net::EffectEvent> batch;
            if (event.channel != Net::Channel::Effects ||
                Net::decode_effect_batch(event.bytes, batch) != Net::DecodeEffect::Ok)
                return false;
            for (const auto& message : batch)
                if (message.world >= offer_.world && (phase_ != Phase::Active || !effects_.receive(message, error)))
                    return false;
            return true;
        }
        if (Net::is_audio_packet(event.bytes))
        {
            Net::AudioEvent message;
            if (event.channel != Net::Channel::Control ||
                Net::decode_audio(event.bytes, message) != Net::DecodeAudio::Ok)
                return false;
            if (message.world < offer_.world)
                return true;
            return phase_ == Phase::Active && audio_.receive(message, resources_, error);
        }
        if (Net::is_weapon_frame_packet(event.bytes))
        {
            Net::WeaponFrameMessage message;
            if (phase_ != Phase::Active || event.channel != Net::Channel::Control ||
                Net::decode_weapon_frame(event.bytes, message) != Net::DecodeWeaponFrame::Ok)
                return false;
            const auto* ack = std::get_if<Net::WeaponFrameAck>(&message);
            if (!ack || ack->owner.world != joined_.world || ack->owner.player != joined_.player ||
                ack->owner.object != joined_.object || ack->sequence > weapon_frames)
                return false;
            if (weapon_ack_ && ack->sequence <= weapon_ack_->sequence)
                return true;
            if (weapon_ack_ && (ack->owner.force_revision < weapon_ack_->owner.force_revision ||
                                ack->fired[0] < weapon_ack_->fired[0] || ack->fired[1] < weapon_ack_->fired[1] ||
                                ack->blocked < weapon_ack_->blocked))
                return false;
            // ACKs may precede a new force setup. These are only
            // cumulative diagnostics; an ACK never moves the player or grants ammo.
            weapon_ack_ = *ack;
            return true;
        }
        if (Net::is_command_packet(event.bytes))
        {
            Net::CommandMessage message;
            if (phase_ != Phase::Active || event.channel != Net::Channel::Control ||
                Net::decode_command(event.bytes, message) != Net::DecodeCommand::Ok)
                return false;
            const auto* ack = std::get_if<Net::CommandAck>(&message);
            if (!ack || ack->owner.world != joined_.world || ack->owner.player != joined_.player ||
                ack->owner.object != joined_.object)
                return false;
            if (ack->sequence <= command_acks)
                return true;
            if (pending_commands_.empty() || ack->sequence != pending_commands_.front().sequence ||
                ack->owner != pending_commands_.front().owner)
                return false;
            command_acks = ack->sequence;
            commands_applied += ack->applied;
            pending_commands_.erase(pending_commands_.begin());
            return true;
        }
        if (event.kind == Net::EventKind::Connected)
        {
            if (phase_ != Phase::Connecting || !send(Net::Hello{Net::Edition::Retail, digest_}, error))
                return false;
            phase_ = Phase::Offer;
            deadline_ = now + 30s;
            return true;
        }
        if (Net::is_private_packet(event.bytes))
        {
            Net::PrivateEvent value;
            if (event.channel != Net::Channel::Control ||
                Net::decode_private(event.bytes, value) != Net::DecodePrivate::Ok)
                return false;
            if (value.owner.world < offer_.world)
                return true;
            if (phase_ != Phase::Active || value.owner != joined_)
                return false;
            const auto received = inbox_.receive(value, error);
            if (received == NetworkPlayerEvents::Reception::Refused)
                return false;
            if (received == NetworkPlayerEvents::Reception::Ignored)
                return true;
            if (camera_.receive(value, error) == NetworkCamera::Reception::Refused ||
                weapons_.receive(value, models_, resources_, error) == NetworkWeapons::Reception::Refused)
                return false;
            if (std::holds_alternative<Net::LevelChanging>(value.value))
            {
                changing_level_ = true;
                zoom_ = false;
                zoom_crosshair_ = 0;
                weapons_.cancel_firing();
            }
            if (const auto* aim = std::get_if<PlayerRotation>(&value.value))
            {
                pitch_ = aim->pitch;
                yaw_ = aim->yaw;
            }
            if (const auto* zoom = std::get_if<ZoomView>(&value.value))
            {
                zoom_ = zoom->on;
                zoom_crosshair_ = zoom->type;
            }
            if (!speech_.receive(value, std::chrono::duration<double>(now - origin_).count(),
                                 bool(camera_.camera().view.object), inbox_.roster(), error))
                return false;
            return observe(double(value.tick) / 60, now);
        }
        if (Net::is_movement_packet(event.bytes))
        {
            Net::MovementMessage value;
            if (Net::decode_movement(event.bytes, value) != Net::DecodeMovement::Ok)
                return false;
            if (std::visit([](const auto& v) { return v.owner.world; }, value) < offer_.world)
                return true;
            if (phase_ != Phase::Active)
                return false;
            if (const auto* setup = std::get_if<Net::MovementSetup>(&value))
            {
                if (event.channel != Net::Channel::Control || setup->owner.world != joined_.world ||
                    setup->owner.player != joined_.player || setup->owner.object != joined_.object)
                    return false;
                if (!setup_ || setup->sequence > setup_->sequence)
                    setup_ = *setup;
                return true;
            }
            if (const auto* impulse = std::get_if<Net::MovementImpulse>(&value))
            {
                if (event.channel != Net::Channel::Control || impulse->owner.world != joined_.world ||
                    impulse->owner.player != joined_.player || impulse->owner.object != joined_.object ||
                    impulses_.size() >= Net::movement_impulse_limit)
                    return false;
                impulses_.push_back(*impulse);
                return true;
            }
            const auto* ack = std::get_if<Net::MovementAck>(&value);
            return ack && event.channel == Net::Channel::State &&
                   movement_.receive(*ack, error) != NetworkMovement::Reception::Refused;
        }
        if (event.channel == Net::Channel::State)
        {
            Net::WorldState value;
            if (!Net::decode_world_state(event.bytes, value))
                return false;
            if (value.world < offer_.world)
                return true;
            if (phase_ != Phase::Active)
                return false;
            const auto result = receiver_.receive(value, error);
            if (result == NetworkWorldReceiver::StateResult::Refused)
                return false;
            if (result == NetworkWorldReceiver::StateResult::Applied)
            {
                ++state_packets;
                return observe(value.time, now);
            }
            return true;
        }
        if (event.channel == Net::Channel::Baseline)
        {
            std::uint64_t world = 0;
            if (!Net::world_chunk_epoch(event.bytes, world))
                return false;
            if (world < offer_.world)
                return true;
            if ((phase_ != Phase::Baseline && phase_ != Phase::Active) || !baseline_)
                return false;
            Net::WorldUpdate value;
            const auto result = baseline_->receive(event.bytes, value);
            if (result == Net::BaselineResult::Refused)
                return false;
            if (result != Net::BaselineResult::Complete)
                return true;
            if (const auto* initial = std::get_if<Net::WorldSnapshot>(&value))
            {
                if (phase_ != Phase::Baseline || initial_)
                    return false;
                initial_ = *initial;
                return complete_baseline(now, error);
            }
            if (phase_ != Phase::Active || !receiver_.receive(value, geometry_, models_, sprites_, error))
                return false;
            return observe(receiver_.world().frame().time, now);
        }
        if (event.channel != Net::Channel::Control)
            return false;
        Net::AdmissionMessage value;
        if (Net::decode_admission(event.bytes, value) != Net::DecodeAdmission::Ok)
            return false;
        if (const auto* offer = std::get_if<Net::WorldOffer>(&value))
        {
            if ((phase_ != Phase::Offer && phase_ != Phase::Active && phase_ != Phase::Baseline) ||
                !network_game_type(offer->mode) || offer->world <= offer_.world)
                return false;
            // End every old consumer before replacing geometry. Sound output and
            // mounted content persist; no live voice/body/model crosses worlds.
            speech_.clear();
            audio_.clear();
            effects_.clear();
            models_.clear();
            sprites_.clear();
            initial_.reset();
            setup_.reset();
            impulses_.clear();
            joined_ = {};
            weapon_ack_.reset();
            pending_commands_.clear();
            command_sequence_ = command_acks = weapon_frames = 0;
            changing_level_ = zoom_ = false;
            zoom_crosshair_ = 0;
            fov_x_ = original_fov_normal;
            pitch_ = yaw_ = 0;
            clock_ = {};
            input_ = {};
            mapped_time_ = 0;
            origin_ = now;
            if (!displayed_.begin(offer->world))
                return false;
            std::vector<std::uint8_t> bytes;
            if (!resources_.read(offer->path, bytes, error) || !read_world(bytes, geometry_, error) ||
                !receiver_.begin(offer->world))
                return false;
            offer_ = *offer;
            baseline_.emplace(offer_.world);
            if (!send(Net::Ready{offer_.world, profile_}, error))
                return false;
            phase_ = Phase::Baseline;
            deadline_ = now + 30s;
            return true;
        }
        if (const auto* joined = std::get_if<Net::Joined>(&value))
        {
            if (phase_ != Phase::Baseline || joined_.player || joined->world != offer_.world ||
                !movement_.begin(*joined) || !inbox_.begin(*joined, offer_.mode) || !camera_.begin(*joined) ||
                !weapons_.begin(*joined) || !effects_.begin(*joined) || !audio_.begin(*joined) ||
                !speech_.begin(*joined))
                return false;
            joined_ = *joined;
            return complete_baseline(now, error);
        }
        if (const auto* heartbeat = std::get_if<Net::Heartbeat>(&value))
            return phase_ == Phase::Active && heartbeat->world == joined_.world && send(*heartbeat, error);
        return false;
    }
    bool NetworkClient::update(Clock::time_point now, const MovementInput& input, bool commands, std::string& error,
                               WeaponInput weapon)
    {
        error.clear();
        if (phase_ == Phase::Closed)
            return true;
        if (phase_ == Phase::Idle || phase_ == Phase::Prepared || phase_ == Phase::Failed || now < last_)
        {
            error = "Client update requires a connected endpoint and monotonic time";
            return false;
        }
        const double elapsed = std::chrono::duration<double>(now - last_).count();
        last_ = now;
        if (now >= deadline_)
        {
            if (phase_ == Phase::Closing)
            {
                transport_.close();
                speech_.clear();
                audio_.clear();
                effects_.clear();
                phase_ = Phase::Closed;
                reason_ = "Disconnected";
                return true;
            }
            error = "Server connection timed out";
            return fail(error, "Server timeout");
        }
        for (unsigned work = 0; work < 64; ++work)
        {
            Net::Event event;
            const auto result = transport_.poll(event, 0, error);
            if (result == Net::PollResult::Failed)
                return fail(error, "Client transport failed");
            if (result == Net::PollResult::Idle)
                break;
            if (event.peer != peer_)
                continue;
            if (!receive(event, now, error))
                return fail(error, "Invalid server message/channel/order");
            if (phase_ == Phase::Closed)
                return true;
            if (phase_ == Phase::Active)
                deadline_ = now + 15s;
        }
        if (phase_ == Phase::Active && setup_)
        {
            const auto result = movement_.receive(*setup_, error);
            if (result == NetworkMovement::Reception::Refused)
                return fail(error, "Invalid movement setup");
            if (result != NetworkMovement::Reception::Deferred)
                setup_.reset();
        }
        while (phase_ == Phase::Active && !impulses_.empty())
        {
            const auto result = movement_.receive(impulses_.front(), error);
            if (result == NetworkMovement::Reception::Refused)
                return fail(error, "Invalid movement impulse");
            if (result == NetworkMovement::Reception::Deferred)
                break;
            impulses_.pop_front();
        }
        commands = commands && !changing_level_;
        if (phase_ == Phase::Active && movement_.ready())
        {
            unsigned count = 0;
            MovementInput sampled;
            if (!input_.advance(elapsed, input, commands, count, sampled))
                return fail(error, "Invalid client tick/input sample");
            discarded_ticks = input_.discarded_ticks();
            if (!commands)
                weapon.primary = weapon.alternate = false;
            for (std::uint64_t i = 0; i < count; ++i)
            {
                std::optional<Net::MovementReport> report;
                if (!movement_.update(sampled, yaw_, pitch_, NetworkInput::step, commands,
                                      weapon.primary || weapon.alternate, bool(camera_.camera().view.object), report,
                                      error))
                    return fail(error, "Client prediction failed");
                // LevelChanging already cancels both server and client weapons.
                // Do not fill reliable input history while the server synchronously
                // loads the next map; the fresh world begins a new weapon sequence.
                if (!changing_level_ && weapons_.ready(movement_.setup().owner.force_revision))
                {
                    const auto acknowledged = weapon_ack_ ? weapon_ack_->sequence : 0;
                    if (weapon_frames == UINT64_MAX)
                    {
                        return fail(error, "Weapon frame sequence exhausted");
                    }
                    // A slow server/ACK must not turn bounded input into a
                    // disconnect. Pause only the weapon timeline while full;
                    // never bank skipped inputs or replay their shots later.
                    // Movement, camera, audio and normal timeouts keep running.
                    if (weapon_frames - acknowledged >= Net::weapon_frame_queue_limit)
                        ++weapon_wait_ticks;
                    else
                    {
                        const Net::WeaponFrame frame{
                            movement_.setup().owner, weapon_frames + 1, weapons_.state_sequence(),
                            {pitch_, yaw_},          weapon.primary,    weapon.alternate};
                        std::vector<std::uint8_t> bytes;
                        if (!Net::encode_weapon_frame(frame, bytes) ||
                            !transport_.send(peer_, Net::Channel::Control, bytes, error))
                            return fail(error, "Cannot send weapon frame");
                        if (!weapons_.update(frame.owner.force_revision, NetworkInput::step, error, weapon, frame.aim))
                            return fail(error, "Client weapon presentation failed");
                        ++weapon_frames;
                    }
                }
                if (!effects_.advance(NetworkInput::step, displayed_, receiver_.collision(), models_, sprites_,
                                      resources_, audio_, error))
                    return fail(error, "Client effects publication failed");
                if (!camera_.camera().view.object)
                {
                    const float speed = 8 * float(NetworkInput::step);
                    if (zoom_ && fov_x_ > original_fov_zoomed)
                        fov_x_ = std::max(fov_x_ - speed, original_fov_zoomed);
                    else if (!zoom_ && fov_x_ < original_fov_normal)
                        fov_x_ = std::min(fov_x_ + speed, original_fov_normal);
                    else if (!zoom_ && fov_x_ > original_fov_normal)
                        fov_x_ = std::max(fov_x_ - speed, original_fov_normal);
                }
                if (report)
                {
                    std::vector<std::uint8_t> bytes;
                    if (!Net::encode_movement(*report, bytes) ||
                        !transport_.send(peer_, Net::Channel::State, bytes, error))
                        return fail(error, "Cannot send movement report");
                    ++reports;
                }
            }
            if (!clock_.sample(std::chrono::duration<double>(now - origin_).count(), mapped_time_) ||
                !receiver_.sample(std::max(0., mapped_time_ - NetworkClock::interpolation_delay), joined_.object,
                                  displayed_, error))
                return fail(error, "Invalid client presentation clock");
            if (camera_.update(displayed_, receiver_.collision(), movement_.movement(),
                               movement_.setup().owner.force_revision, pitch_, yaw_, zoom_, mapped_time_,
                               error) == NetworkCamera::Reception::Refused)
                return fail(error, "Client camera failed");
            const auto& view = camera_.camera();
            const auto rotation = view.view.object && view.view.settings.listener
                                      ? view.view.rotation
                                      : rotation_from_euler({pitch_, yaw_, 0});
            if (!audio_.advance(elapsed, displayed_, view.listener_position, rotation))
                return fail(error, "Client audio playback failed");
        }
        else
            input_ = {};
        if (phase_ == Phase::Active && !speech_.update(std::chrono::duration<double>(now - origin_).count(), error))
            return fail(error, "Client speech clock failed");
        transport_.flush();
        return true;
    }
    float NetworkClient::fov_x() const
    {
        if (camera_.camera().view.object && camera_.camera().kind == ClientCameraKind::Orb)
            return original_fov_normal * 1.5f;
        if (camera_.camera().view.object && camera_.camera().view.settings.type == 1)
            return original_fov_normal;
        return fov_x_;
    }
    bool NetworkClient::command(const Net::CommandAction& action, std::string& error)
    {
        error.clear();
        if (!ready() || changing_level_ || pending_commands_.size() >= Net::command_queue_limit ||
            command_sequence_ == UINT64_MAX)
        {
            error = "Player command unavailable or pending queue full";
            return false;
        }
        const Net::PlayerCommand value{movement_.setup().owner, command_sequence_ + 1, action};
        std::vector<std::uint8_t> bytes;
        if (!Net::encode_command(value, bytes))
        {
            error = "Invalid player command";
            return false;
        }
        if (!transport_.send(peer_, Net::Channel::Control, bytes, error))
            return false;
        pending_commands_.push_back(value);
        ++command_sequence_;
        return true;
    }
    bool NetworkClient::look(float pitch_delta, float yaw_delta)
    {
        if (!std::isfinite(pitch_delta) || !std::isfinite(yaw_delta) || std::abs(pitch_delta) > 100 ||
            std::abs(yaw_delta) > 100)
            return false;
        if (ready() && !changing_level_ && camera_.camera().look_allowed())
        {
            pitch_ = std::clamp(pitch_ + pitch_delta, -1.48f, 1.48f);
            yaw_ = std::remainder(yaw_ + yaw_delta, 6.283185307179586f);
        }
        return true;
    }
    bool NetworkClient::disconnect(Clock::time_point now)
    {
        if (phase_ == Phase::Closed || phase_ == Phase::Failed)
            return true;
        if (phase_ == Phase::Idle || phase_ == Phase::Prepared || now < last_ || !transport_.disconnect(peer_, 0))
            return false;
        phase_ = Phase::Closing;
        deadline_ = now + 2s;
        transport_.flush();
        return true;
    }
} // namespace OpenBlood2
