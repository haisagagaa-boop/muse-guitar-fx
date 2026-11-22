// effect_chain_tests — unit test formal EffectChain (B4, Catch2 v3.7.1).
//
// Mencakup semua butir pengujian B4 untuk EffectChain:
//   - bypass dan urutan pemrosesan,
//   - perubahan parameter (termasuk di tengah render),
//   - beberapa ukuran buffer yang relevan (64/128/256/512/1024),
//   - chain kosong dan reset.
//
// Verifikasi B3 (engine_check) tetap dipertahankan sebagai program terpisah;
// suite ini adalah lapisan unit test formalnya.

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <catch_amalgamated.hpp>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/effect_chain.h"
#include "muse_dsp/gain_processor.h"
#include "test_processors.h"

using namespace muse_dsp;
using test::DcOffsetProcessor;
using test::OrderRecorderProcessor;

namespace {

constexpr double kSampleRate = 48000.0;

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
// channel 0. Parameter boleh diubah di tengah render lewat callback
// `onBlock(blockIndex, samplesDone)` — dipanggil sebelum tiap blok.
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
        // Chain memproses buffer sebesar blockSize; hanya `count` sampel
        // pertama yang valid — sisanya padding nol yang ikut diproses
        // tetapi tidak disalin ke output. Padding tidak memengaruhi
        // sampel valid karena semua prosesor B3 bekerja per-sampel.
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

}  // namespace

TEST_CASE("EffectChain processes processors in explicit order") {
    EffectChain chain;
    std::vector<int> log;
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(1, &log));
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(2, &log));
    chain.addProcessor(std::make_unique<OrderRecorderProcessor>(3, &log));
    AudioBuffer buffer(1, 4);
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    chain.process(buffer);
    CHECK(log == std::vector<int>({1, 2, 3}));

    log.clear();
    chain.moveProcessor(0, 2);  // 1,2,3 -> 2,3,1
    chain.process(buffer);
    CHECK(log == std::vector<int>({2, 3, 1}));

    log.clear();
    auto removed = chain.removeProcessor(1);  // hapus 3 -> 2,1
    REQUIRE(removed != nullptr);
    CHECK(chain.size() == 2);
    chain.process(buffer);
    CHECK(log == std::vector<int>({2, 1}));

    log.clear();
    chain.insertProcessor(0,
                          std::make_unique<OrderRecorderProcessor>(9, &log));
    chain.process(buffer);
    CHECK(log == std::vector<int>({9, 2, 1}));

    chain.clear();
    CHECK(chain.empty());
    CHECK(chain.size() == 0);
}

TEST_CASE("EffectChain order changes the audio result") {
    // (input + 0.5) * 2 = 3.0 vs input * 2 + 0.5 = 2.5
    EffectChain chainA;
    chainA.addProcessor(std::make_unique<DcOffsetProcessor>(0.5f));
    chainA.addProcessor(std::make_unique<GainProcessor>(2.0f, 0.0));
    chainA.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer bufferA(1, 4);
    fillBuffer(bufferA, 1.0f);
    chainA.process(bufferA);
    CHECK(bufferA.getSample(0, 0) == Catch::Approx(3.0f).margin(1e-6f));

    EffectChain chainB;
    chainB.addProcessor(std::make_unique<GainProcessor>(2.0f, 0.0));
    chainB.addProcessor(std::make_unique<DcOffsetProcessor>(0.5f));
    chainB.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer bufferB(1, 4);
    fillBuffer(bufferB, 1.0f);
    chainB.process(bufferB);
    CHECK(bufferB.getSample(0, 0) == Catch::Approx(2.5f).margin(1e-6f));
}

TEST_CASE("Bypass skips a processor inside the chain") {
    EffectChain chain;
    auto gain = std::make_unique<GainProcessor>(0.0f, 0.0);
    GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    chain.addProcessor(std::make_unique<DcOffsetProcessor>(0.25f));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    AudioBuffer buffer(1, 4);
    gainPtr->setBypassed(true);
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    // Gain 0 di-bypass, hanya DC offset yang bekerja: 1.0 + 0.25.
    CHECK(buffer.getSample(0, 0) == Catch::Approx(1.25f).margin(1e-6f));

    gainPtr->setBypassed(false);
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    // Gain 0 aktif: 1.0 * 0 + 0.25.
    CHECK(buffer.getSample(0, 0) == Catch::Approx(0.25f).margin(1e-6f));
}

TEST_CASE("Empty chain passes audio through unchanged") {
    EffectChain chain;
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});
    AudioBuffer buffer(2, 8);
    fillBuffer(buffer, 0.75f);
    chain.process(buffer);
    CHECK(buffer.getSample(0, 0) == Catch::Approx(0.75f).margin(1e-6f));
    CHECK(buffer.getSample(1, 7) == Catch::Approx(0.75f).margin(1e-6f));
}

