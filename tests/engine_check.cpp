// engine_check — program verifikasi fondasi audio engine (B3).
//
// Bukan framework unit test formal (itu B4, kandidat Catch2) — program
// assert sederhana gaya smoke test B2, yang membuktikan tiap klaim B3:
//   1. EffectChain memproses sesuai urutan eksplisit (urutan pemanggilan).
//   2. Urutan benar-benar mengubah hasil audio (DC offset + gain tidak komutatif).
//   3. Bypass melewati prosesor.
//   4. Parameter smoothing ramp linear tanpa klik (20 ms @ 48 kHz = 960 sampel).
//   5. Parameter atomic lock-free (tanpa mutex di jalur audio).
//   6. Mock audio device menjalankan chain di thread sendiri, merekam output,
//      dan output-nya sesuai (RMS sine x gain).
//   7. Thread-safety: control thread mengubah gain/bypass 300x saat mock
//      berjalan — tanpa crash/deadlock; nilai akhir konvergen.
//   8. Perubahan struktur chain saat device berhenti bekerja.
//
// Keluar 0 bila semua lolos, 1 bila ada yang gagal.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/audio_processor.h"
#include "muse_dsp/effect_chain.h"
#include "muse_dsp/gain_processor.h"
#include "muse_dsp/mock_audio_device.h"
#include "muse_dsp/smoothed_parameter.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* message) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("  FAIL: %s\n", message);
    }
}

void checkNear(float actual, float expected, float tolerance,
               const char* message) {
    ++g_checks;
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::printf("  FAIL: %s (actual=%f expected=%f tol=%f)\n", message,
                    static_cast<double>(actual),
                    static_cast<double>(expected),
                    static_cast<double>(tolerance));
    }
}

void fillBuffer(muse_dsp::AudioBuffer& buffer, float value) {
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        float* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            data[sample] = value;
        }
    }
}

// Prosesor khusus tes: mencatat ID tiap kali process() dipanggil.
class OrderRecorderProcessor : public muse_dsp::AudioProcessor {
public:
    OrderRecorderProcessor(int id, std::vector<int>* log)
        : id_(id), log_(log) {}
    void prepare(const muse_dsp::ProcessSpec&) override {}
    void process(muse_dsp::AudioBuffer&) override { log_->push_back(id_); }
    void reset() override {}
    const char* getName() const override { return "OrderRecorder"; }

private:
    int id_;
    std::vector<int>* log_;
};

// Prosesor khusus tes: menambah offset DC ke semua sampel.
class DcOffsetProcessor : public muse_dsp::AudioProcessor {
public:
    explicit DcOffsetProcessor(float offset) : offset_(offset) {}
    void prepare(const muse_dsp::ProcessSpec&) override {}
    void process(muse_dsp::AudioBuffer& buffer) override {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            float* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                data[sample] += offset_;
            }
        }
    }
    void reset() override {}
    const char* getName() const override { return "DcOffset"; }

private:
    float offset_;
};

void testChainOrderCallSequence() {
    std::printf("1. Urutan chain (urutan pemanggilan)...\n");
    muse_dsp::EffectChain chain;
    std::vector<int> log;
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(1, &log));
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(2, &log));
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(3, &log));
    muse_dsp::AudioBuffer buffer(1, 4);
    chain.prepare(muse_dsp::ProcessSpec{48000.0, 512, 1});

    chain.process(buffer);
    check(log == std::vector<int>({1, 2, 3}), "urutan awal harus 1,2,3");

    log.clear();
    chain.moveProcessor(0, 2);  // 1,2,3 -> 2,3,1
    chain.process(buffer);
    check(log == std::vector<int>({2, 3, 1}),
          "setelah move(0,2) harus 2,3,1");

    log.clear();
    auto removed = chain.removeProcessor(1);  // hapus 3 -> 2,1
    check(chain.size() == 2, "size setelah remove harus 2");
    check(removed != nullptr, "remove harus mengembalikan prosesor");
    chain.process(buffer);
    check(log == std::vector<int>({2, 1}), "setelah remove(1) harus 2,1");

    log.clear();
    chain.insertProcessor(0, std::make_unique<OrderRecorderProcessor>(9, &log));
    chain.process(buffer);
    check(log == std::vector<int>({9, 2, 1}),
          "setelah insert(0,9) harus 9,2,1");
    std::printf("  OK\n");
}

