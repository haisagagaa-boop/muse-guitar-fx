// JUCE integration smoke test (B2) — console app, tanpa UI.
// Membuktikan JUCE 8.0.15 ter-link dan juce::dsp bekerja offline.
// Keluar non-nol bila ada pemeriksaan yang gagal.

#include <cmath>
#include <cstdio>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "muse_dsp/muse_dsp.h"

int main() {
    std::printf("MUSE Guitar FX — JUCE integration smoke test\n");
    std::printf("muse_dsp version: %s\n", muse_dsp::getVersionString());
    std::printf("JUCE version: %d.%d.%d\n", JUCE_MAJOR_VERSION, JUCE_MINOR_VERSION,
                JUCE_BUILDNUMBER);

    if (JUCE_MAJOR_VERSION != 8 || JUCE_MINOR_VERSION != 0 ||
        JUCE_BUILDNUMBER != 15) {
        std::printf("FAIL: JUCE bukan 8.0.15\n");
        return 1;
    }

    // juce::dsp::Gain: input 1.0, gain 0.5 -> output harus 0.5.
    juce::dsp::Gain<float> gain;
    gain.setGainLinear(0.5f);
    gain.prepare(juce::dsp::ProcessSpec{44100.0, 64, 1});

    juce::AudioBuffer<float> buffer(1, 64);
    for (int i = 0; i < 64; ++i) buffer.setSample(0, i, 1.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    gain.process(context);

    const float out = buffer.getSample(0, 0);
    std::printf("juce::dsp::Gain: 1.0 * 0.5 = %f\n", out);
    if (std::fabs(out - 0.5f) > 1e-6f) {
        std::printf("FAIL: output gain bukan 0.5\n");
        return 1;
    }

    std::printf("JUCE smoke test: PASS\n");
    return 0;
}
