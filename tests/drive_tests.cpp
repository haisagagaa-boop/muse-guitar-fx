// drive_tests — unit test formal Overdrive + Distortion (Phase 2.2,
// Catch2 v3.7.1).
//
// Mencakup semua butir pengujian 2.2:
//   - Overdrive: THD naik monoton sesuai drive; fundamental terjaga
//     pada drive rendah; (hampir) tanpa harmonik genap (simetris);
//     tone control; bypass.
//   - Distortion: THD naik monoton sesuai drive; THD ~ 0 pada drive
//     rendah (di bawah ambang clip); harmonik genap jelas hadir
//     (asimetris); bypass.
//   - Aliasing: dengan oversampling AKTIF (4x), komponen alias
//     teredam; dengan oversampling MATI (1x), aliasing jelas lebih
//     besar — perbandingan terukur pada sinyal uji yang sama.
//   - Umum: kesamaan output di semua ukuran blok (64/128/256/512/1024),
//     perubahan drive di tengah render, parameter ekstrem tanpa
//     NaN/inf + tanpa DC tak terkendali, reset, thread-safety dengan
//     MockAudioDevice.
//
// Pengukuran spektrum memakai algoritma Goertzel (DFT satu bin, tanpa
// dependency FFT): magnitudo pada frekuensi yang diminta, diukur pada
// jendela 38.400 sampel (0,8 detik) yang memuat bilangan bulat periode
// untuk semua frekuensi ukur (kelipatan 1 kHz @ 48 kHz) sehingga
// tanpa leakage. Semua uji deterministik offline.
// TIDAK ada uji dengar — tidak ada perangkat audio di VM.
//
// Catatan: THD dihitung dari harmonik 2..8 fundamental 1 kHz.
// Ambang aliasing ditetapkan dari pengukuran empiris (program ukur
// /tmp/drive_measure.cpp, lalu dihapus setelah 2.2):
//   - Distorsi drive 24 dB: ON (4x) = -44,9 dB, OFF (1x) = -14,1 dB
//     -> ambang ON < -40 dB, OFF > -20 dB, selisih >= 15 dB.
//   - Distorsi drive 60 dB (maks): ON = -25,8 dB, OFF = -14,1 dB
//     -> ambang ON < -20 dB, OFF > -20 dB, selisih >= 10 dB.
//   - Overdrive drive 30 dB (maks): ON < -40 dB (terukur ~-55 dB).
// Mengapa aliasing distorsi memburuk dengan drive: hard clipper
// menghasilkan harmonik ~1/n; harmonik di atas Nyquist rate 4x
// (96 kHz) melipat DI DALAM domain 4x ke frekuensi rendah (mis.
// harmonik ke-37/39 dari 5 kHz -> 7/3 kHz) SEBELUM filter
// downsample sempat bekerja — filter hanya bisa meredam komponen
// yang memang berada di atas 24 kHz dalam domain 4x. Ini batas
// fisika oversampling 4x, bukan cacat implementasi: saturator halus
// (x/(1+|x|^8)^(1/8)) diukur IDENTIK pada drive 48 dB (-25,81 dB),
// karena pada drive dalam outputnya sama-sama mendekati gelombang
// dua level. Oversampling 8x akan memperbaikinya, di luar cakupan
// 2.2 (target desain 4x) — lihat laporan 2.2 bagian F.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <catch_amalgamated.hpp>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/distortion_processor.h"
#include "muse_dsp/effect_chain.h"
#include "muse_dsp/mock_audio_device.h"
#include "muse_dsp/overdrive_processor.h"

using namespace muse_dsp;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// Jendela ukur steady-state: mulai sampel 9600 (0,2 dtk — smoothing
// 20 ms + delay FIR oversampling sudah settle), panjang 38.400 sampel
// (0,8 dtk = 800 periode @ 1 kHz, 4.000 periode @ 5 kHz).
constexpr int kMeasureStart = 9600;
constexpr int kMeasureCount = 38400;

