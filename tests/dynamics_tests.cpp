// dynamics_tests — unit test formal NoiseGate + Compressor (Phase 2.1,
// Catch2 v3.7.1).
//
// Mencakup semua butir pengujian 2.1:
//   - Noise Gate: atenuasi >= 40 dB di bawah threshold, tanpa chattering
//     (sinyal steady, sinyal dekat threshold, on/off bergantian), bypass.
//   - Compressor: gain reduction sesuai rasio +/-1 dB, attack/release
//     mengikuti konstanta waktu, soft knee, tanpa pumping berlebihan,
//     bypass + makeup gain.
//   - Umum: kesamaan output di semua ukuran blok (64/128/256/512/1024),
//     perubahan parameter di tengah render, parameter ekstrem tanpa
//     NaN/inf, reset, thread-safety dengan MockAudioDevice.
//
// Semua uji deterministik offline (render sinyal uji, ukur dari output).
// TIDAK ada uji dengar — tidak ada perangkat audio di VM.
//
// Catatan pengukuran: sinyal DC dipakai bila GR harus diukur per-sampel
// secara eksak (envelope peak DC = nilai DC itu sendiri); sine 1000 Hz
// dipakai bila level diukur dari peak (periode tepat 48 sampel @ 48 kHz).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <catch_amalgamated.hpp>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/compressor_processor.h"
#include "muse_dsp/effect_chain.h"
#include "muse_dsp/mock_audio_device.h"
#include "muse_dsp/noise_gate_processor.h"

using namespace muse_dsp;

namespace {

constexpr double kSampleRate = 48000.0;

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
    const double phaseStep = 2.0 * 3.14159265358979323846 * frequencyHz /
                             kSampleRate;
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
// channel 0. Sama polanya dengan effect_chain_tests (B4): perubahan
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

// Gain reduction (dB) dari level input dan output (keduanya linear, > 0).
float gainReductionDb(float inLevel, float outLevel) {
    return dbFromLinear(inLevel) - dbFromLinear(outLevel);
}

}  // namespace

// ---------------------------------------------------------------------------
// Noise Gate
// ---------------------------------------------------------------------------

TEST_CASE("NoiseGate attenuates signals below threshold by at least 40 dB") {
    // Sine -50 dB, threshold -40 dB, range -80 dB (default).
    AudioBuffer input(1, 48000);  // 1 detik sine 1000 Hz, 0,00316 (-50 dB)
    fillSine(input, 1000.0, 0.00316f);
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 100.0f, -80.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    auto output = renderChain(chain, input, 256);
    // Ukur 0,2 detik terakhir (gate sudah lama tertutup).
    const float outPeak = peakInRange(output, 38400, 9600);
    const float attenuation = gainReductionDb(0.00316f, outPeak);
    INFO("attenuation=" << attenuation << " dB, outPeak=" << outPeak);
    CHECK(attenuation >= 40.0f);
    // Harapan: mendekati range 80 dB (toleransi longgar karena float).
    CHECK(attenuation >= 70.0f);

    // Varian range -60 dB: atenuasi harus ~60 dB (+/-2 dB).
    EffectChain chain60;
    chain60.addProcessor(
        std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 100.0f, -60.0f));
    chain60.prepare(ProcessSpec{kSampleRate, 512, 1});
    auto output60 = renderChain(chain60, input, 256);
    const float att60 =
        gainReductionDb(0.00316f, peakInRange(output60, 38400, 9600));
    INFO("attenuation range -60 dB: " << att60 << " dB");
    CHECK(att60 == Catch::Approx(60.0f).margin(2.0f));
}

TEST_CASE("NoiseGate passes signals above threshold unchanged") {
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 100.0f, -80.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);  // 1 detik sine 1000 Hz, 0,1 (-20 dB)
    fillSine(input, 1000.0, 0.1f);
    auto output = renderChain(chain, input, 256);
    // 0,2 detik terakhir: gate sudah lama terbuka, gain 0 dB.
    const float outPeak = peakInRange(output, 38400, 9600);
    INFO("outPeak=" << outPeak << " (input 0,1)");
    CHECK(outPeak == Catch::Approx(0.1f).margin(0.006f));  // +/-0,5 dB
}