TEST_CASE("Parameter changes ramp smoothly") {
    GainProcessor gain(0.0f, 0.02);  // mulai 0, ramp 20 ms
    gain.prepare(ProcessSpec{kSampleRate, 512, 1});
    gain.setGain(1.0f);
    AudioBuffer buffer(1, 960);  // 960 sampel = 20 ms @ 48 kHz
    fillBuffer(buffer, 1.0f);
    gain.process(buffer);
    CHECK(buffer.getSample(0, 0) == Catch::Approx(1.0f / 960.0f).margin(1e-6f));
    // Toleransi tengah ramp 1e-4: step float diakumulasi 480x (lihat B3).
    CHECK(buffer.getSample(0, 479) == Catch::Approx(0.5f).margin(1e-4f));
    CHECK(buffer.getSample(0, 959) == Catch::Approx(1.0f).margin(1e-6f));
    CHECK(buffer.getSample(0, 100) > buffer.getSample(0, 0));  // monoton naik
    CHECK(buffer.getSample(0, 959) <= 1.0f + 1e-6f);           // tanpa overshoot
}

TEST_CASE("Parameter changes mid-render take effect after the ramp") {
    EffectChain chain;
    auto gain = std::make_unique<GainProcessor>(0.5f, 0.02);
    GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    AudioBuffer input(1, 2048);
    fillBuffer(input, 1.0f);
    // Set gain ke 1.0 tepat setelah 1024 sampel dirender.
    auto output = renderChain(
        chain, input, 256,
        [&](int /*blockIndex*/, int samplesDone) {
            if (samplesDone == 1024) {
                gainPtr->setGain(1.0f);
            }
        });
    // Paruh pertama: gain 0.5 (tanpa ramp — nilai awal = target).
    CHECK(output[0] == Catch::Approx(0.5f).margin(1e-6f));
    CHECK(output[1023] == Catch::Approx(0.5f).margin(1e-6f));
    // Ramp 960 sampel setelah perubahan: sampel ke-1024 baru mulai naik,
    // sampel terakhir (2047) sudah sampai di 1.0 (1024 sampel > 960).
    CHECK(output[1024] > 0.5f);
    CHECK(output[1024] < 0.6f);
    CHECK(output[2047] == Catch::Approx(1.0f).margin(1e-6f));
}

TEST_CASE("Output is identical for all relevant block sizes") {
    // Chain: DC offset + Gain dengan perubahan target di tengah render.
    // Smoothing bekerja per-sampel, jadi output harus identik apapun
    // ukuran bloknya (64/128/256/512/1024).
    const std::vector<int> blockSizes = {64, 128, 256, 512, 1024};
    AudioBuffer input(1, 4800);  // 0,1 detik sine 440 Hz
    fillSine(input, 440.0, 0.5f);

    std::vector<std::vector<float>> outputs;
    for (int blockSize : blockSizes) {
        EffectChain chain;
        chain.addProcessor(std::make_unique<DcOffsetProcessor>(0.1f));
        auto gain = std::make_unique<GainProcessor>(0.5f, 0.02);
        GainProcessor* gainPtr = gain.get();
        chain.addProcessor(std::move(gain));
        chain.prepare(ProcessSpec{kSampleRate, 1024, 1});
        // Perubahan target di sampel 2048 — kelipatan semua ukuran blok
        // (64/128/256/512/1024) sehingga terjadi di posisi yang sama
        // apapun ukuran bloknya.
        outputs.push_back(renderChain(
            chain, input, blockSize,
            [&](int /*blockIndex*/, int samplesDone) {
                if (samplesDone == 2048) {
                    gainPtr->setGain(1.0f);
                }
            }));
    }

    const auto& reference = outputs[0];
    for (size_t i = 1; i < outputs.size(); ++i) {
        REQUIRE(outputs[i].size() == reference.size());
        float maxDiff = 0.0f;
        for (size_t s = 0; s < reference.size(); ++s) {
            maxDiff = std::max(maxDiff, std::fabs(outputs[i][s] - reference[s]));
        }
        INFO("blockSize " << blockSizes[i] << " vs " << blockSizes[0]
                          << ", maxDiff=" << maxDiff);
        CHECK(maxDiff < 1e-6f);
    }
}

TEST_CASE("Chain reset restores processor state") {
    EffectChain chain;
    auto gain = std::make_unique<GainProcessor>(1.0f, 0.02);
    GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    chain.prepare(ProcessSpec{kSampleRate, 512, 1});

    gainPtr->setGain(0.0f);
    AudioBuffer buffer(1, 100);
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);  // ramp 0 -> 1..0 mulai berjalan
    CHECK(buffer.getSample(0, 99) < 1.0f);

    chain.reset();  // snap kembali ke gain awal 1.0
    fillBuffer(buffer, 1.0f);
    chain.process(buffer);
    CHECK(buffer.getSample(0, 0) == Catch::Approx(1.0f).margin(1e-6f));
}