float dbFromLinear(float linear) {
    return linear > 0.0f ? 20.0f * std::log10(linear) : -120.0f;
}

void fillBuffer(AudioBuffer& buffer, float value) {
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        float* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            data[sample] = value;
        }
    }
}

void fillSine(AudioBuffer& buffer, double frequencyHz, float amplitude,
              double startPhase = 0.0) {
    const double phaseStep = 2.0 * kPi * frequencyHz / kSampleRate;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        float* data = buffer.getWritePointer(channel);
        double phase = startPhase;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            data[sample] =
                static_cast<float>(amplitude * std::sin(phase));
            phase += phaseStep;
        }
    }
}

// Render seluruh input lewat chain dengan ukuran blok tertentu, kembalikan
// channel 0. Sama polanya dengan dynamics_tests (2.1): perubahan
// parameter di tengah render lewat `onBlock(blockIndex, samplesDone)` —
// dipanggil sebelum tiap blok. Perubahan di sampel 2048 (kelipatan semua
// ukuran blok) terjadi di posisi yang sama apapun ukuran bloknya.
std::vector<float> renderChain(EffectChain& chain, const AudioBuffer& input,
                               int blockSize,
                               const std::function<void(int, int)>& onBlock = nullptr) {
    const int totalSamples = input.getNumSamples();
    const int numChannels = input.getNumChannels();
    std::vector<float> output(static_cast<size_t>(totalSamples), 0.0f);
    AudioBuffer block(numChannels, blockSize);
    int samplesDone = 0;
    int blockIndex = 0;
    while (samplesDone < totalSamples) {
        const int count = std::min(blockSize, totalSamples - samplesDone);
        if (onBlock) {
            onBlock(blockIndex, samplesDone);
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            const float* in = input.getReadPointer(channel);
            float* out = block.getWritePointer(channel);
            for (int i = 0; i < count; ++i) {
                out[i] = in[samplesDone + i];
            }
            for (int i = count; i < blockSize; ++i) {
                out[i] = 0.0f;
            }
        }
        chain.process(block);
        const float* out = block.getReadPointer(0);
        for (int i = 0; i < count; ++i) {
            output[static_cast<size_t>(samplesDone + i)] = out[i];
        }
        samplesDone += count;
        ++blockIndex;
    }
    return output;
}

// Magnitudo (amplitudo sine) pada frekuensi tertentu, diukur dengan
// algoritma Goertzel pada [start, start+count). Jendela harus memuat
// bilangan bulat periode pada frekuensi itu (tanpa leakage).
double goertzelMagnitude(const std::vector<float>& samples, double freqHz,
                         int start, int count) {
    const double w = 2.0 * kPi * freqHz / kSampleRate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < count; ++i) {
        const double s0 = samples[static_cast<size_t>(start + i)] +
                          coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double re = s1 - s2 * std::cos(w);
    const double im = s2 * std::sin(w);
    return 2.0 * std::sqrt(re * re + im * im) / count;
}

// THD (rasio linear, 0..~1) dari fundamental: akar jumlah kuadrat
// harmonik 2..8 dibagi fundamental. fundamentalHz = 1000 Hz.
// start/count default = jendela ukur steady-state (lihat atas).
double thd(const std::vector<float>& output, double fundamentalHz = 1000.0,
           int start = kMeasureStart, int count = kMeasureCount) {
    const double fund = goertzelMagnitude(output, fundamentalHz, start, count);
    double harmSq = 0.0;
    for (int k = 2; k <= 8; ++k) {
        const double mag =
            goertzelMagnitude(output, k * fundamentalHz, start, count);
        harmSq += mag * mag;
    }
    return std::sqrt(harmSq) / fund;
}

