#pragma once
#include "engine/ima_curve.h"
#include "engine/ima_note_event.h"

namespace OpenBlood2
{
    // Original style-point expansion with an explicit checked owner and caller tick.
    // Messages retain source order and raw data bytes (key/velocity for this shared
    // message type). No admission, header-offset application, scheduling or MIDI send.
    // Failure clears output. Up to 4096 points and 65536 messages per call.
    bool prepare_ima_curve_events(const ImaCurve&, std::int32_t tick, std::vector<ImaTimedMidiMessage>& out,
                                  std::string& error, std::size_t message_budget = 65536);
} // namespace OpenBlood2
