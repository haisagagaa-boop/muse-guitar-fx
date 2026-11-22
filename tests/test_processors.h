// test_processors.h — prosesor khusus tes (B3/B4).
//
// Dipakai engine_check.cpp dan effect_chain_tests.cpp. BUKAN bagian dari
// core: hanya untuk pengujian.

#pragma once

#include <vector>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/audio_processor.h"

namespace test {

// Mencatat ID tiap kali process() dipanggil — untuk menguji urutan chain.
class OrderRecorderProcessor : public muse_dsp::AudioProcessor {
public:
    OrderRecorderProcessor(int id, std::vector<int>* log) : id_(id), log_(log) {}
    void prepare(const muse_dsp::ProcessSpec&) override {}
    void process(muse_dsp::AudioBuffer&) override { log_->push_back(id_); }
    void reset() override {}
    const char* getName() const override { return "OrderRecorder"; }

private:
    int id_;
    std::vector<int>* log_;
};

// Menambah offset DC ke semua sampel — untuk menguji bahwa urutan prosesor
// benar-benar mengubah hasil audio (DC + gain tidak komutatif).
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

}  // namespace test
