#include "server/network.h"
#include "game/game_settings.h"
#include <algorithm>

namespace OpenBlood2
{
    using namespace std::chrono_literals;
    ServerNetwork::~ServerNetwork()
    {
        close();
    }
    bool ServerNetwork::listen(std::string_view address, std::uint16_t port, unsigned capacity, std::string& error)
    {
        if (!session_.world_id() || !capacity || capacity > Net::max_players || !peers_.empty())
        {
            error = "Network listener requires a loaded world and valid capacity";
            return false;
        }
        if (!transport_.listen(address, port, capacity, error))
            return false;
        capacity_ = capacity;
        clock_started_ = false;
        return true;
    }
    bool ServerNetwork::send(Net::PeerId peer, const Net::AdmissionMessage& message, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!Net::encode_admission(message, bytes))
        {
            error = "Invalid server admission reply";
            return false;
        }
        return transport_.send(peer, Net::Channel::Control, bytes, error);
    }
    bool ServerNetwork::remove(std::size_t index, std::uint32_t reason, bool disconnect, std::string& error)
    {
        const auto peer = peers_[index];
        if (peer.player && !session_.leave(peer.player))
        {
            error = "Cannot detach departed original player";
            return false;
        }
        if (peer.player)
            ++departed;
        if (disconnect)
        {
            transport_.disconnect(peer.peer, reason);
            if (reason != std::uint32_t(Net::Reject::Shutdown))
            {
                last_rejection = Net::reject_text(reason);
                if (!error.empty())
                    last_rejection += ": " + error;
                ++rejected;
            }
            timed_out += reason == std::uint32_t(Net::Reject::Timeout);
        }
        peers_.erase(peers_.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
    bool ServerNetwork::receive(std::size_t index, const Net::Event& event, Clock::time_point now, std::string& error)
    {
        const auto reject = [&](Net::Reject reason) { return remove(index, std::uint32_t(reason), true, error); };
        if (event.channel != Net::Channel::Control)
            return reject(Net::Reject::Order);
        Net::AdmissionMessage message;
        const auto decoded = Net::decode_admission(event.bytes, message);
        if (decoded != Net::DecodeAdmission::Ok)
            return reject(decoded == Net::DecodeAdmission::Protocol ? Net::Reject::Protocol : Net::Reject::Malformed);
        auto& peer = peers_[index];
        if (peer.phase == Phase::Hello)
        {
            const auto* hello = std::get_if<Net::Hello>(&message);
            if (!hello)
                return reject(Net::Reject::Order);
            if (hello->edition != Net::Edition::Retail)
                return reject(Net::Reject::Edition);
            if (hello->content != session_.fingerprint())
                return reject(Net::Reject::Content);
            if (session_.mode() != GAMETYPE_BLOODBATH)
                return reject(Net::Reject::Mode);
            Net::WorldOffer offer{session_.world_id(), std::uint8_t(session_.mode()), std::uint8_t(capacity_),
                                  session_.game().world_path};
            if (!send(peer.peer, offer, error))
                return reject(Net::Reject::Capacity);
            peer.phase = Phase::Loading;
            peer.deadline = now + 30s;
            return true;
        }
        if (peer.phase == Phase::Loading)
        {
            const auto* ready = std::get_if<Net::Ready>(&message);
            if (!ready)
                return reject(Net::Reject::Order);
            if (ready->world != session_.world_id())
                return reject(Net::Reject::World);
            if (session_.player_count() >= capacity_)
                return reject(Net::Reject::Capacity);
            Net::Joined joined;
            if (!session_.join(ready->profile, joined, error))
                return reject(Net::Reject::Player);
            peer.player = joined.player;
            ++admitted;
            if (!send(peer.peer, joined, error))
                return reject(Net::Reject::Capacity);
            peer.phase = Phase::Joined;
            peer.deadline = now + 15s;
            peer.heartbeat = now + 1s;
            return true;
        }
        const auto* heartbeat = std::get_if<Net::Heartbeat>(&message);
        if (!heartbeat)
            return reject(Net::Reject::Order);
        if (heartbeat->world != session_.world_id())
            return reject(Net::Reject::World);
        peer.deadline = now + 15s;
        return true;
    }
    bool ServerNetwork::pump(Clock::time_point now, std::string& error)
    {
        error.clear();
        if (!transport_.is_open() || (clock_started_ && now < last_time_))
        {
            error = "Network pump requires an open listener and monotonic time";
            return false;
        }
        last_time_ = now;
        clock_started_ = true;
        // Simulation work is owned by the caller. A flood cannot turn one pump
        // into an unbounded receive loop or grow this registry beyond capacity.
        for (unsigned work = 0; work < 64; ++work)
        {
            Net::Event event;
            const auto result = transport_.poll(event, 0, error);
            if (result == Net::PollResult::Failed)
                return false;
            if (result == Net::PollResult::Idle)
                break;
            const auto found =
                std::find_if(peers_.begin(), peers_.end(), [&](const auto& p) { return p.peer == event.peer; });
            if (event.kind == Net::EventKind::Connected)
            {
                if (found != peers_.end() || peers_.size() >= capacity_)
                    transport_.disconnect(event.peer, std::uint32_t(Net::Reject::Capacity));
                else
                    peers_.push_back({event.peer, Phase::Hello, 0, now + 5s, {}});
            }
            else if (found != peers_.end())
            {
                const auto index = std::size_t(found - peers_.begin());
                if (event.kind == Net::EventKind::Disconnected)
                {
                    if (!remove(index, event.reason, false, error))
                        return false;
                }
                else if (now >= found->deadline)
                {
                    if (!remove(index, std::uint32_t(Net::Reject::Timeout), true, error))
                        return false;
                }
                else if (!receive(index, event, now, error))
                    return false;
            }
        }
        for (std::size_t i = 0; i < peers_.size();)
        {
            auto& peer = peers_[i];
            if (now >= peer.deadline)
            {
                if (!remove(i, std::uint32_t(Net::Reject::Timeout), true, error))
                    return false;
                continue;
            }
            if (peer.phase == Phase::Joined && now >= peer.heartbeat)
            {
                if (!send(peer.peer, Net::Heartbeat{session_.world_id()}, error))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Capacity), true, error))
                        return false;
                    continue;
                }
                peer.heartbeat = now + 1s;
            }
            ++i;
        }
        transport_.flush();
        error.clear(); // Rejected peers have local diagnostics; the listener remains usable.
        return true;
    }
    bool ServerNetwork::close()
    {
        bool ok = true;
        std::string error;
        while (!peers_.empty())
            if (!remove(peers_.size() - 1, std::uint32_t(Net::Reject::Shutdown), true, error))
            {
                ok = false;
                break;
            }
        transport_.flush();
        transport_.close();
        return ok;
    }
} // namespace OpenBlood2