// Rasio energi alias terhadap energi harmonik (dB) untuk sine 5 kHz.
// Harmonik benar di bawah Nyquist: 5, 10, 15, 20 kHz. Alias (lipatan
// harmonik > 24 kHz): 1, 3, 7, 11, 13, 17, 21, 23 kHz.
double aliasRatioDb(const std::vector<float>& output) {
    const double aliasFreqs[] = {1000.0, 3000.0, 7000.0, 11000.0,
                                 13000.0, 17000.0, 21000.0, 23000.0};
    const double harmFreqs[] = {5000.0, 10000.0, 15000.0, 20000.0};
    double aliasSq = 0.0, harmSq = 0.0;
    for (double f : aliasFreqs) {
        const double mag =
            goertzelMagnitude(output, f, kMeasureStart, kMeasureCount);
        aliasSq += mag * mag;
    }
    for (double f : harmFreqs) {
        const double mag =
            goertzelMagnitude(output, f, kMeasureStart, kMeasureCount);
        harmSq += mag * mag;
    }
    return 20.0 * std::log10(std::sqrt(aliasSq) / std::sqrt(harmSq));
}

// Peak |x| pada rentang [start, start+count) dari output channel 0.
float peakInRange(const std::vector<float>& samples, int start, int count) {
    float peak = 0.0f;
    const int end = std::min(start + count,
                             static_cast<int>(samples.size()));
    for (int i = start; i < end; ++i) {
        peak = std::max(peak, std::fabs(samples[static_cast<size_t>(i)]));
    }
    return peak;
}

// Rata-rata nilai pada rentang (untuk sinyal DC = nilai DC itu sendiri).
float meanInRange(const std::vector<float>& samples, int start, int count) {
    double sum = 0.0;
    const int end = std::min(start + count,
                             static_cast<int>(samples.size()));
    for (int i = start; i < end; ++i) {
        sum += samples[static_cast<size_t>(i)];
    }
    return static_cast<float>(sum / (end - start));
}

}  // namespace

// ---------------------------------------------------------------------------
// Overdrive
// ---------------------------------------------------------------------------

TEST_CASE("Overdrive THD rises monotonically with drive") {
    // Sine 1 kHz, 0,5. Tone 8000 Hz (maks) agar filter tone tidak
    // menekan harmonik yang diukur. Drive 0..30 dB.
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    const float drives[] = {0.0f, 6.0f, 12.0f, 18.0f, 24.0f, 30.0f};
    double prevThd = -1.0;
    double firstThd = 0.0;
    double lastThd = 0.0;
    for (size_t i = 0; i < 6; ++i) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<OverdriveProcessor>(
            drives[i], 8000.0f, 0.0f, 4));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        auto output = renderChain(chain, input, 256);
        const double t = thd(output);
        INFO("Overdrive drive=" << drives[i] << " dB, THD=" << t * 100.0
                                << "%");
        if (i == 0) {
            firstThd = t;
        }
        if (i == 5) {
            lastThd = t;
        }
        if (prevThd >= 0.0) {
            // Monoton naik (toleransi 5% relatif untuk noise float).
            CHECK(t > prevThd * 0.95);
        }
        prevThd = t;
    }
    INFO("Overdrive THD drive 0 dB = " << firstThd * 100.0
                                       << "%, drive 30 dB = "
                                       << lastThd * 100.0 << "%");
    // Pada drive rendah hampir bersih; pada drive maks jauh lebih besar.
    CHECK(firstThd < 0.05);
    CHECK(lastThd > firstThd * 10.0);
    CHECK(lastThd > 0.20);  // mendekati gelombang kotak (~48%)
}

TEST_CASE("Overdrive preserves the fundamental at low drive") {
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<OverdriveProcessor>(0.0f, 8000.0f, 0.0f, 4));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    auto output = renderChain(chain, input, 256);
    const double fund =
        goertzelMagnitude(output, 1000.0, kMeasureStart, kMeasureCount);
    INFO("Overdrive fundamental at drive 0 dB = " << fund << " (input 0,5)");
    // tanh(0,5*sin) sedikit menekan fundamental: ~0,47; toleransi ±2 dB.
    CHECK(fund == Catch::Approx(0.5).margin(0.12));
    // Simetris: tidak ada offset DC.
    const float dc = meanInRange(output, kMeasureStart, kMeasureCount);
    INFO("Overdrive DC offset = " << dc);
    CHECK(std::fabs(dc) < 0.01f);
}

