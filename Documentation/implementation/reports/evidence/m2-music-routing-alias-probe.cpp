#include "engine/dls_sampler.h"
#include <string>
using namespace OpenBlood2;
int main()
{
    auto bank = std::make_shared<DlsBank>();
    bank->waves.push_back({0, 48000, {1, 1}, {}});
    DlsRegion region;
    region.key_high = region.velocity_high = 127;
    region.channel = 1;
    region.settings.unity_note = 60;
    region.settings.loop = DlsLoop{0, 2};
    DlsInstrument instrument;
    instrument.regions.push_back(region);
    bank->instruments.push_back(instrument);
    DlsSampler sampler;
    std::string error;
    if (!sampler.play_channel(bank, {}, 60, 100, 1, 1, error)) return 2;
    for (unsigned i = 0; i < 16; ++i)
        if (!sampler.play_channel(bank, *sampler.voices().front().channel, 60, 100, 1, 1, error)) return 3;
    return 0;
}
