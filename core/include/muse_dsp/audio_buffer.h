// AudioBuffer — buffer audio antar prosesor (B3).
//
// Buffer dimiliki pemanggil (mock device / offline render), dialokasikan sekali
// saat prepare, lalu dipakai ulang tiap blok. process() TIDAK boleh mengalokasi:
// kelas ini tidak pernah mengubah ukuran di dalam process — resize() hanya
// dipanggil dari prepare/start, bukan dari callback audio.

#pragma once

#include <vector>

namespace muse_dsp {

class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(int numChannels, int numSamples) { resize(numChannels, numSamples); }

    // Mengalokasikan (ulang) isi buffer. HANYA dipanggil dari prepare/start,
    // tidak pernah dari dalam process().
    void resize(int numChannels, int numSamples);

    int getNumChannels() const noexcept { return numChannels_; }
    int getNumSamples() const noexcept { return numSamples_; }

    float* getWritePointer(int channel) noexcept;
    const float* getReadPointer(int channel) const noexcept;

    float getSample(int channel, int sample) const noexcept;
    void setSample(int channel, int sample, float value) noexcept;

    void clear() noexcept;
    void clearChannel(int channel) noexcept;

private:
    int numChannels_ = 0;
    int numSamples_ = 0;
    std::vector<std::vector<float>> channels_;
};

}  // namespace muse_dsp