TEST_CASE("Overdrive produces almost no even harmonics") {
    // tanh simetris -> harmonik genap (2, 4 kHz) harus ~tidak ada.
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<OverdriveProcessor>(30.0f, 8000.0f, 0.0f, 4));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    auto output = renderChain(chain, input, 256);
    const double fund =
        goertzelMagnitude(output, 1000.0, kMeasureStart, kMeasureCount);
    const double second =
        goertzelMagnitude(output, 2000.0, kMeasureStart, kMeasureCount);
    const double fourth =
        goertzelMagnitude(output, 4000.0, kMeasureStart, kMeasureCount);
    const double secondDb = 20.0 * std::log10(second / fund);
    const double fourthDb = 20.0 * std::log10(fourth / fund);
    INFO("Overdrive 2nd harmonic = " << secondDb << " dB rel. fundamental");
    INFO("Overdrive 4th harmonic = " << fourthDb << " dB rel. fundamental");
    CHECK(secondDb < -50.0);
    CHECK(fourthDb < -50.0);
}

TEST_CASE("Overdrive tone control changes the treble") {
    // Sine 8 kHz, 0,3, drive 0 dB: tone 500 Hz harus menekan jauh
    // lebih kuat daripada tone 8000 Hz (lowpass satu-pole).
    const auto measureAtTone = [](float toneHz) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<OverdriveProcessor>(
            0.0f, toneHz, 0.0f, 4));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        AudioBuffer input(1, 48000);
        fillSine(input, 8000.0, 0.3f);
        auto output = renderChain(chain, input, 256);
        return goertzelMagnitude(output, 8000.0, kMeasureStart,
                                 kMeasureCount);
    };
    const double magLow = measureAtTone(500.0f);
    const double magHigh = measureAtTone(8000.0f);
    const double diffDb = 20.0 * std::log10(magHigh / magLow);
    INFO("Overdrive tone 500 Hz: mag(8 kHz)=" << magLow
                                              << ", tone 8000 Hz: "
                                              << magHigh << ", selisih "
                                              << diffDb << " dB");
    CHECK(diffDb > 10.0);
}

TEST_CASE("Overdrive bypass passes audio unchanged") {
    EffectChain chain;
    auto od = std::make_unique<OverdriveProcessor>(30.0f, 500.0f, 0.0f, 4);
    OverdriveProcessor* odPtr = od.get();
    chain.addProcessor(std::move(od));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    odPtr->setBypassed(true);
    auto output = renderChain(chain, input, 256);
    const double fund =
        goertzelMagnitude(output, 1000.0, kMeasureStart, kMeasureCount);
    CHECK(fund == Catch::Approx(0.5).margin(1e-6));
}

// ---------------------------------------------------------------------------
// Distortion
// ---------------------------------------------------------------------------

TEST_CASE("Distortion THD rises monotonically with drive") {
    // Sine 1 kHz, 0,5. Tone 8000 Hz. Drive 0..60 dB.
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    const float drives[] = {0.0f, 12.0f, 24.0f, 36.0f, 48.0f, 60.0f};
    double prevThd = -1.0;
    double firstThd = 0.0;
    double lastThd = 0.0;
    for (size_t i = 0; i < 6; ++i) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<DistortionProcessor>(
            drives[i], 8000.0f, 0.0f, 4));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        auto output = renderChain(chain, input, 256);
        const double t = thd(output);
        INFO("Distortion drive=" << drives[i] << " dB, THD=" << t * 100.0
                                << "%");
        if (i == 0) {
            firstThd = t;
        }
        if (i == 5) {
            lastThd = t;
        }
        if (prevThd >= 0.0) {
            CHECK(t > prevThd * 0.95);
        }
        prevThd = t;
    }
    INFO("Distortion THD drive 0 dB = " << firstThd * 100.0
                                        << "%, drive 60 dB = "
                                        << lastThd * 100.0 << "%");
    // Di bawah ambang clip (drive 0 dB, input 0,5 < 0,65): hampir
    // tidak ada distorsi sama sekali — berbeda dari tanh overdrive.
    CHECK(firstThd < 0.005);
    CHECK(lastThd > 0.20);
}

