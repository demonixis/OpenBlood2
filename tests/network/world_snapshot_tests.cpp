#include "net/world_snapshot.h"
#include <iostream>
#include <limits>
#include <random>

using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    WorldSnapshot sample(unsigned count = 3)
    {
        WorldSnapshot s;
        s.world = 9;
        s.sequence = 7;
        s.time = .125;
        for (unsigned i = 0; i < count; ++i)
        {
            ObjectState o;
            o.id = {i, 3};
            o.pose.position = {float(i), 2, -3};
            o.animation.time = 3.75;
            o.animation.revision = 27;
            o.animation.looping = false;
            o.sprite_track.milliseconds = 7.25;
            o.sprite_track.has_frame = false;
            s.objects.push_back(o);
        }
        return s;
    }
    bool run()
    {
        auto source = sample();
        std::vector<std::uint8_t> bytes;
        CHECK(encode_world_snapshot(source, bytes));
        CHECK(bytes[0] == 0x42 && bytes[1] == 0x32 && bytes[2] == 0x4d && bytes[3] == 0x57);
        CHECK(bytes[28] == 0xc0 && bytes[29] == 0x3f); // IEEE double .125, little endian.
        WorldSnapshot out;
        CHECK(decode_world_snapshot(bytes, out));
        std::vector<std::uint8_t> again;
        CHECK(encode_world_snapshot(out, again) && again == bytes);
        CHECK(out.objects[1].animation.time == 3.75 && out.objects[1].animation.revision == 27);
        const auto frame = snapshot_frame_values(out);
        CHECK(!frame.objects[0].model && !frame.objects[0].sprite);
        CHECK(encode_world_snapshot(snapshot_values(frame), again) && again == bytes);
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            out.sequence = 99;
            CHECK(!decode_world_snapshot(std::span(bytes).first(i), out) && out.sequence == 99);
        }
        auto invalid = [&](WorldSnapshot s)
        {
            std::vector<std::uint8_t> previous{9, 8, 7};
            return !encode_world_snapshot(s, previous) && previous == std::vector<std::uint8_t>({9, 8, 7});
        };
        auto bad = source;
        bad.objects[0].id.generation = 0;
        CHECK(invalid(bad));
        bad = source;
        bad.objects[1].id = bad.objects[0].id;
        CHECK(invalid(bad));
        bad = source;
        bad.objects[0].pose.rotation.m_Spin = 0;
        CHECK(invalid(bad));
        bad = source;
        bad.time = std::numeric_limits<double>::quiet_NaN();
        CHECK(invalid(bad));
        bad = source;
        bad.objects[0].skin = "../outside.dtx";
        CHECK(invalid(bad));
        bad = source;
        bad.objects[0].hidden_nodes = {2};
        CHECK(invalid(bad));
        bad = source;
        bad.objects[0].has_model = true;
        CHECK(invalid(bad));
        bad = source;
        bad.objects[0].attachment_resolved = true;
        CHECK(invalid(bad));
        // A parent pending removal has no published identity, but its child's
        // last resolved pose can remain until deferred cleanup. Presentation hides it.
        bad.objects[0].attached = true;
        CHECK(encode_world_snapshot(bad, again));
        bad = source;
        bad.polygrids = {{1, {}}, {1, {}}};
        CHECK(invalid(bad));
        bad = sample(9000);
        CHECK(invalid(bad)); // Byte budget, even though identities fit.

        std::vector<std::vector<std::uint8_t>> chunks;
        auto large = sample(2000);
        CHECK(encode_baseline_chunks(large, chunks) && chunks.size() > 1);
        std::uint64_t envelope_world = 99;
        CHECK(world_chunk_epoch(chunks[0], envelope_world) && envelope_world == source.world);
        for (std::size_t length = 0; length < chunks[0].size(); ++length)
        {
            envelope_world = 99;
            CHECK(!world_chunk_epoch(std::span(chunks[0]).first(length), envelope_world) && envelope_world == 99);
        }
        std::mt19937 envelope_random(0xB216);
        for (unsigned mutation = 0; mutation < 10000; ++mutation)
        {
            auto changed = chunks[0];
            changed[envelope_random() % changed.size()] ^= std::uint8_t(1u << (envelope_random() % 8));
            envelope_world = 99;
            const auto valid = world_chunk_epoch(changed, envelope_world);
            CHECK(valid ? envelope_world != 0 : envelope_world == 99);
        }
        BaselineReceiver receiver(source.world), wrong_world(source.world + 1);
        out.sequence = 99;
        CHECK(wrong_world.receive(chunks[0], out) == BaselineResult::Refused && out.sequence == 99);
        CHECK(receiver.receive(chunks[1], out) == BaselineResult::Refused && !receiver.pending_bytes());
        for (std::size_t i = 0; i < chunks.size(); ++i)
        {
            const auto pending = receiver.pending_bytes();
            CHECK(receiver.receive(std::span(chunks[i]).first(chunks[i].size() - 1), out) == BaselineResult::Refused);
            CHECK(receiver.pending_bytes() == pending);
            const auto expected = i + 1 == chunks.size() ? BaselineResult::Complete : BaselineResult::Pending;
            CHECK(receiver.receive(chunks[i], out) == expected);
            if (expected == BaselineResult::Pending)
            {
                CHECK(out.sequence == 99);
                const auto accepted = receiver.pending_bytes();
                CHECK(receiver.receive(chunks[i], out) == BaselineResult::Refused);
                CHECK(receiver.pending_bytes() == accepted);
            }
        }
        CHECK(out.sequence == 7 && out.objects.size() == 2000 && receiver.pending_bytes() == 0);
        CHECK(receiver.receive(chunks[0], out) == BaselineResult::Refused);
        large.sequence++;
        CHECK(encode_baseline_chunks(large, chunks));
        for (const auto& chunk : chunks)
            CHECK(receiver.receive(chunk, out) != BaselineResult::Refused);
        CHECK(out.sequence == 8);
        std::mt19937 rng(0xB209B);
        for (unsigned i = 0; i < 25000; ++i)
        {
            auto fuzz = bytes;
            if (i & 1)
                fuzz[rng() % fuzz.size()] ^= std::uint8_t(rng());
            else
            {
                fuzz.resize(rng() % 2048);
                for (auto& b : fuzz)
                    b = std::uint8_t(rng());
            }
            if (decode_world_snapshot(fuzz, out))
                CHECK(encode_world_snapshot(out, again) && again == fuzz);
        }
        std::cout << "Snapshot values, every truncation, atomic bounds, ordered chunks and 25000 mutations\n";
        auto after = source;
        after.sequence++;
        after.objects.erase(after.objects.begin());
        after.objects[0].flags ^= FLAG_VISIBLE;
        WorldPatch patch;
        bool changed = false;
        CHECK(make_world_patch(source, after, patch, changed) && changed);
        CHECK(encode_world_patch(patch, bytes));
        WorldPatch decoded;
        CHECK(decode_world_patch(bytes, decoded));
        CHECK(encode_world_patch(decoded, again) && again == bytes);
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            decoded.base = 99;
            CHECK(!decode_world_patch(std::span(bytes).first(size), decoded) && decoded.base == 99);
        }
        CHECK(encode_patch_chunks(patch, chunks));
        BaselineReceiver patch_receiver(source.world);
        WorldUpdate update;
        for (const auto& chunk : chunks)
            CHECK(patch_receiver.receive(chunk, update) != BaselineResult::Refused);
        CHECK(std::holds_alternative<WorldPatch>(update) && std::get<WorldPatch>(update).base == source.sequence);
        for (unsigned i = 0; i < 10000; ++i)
        {
            auto fuzz = bytes;
            fuzz[rng() % fuzz.size()] ^= std::uint8_t(rng());
            if (decode_world_patch(fuzz, decoded))
                CHECK(encode_world_patch(decoded, again) && again == fuzz);
        }
        std::cout << "Reliable membership diff, patch round trip/truncations and 10000 mutations\n";
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
