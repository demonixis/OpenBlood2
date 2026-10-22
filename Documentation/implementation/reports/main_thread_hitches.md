# Main-thread hitch reduction

Snapshot of 2026-10-22. Two periodic main-thread costs were measured on an Apple
M5 after the kill-slowdown fixes; they weigh more on Quest 3 at 72–90 Hz:

- the original player's `PingObjects` every 0.5 s (`CPlayerObj::Update`), which
  casts one segment per auto-deactivating object;
- streamed Sound/SoundFX windows read from the REZ archive inside the mixer on
  the main thread.

Both changes are replacement-engine policies with identical results. See the
[activation](../../contracts/activation_visibility_contracts.md#ping-cost)
and [stream](../../contracts/audio_stream_contracts.md) contracts.

Superseded the same day for PingObjects: activation now follows the retail visibility
set (objects never block), tested with up to four primary-world sweeps per candidate and
no object scan, so the prepared IntersectSegment batch described below was removed. The
stream measurements stand.

## Measurement

`blood2 --bench` now reports `activation pings (PingObjects)` and
`streamed PCM reads` rows (ms per frame) and a `Hitches` line (calls, mean cost
per ping, reads, frames with reads, most reads in one frame). `--bench-audio`
renders the game mix offline (`AudioMixer::inspect_pcm`); without it the bench has
no device and reads no streamed samples. Runs use 640×360, 30 warm-up and 1800
measured frames at 1/60 s, a scratch `HOME`/`CFFIXED_USER_HOME` and the retail
data. Other sessions were building on the same machine, so absolute times drift
between runs; the table pairs interleaved runs of the baseline (`60be517d` plus
the same probes only) and the change.

| World, run | PingObjects per call (ms) | Worst frame's ping (ms) | Streamed reads (frames with reads) | Most reads in one frame |
| --- | --- | --- | --- | --- |
| SubwayA 1, before / after | 0.640 / 0.375 | 0.977 / 0.499 | none (no streams) | 0 / 0 |
| SubwayA 2, before / after | 0.643 / 0.334 | 0.921 / 0.449 | none | 0 / 0 |
| CityHub 1, before / after | 6.679 / 1.228 | 8.390 / 1.610 | 1976 (984) / 1987 (1306) | 12 / 4 |
| CityHub 2, before / after | 6.543 / 1.213 | 7.145 / 1.587 | 1976 (984) / 1987 (1306) | 12 / 4 |

Each run has 59 pings. On a quieter machine at the start of the work, CityHub
measured 4.03 ms per ping before and 0.63 ms after. Streamed read time per frame
stays small with a warm page cache (worst frame 0.09–2.25 ms before, 0.09–0.27 ms
after; the 2.25 ms frame was most likely a cold read). The samples read are the same;
there are 11 more reads because a phased stream's first window [0, phase) is short.

## Findings

**PingObjects.** Each candidate ray went through the full `IntersectSegment`: a
fresh snapshot copy, the per-record class filter (up to six `dynamic_cast`s) for
every object, a sweep of each solid record and a refresh of every attachment
pose. CityHub pings about 150 candidates against about 490 objects. The filter and
record bounds do not change between the rays of one ping (no callback runs), so
they are prepared once. With the ray rejected against widened bounds and the
attachment refresh stopped at its fixed point, a CityHub ray costs about 3 µs of
primary-world BVH sweep plus under 1 µs of prepared records. The world sweep is
the shared collision core and stays unchanged: a front-to-back traversal was not
attempted, because equal-fraction hits within one polygon resolve by visit order.

**Streams.** CityHub plays 35 streamed voices over 12 PCM streams, all 22050 Hz
mono, all started at world load. With windows aligned to multiples of 4096 frames
all 12 streams refill in the same mixer step every 186 ms (every 11 frames at
60 Hz, about 22 at 120 Hz). On the M5 with a warm page cache a refill costs
5–7 µs, so the burst is about 0.1 ms; cold pages or Quest's `/sdcard` storage
make each refill and each archive `stat` a real I/O, which is where multi-ms
spikes come from. Per-stream window phases spread the refills; checking each
stream's archive once per advance cuts the per-advance `stat` calls from 35 to 12.
After the streams' first loops their boundaries drift apart anyway, so a few
steps still see up to four reads. No worker thread or prefetch is added.

## Equivalence and tests

- `ai_runtime.ping_private` (new, private data): 40 pings each in SubwayA and
  CityHub; every probe (visibility, hit object, polygon, fraction, time,
  observer, committed refresh) equals an unprepared `IntersectSegment` for all
  6600 rays (SubwayA 640, CityHub 5960; 2.6 million prepared records skipped).
- `audio_stream.pcm` reads phased windows against the SDL decoder at the phase
  boundaries; `audio_stream.phases` (new) plays three phased streams and a voice
  sharing the first: output equals buffered playback every step, no two streams
  refill in one step before the first loop (it fails with phase 0), and a
  truncated archive latches the failure on all four voices.
- During development, one binary switched between the old and new paths; per
  frame, an FNV hash of the mix buffer and of every object's position, rotation,
  activity, ping count, deactivation deadline and probe was identical for 1800
  frames in SubwayA and CityHub. That temporary instrumentation is not retained.

Rebased on `main` (`761b135d`), the full headless suite including private-data
cases passes: 1363/1363, Release with debug info, inspector build. On the original
`60be517d` base the same seven stale catalog/AI/lightning cases failed that later
`main` commits fix. GPU cases were not run.

Quest 3 was not measured (device runs need the user's go-ahead); the policies are
platform-independent.