TEST_CASE("Distortion produces even harmonics") {
    // Clip asimetris (+1,0/-0,65) -> harmonik ke-2 jelas hadir,
    // berbeda dari overdrive simetris. Diukur pada drive 12 dB
    // (terukur -21,9 dB rel. fundamental): harmonik genap MEMUNCAK
    // pada drive moderat — di sanalah asimetri transfer function
    // berada di daerah aktif. Pada drive ekstrem (48-60 dB, terukur
    // -145 dB) output mendekati gelombang dua level duty 50% yang
    // harmonik genapnya ~0 untuk WAVESHAPER saturasi apa pun (bahkan
    // saturator halus terukur sama) — jadi mengukur harmonik genap
    // pada drive maks adalah tes yang salah, bukan implementasi
    // yang salah. Terukur: 6 dB -> -19,8 dB; 12 dB -> -21,9 dB;
    // 24 dB -> -33,2 dB; 36 dB -> -43,4 dB; 48 dB -> -145 dB.
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<DistortionProcessor>(12.0f, 8000.0f, 0.0f, 4));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    auto output = renderChain(chain, input, 256);
    const double fund =
        goertzelMagnitude(output, 1000.0, kMeasureStart, kMeasureCount);
    const double second =
        goertzelMagnitude(output, 2000.0, kMeasureStart, kMeasureCount);
    const double secondDb = 20.0 * std::log10(second / fund);
    INFO("Distortion 2nd harmonic = " << secondDb
                                      << " dB rel. fundamental");
    CHECK(secondDb > -30.0);
}

TEST_CASE("Distortion bypass passes audio unchanged") {
    EffectChain chain;
    auto dist =
        std::make_unique<DistortionProcessor>(60.0f, 500.0f, 0.0f, 4);
    DistortionProcessor* distPtr = dist.get();
    chain.addProcessor(std::move(dist));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    distPtr->setBypassed(true);
    auto output = renderChain(chain, input, 256);
    const double fund =
        goertzelMagnitude(output, 1000.0, kMeasureStart, kMeasureCount);
    CHECK(fund == Catch::Approx(0.5).margin(1e-6));
}

// ---------------------------------------------------------------------------
// Aliasing: oversampling ON vs OFF
// ---------------------------------------------------------------------------

TEST_CASE("Oversampling suppresses aliasing (distortion, worst case)") {
    // Sine 5 kHz, 0,5, drive 24 dB (hard clipping kuat). Tone 8000 Hz.
    // Harmonik di atas Nyquist (25, 35, ... kHz) melipat ke 23, 13,
    // ... kHz bila tidak difilter sebelum decimate.
    // Terukur: ON (4x) = -44,9 dB, OFF (1x) = -14,1 dB, selisih 30,8 dB.
    AudioBuffer input(1, 48000);
    fillSine(input, 5000.0, 0.5f);

    const auto measureAlias = [&](int factor) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<DistortionProcessor>(
            24.0f, 8000.0f, 0.0f, factor));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        auto output = renderChain(chain, input, 256);
        return aliasRatioDb(output);
    };

    const double ratioOn = measureAlias(4);
    const double ratioOff = measureAlias(1);
    INFO("Distortion alias ratio oversampling 4x (ON) = " << ratioOn
                                                          << " dB");
    INFO("Distortion alias ratio oversampling 1x (OFF) = " << ratioOff
                                                           << " dB");
    // Ambang (lihat header): ON < -40 dB, OFF > -20 dB, selisih >= 15 dB.
    CHECK(ratioOn < -40.0);
    CHECK(ratioOff > -20.0);
    CHECK(ratioOff - ratioOn >= 15.0);
}

