// GainProcessor — implementasi (B3). process() bebas alokasi/mutex/file I/O:
// hanya membaca atomic target dan mengalikan sampel.

#include "muse_dsp/gain_processor.h"

#include "muse_dsp/audio_buffer.h"

namespace muse_dsp {

GainProcessor::GainProcessor(float initialGainLinear, double smoothingTimeSeconds)
    : gain_(initialGainLinear, smoothingTimeSeconds),
      smoothingTimeSeconds_(smoothingTimeSeconds),
      initialGain_(initialGainLinear) {}

void GainProcessor::prepare(const ProcessSpec& spec) {
    gain_.setSmoothingTime(smoothingTimeSeconds_);
    gain_.prepare(spec.sampleRate);
}

void GainProcessor::process(AudioBuffer& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    for (int sample = 0; sample < numSamples; ++sample) {
        // Satu nilai gain per frame, dipakai semua channel.
        const float gain = gain_.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel) {
            float* data = buffer.getWritePointer(channel);
            data[sample] *= gain;
        }
    }
}

void GainProcessor::reset() {
    gain_.reset(initialGain_);
}

}  // namespace muse_dsp
