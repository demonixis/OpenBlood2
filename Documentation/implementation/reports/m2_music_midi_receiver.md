# M2 — immediate MIDI channel delivery

M2 remains **incomplete**. Prepared MIDI messages now have a checked receiver
that applies bank/program and live controls to the correct channel, starts held
DLS notes and releases them through the original note-off/repeat selection rules.
The in-game inspector shows actual receiver state and exposes labelled manual
channel-note controls. Timed grid delivery and original playlists are still absent;
MUSIC remains explicitly unsupported.

See [contracts](../../contracts/music_midi_receiver_contracts.md) and
[source ledger](../sources/m2_music_midi_receiver_sources.json).

## Original comparison

The [read-only executable probe](evidence/m2-midi-receiver-probe.py) executes the
installed MSynth25 selection branches on synthetic voice lists. 146 cases cover
all channels, extreme and ordinary keys, newest-held note-off, released/sustained
exclusions, self-nonexclusive overlap and same-key fast-stop selection. Another
18 cases execute the original fast-release integer cap with envelope evaluation
intercepted. Original release routines are intercepted in selection tests; native
DLL startup, OS calls and PCM synthesis are never executed.

Original note-off normalizes velocity to zero; only the newest held match stops.
The new region's option bit controls replacement across all older channel/key
voices, including release tails and different programs. The replacement retains
its own full-precision DLS envelope and caps fast release to `48000 / 70` whole
frames. It does not claim bit-identical original quantization or retail audio.
The combined probe decisions hash is
`8cc14cadf148829ed7a27615a0f447f18ab6f83088fe0262385f62b8a4426a90`.

## Behavioral verification

Synthetic tests exercise actual sample clocks/PCM, channel and audition isolation,
release tails, controller rollback, held notes beyond the old diagnostic limit,
start-boundary stops, overlap/replacement, invalid data, capacity failures and
shared bank cleanup. The private sequence uses all 38 Cabal regions with labelled
synthetic messages, finite nonzero PCM and identical rendered/hidden clocks.
The original selected band's 80 setup messages are also delivered in the GPU
smoke before held-note, controller, self-nonexclusive overlap and ordered stop operations
across world reload.

The first lifetime test retained a copied inspection snapshot and therefore kept
the bank alive after the sampler finished. The test now releases that owning
snapshot before checking final bank destruction. A subsequent test-only compile
error from placing the clear in an unrelated scope was corrected. Neither required
a production ownership workaround; the initial logs remain in the evidence.

The first NVIDIA smoke assumed the selected Cabal instrument replaced repeated
notes. Debugger inspection showed its authored region options are 1, explicitly
allowing overlap. The corrected smoke requires that exact property, verifies two
voices, stops the newest first and confirms the older voice remains until its own
stop. Synthetic tests retain the separate fast-release checks. The production
receiver was unchanged; original GPU failure evidence is preserved.

| Configuration | Result | Wall time |
| --- | --- | --- |
| debug-tests | 132/132 passed | 20.26 s |
| release-tests | 132/132 passed | 1.87 s |
| sanitize-tests | 132/132 passed | 82.68 s |
| debug-gpu | 2/2 passed | 10.99 s |
| release-gpu | 2/2 passed | 5.40 s |

The [validation ledger](evidence/m2-midi-receiver.json) records frozen source hashes,
commands and raw logs. The Debug application was rebuilt after correcting the
smoke expectation; the engine and headless-test inputs remained unchanged. The [driver](evidence/m2-midi-receiver-validation.py) selects
music, world/object, audio and configuration suites. Modified C++ files pass
formatting and the final builds report no warnings in changed code. Existing
PlayerVoice, ItemPickups and PlayerContainers enum/float warnings remain visible
in the broader rebuilds; those sources match the implementation parent.

Both GPU runs report NVIDIA GeForce RTX 3060 Laptop Vulkan. The sound test
verifies real sample/output advancement, 86 accepted MIDI messages, two ordinary
stops, authored same-key overlap and no receiver failure, including hidden inspection and
reload. Release builds without the inspector. The reviewed Debug screenshot
shows receiver accounting; transient held-note controls were not exercised by
hand. Dummy audio is not a listening comparison. Native Metal, Windows and mobile
were not tested in this increment.

All 10,923 inventoried reference files and six reference repositories remain
unchanged. Local Markdown links and owned source/documentation whitespace checks
pass; raw tool logs/XML retain original whitespace. The user's root README remains
unchanged and excluded from the commits.


Remaining work includes timed delivery, section/playlist transport, unsupported
controllers and other original MUSIC service requirements. The M2 gate is unchanged.

Implementation commit: `f7f7c5a9a5b2441be48a8862152f266b9d640edd`.