TEST_CASE("Oversampling suppresses aliasing (distortion, max drive)") {
    // Kasus terburuk: drive 60 dB (maks). Pada drive ini aliasing
    // 4x terukur -25,8 dB — masih 26 dB di bawah fundamental (tidak
    // dominan) dan 11,7 dB lebih baik dari 1x (-14,1 dB), tetapi
    // tidak mencapai -40 dB karena lipatan di dalam domain 4x
    // (lihat catatan di header file). Ambang di sini MENJEPIT
    // perilaku worst-case secara jujur, bukan menyembunyikannya:
    // bila implementasi memburuk (mis. filter rusak), tes ini gagal.
    AudioBuffer input(1, 48000);
    fillSine(input, 5000.0, 0.5f);

    const auto measureAlias = [&](int factor) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<DistortionProcessor>(
            60.0f, 8000.0f, 0.0f, factor));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        auto output = renderChain(chain, input, 256);
        return aliasRatioDb(output);
    };

    const double ratioOn = measureAlias(4);
    const double ratioOff = measureAlias(1);
    INFO("Distortion alias ratio (60 dB) oversampling 4x (ON) = "
         << ratioOn << " dB");
    INFO("Distortion alias ratio (60 dB) oversampling 1x (OFF) = "
         << ratioOff << " dB");
    CHECK(ratioOn < -20.0);
    CHECK(ratioOff > -20.0);
    CHECK(ratioOff - ratioOn >= 10.0);
}

TEST_CASE("Oversampling suppresses aliasing (overdrive)") {
    // Sine 5 kHz, 0,5, drive 30 dB (maks overdrive). Tone 8000 Hz.
    AudioBuffer input(1, 48000);
    fillSine(input, 5000.0, 0.5f);
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<OverdriveProcessor>(30.0f, 8000.0f, 0.0f, 4));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    auto output = renderChain(chain, input, 256);
    const double ratioOn = aliasRatioDb(output);
    INFO("Overdrive alias ratio oversampling 4x (ON) = " << ratioOn
                                                         << " dB");
    CHECK(ratioOn < -40.0);
}

// ---------------------------------------------------------------------------
// Umum: ukuran blok, perubahan parameter, ekstrem, reset, thread-safety
// ---------------------------------------------------------------------------

TEST_CASE("Drive output is identical for all relevant block sizes") {
    // Chain Overdrive + Distortion, sine 440 Hz, perubahan drive
    // overdrive di sampel 2048 (kelipatan semua ukuran blok).
    const std::vector<int> blockSizes = {64, 128, 256, 512, 1024};
    AudioBuffer input(1, 4800);  // 0,1 detik sine 440 Hz, 0,5
    fillSine(input, 440.0, 0.5f);

    std::vector<std::vector<float>> outputs;
    for (int blockSize : blockSizes) {
        EffectChain chain;
        auto od = std::make_unique<OverdriveProcessor>(12.0f, 4000.0f, 0.0f, 4);
        OverdriveProcessor* odPtr = od.get();
        chain.addProcessor(std::move(od));
        chain.addProcessor(
            std::make_unique<DistortionProcessor>(24.0f, 4000.0f, 0.0f, 4));
        chain.prepare(ProcessSpec{kSampleRate, 1024, 1});
        outputs.push_back(renderChain(
            chain, input, blockSize,
            [&](int /*blockIndex*/, int samplesDone) {
                if (samplesDone == 2048) {
                    odPtr->setDriveDb(24.0f);
                }
            }));
    }

    const auto& reference = outputs[0];
    for (size_t i = 1; i < outputs.size(); ++i) {
        REQUIRE(outputs[i].size() == reference.size());
        float maxDiff = 0.0f;
        for (size_t s = 0; s < reference.size(); ++s) {
            maxDiff =
                std::max(maxDiff, std::fabs(outputs[i][s] - reference[s]));
        }
        INFO("blockSize " << blockSizes[i] << " vs " << blockSizes[0]
                          << ", maxDiff=" << maxDiff);
        CHECK(maxDiff < 1e-6f);
    }
}

