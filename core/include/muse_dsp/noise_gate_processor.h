// NoiseGateProcessor — noise gate (Phase 2.1).
//
// Menutup sinyal di bawah threshold (atenuasi sampai range dB) dan
// membiarkan sinyal di atas threshold lewat tanpa perubahan. Dipakai
// untuk menghilangkan noise/hum di antara not gitar.
//
// Desain (per frame, linked stereo — satu gain untuk semua channel):
//   1. Envelope follower peak satu-pole: naik dengan attack, turun
//      dengan release. Envelope dihitung dari |sampel| terbesar di
//      semua channel pada frame itu.
//   2. Gain computer dengan hysteresis 3 dB: gate TERBUKA bila envelope
//      > threshold, TERTUTUP bila envelope < threshold - 3 dB. Di antara
//      keduanya gate mempertahankan state sebelumnya — inilah yang
//      mencegah chattering pada sinyal yang diam di sekitar threshold.
//   3. Gain ramp satu-pole menuju 0 dB (terbuka) atau range dB
//      (tertutup), dengan laju attack saat membuka dan release saat
//      menutup — tanpa klik saat gate membuka/menutup.
//
// Semua parameter lewat SmoothedParameter (atomic, lock-free; smoothing
// 20 ms) — aman diubah dari control thread kapan pun.
//
// Denormal protection (TANPA JUCE — core tidak boleh bergantung JUCE):
// envelope adalah state rekursif yang meluruh eksponensial menuju nol
// saat input sunyi; tanpa perlakuan ia akan masuk rentang denormal
// (CPU spike). Mekanisme yang dipakai: flush-to-zero eksplisit —
// envelope yang jatuh di bawah 1e-30 (sekitar -600 dB, jauh di bawah
// ambang dengar) langsung di-nol-kan. Tidak ada instruksi platform
// (FTZ/DAZ via CSR) dan tidak ada DC offset yang ditambahkan ke sinyal.
// State gain (gainDb_) bergerak menuju target dan di-clamp tepat di
// target, sehingga tidak ada ekor denormal di sana.

#pragma once

#include "muse_dsp/audio_processor.h"
#include "muse_dsp/smoothed_parameter.h"

namespace muse_dsp {

class NoiseGateProcessor : public AudioProcessor {
public:
    // thresholdDb: -100..0 dB (default -45). attackMs: 0,01..1000 ms
    // (default 1). releaseMs: 1..5000 ms (default 100).
    // rangeDb: atenuasi saat tertutup, -100..0 dB (default -80).
    NoiseGateProcessor(float thresholdDb = -45.0f, float attackMs = 1.0f,
                       float releaseMs = 100.0f, float rangeDb = -80.0f);

    void prepare(const ProcessSpec& spec) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* getName() const override { return "NoiseGate"; }

    // Thread-safe: dari control/UI thread kapan pun (atomic, lock-free).
    // Nilai di-clamp ke rentang yang valid (lihat konstruktor).
    void setThresholdDb(float db) noexcept;
    void setAttackMs(float ms) noexcept;
    void setReleaseMs(float ms) noexcept;
    void setRangeDb(float db) noexcept;
    float getThresholdDb() const noexcept { return thresholdDb_.getTarget(); }
    float getAttackMs() const noexcept { return attackMs_.getTarget(); }
    float getReleaseMs() const noexcept { return releaseMs_.getTarget(); }
    float getRangeDb() const noexcept { return rangeDb_.getTarget(); }

    // Audio thread saja: apakah gate sedang terbuka.
    bool isOpen() const noexcept { return open_; }
    // Audio thread saja: gain yang sedang diterapkan (dB).
    float getCurrentGainDb() const noexcept { return gainDb_; }

private:
    // Hysteresis (dB): gate menutup 3 dB di bawah threshold pembuka.
    static constexpr float kHysteresisDb = 3.0f;
    // Lantai envelope untuk konversi dB (di bawah ini dianggap sunyi).
    static constexpr float kSilenceFloorDb = -120.0f;
    // Ambang flush-to-zero envelope (denormal protection).
    static constexpr float kDenormalFlushLevel = 1e-30f;

    SmoothedParameter thresholdDb_;
    SmoothedParameter attackMs_;
    SmoothedParameter releaseMs_;
    SmoothedParameter rangeDb_;
    float initialThresholdDb_;
    float initialAttackMs_;
    float initialReleaseMs_;
    float initialRangeDb_;

    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;  // peak envelope linear (linked, semua channel)
    float gainDb_ = 0.0f;    // gain yang diterapkan saat ini (dB)
    bool open_ = false;      // state hysteresis
};

}  // namespace muse_dsp
