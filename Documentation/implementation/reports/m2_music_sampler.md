# M2 — DLS diagnostic voice synthesis

M2 is **incomplete**. The engine now synthesizes bounded notes from the original
external Cabal instrument bank through the existing audio mixer. This provides a
way to verify the sample/envelope prerequisite. Original section/style sequences
and `MUSIC LEVEL 3` remain unsupported.

## Implemented and inspected

The new independent voice engine selects the authored region, retains its immutable
PCM bank, applies tuning, velocity/gain, forward loops, amplitude and pitch envelopes,
LFO and stereo pan, and releases voices on their gate/envelope/sample boundary.
It shares the world sound mixer's 48 kHz clock and SDL output. Closing the inspector
does not stop or advance it differently; unload releases notes and bank ownership.
There is no new dependency or imported reference-engine synthesizer.

The World sounds inspector adds explicit one-second region auditions and note
release, with live cursor/loop/envelope state and truthful refusal/completion counts.
Audition gain is 20%; this does not implement original music-volume controls.
Unsupported articulation refuses the note without altering existing voices.

See [contracts](../../contracts/music_sampler_contracts.md) for exact
bounds and [source references](../sources/m2_music_sampler_sources.json).

## Format decisions and observed data

The [MIDI Association's DLS Level 1 specification](https://midi.org/dls-level-1),
including its 1.1 errata, supplies synthesis units and envelopes. The implementation
uses the corrected signed gain convention, absorbing zero-time value, `/128`
envelope-time scaling and partial-envelope release rates. A later Microsoft
[DMUS_REGION definition](https://learn.microsoft.com/nl-nl/previous-versions/ms808241(v=msdn.10))
documents region articulation with global fallback. Applying that rule to Cabal's
melodic per-region graphs is a stated compatibility adaptation; retail parity is
not inferred from this later document.

At velocity 100 and each region's midpoint key, **33 of Cabal's 38 regions produce
finite nonzero PCM**. The other **five refuse unknown destination 2**. Those records
coexist with standard pan destination 4, but are neither aliased nor discarded.
The earlier decoder's invalid effective-loop refusals in Crudux and Track1 remain.
No commercial samples, soundtrack, extracted file or copyrighted specification
is committed.

## Verification

| Configuration | Result | Wall time |
| --- | --- | --- |
| GCC Debug, inspector enabled | 81/81 passed | 52.86 s |
| GCC Release, inspector disabled | 81/81 passed | 7.93 s |
| Clang ASan/UBSan, leak detection enabled | 81/81 passed | 261.74 s |
| NVIDIA Vulkan, Debug integration | 3/3 passed | 46.05 s |
| NVIDIA Vulkan, final Debug sound/UI recheck | 1/1 passed | 5.21 s |
| NVIDIA Vulkan, final Release integration | 3/3 passed | 11.84 s |

Validation results and commands are recorded in
[the evidence ledger](evidence/m2-music-sampler.json).

Synthetic checks cover PCM interpolation and loop seams, octave/fine tuning,
signed gain and velocity, stereo pan, one-shot completion, attack/decay/sustain,
early release, absorbing zero, the first-sample zero-attack transient, pitch
envelopes/LFO, region/global/default precedence,
block partitioning, additive mixing, explicit unsupported/budget failures, stale
note IDs, retained PCM and world cleanup. Private checks render all supported Cabal
regions, compare rendered/offline cursors, verify the five refusals and finish every
release. Existing world/object/audio/service/train cases guard integration.

The NVIDIA sound probe starts and releases an explicit Cabal note through the real
SDL stream while the inspector is hidden and again after reload. This is labelled
diagnostic input, not an original playlist. GPU runs use dummy audio: submitted PCM
and mixer behavior are checked, not human listening. Native macOS/Windows/mobile
execution and retail musical fidelity remain unverified.

The initial checks exposed test-link/path/oracle setup errors. An intermediate
Release check also paired the new zero-attack assertion with a library built before
that correction. These logs remain in the evidence; final verification uses
consistent rebuilt artifacts. Numerical tolerances and unsupported-service
assertions were not relaxed. Wall times are observed validation times, not audio
performance benchmarks.

## Remaining work

Resolve legacy articulation records with evidence, then implement the actual
AASE/AASY note/style/personality semantics and original playlist timing/transitions.
MIDI controllers, note exclusivity and scheduling need their own behavioral checks.
Whole-world restoration and other required services also keep the M2 gate open.
The user's workspace README changes remain untouched.

Implementation commit: `ee0cd4a78b49d75e050db606bbfb11177265cbcf`.
