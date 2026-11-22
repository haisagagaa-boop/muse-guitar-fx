// CompressorProcessor — kompresor dynamics (Phase 2.1).
//
// Menurunkan gain sinyal di atas threshold sesuai rasio, dengan soft knee
// dan makeup gain. Dipakai untuk meratakan dinamika gitar (sustain lebih
// panjang, transient lebih terkendali).
//
// Desain (per frame, linked stereo — satu gain untuk semua channel):
//   1. Envelope follower peak satu-pole: naik dengan attack, turun
//      dengan release. Envelope dihitung dari |sampel| terbesar di
//      semua channel pada frame itu.
//   2. Gain computer soft knee (dalam dB): di bawah knee tidak ada
//      reduksi; di atas knee reduksi = (level - threshold) * (1 - 1/ratio);
//      di dalam knee reduksi mengikuti kurva kuadrat yang kontinu (nilai
//      dan turunan cocok di kedua ujung knee).
//   3. Gain yang diterapkan = -gainReduction + makeupGain, di-smoothing
//      oleh envelope follower itu sendiri (attack/release) — tanpa
//      lompatan mendadak, tanpa pumping pada sinyal steady (reduksi
//      konstan bila envelope konstan).
//
// Semua parameter lewat SmoothedParameter (atomic, lock-free; smoothing
// 20 ms) — aman diubah dari control thread kapan pun.
//
// Denormal protection (TANPA JUCE — core tidak boleh bergantung JUCE):
// sama seperti NoiseGate — flush-to-zero eksplisit pada envelope yang
// jatuh di bawah 1e-30 (sekitar -600 dB). Tidak ada instruksi platform
// dan tidak ada DC offset pada sinyal. Tidak ada state rekursif lain:
// gain dihitung langsung dari envelope tiap frame.

#pragma once

#include "muse_dsp/audio_processor.h"
#include "muse_dsp/smoothed_parameter.h"

namespace muse_dsp {

class CompressorProcessor : public AudioProcessor {
public:
    // thresholdDb: -60..0 dB (default -20). ratio: 1..20 (default 4).
    // attackMs: 0,01..1000 ms (default 10). releaseMs: 1..5000 ms
    // (default 100). makeupGainDb: -12..+24 dB (default 0).
    // kneeDb: lebar soft knee, 0..20 dB (default 6; 0 = hard knee).
    CompressorProcessor(float thresholdDb = -20.0f, float ratio = 4.0f,
                        float attackMs = 10.0f, float releaseMs = 100.0f,
                        float makeupGainDb = 0.0f, float kneeDb = 6.0f);

    void prepare(const ProcessSpec& spec) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* getName() const override { return "Compressor"; }

    // Thread-safe: dari control/UI thread kapan pun (atomic, lock-free).
    // Nilai di-clamp ke rentang yang valid (lihat konstruktor).
    void setThresholdDb(float db) noexcept;
    void setRatio(float ratio) noexcept;
    void setAttackMs(float ms) noexcept;
    void setReleaseMs(float ms) noexcept;
    void setMakeupGainDb(float db) noexcept;
    void setKneeDb(float db) noexcept;
    float getThresholdDb() const noexcept { return thresholdDb_.getTarget(); }
    float getRatio() const noexcept { return ratio_.getTarget(); }
    float getAttackMs() const noexcept { return attackMs_.getTarget(); }
    float getReleaseMs() const noexcept { return releaseMs_.getTarget(); }
    float getMakeupGainDb() const noexcept { return makeupGainDb_.getTarget(); }
    float getKneeDb() const noexcept { return kneeDb_.getTarget(); }

    // Audio thread saja: gain reduction saat ini (dB, >= 0) dan gain
    // total yang diterapkan (dB, sudah termasuk makeup).
    float getCurrentGainReductionDb() const noexcept {
        return gainReductionDb_;
    }
    float getCurrentGainDb() const noexcept {
        return makeupGainDb_.getCurrentValue() - gainReductionDb_;
    }

private:
    // Lantai envelope untuk konversi dB (di bawah ini dianggap sunyi).
    static constexpr float kSilenceFloorDb = -120.0f;
    // Ambang flush-to-zero envelope (denormal protection).
    static constexpr float kDenormalFlushLevel = 1e-30f;

    SmoothedParameter thresholdDb_;
    SmoothedParameter ratio_;
    SmoothedParameter attackMs_;
    SmoothedParameter releaseMs_;
    SmoothedParameter makeupGainDb_;
    SmoothedParameter kneeDb_;
    float initialThresholdDb_;
    float initialRatio_;
    float initialAttackMs_;
    float initialReleaseMs_;
    float initialMakeupGainDb_;
    float initialKneeDb_;

    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;           // peak envelope linear (linked)
    float gainReductionDb_ = 0.0f;    // GR frame terakhir (dB, >= 0)
};

}  // namespace muse_dsp
