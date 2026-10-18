# IMA band channels and DLS note routing

M2 and MUSIC remain incomplete. The selected band's verified 80-message setup now
supplies owned MIDI channel snapshots to the diagnostic DLS sampler. This is not
a running playlist, a download manager or a claim of complete retail synthesis.
See [band selection](music_band_selection_contracts.md),
[sampler contracts](music_sampler_contracts.md),
[source ledger](../implementation/sources/m2_music_routing_sources.json) and
[verification](../../../Documentation/Implementation/m2_music_routing.md).

## Original identity and lookup

The installed MSynth25 converts a DLS instrument's raw `bank` and `program` to:

```cpp
program | ((bank & 0x7f) << 7) | ((bank & 0x7f00) << 6) | (bank & 0x80000000)
```

The remaining bank bits are ignored by that conversion, not rewritten in the raw
reader. MIDI lookup builds `(MSB << 14) | (LSB << 7) | program` with bit31 for
percussion. Preserve zero-based program numbers. Search the exact patch for a
region containing the requested key, inclusively. On a percussion miss, search
patch `0x80000000` for that key. There is no melodic bank-zero fallback. Velocity
is checked separately by the existing sampler, after instrument selection; an
unsupported graph or velocity region does not cause another instrument fallback.
Key 128 is the original any-key lookup sentinel for inspection, never a playable
note. Missing programs and keys remain explicit failures.

The public lookup takes a reader-produced immutable bank, with at most 256
instruments and 4,096 regions per selected instrument. Output is cleared on any
failure. Different raw bank identifiers can collapse to the same packed patch;
such a collision is explicitly refused. The historical linked-list first-match
policy is not reproduced for ambiguous input. The original resident/refcount
conditions are not a completed download manager: this diagnostic treats decoded
instruments in the supplied bank as resident.

## Controller snapshots and ownership

The catalogue applies the prepared CC7, CC10, bank-MSB, bank-LSB and program values
to 16 source-order channel snapshots. Source channel 5 maps to MIDI 9/percussion;
all other channels use `(source + 4) & 15`. Expression is 127. Raw IMA bank flags
are not DLS bank IDs and are not applied a second time after MIDI setup conversion.
The configured single `InstrumentFiles` DLS bank is retained by shared ownership;
missing/invalid banks fail binding. Multiple banks are refused. A collection label
never triggers a filesystem read or an invented filename match. Cabal section
bands have empty collection labels, yet their legacy MIDI programs select the
already loaded Cabal instruments. Dynamic collection resolution and the OS GM
bank are still unavailable.

`play_channel` resolves the instrument/key then uses the existing sampler. Each
voice retains its immutable bank and an owned copy of the MIDI/controller state;
changing or unloading the catalogue cannot mutate a sounding note. Copy the channel
before adding a voice: the argument may alias an existing voice, and vector growth
can invalidate that reference. Direct region
auditions retain their old default controllers. CC7 volume and CC11 expression
multiply gain by their squared normalized values. CC10 adds to the constant-pan
articulation before the existing clamp and square-root stereo gain calculation.
All three controllers must be in 0..127. Zero volume/expression makes a silent
voice whose clock and cleanup still run. Failed routing/controllers allocate no
voice, preserve existing voices and increment refusal accounting exactly once.
[Explicit live controls](music_live_control_contracts.md) can now change current-voice
volume/expression/pan and default-range pitch bend while keeping the note-on snapshot.
The [immediate receiver](music_midi_receiver_contracts.md) adds explicit channel
identity and original same-key stop/repeat selection. Sustain, nonzero key groups
and the timed MIDI event queue remain unavailable.

World reinspection/unload drops catalogue bank aliases. Audio voices own their
bank until completion/clear, with existing non-reused IDs and world audio cleanup.
Inspection reports source/MIDI channel, bank/program, instrument, controllers,
missing routes, active note snapshots and sampler failures. Channel audition uses
a first-region midpoint note, velocity clamped around 100 and 20% diagnostic gain.
It does not transpose the final key again: original note preparation owns that
conversion. Drawing does not advance transport or generate notes automatically.
