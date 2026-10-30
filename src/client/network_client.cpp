#include "client/network_client.h"
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
        phase_ = Phase::Failed;
        return false;
    }
    bool NetworkClient::send(const Net::AdmissionMessage& value, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        return Net::encode_admission(value, bytes) && transport_.send(peer_, Net::Channel::Control, bytes, error);
    }
    bool NetworkClient::connect(const Config& config, std::string_view host, std::uint16_t port,
                                const Net::JoinProfile& profile, Clock::time_point now, std::string& error)
    {
        error.clear();
        if (phase_ != Phase::Idle || !Net::valid_profile(profile))
        {
            error = "Client connection requires a fresh endpoint and valid profile";
            return false;
        }
        if (!resources_.mount_game(config, error) || !Net::content_fingerprint(resources_, digest_, error) ||
            !transport_.open_client(error))
            return fail(error, "Client content/transport initialization failed");
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
            phase_ = Phase::Closed;
            transport_.close();
            return true;
        }
        if (phase_ == Phase::Closing)
            return true;
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
            if (phase_ != Phase::Active || event.channel != Net::Channel::Control ||
                Net::decode_private(event.bytes, value) != Net::DecodePrivate::Ok || value.owner != joined_)
                return false;
            const auto received = inbox_.receive(value, error);
            if (received == NetworkPlayerEvents::Reception::Refused)
                return false;
            if (received == NetworkPlayerEvents::Reception::Ignored)
                return true;
            if (camera_.receive(value, error) == NetworkCamera::Reception::Refused)
                return false;
            if (const auto* aim = std::get_if<PlayerRotation>(&value.value))
            {
                pitch_ = aim->pitch;
                yaw_ = aim->yaw;
            }
            if (const auto* zoom = std::get_if<ZoomView>(&value.value))
                zoom_ = zoom->on;
            return observe(double(value.tick) / 60, now);
        }
        if (Net::is_movement_packet(event.bytes))
        {
            Net::MovementMessage value;
            if (phase_ != Phase::Active || Net::decode_movement(event.bytes, value) != Net::DecodeMovement::Ok)
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
            const auto* ack = std::get_if<Net::MovementAck>(&value);
            return ack && event.channel == Net::Channel::State &&
                   movement_.receive(*ack, error) != NetworkMovement::Reception::Refused;
        }
        if (event.channel == Net::Channel::State)
        {
            Net::WorldState value;
            if (phase_ != Phase::Active || !Net::decode_world_state(event.bytes, value))
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
            if (phase_ != Phase::Offer || offer->mode != 1)
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
                !movement_.begin(*joined) || !inbox_.begin(*joined) || !camera_.begin(*joined))
                return false;
            joined_ = *joined;
            return complete_baseline(now, error);
        }
        if (const auto* heartbeat = std::get_if<Net::Heartbeat>(&value))
            return phase_ == Phase::Active && heartbeat->world == joined_.world && send(*heartbeat, error);
        return false;
    }
    bool NetworkClient::update(Clock::time_point now, const MovementInput& input, bool commands, std::string& error)
    {
        error.clear();
        if (phase_ == Phase::Closed)
            return true;
        if (phase_ == Phase::Idle || phase_ == Phase::Failed || now < last_)
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
        if (phase_ == Phase::Active && movement_.ready())
        {
            unsigned count = 0;
            MovementInput sampled;
            if (!input_.advance(elapsed, input, commands, count, sampled))
                return fail(error, "Invalid client tick/input sample");
            discarded_ticks = input_.discarded_ticks();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                std::optional<Net::MovementReport> report;
                if (!movement_.update(sampled, yaw_, pitch_, NetworkInput::step, commands, false,
                                      bool(camera_.camera().view.object), report, error))
                    return fail(error, "Client prediction failed");
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
        }
        else
            input_ = {};
        transport_.flush();
        return true;
    }
    bool NetworkClient::look(float pitch_delta, float yaw_delta)
    {
        if (!std::isfinite(pitch_delta) || !std::isfinite(yaw_delta) || std::abs(pitch_delta) > 100 ||
            std::abs(yaw_delta) > 100)
            return false;
        if (ready() && camera_.camera().look_allowed())
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
        if (phase_ == Phase::Idle || now < last_ || !transport_.disconnect(peer_, 0))
            return false;
        phase_ = Phase::Closing;
        deadline_ = now + 2s;
        transport_.flush();
        return true;
    }
} // namespace OpenBlood2
