// SmoothedParameter — parameter float yang aman antar-thread + smoothing (B3).
//
// Aturan thread (kontrak):
// - setTarget() boleh dipanggil dari thread mana pun, kapan pun. Target
//   disimpan di std::atomic<float> (lock-free) — tidak ada race condition,
//   tidak ada mutex.
// - getNextValue() HANYA dipanggil dari audio thread, sekali per frame
//   (sampel). Ia memajukan nilai saat ini menuju target dengan ramp linear
//   selama smoothingTime, sehingga perubahan gain tidak menimbulkan klik /
//   perubahan suara mendadak.
// - setSmoothingTime() dan reset() hanya dipanggil saat audio thread berhenti
//   (prepare/reset) — keduanya menyentuh state milik audio thread.
// - Tidak ada alokasi, tidak ada mutex, tidak ada file I/O di jalur ini.

#pragma once

#include <atomic>
#include <cmath>

namespace muse_dsp {

class SmoothedParameter {
public:
    explicit SmoothedParameter(float initialValue = 0.0f,
                               double smoothingTimeSeconds = 0.02)
        : target_(initialValue),
          current_(initialValue),
          rampTarget_(initialValue),
          smoothingTimeSeconds_(smoothingTimeSeconds) {}

    // Thread-safe: dari control/UI thread kapan pun.
    void setTarget(float value) noexcept {
        target_.store(value, std::memory_order_relaxed);
    }
    float getTarget() const noexcept {
        return target_.load(std::memory_order_relaxed);
    }

    // Hanya saat audio thread berhenti (prepare). Waktu ramp dalam detik.
    void setSmoothingTime(double seconds) noexcept {
        smoothingTimeSeconds_ = seconds > 0.0 ? seconds : 0.0;
    }
    double getSmoothingTime() const noexcept { return smoothingTimeSeconds_; }

    // Dipanggil dari prepare(): tetapkan sample rate, hitung ulang panjang ramp.
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    }

    // Snap nilai saat ini = target = value. Hanya saat audio thread berhenti.
    void reset(float value) noexcept {
        target_.store(value, std::memory_order_relaxed);
        current_ = value;
        rampTarget_ = value;
        step_ = 0.0f;
        rampSamplesRemaining_ = 0;
    }

    // Audio thread saja. Nilai saat ini (setelah ramp terakhir).
    float getCurrentValue() const noexcept { return current_; }

    // Audio thread saja, sekali per frame. Mengembalikan nilai untuk frame ini.
    float getNextValue() noexcept {
        const float target = target_.load(std::memory_order_relaxed);
        if (target != rampTarget_) {
            // Target berubah: mulai ramp linear baru dari nilai saat ini.
            rampTarget_ = target;
            const int rampLength = getRampLengthSamples();
            rampSamplesRemaining_ = rampLength;
            step_ = (target - current_) / static_cast<float>(rampLength);
        }
        if (rampSamplesRemaining_ > 0) {
            current_ += step_;
            if (--rampSamplesRemaining_ == 0) {
                current_ = rampTarget_;
            }
        }
        return current_;
    }

    // Audio thread saja: true bila ramp masih berjalan.
    bool isSmoothing() const noexcept { return rampSamplesRemaining_ > 0; }

private:
    int getRampLengthSamples() const noexcept {
        const int length =
            static_cast<int>(std::lround(smoothingTimeSeconds_ * sampleRate_));
        return length > 0 ? length : 1;  // minimal 1 sampel (langsung sampai)
    }

    std::atomic<float> target_;
    float current_ = 0.0f;
    float rampTarget_ = 0.0f;
    float step_ = 0.0f;
    int rampSamplesRemaining_ = 0;
    double smoothingTimeSeconds_ = 0.02;
    double sampleRate_ = 48000.0;
};

}  // namespace muse_dsp