TEST_CASE("NoiseGate does not chatter on a signal near the threshold") {
    // Sine tepat 1 dB di bawah threshold (-41 dB vs threshold -40 dB):
    // hysteresis 3 dB membuat gate tetap tertutup — tidak boleh ada
    // toggling buka/tutup (chattering) selama 1 detik penuh.
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 100.0f, -80.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);  // sine 1000 Hz, 0,00891 (-41 dB)
    fillSine(input, 1000.0, 0.00891f);
    auto output = renderChain(chain, input, 256);
    // 10 jendela 50 ms dalam 0,5 detik terakhir: semuanya harus
    // teratenuasi >= 40 dB DAN stabil antar-jendela (< 1 dB spread).
    float minAtt = 1e9f;
    float maxAtt = -1e9f;
    for (int w = 0; w < 10; ++w) {
        const float peak = peakInRange(output, 24000 + w * 2400, 2400);
        const float att = gainReductionDb(0.00891f, peak);
        minAtt = std::min(minAtt, att);
        maxAtt = std::max(maxAtt, att);
    }
    INFO("attenuation min=" << minAtt << " dB, max=" << maxAtt << " dB");
    CHECK(minAtt >= 40.0f);
    CHECK(maxAtt - minAtt < 1.0f);  // stabil: tidak ada toggling
}

TEST_CASE("NoiseGate opens and closes cleanly on alternating signal") {
    // 4 siklus: 400 ms ON (-20 dB) / 400 ms OFF (-60 dB). Release 50 ms.
    // Gate harus membuka penuh di tiap periode ON dan menutup penuh di
    // tiap periode OFF, tanpa membuka kembali di tengah periode OFF
    // (chattering) — level tiap jendela dalam satu periode OFF harus
    // monoton turun.
    EffectChain chain;
    chain.addProcessor(
        std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 50.0f, -80.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    const int totalSamples = 4 * 2 * 19200;  // 4 siklus x 800 ms
    AudioBuffer input(1, totalSamples);
    for (int cycle = 0; cycle < 4; ++cycle) {
        // ON: sine 1000 Hz, 0,1 (-20 dB), 400 ms.
        AudioBuffer on(1, 19200);
        fillSine(on, 1000.0, 0.1f);
        for (int i = 0; i < 19200; ++i) {
            input.getWritePointer(0)[cycle * 38400 + i] = on.getSample(0, i);
        }
        // OFF: sine 1000 Hz, 0,001 (-60 dB), 400 ms.
        AudioBuffer off(1, 19200);
        fillSine(off, 1000.0, 0.001f);
        for (int i = 0; i < 19200; ++i) {
            input.getWritePointer(0)[cycle * 38400 + 19200 + i] =
                off.getSample(0, i);
        }
    }

    auto output = renderChain(chain, input, 256);
    for (int cycle = 0; cycle < 4; ++cycle) {
        const int onStart = cycle * 38400;
        const int offStart = onStart + 19200;
        // 100 ms terakhir periode ON: gate terbuka penuh, output = input.
        const float onPeak = peakInRange(output, onStart + 14400, 4800);
        INFO("cycle " << cycle << " ON peak=" << onPeak);
        CHECK(onPeak == Catch::Approx(0.1f).margin(0.006f));
        // 100 ms terakhir periode OFF: atenuasi >= 40 dB dari -60 dB.
        const float offPeak = peakInRange(output, offStart + 14400, 4800);
        const float offAtt = gainReductionDb(0.001f, offPeak);
        INFO("cycle " << cycle << " OFF attenuation=" << offAtt << " dB");
        CHECK(offAtt >= 40.0f);
        // Monoton turun dalam periode OFF: 8 jendela 50 ms.
        float prevPeak = 1e9f;
        for (int w = 0; w < 8; ++w) {
            const float peak = peakInRange(output, offStart + w * 2400, 2400);
            INFO("cycle " << cycle << " OFF window " << w
                          << " peak=" << peak);
            // Toleransi 5% untuk noise float; tidak boleh naik kembali
            // (itu tanda gate membuka lagi = chattering).
            CHECK(peak <= prevPeak * 1.05f + 1e-9f);
            prevPeak = peak;
        }
    }
}

TEST_CASE("NoiseGate bypass passes audio unchanged") {
    EffectChain chain;
    auto gate = std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 100.0f,
                                                     -80.0f);
    NoiseGateProcessor* gatePtr = gate.get();
    chain.addProcessor(std::move(gate));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.00316f);  // -50 dB (di bawah threshold)
    gatePtr->setBypassed(true);
    auto output = renderChain(chain, input, 256);
    const float outPeak = peakInRange(output, 38400, 9600);
    CHECK(outPeak == Catch::Approx(0.00316f).margin(1e-6f));
}

