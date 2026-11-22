// AudioBuffer — implementasi (B3). Lihat audio_buffer.h untuk kontraknya.

#include "muse_dsp/audio_buffer.h"

#include <cassert>

namespace muse_dsp {

void AudioBuffer::resize(int numChannels, int numSamples) {
    // resize() mengalokasi — hanya dari prepare/start, bukan dari callback.
    numChannels_ = numChannels > 0 ? numChannels : 0;
    numSamples_ = numSamples > 0 ? numSamples : 0;
    channels_.assign(static_cast<size_t>(numChannels_),
                     std::vector<float>(static_cast<size_t>(numSamples_), 0.0f));
}

float* AudioBuffer::getWritePointer(int channel) noexcept {
    assert(channel >= 0 && channel < numChannels_);
    return channels_[static_cast<size_t>(channel)].data();
}

const float* AudioBuffer::getReadPointer(int channel) const noexcept {
    assert(channel >= 0 && channel < numChannels_);
    return channels_[static_cast<size_t>(channel)].data();
}

float AudioBuffer::getSample(int channel, int sample) const noexcept {
    assert(channel >= 0 && channel < numChannels_);
    assert(sample >= 0 && sample < numSamples_);
    return channels_[static_cast<size_t>(channel)][static_cast<size_t>(sample)];
}

void AudioBuffer::setSample(int channel, int sample, float value) noexcept {
    assert(channel >= 0 && channel < numChannels_);
    assert(sample >= 0 && sample < numSamples_);
    channels_[static_cast<size_t>(channel)][static_cast<size_t>(sample)] = value;
}

void AudioBuffer::clear() noexcept {
    for (auto& channel : channels_) {
        std::fill(channel.begin(), channel.end(), 0.0f);
    }
}

void AudioBuffer::clearChannel(int channel) noexcept {
    assert(channel >= 0 && channel < numChannels_);
    auto& data = channels_[static_cast<size_t>(channel)];
    std::fill(data.begin(), data.end(), 0.0f);
}

}  // namespace muse_dsp
