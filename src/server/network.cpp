#include "server/network.h"
#include "game/game_settings.h"
#include "net/world_state.h"
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
        const auto& peer = peers_[index];
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
    bool ServerNetwork::send_movement(Net::PeerId peer, const Net::MovementMessage& message, Net::Channel channel,
                                      std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!Net::encode_movement(message, bytes))
        {
            error = "Invalid server movement publication";
            return false;
        }
        return transport_.send(peer, channel, bytes, error);
    }
    bool ServerNetwork::receive(std::size_t index, const Net::Event& event, Clock::time_point now, std::string& error)
    {
        const auto reject = [&](Net::Reject reason) { return remove(index, std::uint32_t(reason), true, error); };
        if (Net::is_weapon_frame_packet(event.bytes))
        {
            auto& peer = peers_[index];
            if (peer.phase != Phase::Joined || event.channel != Net::Channel::Control)
                return reject(Net::Reject::Order);
            Net::WeaponFrameMessage message;
            const auto decoded = Net::decode_weapon_frame(event.bytes, message);
            if (decoded != Net::DecodeWeaponFrame::Ok)
                return reject(decoded == Net::DecodeWeaponFrame::Protocol ? Net::Reject::Protocol
                                                                          : Net::Reject::Malformed);
            const auto* frame = std::get_if<Net::WeaponFrame>(&message);
            if (!frame)
                return reject(Net::Reject::Order);
            const auto result = session_.queue_weapon_frame(peer.player, *frame);
            if (result == ServerSession::CommandResult::Invalid)
                return reject(Net::Reject::Player);
            if (result == ServerSession::CommandResult::Queued)
                peer.deadline = now + 15s;
            return true;
        }
        if (Net::is_command_packet(event.bytes))
        {
            auto& peer = peers_[index];
            if (peer.phase != Phase::Joined || event.channel != Net::Channel::Control)
                return reject(Net::Reject::Order);
            Net::CommandMessage message;
            const auto decoded = Net::decode_command(event.bytes, message);
            if (decoded != Net::DecodeCommand::Ok)
                return reject(decoded == Net::DecodeCommand::Protocol ? Net::Reject::Protocol : Net::Reject::Malformed);
            const auto* command = std::get_if<Net::PlayerCommand>(&message);
            if (!command)
                return reject(Net::Reject::Order);
            const auto result = session_.queue_command(peer.player, *command);
            if (result == ServerSession::CommandResult::Invalid)
                return reject(Net::Reject::Player);
            if (result == ServerSession::CommandResult::Queued)
                peer.deadline = now + 15s;
            return true;
        }
        if (Net::is_movement_packet(event.bytes))
        {
            auto& peer = peers_[index];
            if (peer.phase != Phase::Joined || event.channel != Net::Channel::State)
                return reject(Net::Reject::Order);
            Net::MovementMessage value;
            const auto decoded = Net::decode_movement(event.bytes, value);
            if (decoded != Net::DecodeMovement::Ok)
                return reject(decoded == Net::DecodeMovement::Protocol ? Net::Reject::Protocol
                                                                       : Net::Reject::Malformed);
            const auto* report = std::get_if<Net::MovementReport>(&value);
            if (!report)
                return reject(Net::Reject::Order);
            const auto result = session_.queue_movement(peer.player, *report);
            if (result == ServerSession::MoveResult::Invalid)
                return reject(Net::Reject::Player);
            if (result == ServerSession::MoveResult::Queued)
                peer.deadline = now + 15s;
            return true;
        }
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
            Net::WorldSnapshot snapshot;
            std::vector<std::vector<std::uint8_t>> chunks;
            if (!session_.snapshot(peer.player, snapshot, error) || !Net::encode_baseline_chunks(snapshot, chunks))
            {
                if (error.empty())
                    error = "Initial world snapshot cannot be encoded";
                return reject(Net::Reject::World);
            }
            for (const auto& bytes : chunks)
                if (!transport_.send(peer.peer, Net::Channel::Baseline, bytes, error))
                    return reject(Net::Reject::Capacity);
            peer.phase = Phase::Baseline;
            peer.previous = std::move(snapshot);
            peer.deadline = now + 30s;
            return true;
        }
        if (peer.phase == Phase::Baseline)
        {
            const auto* ready = std::get_if<Net::BaselineReady>(&message);
            if (!ready)
                return reject(Net::Reject::Order);
            if (ready->world != session_.world_id() || ready->sequence != peer.previous.sequence)
                return reject(Net::Reject::World);
            if (!session_.begin_audio(peer.player, error))
                return reject(Net::Reject::Capacity);
            peer.phase = Phase::Joined;
            peer.deadline = now + 15s;
            peer.heartbeat = now + 1s;
            peer.publish_at = session_.ticks() + 3;
            if (!send(peer.peer, Net::Heartbeat{session_.world_id()}, error))
                return reject(Net::Reject::Capacity);
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
                    peers_.push_back({event.peer, Phase::Hello, 0, now + 5s, {}, {}, 0});
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
            if (peer.phase == Phase::Joined)
            {
                Net::MovementSetup setup;
                if (!session_.movement_setup(peer.player, setup, error) ||
                    (setup.sequence != peer.movement_setup &&
                     !send_movement(peer.peer, setup, Net::Channel::Control, error)))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Player), true, error))
                        return false;
                    continue;
                }
                peer.movement_setup = setup.sequence;
                if (peer.movement_force != setup.owner.force_revision)
                {
                    peer.movement_force = setup.owner.force_revision;
                    peer.movement_impulse = 0;
                }
                std::vector<Net::MovementImpulse> impulses;
                bool impulses_sent = session_.movement_impulses(peer.player, peer.movement_impulse, impulses, error);
                for (const auto& impulse : impulses)
                {
                    if (!impulses_sent)
                        break;
                    impulses_sent = send_movement(peer.peer, impulse, Net::Channel::Control, error);
                    if (impulses_sent)
                        peer.movement_impulse = impulse.sequence;
                }
                if (!impulses_sent)
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Capacity), true, error))
                        return false;
                    continue;
                }
                std::vector<Net::PrivateEvent> events;
                Net::WeaponLoadout weapons;
                bool sent = session_.weapon_loadout(peer.player, weapons, error) &&
                            session_.take_private_events(peer.player, events, error);
                for (const auto& event : events)
                {
                    if (!sent)
                        break;
                    std::vector<std::uint8_t> bytes;
                    sent = Net::encode_private(event, bytes) &&
                           transport_.send(peer.peer, Net::Channel::Control, bytes, error);
                }
                std::vector<Net::CommandAck> acknowledgements;
                sent = sent && session_.take_command_acks(peer.player, acknowledgements);
                for (const auto& ack : acknowledgements)
                {
                    if (!sent)
                        break;
                    std::vector<std::uint8_t> bytes;
                    sent = Net::encode_command(ack, bytes) &&
                           transport_.send(peer.peer, Net::Channel::Control, bytes, error);
                }
                std::vector<Net::AudioEvent> audio;
                sent = sent && session_.take_audio_events(peer.player, audio, error);
                for (const auto& event : audio)
                {
                    if (!sent)
                        break;
                    std::vector<std::uint8_t> bytes;
                    sent = Net::encode_audio(event, bytes) &&
                           transport_.send(peer.peer, Net::Channel::Control, bytes, error);
                }
                if (!sent)
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Capacity), true, error))
                        return false;
                    continue;
                }
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
            if (peer.phase == Phase::Joined && session_.ticks() >= peer.publish_at)
            {
                Net::WorldSnapshot snapshot;
                Net::WorldPatch patch;
                bool changed = false;
                if (!session_.snapshot(peer.player, snapshot, error) ||
                    !Net::make_world_patch(peer.previous, snapshot, patch, changed))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::World), true, error))
                        return false;
                    continue;
                }
                if (changed)
                {
                    std::vector<std::vector<std::uint8_t>> chunks;
                    bool sent = Net::encode_patch_chunks(patch, chunks);
                    for (const auto& bytes : chunks)
                        if (sent)
                            sent = transport_.send(peer.peer, Net::Channel::Baseline, bytes, error);
                    if (!sent)
                    {
                        if (!remove(i, std::uint32_t(Net::Reject::Capacity), true, error))
                            return false;
                        continue;
                    }
                    ++membership_updates;
                }
                std::vector<std::vector<std::uint8_t>> states;
                if (!Net::encode_state_packets(snapshot, changed ? snapshot.sequence : peer.previous.sequence, states))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::World), true, error))
                        return false;
                    continue;
                }
                if (changed)
                    peer.previous = std::move(snapshot);
                std::optional<Net::MovementAck> ack;
                if (!session_.movement_ack(peer.player, ack))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Player), true, error))
                        return false;
                    continue;
                }
                if (ack)
                {
                    std::vector<std::uint8_t> packet;
                    if (!Net::encode_movement(*ack, packet))
                    {
                        if (!remove(i, std::uint32_t(Net::Reject::Player), true, error))
                            return false;
                        continue;
                    }
                    states.push_back(std::move(packet));
                }
                std::optional<Net::WeaponFrameAck> fire_ack;
                if (!session_.weapon_frame_ack(peer.player, fire_ack))
                {
                    if (!remove(i, std::uint32_t(Net::Reject::Player), true, error))
                        return false;
                    continue;
                }
                if (fire_ack && fire_ack->sequence > peer.weapon_ack)
                {
                    std::vector<std::uint8_t> packet;
                    // This ACK releases the client's bounded input window.
                    // It must not be starved by best-effort snapshot throttling.
                    if (!Net::encode_weapon_frame(*fire_ack, packet) ||
                        !transport_.send(peer.peer, Net::Channel::Control, packet, error))
                    {
                        if (!remove(i, std::uint32_t(Net::Reject::Player), true, error))
                            return false;
                        continue;
                    }
                    peer.weapon_ack = fire_ack->sequence;
                }
                Net::PeerStats stats;
                std::size_t bytes = 0;
                for (const auto& packet : states)
                    bytes += packet.size();
                if (!transport_.stats(peer.peer, stats) || stats.queued_bytes > Net::Transport::queue_limit ||
                    bytes > Net::Transport::queue_limit - stats.queued_bytes)
                    ++state_drops;
                else
                {
                    bool sent = true;
                    for (const auto& packet : states)
                        if (sent)
                            sent = transport_.send(peer.peer, Net::Channel::State, packet, error);
                    if (!sent)
                    {
                        if (!remove(i, std::uint32_t(Net::Reject::Capacity), true, error))
                            return false;
                        continue;
                    }
                    ++state_updates;
                    state_bytes += bytes;
                }
                peer.publish_at = session_.ticks() + 3;
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