TEST_CASE("Overdrive drive change mid-render changes THD") {
    // Sine 1 kHz, 0,5. Drive 6 dB -> 24 dB di sampel 2048.
    // (2048 = kelipatan ukuran blok 256, agar callback onBlock
    // benar-benar terpicu — lihat pelajaran B4 soal sampel 2048.)
    EffectChain chain;
    auto od = std::make_unique<OverdriveProcessor>(6.0f, 8000.0f, 0.0f, 4);
    OverdriveProcessor* odPtr = od.get();
    chain.addProcessor(std::move(od));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.5f);
    auto output = renderChain(
        chain, input, 256,
        [&](int /*blockIndex*/, int samplesDone) {
            if (samplesDone == 2048) {
                odPtr->setDriveDb(24.0f);
            }
        });
    // Sebelum perubahan (sampel 128-2048 = 1.920 sampel = 40 periode
    // @ 1 kHz, bilangan bulat sehingga tanpa leakage): THD drive 6 dB.
    // (Sampel 0-127 dilewati: settling FIR oversampling. Jendela HARUS
    // berakhir di 2048 — perubahan drive terjadi di sampel 2048,
    // jendela yang melewatinya (draf awal: 480-4320) mengukur segmen
    // non-stasioner dan menggelembungkan THD "sebelum" menjadi 11%.)
    const double thdBefore = thd(output, 1000.0, 128, 1920);
    // Sesudah perubahan + settle (0,2-0,28 dtk): THD drive 24 dB.
    // (Smoothing drive 20 ms sudah lama selesai.)
    const double thdAfter = thd(output, 1000.0, 9600, 3840);
    INFO("THD before drive change = " << thdBefore * 100.0 << "%");
    INFO("THD after drive change = " << thdAfter * 100.0 << "%");
    CHECK(thdAfter > thdBefore * 3.0);
}

TEST_CASE("Drive parameters at extremes do not produce NaN, inf, or DC") {
    // Overdrive (30 dB / tone 500 / level +24 dB) + Distortion
    // (60 dB / tone 8000 / level +24 dB).
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<OverdriveProcessor>(30.0f, 500.0f, 24.0f, 4));
    chain.addProcessor(
        std::make_unique<DistortionProcessor>(60.0f, 8000.0f, 24.0f, 4));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    // Sinyal keras (sine 0,9) 1 detik + sunyi 0,5 detik (jalur
    // flush-to-zero tone filter + DC blocker).
    AudioBuffer input(1, 72000);
    fillSine(input, 1000.0, 0.9f);
    for (int i = 48000; i < 72000; ++i) {
        input.getWritePointer(0)[i] = 0.0f;
    }
    auto output = renderChain(chain, input, 256);
    float maxAbs = 0.0f;
    for (float v : output) {
        CHECK(std::isfinite(v));
        maxAbs = std::max(maxAbs, std::fabs(v));
    }
    INFO("max |output| at extremes = " << maxAbs);
    // Batas diturunkan dari desain, bukan dari angka terukur: clip
    // membatasi tiap efek ke |y| <= 1,0, lalu level +24 dB
    // (x15,85) -> maks |out| ~15,85; ambang 20 memberi margin untuk
    // overshoot transien tone filter. (Draf awal 10,0 salah: level
    // +24 dB pada DUA efek seri memang mengizinkan ~15,85.)
    CHECK(maxAbs < 20.0f);
    // Bagian sunyi harus tetap nol (state ter-flush).
    CHECK(peakInRange(output, 67200, 4800) < 1e-6f);
    // Tidak ada offset DC tak terkendali (DC blocker 20 Hz):
    // rata-rata 0,2 detik terakhir bagian keras < 0,05.
    const float dc = meanInRange(output, 38400, 9600);
    INFO("DC offset at extremes = " << dc);
    CHECK(std::fabs(dc) < 0.05f);
}

