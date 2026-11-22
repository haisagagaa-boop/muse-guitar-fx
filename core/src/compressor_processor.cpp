// CompressorProcessor — implementasi (Phase 2.1).
//
// process() bebas alokasi/mutex/file I/O: hanya membaca atomic target,
// aritmetika float per frame, dan state internal. Tidak ada panggilan
// sistem di jalur audio.

#include "muse_dsp/compressor_processor.h"

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

// Gain reduction (dB, >= 0) dari level envelope (dB) dengan soft knee.
// Kurva kontinu (nilai dan turunan cocok di kedua ujung knee):
//   di bawah knee: 0; di atas knee: (x - T) * (1 - 1/R);
//   di dalam knee: (1 - 1/R) * (x - T + W/2)^2 / (2W).
float gainReductionDb(float levelDb, float thresholdDb, float ratio,
                      float kneeDb) noexcept {
    const float slope = 1.0f - 1.0f / ratio;  // 0 bila ratio = 1
    if (kneeDb < 1e-6f) {
        // Hard knee.
        const float over = levelDb - thresholdDb;
        return over > 0.0f ? slope * over : 0.0f;
    }
    const float d = levelDb - thresholdDb;
    if (2.0f * d < -kneeDb) {
        return 0.0f;
    }
    if (2.0f * d > kneeDb) {
        return slope * d;
    }
    const float kneePart = d + kneeDb * 0.5f;
    return slope * kneePart * kneePart / (2.0f * kneeDb);
}

}  // namespace

CompressorProcessor::CompressorProcessor(float thresholdDb, float ratio,
                                         float attackMs, float releaseMs,
                                         float makeupGainDb, float kneeDb)
    : thresholdDb_(clamp(thresholdDb, -60.0f, 0.0f), 0.02),
      ratio_(clamp(ratio, 1.0f, 20.0f), 0.02),
      attackMs_(clamp(attackMs, 0.01f, 1000.0f), 0.02),
      releaseMs_(clamp(releaseMs, 1.0f, 5000.0f), 0.02),
      makeupGainDb_(clamp(makeupGainDb, -12.0f, 24.0f), 0.02),
      kneeDb_(clamp(kneeDb, 0.0f, 20.0f), 0.02),
      initialThresholdDb_(clamp(thresholdDb, -60.0f, 0.0f)),
      initialRatio_(clamp(ratio, 1.0f, 20.0f)),
      initialAttackMs_(clamp(attackMs, 0.01f, 1000.0f)),
      initialReleaseMs_(clamp(releaseMs, 1.0f, 5000.0f)),
      initialMakeupGainDb_(clamp(makeupGainDb, -12.0f, 24.0f)),
      initialKneeDb_(clamp(kneeDb, 0.0f, 20.0f)) {}

void CompressorProcessor::prepare(const ProcessSpec& spec) {
    sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    thresholdDb_.prepare(sampleRate_);
    ratio_.prepare(sampleRate_);
    attackMs_.prepare(sampleRate_);
    releaseMs_.prepare(sampleRate_);
    makeupGainDb_.prepare(sampleRate_);
    kneeDb_.prepare(sampleRate_);
}

void CompressorProcessor::process(AudioBuffer& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample) {
        // Parameter yang di-smoothing per frame (target dibaca dari atomic).
        const float thresholdDb = thresholdDb_.getNextValue();
        const float ratio = ratio_.getNextValue();
        const float attackMs = attackMs_.getNextValue();
        const float releaseMs = releaseMs_.getNextValue();
        const float makeupGainDb = makeupGainDb_.getNextValue();
        const float kneeDb = kneeDb_.getNextValue();

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

        // 2. Gain computer soft knee + makeup gain.
        const float envelopeDb =
            envelope_ > 0.0f
                ? 20.0f * std::log10(envelope_)
                : kSilenceFloorDb;
        gainReductionDb_ =
            gainReductionDb(envelopeDb, thresholdDb, ratio, kneeDb);
        const float gainDb = makeupGainDb - gainReductionDb_;
        const float gainLinear =
            static_cast<float>(std::pow(10.0, gainDb * 0.05));
        for (int channel = 0; channel < numChannels; ++channel) {
            buffer.getWritePointer(channel)[sample] *= gainLinear;
        }
    }
}

void CompressorProcessor::reset() {
    thresholdDb_.reset(initialThresholdDb_);
    ratio_.reset(initialRatio_);
    attackMs_.reset(initialAttackMs_);
    releaseMs_.reset(initialReleaseMs_);
    makeupGainDb_.reset(initialMakeupGainDb_);
    kneeDb_.reset(initialKneeDb_);
    envelope_ = 0.0f;
    gainReductionDb_ = 0.0f;
}

void CompressorProcessor::setThresholdDb(float db) noexcept {
    thresholdDb_.setTarget(clamp(db, -60.0f, 0.0f));
}

void CompressorProcessor::setRatio(float ratio) noexcept {
    ratio_.setTarget(clamp(ratio, 1.0f, 20.0f));
}

void CompressorProcessor::setAttackMs(float ms) noexcept {
    attackMs_.setTarget(clamp(ms, 0.01f, 1000.0f));
}

void CompressorProcessor::setReleaseMs(float ms) noexcept {
    releaseMs_.setTarget(clamp(ms, 1.0f, 5000.0f));
}

void CompressorProcessor::setMakeupGainDb(float db) noexcept {
    makeupGainDb_.setTarget(clamp(db, -12.0f, 24.0f));
}

void CompressorProcessor::setKneeDb(float db) noexcept {
    kneeDb_.setTarget(clamp(db, 0.0f, 20.0f));
}

}  // namespace muse_dsp
