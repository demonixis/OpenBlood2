# Session tracing and zero-depth audio work

This optimization changes replacement-engine bookkeeping and synthesis work only.
Original AI rules, scheduling, collision queries, callbacks, musical events,
voice limits and test deadlines are unchanged. See [evidence](../../../Documentation/Implementation/m2_session_cost.md),
[service coverage](service_coverage_contracts.md), [sample voices](music_sampler_contracts.md)
and the [source ledger](../implementation/sources/m2_session_cost_sources.json).

Service counters still own their string keys, retain lexical order, count failed
attempts and preserve unknown names. A transparent ordered-map lookup borrows a
string view only for the duration of the lookup. Existing names increment their
counters without constructing temporary strings; first insertion copies the name.
Disabled tracing performs no lookup/insertion. Clear, nested scopes, world
replacement and owned inspection snapshots keep their previous semantics.
The Services tab uses the same counters; reading it adds no calls.

The DLS voice loop skips sine evaluation when pitch depth is zero and no rendered
sample needs nonzero LFO gain. Zero gain depth is exactly unity gain. With both
pitch modulation depths zero, the sample step is the unchanged product of base
pitch and current bend. Absolute voice age supplies LFO phase, so skipped
arithmetic retains no hidden oscillator state. Current controller changes still
apply on their existing sample boundaries.

Every frame still checks completion, performs the original cursor addition and
loop wrap, and increments the voice clock. No bulk cursor jump, time approximation,
voice culling, changed sample rate or skipped music update is introduced. Nonzero
modulation, amplitude envelopes, interpolation, choke/release, note ordering and
world ownership retain their existing computations. Headless and truncated PCM
prefixes advance the same voice state as rendered samples.

The frozen scalar reference in `tests/dls_scalar_reference.h` is copied from the
pre-optimization project implementation at commit `309ae21`; it is not a claim of
retail-synth equivalence. Tests compare exact PCM, cursor, frame/loop counts and
completion for all eight pitch-envelope/LFO-pitch/LFO-gain combinations, one-shots
and loops, delayed starts, control changes, muted prefixes and choke/release.
Private tests make the same comparisons for all 38 Cabal regions, including live
controls. Existing timing/playlist/lifetime tests remain required.

Performance observations are local measurements, not guaranteed frame rates.
The 90-second AI-clearance sanitizer deadline and both complete encounter lifetimes
remain unchanged. M2 still requires the consolidated acceptance review.