// ---------------------------------------------------------------------------
// Compressor
// ---------------------------------------------------------------------------

TEST_CASE("Compressor applies gain reduction matching the ratio") {
    // DC -10 dB, threshold -20 dB, hard knee (0 dB), makeup 0 dB.
    // GR = (level - threshold) * (1 - 1/ratio).
    const auto measureGr = [](float ratio) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<CompressorProcessor>(
            -20.0f, ratio, 10.0f, 100.0f, 0.0f, 0.0f));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        AudioBuffer input(1, 48000);
        fillBuffer(input, 0.31623f);  // DC -10 dB
        auto output = renderChain(chain, input, 256);
        const float outMean = meanInRange(output, 38400, 9600);
        return gainReductionDb(0.31623f, outMean);
    };

    // Ratio 4:1 -> GR = 10 * 0,75 = 7,5 dB (+/-1 dB).
    const float gr4 = measureGr(4.0f);
    INFO("GR ratio 4:1 = " << gr4 << " dB");
    CHECK(gr4 == Catch::Approx(7.5f).margin(1.0f));
    // Ratio 2:1 -> GR = 10 * 0,5 = 5 dB (+/-1 dB).
    const float gr2 = measureGr(2.0f);
    INFO("GR ratio 2:1 = " << gr2 << " dB");
    CHECK(gr2 == Catch::Approx(5.0f).margin(1.0f));

    // Di bawah threshold: tidak ada reduksi (DC -30 dB).
    EffectChain chain;
    chain.addProcessor(std::make_unique<CompressorProcessor>(
        -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillBuffer(input, 0.03162f);  // DC -30 dB
    auto output = renderChain(chain, input, 256);
    const float grBelow =
        gainReductionDb(0.03162f, meanInRange(output, 38400, 9600));
    INFO("GR below threshold = " << grBelow << " dB");
    CHECK(grBelow == Catch::Approx(0.0f).margin(0.5f));
}

TEST_CASE("Compressor soft knee smooths the transition") {
    // Threshold -20 dB, ratio 4:1, knee 12 dB.
    //   -26 dB (awal knee): GR = 0.
    //   -20 dB (tengah knee): GR = 0,75 * 6^2 / 24 = 1,125 dB.
    //   -14 dB (akhir knee): GR = 0,75 * 6 = 4,5 dB.
    const auto measureGrAtLevel = [](float levelDb) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<CompressorProcessor>(
            -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 12.0f));
        chain.prepare(ProcessSpec{kSampleRate, 512, 1});
        const float level =
            static_cast<float>(std::pow(10.0, levelDb * 0.05));
        AudioBuffer input(1, 48000);
        fillBuffer(input, level);
        auto output = renderChain(chain, input, 256);
        return gainReductionDb(level, meanInRange(output, 38400, 9600));
    };

    const float grLow = measureGrAtLevel(-26.0f);
    INFO("GR at -26 dB (knee start) = " << grLow << " dB");
    CHECK(grLow == Catch::Approx(0.0f).margin(0.5f));
    const float grMid = measureGrAtLevel(-20.0f);
    INFO("GR at -20 dB (knee middle) = " << grMid << " dB");
    CHECK(grMid == Catch::Approx(1.125f).margin(0.5f));
    const float grHigh = measureGrAtLevel(-14.0f);
    INFO("GR at -14 dB (knee end) = " << grHigh << " dB");
    CHECK(grHigh == Catch::Approx(4.5f).margin(0.5f));
}