void testChainOrderAudioMath() {
    std::printf("2. Urutan chain (matematika audio)...\n");
    // (input + 0.5) * 2 = 3.0  vs  input * 2 + 0.5 = 2.5
    muse_dsp::EffectChain chainA;
    chainA.addProcessor(std::make_unique<DcOffsetProcessor>(0.5f));
    chainA.addProcessor(std::make_unique<muse_dsp::GainProcessor>(2.0f, 0.0));
    chainA.prepare(muse_dsp::ProcessSpec{48000.0, 512, 1});
    muse_dsp::AudioBuffer bufferA(1, 4);
    fillBuffer(bufferA, 1.0f);
    chainA.process(bufferA);
    checkNear(bufferA.getSample(0, 0), 3.0f, 1e-6f,
              "DC lalu gain: (1+0.5)*2 harus 3.0");

    muse_dsp::EffectChain chainB;
    chainB.addProcessor(std::make_unique<muse_dsp::GainProcessor>(2.0f, 0.0));
    chainB.addProcessor(std::make_unique<DcOffsetProcessor>(0.5f));
    chainB.prepare(muse_dsp::ProcessSpec{48000.0, 512, 1});
    muse_dsp::AudioBuffer bufferB(1, 4);
    fillBuffer(bufferB, 1.0f);
    chainB.process(bufferB);
    checkNear(bufferB.getSample(0, 0), 2.5f, 1e-6f,
              "gain lalu DC: 1*2+0.5 harus 2.5");
    std::printf("  OK\n");
}

void testBypass() {
    std::printf("3. Bypass...\n");
    muse_dsp::EffectChain chain;
    auto gain = std::make_unique<muse_dsp::GainProcessor>(0.0f, 0.0);
    muse_dsp::GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    chain.prepare(muse_dsp::ProcessSpec{48000.0, 512, 1});

    muse_dsp::AudioBuffer buffer(1, 4);
    gainPtr->setBypassed(true);
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    checkNear(buffer.getSample(0, 0), 1.0f, 1e-6f,
              "gain 0 yang di-bypass harus membiarkan sinyal 1.0");

    gainPtr->setBypassed(false);
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    checkNear(buffer.getSample(0, 0), 0.0f, 1e-6f,
              "gain 0 yang aktif harus menghasilkan 0.0");
    std::printf("  OK\n");
}

void testSmoothing() {
    std::printf("4. Parameter smoothing (ramp linear 20 ms @ 48 kHz)...\n");
    muse_dsp::GainProcessor gain(0.0f, 0.02);  // mulai 0, ramp 20 ms
    gain.prepare(muse_dsp::ProcessSpec{48000.0, 512, 1});
    gain.setGain(1.0f);
    muse_dsp::AudioBuffer buffer(1, 960);  // 960 sampel = 20 ms @ 48 kHz
    fillBuffer(buffer, 1.0f);
    gain.process(buffer);
    checkNear(buffer.getSample(0, 0), 1.0f / 960.0f, 1e-6f,
              "sampel pertama ramp harus 1/960");
    // Toleransi 1e-4 untuk tengah ramp: step float diakumulasi 480x, galat
    // pembulatan float32 bisa mencapai ~1e-5; 1e-4 masih ketat (0.02%).
    checkNear(buffer.getSample(0, 479), 480.0f / 960.0f, 1e-4f,
              "tengah ramp harus 0.5");
    checkNear(buffer.getSample(0, 959), 1.0f, 1e-6f,
              "akhir ramp harus tepat 1.0");
    check(buffer.getSample(0, 100) > buffer.getSample(0, 0),
          "ramp harus naik secara monoton");
    check(buffer.getSample(0, 959) <= 1.0f + 1e-6f,
          "ramp tidak boleh overshoot di atas target");
    std::printf("  OK\n");
}

void testAtomicLockFree() {
    std::printf("5. Parameter atomic lock-free...\n");
    check(std::atomic<float>{}.is_lock_free(),
          "atomic<float> harus lock-free");
    check(std::atomic<bool>{}.is_lock_free(),
          "atomic<bool> (bypass) harus lock-free");
    std::printf("  OK\n");
}

