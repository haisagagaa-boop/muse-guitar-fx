// MockAudioDevice — perangkat audio tiruan untuk pengujian (B3).
//
// Menjalankan AudioProcessor (biasanya EffectChain) di thread-nya sendiri
// — inilah "audio thread" yang terpisah dari UI/control thread. Tiap blok:
// isi input (sine), panggil processor.process(), rekam output, ukur waktu.
// Tanpa perangkat fisik, tanpa ALSA/JACK, tanpa file I/O.
//
// Kontrak:
// - start() mengalokasikan semua yang dibutuhkan (buffer + rekam), lalu
//   memanggil processor.prepare(). process() di thread audio TIDAK mengalokasi.
// - stop() menghentikan thread dan join — setelah stop(), output rekaman aman
//   dibaca dari control thread (join memberi happens-before).
// - Perubahan parameter & bypass aman dilakukan dari control thread kapan pun
//   (atomic). Perubahan struktur chain hanya saat device berhenti.
// - setInputFrequency()/setInputAmplitude() hanya saat device berhenti.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/audio_processor.h"

namespace muse_dsp {

class MockAudioDevice {
public:
    MockAudioDevice(AudioProcessor& processor, double sampleRate = 48000.0,
                    int blockSize = 256, int numChannels = 2,
                    double inputFrequencyHz = 440.0, double inputAmplitude = 0.5,
                    double maxRecordSeconds = 5.0);
    ~MockAudioDevice();

    MockAudioDevice(const MockAudioDevice&) = delete;
    MockAudioDevice& operator=(const MockAudioDevice&) = delete;

    // Mulai thread audio. Mengembalikan false bila sudah berjalan.
    bool start();
    // Hentikan thread audio dan tunggu selesai. Aman dipanggil berkali-kali.
    void stop();

    bool isRunning() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }
    double getSampleRate() const noexcept { return sampleRate_; }
    int getBlockSize() const noexcept { return blockSize_; }
    int getNumChannels() const noexcept { return numChannels_; }

    // Statistik (aman dibaca dari thread mana pun; atomic).
    uint64_t getBlocksProcessed() const noexcept {
        return blocksProcessed_.load(std::memory_order_relaxed);
    }
    uint64_t getOverrunCount() const noexcept {
        return overrunCount_.load(std::memory_order_relaxed);
    }
    double getMaxCallbackTimeUs() const noexcept {
        return maxCallbackTimeUs_.load(std::memory_order_relaxed);
    }

    // Output channel 0 yang direkam. Hanya getRecordedSampleCount() entri
    // pertama yang valid. Hanya dibaca SETELAH stop().
    const std::vector<float>& getRecordedOutput() const noexcept {
        return recorded_;
    }
    size_t getRecordedSampleCount() const noexcept { return recordedCount_; }

    // Hanya saat device berhenti.
    void setInputFrequency(double hz) noexcept { inputFrequencyHz_ = hz; }
    void setInputAmplitude(double amplitude) noexcept {
        inputAmplitude_ = amplitude;
    }

private:
    void run();  // audio thread
    void fillInput() noexcept;
    void recordOutput() noexcept;
    void updateMaxCallbackTime(double callbackUs) noexcept;

    AudioProcessor& processor_;
    double sampleRate_;
    int blockSize_;
    int numChannels_;
    double inputFrequencyHz_;
    double inputAmplitude_;
    double maxRecordSeconds_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> blocksProcessed_{0};
    std::atomic<uint64_t> overrunCount_{0};
    std::atomic<double> maxCallbackTimeUs_{0.0};

    AudioBuffer buffer_;            // dialokasikan di start(), dipakai ulang
    std::vector<float> recorded_;   // dialokasikan di start(), channel 0
    size_t recordedCount_ = 0;      // audio thread saja
    double phase_ = 0.0;            // audio thread saja
    double phaseStep_ = 0.0;        // audio thread saja
};

}  // namespace muse_dsp
