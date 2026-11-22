// EffectChain — sumber kebenaran urutan efek (B3).
//
// Chain adalah AudioProcessor: ia memproses buffer dengan menjalankan tiap
// prosesor secara berurutan, dalam urutan eksplisit yang disimpan di vector.
// Reorder (moveProcessor) benar-benar mengubah urutan pemrosesan.
//
// Kontrak thread:
// - process(): bebas alokasi (iterasi vector saja), bebas mutex, bebas file I/O.
// - add/insert/remove/move/clear dan prepare()/reset(): TIDAK real-time safe —
//   hanya dilakukan saat audio thread berhenti. Perubahan parameter dan bypass
//   tetap aman kapan pun (atomic).

#pragma once

#include <memory>
#include <vector>

#include "muse_dsp/audio_processor.h"

namespace muse_dsp {

class EffectChain : public AudioProcessor {
public:
    void prepare(const ProcessSpec& spec) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;
    const char* getName() const override { return "EffectChain"; }

    // Perubahan struktur — hanya saat audio thread berhenti.
    void addProcessor(std::unique_ptr<AudioProcessor> processor);
    void insertProcessor(size_t index, std::unique_ptr<AudioProcessor> processor);
    std::unique_ptr<AudioProcessor> removeProcessor(size_t index);
    void moveProcessor(size_t fromIndex, size_t toIndex);
    void clear();

    AudioProcessor* getProcessor(size_t index) const;
    size_t size() const noexcept { return processors_.size(); }
    bool empty() const noexcept { return processors_.empty(); }

private:
    std::vector<std::unique_ptr<AudioProcessor>> processors_;
    ProcessSpec spec_;
    bool prepared_ = false;
};

}  // namespace muse_dsp
