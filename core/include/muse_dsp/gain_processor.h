// GainProcessor — prosesor referensi B3 (bukan efek gitar).
//
// Mengalikan tiap sampel dengan gain linear yang di-smoothing per frame.
// Dipakai untuk membuktikan plumbing EffectChain + parameter + mock device
// bekerja. Efek gitar sesungguhnya (Overdrive, Distortion, dll.) adalah
// Phase 2 — bukan di sini.

#pragma once

#include "muse_dsp/audio_processor.h"
#include "muse_dsp/smoothed_parameter.h"

namespace muse_dsp {

class GainProcessor : public AudioProcessor {
public:
    explicit GainProcessor(float initialGainLinear = 1.0f,
                           double smoothingTimeSeconds = 0.02);

    void prepare(const ProcessSpec& spec) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* getName() const override { return "Gain"; }

    // Thread-safe: dari control/UI thread kapan pun (atomic, lock-free).
    void setGain(float gainLinear) noexcept { gain_.setTarget(gainLinear); }
    float getGain() const noexcept { return gain_.getTarget(); }

    // Audio thread saja: gain yang sedang dipakai frame ini.
    float getCurrentGain() const noexcept { return gain_.getCurrentValue(); }

    // Hanya saat audio thread berhenti (sebelum prepare).
    void setSmoothingTime(double seconds) noexcept {
        smoothingTimeSeconds_ = seconds;
    }

private:
    SmoothedParameter gain_;
    double smoothingTimeSeconds_;
    float initialGain_;
};

}  // namespace muse_dsp