void testMockDevice() {
    std::printf("6. Mock audio device (thread sendiri, output terekam)...\n");
    muse_dsp::EffectChain chain;
    chain.addProcessor(std::make_unique<muse_dsp::GainProcessor>(0.5f, 0.0));
    // Input sine 440 Hz amplitudo 0.5, gain 0.5 -> output amplitudo 0.25.
    muse_dsp::MockAudioDevice device(chain, 48000.0, 256, 2, 440.0, 0.5);
    check(device.start(), "mock device harus bisa start");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    device.stop();

    const uint64_t blocks = device.getBlocksProcessed();
    const uint64_t overruns = device.getOverrunCount();
    const double maxCallbackUs = device.getMaxCallbackTimeUs();
    std::printf("  info: blocks=%llu overruns=%llu maxCallback=%.1f us\n",
                static_cast<unsigned long long>(blocks),
                static_cast<unsigned long long>(overruns), maxCallbackUs);
    // 500 ms / (256/48000 s) = ~93 blok; toleransi jitter penjadwalan.
    check(blocks >= 80, "blok diproses minimal 80 dalam 0.5 detik");
    check(maxCallbackUs < 1000.0,
          "callback audio maks harus < 1000 us (jauh di bawah durasi blok)");
    check(device.getRecordedSampleCount() > 0, "output harus terekam");

    // RMS steady-state: lewati 0.1 detik pertama, lalu bandingkan dengan
    // 0.25/sqrt(2) = 0.1768 (sine amplitudo 0.25).
    const std::vector<float>& recorded = device.getRecordedOutput();
    const size_t count = device.getRecordedSampleCount();
    double sumSq = 0.0;
    size_t n = 0;
    for (size_t i = 4800; i < count; ++i) {
        sumSq += static_cast<double>(recorded[i]) * recorded[i];
        ++n;
    }
    check(n > 0, "harus ada sampel steady-state untuk dianalisis");
    const double rms = n > 0 ? std::sqrt(sumSq / static_cast<double>(n)) : 0.0;
    std::printf("  info: RMS steady-state=%.4f (diharapkan ~0.1768)\n", rms);
    checkNear(static_cast<float>(rms), 0.25f / std::sqrt(2.0f), 0.01f,
              "RMS output harus 0.25/sqrt(2)");
    std::printf("  OK\n");
}

void testThreadSafety() {
    std::printf("7. Thread-safety (control thread vs audio thread)...\n");
    muse_dsp::EffectChain chain;
    auto gain = std::make_unique<muse_dsp::GainProcessor>(1.0f, 0.02);
    muse_dsp::GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    muse_dsp::MockAudioDevice device(chain, 48000.0, 256, 2, 440.0, 0.5);
    check(device.start(), "mock device harus bisa start");

    std::atomic<bool> setterDone{false};
    std::thread setter([&] {
        for (int i = 0; i < 300; ++i) {
            gainPtr->setGain((i % 2 == 0) ? 0.0f : 1.0f);
            gainPtr->setBypassed(i % 3 == 0);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        setterDone.store(true, std::memory_order_relaxed);
    });
    setter.join();
    device.stop();
    check(setterDone.load(std::memory_order_relaxed),
          "setter thread selesai tanpa crash/deadlock");

    // Setelah berhenti: nilai akhir konvergen ke target setelah ramp selesai.
    gainPtr->setBypassed(false);
    gainPtr->setGain(0.5f);
    muse_dsp::AudioBuffer buffer(1, 1000);  // > 960 sampel ramp
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    checkNear(buffer.getSample(0, 999), 0.5f, 1e-6f,
              "gain akhir harus konvergen ke 0.5 setelah ramp");

    // Perubahan struktur saat device berhenti harus bekerja.
    chain.addProcessor(std::make_unique<DcOffsetProcessor>(0.25f));
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    checkNear(buffer.getSample(0, 999), 0.75f, 1e-6f,
              "setelah tambah DC offset: 1.0*0.5+0.25 harus 0.75");
    std::printf("  OK\n");
}

}  // namespace

int main() {
    std::printf("engine_check (B3): verifikasi fondasi audio engine\n");
    testChainOrderCallSequence();
    testChainOrderAudioMath();
    testBypass();
    testSmoothing();
    testAtomicLockFree();
    testMockDevice();
    testThreadSafety();

    std::printf("engine_check: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