TEST_CASE("Drive processors reset to a fresh state") {
    // Overdrive: diproses keras -> reset -> sunyi harus tetap nol
    // (state tone filter + DC blocker bersih seperti baru).
    OverdriveProcessor od(30.0f, 500.0f, 0.0f, 4);
    od.prepare(ProcessSpec{kSampleRate, 512, 1});
    {
        AudioBuffer loud(1, 24000);
        fillSine(loud, 1000.0, 0.9f);
        od.process(loud);
        CHECK(peakInRange(std::vector<float>(loud.getReadPointer(0),
                                             loud.getReadPointer(0) + 24000),
                          19200, 4800) > 0.1f);
    }
    od.reset();
    {
        AudioBuffer quiet(1, 48000);
        // Sunyi penuh: output harus tetap nol.
        od.process(quiet);
        CHECK(peakInRange(std::vector<float>(quiet.getReadPointer(0),
                                             quiet.getReadPointer(0) + 48000),
                          0, 48000) < 1e-6f);
    }

    // Distortion: diproses keras -> reset -> drive kembali ke nilai
    // awal (24 dB) dan sunyi tetap nol.
    DistortionProcessor dist(24.0f, 4000.0f, 0.0f, 4);
    dist.prepare(ProcessSpec{kSampleRate, 512, 1});
    {
        AudioBuffer loud(1, 24000);
        fillSine(loud, 1000.0, 0.9f);
        dist.process(loud);
    }
    dist.setDriveDb(60.0f);
    dist.reset();
    CHECK(dist.getDriveDb() == Catch::Approx(24.0f));
    {
        AudioBuffer quiet(1, 48000);
        dist.process(quiet);
        CHECK(peakInRange(std::vector<float>(quiet.getReadPointer(0),
                                             quiet.getReadPointer(0) + 48000),
                          0, 48000) < 1e-6f);
    }
}

TEST_CASE("Drive parameters can change from the control thread while "
          "the mock device runs") {
    // Pola dynamics_tests (2.1) test 14: mock device menjalankan chain
    // [Overdrive, Distortion] di thread-nya sendiri sementara test
    // thread mengubah parameter 200x. Tidak boleh crash/deadlock;
    // nilai akhir harus konvergen ke target terakhir.
    EffectChain chain;
    auto od = std::make_unique<OverdriveProcessor>(12.0f, 4000.0f, 0.0f, 4);
    OverdriveProcessor* odPtr = od.get();
    auto dist =
        std::make_unique<DistortionProcessor>(24.0f, 4000.0f, 0.0f, 4);
    DistortionProcessor* distPtr = dist.get();
    chain.addProcessor(std::move(od));
    chain.addProcessor(std::move(dist));

    MockAudioDevice device(chain, kSampleRate, 256, 2, 440.0, 0.5);
    REQUIRE(device.start());
    for (int i = 0; i < 200; ++i) {
        odPtr->setDriveDb((i % 2 == 0) ? 6.0f : 24.0f);
        odPtr->setToneHz((i % 2 == 0) ? 1000.0f : 6000.0f);
        distPtr->setDriveDb((i % 2 == 0) ? 12.0f : 48.0f);
        distPtr->setLevelDb((i % 2 == 0) ? -6.0f : 6.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // Nilai akhir yang deterministik (setelah loop).
    odPtr->setDriveDb(18.0f);
    odPtr->setToneHz(3000.0f);
    distPtr->setDriveDb(36.0f);
    distPtr->setLevelDb(0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    device.stop();

    CHECK(odPtr->getDriveDb() == Catch::Approx(18.0f));
    CHECK(odPtr->getToneHz() == Catch::Approx(3000.0f));
    CHECK(distPtr->getDriveDb() == Catch::Approx(36.0f));
    CHECK(distPtr->getLevelDb() == Catch::Approx(0.0f));
    CHECK(device.getRecordedSampleCount() > 0);
    // Output rekaman harus finite (tidak ada NaN/inf dari perubahan
    // parameter saat berjalan).
    const auto& recorded = device.getRecordedOutput();
    const size_t count = device.getRecordedSampleCount();
    for (size_t i = 0; i < count; i += 100) {
        CHECK(std::isfinite(recorded[i]));
    }
}
