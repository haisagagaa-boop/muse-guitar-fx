// NoiseGateProcessor — implementasi (Phase 2.1).
//
// process() bebas alokasi/mutex/file I/O: hanya membaca atomic target,
// aritmetika float per frame, dan state internal. Tidak ada panggilan
// sistem di jalur audio.

#include "muse_dsp/noise_gate_processor.h"

#include <algorithm>
#include <cmath>

#include "muse_dsp/audio_buffer.h"

namespace muse_dsp {

namespace {

float clamp(float value, float low, float high) noexcept {
    return std::max(low, std::min(high, value));
}

// Koefisien satu-pole untuk konstanta waktu (detik) pada sample rate
// tertentu: setelah satu konstanta waktu, envelope mencapai ~63% langkah.
float onePoleCoefficient(double timeSeconds, double sampleRate) noexcept {
    if (timeSeconds <= 0.0) {
        return 0.0f;  // langsung sampai
    }
    return static_cast<float>(
        std::exp(-1.0 / (timeSeconds * sampleRate)));
}

}  // namespace

NoiseGateProcessor::NoiseGateProcessor(float thresholdDb, float attackMs,
                                       float releaseMs, float rangeDb)
    : thresholdDb_(clamp(thresholdDb, -100.0f, 0.0f), 0.02),
      attackMs_(clamp(attackMs, 0.01f, 1000.0f), 0.02),
      releaseMs_(clamp(releaseMs, 1.0f, 5000.0f), 0.02),
      rangeDb_(clamp(rangeDb, -100.0f, 0.0f), 0.02),
      initialThresholdDb_(clamp(thresholdDb, -100.0f, 0.0f)),
      initialAttackMs_(clamp(attackMs, 0.01f, 1000.0f)),
      initialReleaseMs_(clamp(releaseMs, 1.0f, 5000.0f)),
      initialRangeDb_(clamp(rangeDb, -100.0f, 0.0f)) {
    // State awal = tertutup (envelope nol): gain mulai dari range dB,
    // konsisten dengan reset().
    gainDb_ = initialRangeDb_;
}

void NoiseGateProcessor::prepare(const ProcessSpec& spec) {
    sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    thresholdDb_.prepare(sampleRate_);
    attackMs_.prepare(sampleRate_);
    releaseMs_.prepare(sampleRate_);
    rangeDb_.prepare(sampleRate_);
}

void NoiseGateProcessor::process(AudioBuffer& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample) {
        // Parameter yang di-smoothing per frame (target dibaca dari atomic).
        const float thresholdDb = thresholdDb_.getNextValue();
        const float attackMs = attackMs_.getNextValue();
        const float releaseMs = releaseMs_.getNextValue();
        const float rangeDb = rangeDb_.getNextValue();

        // 1. Envelope follower peak (linked): |sampel| terbesar di semua
        //    channel pada frame ini.
        float peak = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel) {
            peak = std::max(
                peak, std::fabs(buffer.getReadPointer(channel)[sample]));
        }
        const float attackCoeff = onePoleCoefficient(attackMs * 0.001,
                                                     sampleRate_);
        const float releaseCoeff = onePoleCoefficient(releaseMs * 0.001,
                                                      sampleRate_);
        if (peak > envelope_) {
            envelope_ += (1.0f - attackCoeff) * (peak - envelope_);
        } else {
            envelope_ *= releaseCoeff;
        }
        // Denormal protection: flush-to-zero eksplisit (lihat header).
        if (envelope_ < kDenormalFlushLevel) {
            envelope_ = 0.0f;
        }

        // 2. Gain computer dengan hysteresis 3 dB.
        const float envelopeDb =
            envelope_ > 0.0f
                ? 20.0f * std::log10(envelope_)
                : kSilenceFloorDb;
        if (open_) {
            if (envelopeDb < thresholdDb - kHysteresisDb) {
                open_ = false;
            }
        } else if (envelopeDb > thresholdDb) {
            open_ = true;
        }
        const float targetGainDb = open_ ? 0.0f : rangeDb;

        // 3. Gain ramp satu-pole menuju target: laju attack saat membuka,
        //    laju release saat menutup. Di-clamp tepat di target agar
        //    tidak ada ekor asimtotik (dan tidak ada state denormal).
        const float rampCoeff = targetGainDb > gainDb_ ? attackCoeff
                                                       : releaseCoeff;
        gainDb_ += (1.0f - rampCoeff) * (targetGainDb - gainDb_);
        if (std::fabs(targetGainDb - gainDb_) < 1e-6f) {
            gainDb_ = targetGainDb;
        }

        const float gainLinear =
            static_cast<float>(std::pow(10.0, gainDb_ * 0.05));
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.getWritePointer(channel)[sample] *= gainLinear;
        }
    }
}

void NoiseGateProcessor::reset() {
    thresholdDb_.reset(initialThresholdDb_);
    attackMs_.reset(initialAttackMs_);
    releaseMs_.reset(initialReleaseMs_);
    rangeDb_.reset(initialRangeDb_);
    envelope_ = 0.0f;
    gainDb_ = initialRangeDb_;  // mulai tertutup (envelope nol)
    open_ = false;
}

void NoiseGateProcessor::setThresholdDb(float db) noexcept {
    thresholdDb_.setTarget(clamp(db, -100.0f, 0.0f));
}

void NoiseGateProcessor::setAttackMs(float ms) noexcept {
    attackMs_.setTarget(clamp(ms, 0.01f, 1000.0f));
}

void NoiseGateProcessor::setReleaseMs(float ms) noexcept {
    releaseMs_.setTarget(clamp(ms, 1.0f, 5000.0f));
}

void NoiseGateProcessor::setRangeDb(float db) noexcept {
    rangeDb_.setTarget(clamp(db, -100.0f, 0.0f));
}

}  // namespace muse_dsp
