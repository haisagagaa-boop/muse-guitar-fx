// MockAudioDevice — implementasi (B3).
//
// Thread audio: loop berbasis deadline (sleep_until) dengan ukuran blok tetap.
// Callback (fillInput + process + record) tidak mengalokasi, tidak mengunci
// mutex, dan tidak melakukan file I/O.

#include "muse_dsp/mock_audio_device.h"

#include <cmath>

namespace muse_dsp {

namespace {
constexpr double kTwoPi = 6.28318530717958647692;
}  // namespace

MockAudioDevice::MockAudioDevice(AudioProcessor& processor, double sampleRate,
                                 int blockSize, int numChannels,
                                 double inputFrequencyHz, double inputAmplitude,
                                 double maxRecordSeconds)
    : processor_(processor),
      sampleRate_(sampleRate),
      blockSize_(blockSize),
      numChannels_(numChannels),
      inputFrequencyHz_(inputFrequencyHz),
      inputAmplitude_(inputAmplitude),
      maxRecordSeconds_(maxRecordSeconds) {}

MockAudioDevice::~MockAudioDevice() {
    stop();
}

bool MockAudioDevice::start() {
    if (running_.load(std::memory_order_relaxed)) {
        return false;
    }
    // Semua alokasi terjadi di sini (control thread), bukan di callback.
    processor_.prepare(ProcessSpec{sampleRate_, blockSize_, numChannels_});
    buffer_.resize(numChannels_, blockSize_);
    const size_t maxSamples =
        static_cast<size_t>(sampleRate_ * maxRecordSeconds_);
    recorded_.assign(maxSamples, 0.0f);
    recordedCount_ = 0;
    phase_ = 0.0;
    phaseStep_ = kTwoPi * inputFrequencyHz_ / sampleRate_;
    blocksProcessed_.store(0, std::memory_order_relaxed);
    overrunCount_.store(0, std::memory_order_relaxed);
    maxCallbackTimeUs_.store(0.0, std::memory_order_relaxed);

    running_.store(true, std::memory_order_relaxed);
    thread_ = std::thread(&MockAudioDevice::run, this);
    return true;
}

void MockAudioDevice::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void MockAudioDevice::run() {
    const double blockDurationUs =
        (static_cast<double>(blockSize_) / sampleRate_) * 1e6;
    auto nextDeadline = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_relaxed)) {
        const auto t0 = std::chrono::steady_clock::now();
        fillInput();
        processor_.process(buffer_);
        const auto t1 = std::chrono::steady_clock::now();

        const double callbackUs =
            std::chrono::duration<double, std::micro>(t1 - t0).count();
        updateMaxCallbackTime(callbackUs);
        if (callbackUs > blockDurationUs) {
            overrunCount_.fetch_add(1, std::memory_order_relaxed);
        }
        recordOutput();
        blocksProcessed_.fetch_add(1, std::memory_order_relaxed);

        nextDeadline +=
            std::chrono::microseconds(static_cast<long long>(blockDurationUs));
        const auto now = std::chrono::steady_clock::now();
        if (nextDeadline < now) {
            // Tertinggal dari jadwal: jangan kejar ketertinggalan (hindari
            // spiral), lanjut dari sekarang.
            nextDeadline = now;
        }
        // Tunggu sampai deadline: tidur sampai 1 ms sebelum deadline, lalu
        // spin (cek jam rapat-rapat) untuk sisanya. sleep_until saja tidak
        // cukup presisi: di Windows granularitas timer default ~15,6 ms —
        // lebih kasar dari durasi blok (5,33 ms @ 256 sampel/48 kHz) —
        // sehingga tiap iterasi molor, device tertinggal dari real-time, dan
        // jumlah blok dalam jendela 0,5 detik jatuh jauh di bawah ~93
        // (di runner Windows CI: 63 blok, padahal callback hanya 42 us dan
        // 0 overrun — mesin tidak kewalahan, jam tidurnya yang kasar).
        // Spin dibatasi 1 ms per blok dan terjadi DI ANTARA callback, bukan
        // di dalam callback, jadi jalur audio tetap bebas alokasi/blokir.
        // (Temuan CI Windows run 45009915655; perbaikan final verification.)
        const auto spinStart =
            nextDeadline - std::chrono::milliseconds(1);
        if (spinStart > now) {
            std::this_thread::sleep_until(spinStart);
        }
        while (running_.load(std::memory_order_relaxed) &&
               std::chrono::steady_clock::now() < nextDeadline) {
            // Spin rapat: yield() di sini justru bisa melepas thread sampai
            // satu kuantum penjadwal dan mengacaukan deadline.
        }
    }
}

void MockAudioDevice::fillInput() noexcept {
    for (int channel = 0; channel < numChannels_; ++channel) {
        float* data = buffer_.getWritePointer(channel);
        for (int sample = 0; sample < blockSize_; ++sample) {
            data[sample] =
                static_cast<float>(inputAmplitude_ * std::sin(phase_));
            phase_ += phaseStep_;
            if (phase_ >= kTwoPi) {
                phase_ -= kTwoPi;
            }
        }
    }
}

void MockAudioDevice::recordOutput() noexcept {
    if (recordedCount_ >= recorded_.size()) {
        return;  // kapasitas rekam habis: berhenti merekam, tetap memproses
    }
    const float* data = buffer_.getReadPointer(0);
    const size_t remaining = recorded_.size() - recordedCount_;
    const size_t toCopy =
        remaining < static_cast<size_t>(blockSize_)
            ? remaining
            : static_cast<size_t>(blockSize_);
    for (size_t i = 0; i < toCopy; ++i) {
        recorded_[recordedCount_ + i] = data[i];
    }
    recordedCount_ += toCopy;
}

void MockAudioDevice::updateMaxCallbackTime(double callbackUs) noexcept {
    double current = maxCallbackTimeUs_.load(std::memory_order_relaxed);
    while (callbackUs > current &&
           !maxCallbackTimeUs_.compare_exchange_weak(
               current, callbackUs, std::memory_order_relaxed)) {
        // current dimuat ulang oleh compare_exchange_weak; ulangi bila perlu.
    }
}

}  // namespace muse_dsp