TEST_CASE("Compressor attack and release follow the time constants") {
    // DC bertahap (satu-pole): attack 10 ms, release 100 ms.
    //   Naik -60 dB -> -10 dB di sampel 48000 (t = 1 dtk).
    //   Turun -10 dB -> -60 dB di sampel 96000 (t = 2 dtk).
    // GR akhir (steady, -10 dB, T=-20, R=4, knee 0) = 7,5 dB.
    EffectChain chain;
    chain.addProcessor(std::make_unique<CompressorProcessor>(
        -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 168000);  // 3,5 detik
    for (int i = 0; i < 48000; ++i) {
        input.getWritePointer(0)[i] = 0.001f;  // -60 dB
    }
    for (int i = 48000; i < 96000; ++i) {
        input.getWritePointer(0)[i] = 0.31623f;  // -10 dB
    }
    for (int i = 96000; i < 168000; ++i) {
        input.getWritePointer(0)[i] = 0.001f;  // -60 dB
    }
    auto output = renderChain(chain, input, 256);
    // GR di sampel tertentu, diukur terhadap level input yang berlaku
    // pada sampel itu (0,31623 saat -10 dB; 0,001 saat -60 dB).
    const auto grAt = [&](int sample, float inLevel) {
        return gainReductionDb(inLevel, output[static_cast<size_t>(sample)]);
    };

    // Attack: setelah 1 konstanta waktu (10 ms = 480 sampel), envelope
    // mencapai ~63% langkah -> GR ~60% dari akhir. Setelah 5 tau, >= 95%.
    const float grAttack1 = grAt(48000 + 480, 0.31623f);
    INFO("GR 1 attack-tau after step up = " << grAttack1 << " dB");
    CHECK(grAttack1 >= 7.5f * 0.4f);
    CHECK(grAttack1 <= 7.5f * 0.8f);
    const float grAttack5 = grAt(48000 + 2400, 0.31623f);
    INFO("GR 5 attack-tau after step up = " << grAttack5 << " dB");
    CHECK(grAttack5 >= 7.5f * 0.95f);

    // Release: setelah 1 tau (100 ms = 4800 sampel), GR tinggal ~14%
    // dari awal; setelah 5 tau, <= 5%.
    const float grRelease1 = grAt(96000 + 4800, 0.001f);
    INFO("GR 1 release-tau after step down = " << grRelease1 << " dB");
    CHECK(grRelease1 <= 7.5f * 0.4f);
    const float grRelease5 = grAt(96000 + 24000, 0.001f);
    INFO("GR 5 release-tau after step down = " << grRelease5 << " dB");
    CHECK(grRelease5 <= 7.5f * 0.05f);
}

TEST_CASE("Compressor does not pump on steady signals") {
    // Sine 1000 Hz steady -10 dB selama 1 detik: GR harus stabil
    // (variasi < 0,5 dB antar-jendela) — tidak ada osilasi gain.
    EffectChain chain;
    chain.addProcessor(std::make_unique<CompressorProcessor>(
        -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.31623f);  // -10 dB peak
    auto output = renderChain(chain, input, 256);
    float minGr = 1e9f;
    float maxGr = -1e9f;
    for (int w = 0; w < 10; ++w) {
        const float peak = peakInRange(output, 24000 + w * 2400, 2400);
        const float gr = gainReductionDb(0.31623f, peak);
        minGr = std::min(minGr, gr);
        maxGr = std::max(maxGr, gr);
    }
    INFO("steady GR min=" << minGr << " dB, max=" << maxGr << " dB");
    CHECK(maxGr - minGr < 0.5f);
    // GR absolut pada sine (6,12 dB) memang di bawah rumus peak (7,5 dB):
    // envelope satu-pole dengan attack 10 ms mengikuti puncak sine yang
    // telah dihaluskan, bukan puncak sesaat — itu perilaku yang benar
    // untuk topologi ini. Akurasi rumus GR diverifikasi eksak dengan DC
    // di test "applies gain reduction matching the ratio". Di sini yang
    // diuji adalah stabilitas (tidak ada pumping), dan kompresi aktif.
    CHECK(minGr > 0.0f);

    // Release monoton: setelah step down, GR hanya boleh turun
    // (tidak ada osilasi naik-turun = pumping).
    EffectChain chain2;
    chain2.addProcessor(std::make_unique<CompressorProcessor>(
        -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f));
    chain2.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input2(1, 168000);
    for (int i = 0; i < 96000; ++i) {
        input2.getWritePointer(0)[i] = 0.31623f;  // -10 dB
    }
    for (int i = 96000; i < 168000; ++i) {
        input2.getWritePointer(0)[i] = 0.001f;  // -60 dB
    }
    auto output2 = renderChain(chain2, input2, 256);
    float prevGr = 1e9f;
    for (int step = 0; step < 50; ++step) {
        const int sample = 96000 + step * 480;  // tiap 10 ms
        // Referensi = level input saat itu (0,001 = -60 dB).
        const float gr = gainReductionDb(
            0.001f, output2[static_cast<size_t>(sample)]);
        INFO("release step " << step << " GR=" << gr << " dB");
        // Toleransi 0,05 dB untuk noise float; GR tidak boleh naik kembali.
        CHECK(gr <= prevGr + 0.05f);
        prevGr = gr;
    }
}

TEST_CASE("Compressor bypass passes audio unchanged and makeup gain works") {
    // Bypass: sinyal di atas threshold lewat tanpa perubahan.
    EffectChain chain;
    auto comp = std::make_unique<CompressorProcessor>(-20.0f, 4.0f, 10.0f,
                                                      100.0f, 0.0f, 0.0f);
    CompressorProcessor* compPtr = comp.get();
    chain.addProcessor(std::move(comp));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillSine(input, 1000.0, 0.31623f);  // -10 dB
    compPtr->setBypassed(true);
    auto output = renderChain(chain, input, 256);
    CHECK(peakInRange(output, 38400, 9600) ==
          Catch::Approx(0.31623f).margin(1e-6f));

    // Makeup gain +6 dB di bawah threshold (GR = 0): output = input x 2.
    EffectChain chain2;
    chain2.addProcessor(std::make_unique<CompressorProcessor>(
        -20.0f, 4.0f, 10.0f, 100.0f, 6.0f, 0.0f));
    chain2.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input2(1, 48000);
    fillSine(input2, 1000.0, 0.03162f);  // -30 dB, di bawah threshold
    auto output2 = renderChain(chain2, input2, 256);
    const float peak2 = peakInRange(output2, 38400, 9600);
    INFO("makeup +6 dB: out peak=" << peak2 << " (input 0,03162)");
    CHECK(peak2 == Catch::Approx(0.06324f).margin(0.003f));  // +/-0,5 dB
}

// ---------------------------------------------------------------------------
// Umum: ukuran blok, perubahan parameter, ekstrem, reset, thread-safety
// ---------------------------------------------------------------------------

TEST_CASE("Dynamics output is identical for all relevant block sizes") {
    // Chain Gate + Compressor, sine 440 Hz, perubahan threshold
    // kompresor di sampel 2048 (kelipatan semua ukuran blok).
    const std::vector<int> blockSizes = {64, 128, 256, 512, 1024};
    AudioBuffer input(1, 4800);  // 0,1 detik sine 440 Hz, 0,5
    fillSine(input, 440.0, 0.5f);

    std::vector<std::vector<float>> outputs;
    for (int blockSize : blockSizes) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<NoiseGateProcessor>(
            -40.0f, 1.0f, 50.0f, -80.0f));
        auto comp = std::make_unique<CompressorProcessor>(
            -20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 6.0f);
        CompressorProcessor* compPtr = comp.get();
        chain.addProcessor(std::move(comp));
        chain.prepare(ProcessSpec{kSampleRate, 1024, 1});
        outputs.push_back(renderChain(
            chain, input, blockSize,
            [&](int /*blockIndex*/, int samplesDone) {
                if (samplesDone == 2048) {
                    compPtr->setThresholdDb(-40.0f);
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

TEST_CASE("Compressor parameter changes mid-render take effect") {
    // DC -10 dB; threshold -20 dB -> -40 dB di sampel 24064.
    // (24064 = 94 x 256: kelipatan ukuran blok 256, agar callback
    // onBlock benar-benar terpicu — lihat pelajaran B4 soal sampel 2048.)
    // GR: 7,5 dB -> 22,5 dB (knee 0, ratio 4).
    EffectChain chain;
    auto comp = std::make_unique<CompressorProcessor>(-20.0f, 4.0f, 10.0f,
                                                      100.0f, 0.0f, 0.0f);
    CompressorProcessor* compPtr = comp.get();
    chain.addProcessor(std::move(comp));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer input(1, 48000);
    fillBuffer(input, 0.31623f);  // DC -10 dB
    auto output = renderChain(
        chain, input, 256,
        [&](int /*blockIndex*/, int samplesDone) {
            if (samplesDone == 24064) {
                compPtr->setThresholdDb(-40.0f);
            }
        });
    // Sebelum perubahan (0,4-0,5 detik): GR ~7,5 dB.
    const float grBefore =
        gainReductionDb(0.31623f, meanInRange(output, 19200, 4800));
    INFO("GR before threshold change = " << grBefore << " dB");
    CHECK(grBefore == Catch::Approx(7.5f).margin(1.0f));
    // Sesudah perubahan + settle (0,9-1,0 detik): GR ~22,5 dB.
    // (Smoothing threshold 20 ms sudah lama selesai.)
    const float grAfter =
        gainReductionDb(0.31623f, meanInRange(output, 43200, 4800));
    INFO("GR after threshold change = " << grAfter << " dB");
    CHECK(grAfter == Catch::Approx(22.5f).margin(1.0f));
}

TEST_CASE("Dynamics parameters at extremes do not produce NaN or inf") {
    // Gate: threshold -100 dB, attack 0,01 ms, release 5000 ms,
    // range -100 dB. Comp: threshold -60 dB, ratio 20:1, attack 0,01 ms,
    // release 5000 ms, makeup +24 dB, knee 20 dB.
    EffectChain chain;
    chain.addProcessor(std::make_unique<NoiseGateProcessor>(
        -100.0f, 0.01f, 5000.0f, -100.0f));
    chain.addProcessor(std::make_unique<CompressorProcessor>(
        -60.0f, 20.0f, 0.01f, 5000.0f, 24.0f, 20.0f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    // Sinyal keras (sine 0,9) 1 detik + sunyi 0,5 detik (jalur FTZ).
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
    CHECK(maxAbs < 10.0f);
    // Bagian sunyi harus tetap nol (gate tertutup, envelope ter-flush).
    CHECK(peakInRange(output, 67200, 4800) < 1e-6f);
}

TEST_CASE("Dynamics processors reset to a fresh state") {
    // Gate: diproses keras (terbuka) -> reset -> sinyal sunyi harus
    // tertutup seperti baru (atenuasi >= 40 dB).
    NoiseGateProcessor gate(-40.0f, 1.0f, 100.0f, -80.0f);
    gate.prepare(ProcessSpec{kSampleRate, 512, 1});
    {
        AudioBuffer loud(1, 24000);
        fillSine(loud, 1000.0, 0.1f);
        gate.process(loud);
        CHECK(gate.isOpen());
    }
    gate.reset();
    {
        AudioBuffer quiet(1, 48000);
        fillSine(quiet, 1000.0, 0.00316f);  // -50 dB
        gate.process(quiet);
        // 0,2 detik terakhir: gate tertutup seperti baru.
        const float peak = peakInRange(
            std::vector<float>(quiet.getReadPointer(0),
                               quiet.getReadPointer(0) + 48000),
            38400, 9600);
        CHECK(gainReductionDb(0.00316f, peak) >= 40.0f);
    }

    // Compressor: diproses keras (GR ~7,5 dB) -> reset -> sampel pertama
    // setelah reset tidak ada reduksi (envelope nol), lalu GR kembali
    // ke 7,5 dB dalam steady state.
    CompressorProcessor comp(-20.0f, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f);
    comp.prepare(ProcessSpec{kSampleRate, 512, 1});
    {
        AudioBuffer loud(1, 24000);
        fillBuffer(loud, 0.31623f);
        comp.process(loud);
        CHECK(comp.getCurrentGainReductionDb() > 5.0f);
    }
    comp.reset();
    {
        AudioBuffer dc(1, 48000);
        fillBuffer(dc, 0.31623f);
        comp.process(dc);
        // Sampel pertama: envelope masih nol -> GR ~0.
        CHECK(dc.getSample(0, 0) ==
              Catch::Approx(0.31623f).margin(0.01f));
        // Steady state (0,2 detik terakhir): GR kembali ~7,5 dB.
        const float gr = gainReductionDb(
            0.31623f, meanInRange(
                          std::vector<float>(dc.getReadPointer(0),
                                             dc.getReadPointer(0) + 48000),
                          38400, 9600));
        CHECK(gr == Catch::Approx(7.5f).margin(1.0f));
    }
}

TEST_CASE("Dynamics parameters can change from the control thread while "
          "the mock device runs") {
    // Pola engine_check (B3) check 7: mock device menjalankan chain
    // [Gate, Compressor] di thread-nya sendiri sementara test thread
    // mengubah parameter 200x. Tidak boleh crash/deadlock; nilai akhir
    // harus konvergen ke target terakhir.
    EffectChain chain;
    auto gate = std::make_unique<NoiseGateProcessor>(-40.0f, 1.0f, 50.0f,
                                                     -80.0f);
    NoiseGateProcessor* gatePtr = gate.get();
    auto comp = std::make_unique<CompressorProcessor>(-20.0f, 4.0f, 10.0f,
                                                      100.0f, 0.0f, 6.0f);
    CompressorProcessor* compPtr = comp.get();
    chain.addProcessor(std::move(gate));
    chain.addProcessor(std::move(comp));

    MockAudioDevice device(chain, kSampleRate, 256, 2, 440.0, 0.5);
    REQUIRE(device.start());
    for (int i = 0; i < 200; ++i) {
        gatePtr->setThresholdDb((i % 2 == 0) ? -50.0f : -30.0f);
        compPtr->setRatio((i % 2 == 0) ? 2.0f : 8.0f);
        compPtr->setThresholdDb((i % 2 == 0) ? -30.0f : -10.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // Nilai akhir yang deterministik (setelah loop).
    gatePtr->setThresholdDb(-35.0f);
    compPtr->setRatio(6.0f);
    compPtr->setThresholdDb(-25.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    device.stop();

    CHECK(gatePtr->getThresholdDb() == Catch::Approx(-35.0f));
    CHECK(compPtr->getRatio() == Catch::Approx(6.0f));
    CHECK(compPtr->getThresholdDb() == Catch::Approx(-25.0f));
    CHECK(device.getRecordedSampleCount() > 0);
    // Output rekaman harus finite (tidak ada NaN/inf dari perubahan
    // parameter saat berjalan).
    const auto& recorded = device.getRecordedOutput();
    const size_t count = device.getRecordedSampleCount();
    for (size_t i = 0; i < count; i += 100) {
        CHECK(std::isfinite(recorded[i]));
    }
}
