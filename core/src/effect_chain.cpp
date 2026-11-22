// EffectChain — implementasi (B3).

#include "muse_dsp/effect_chain.h"

#include <algorithm>
#include <cassert>

#include "muse_dsp/audio_buffer.h"

namespace muse_dsp {

void EffectChain::prepare(const ProcessSpec& spec) {
    spec_ = spec;
    prepared_ = true;
    for (const auto& processor : processors_) {
        processor->prepare(spec);
    }
}

void EffectChain::process(AudioBuffer& buffer) {
    // Tidak ada alokasi di sini: hanya iterasi dan pemanggilan process().
    for (const auto& processor : processors_) {
        if (!processor->isBypassed()) {
            processor->process(buffer);
        }
    }
}

void EffectChain::reset() {
    for (const auto& processor : processors_) {
        processor->reset();
    }
}

void EffectChain::addProcessor(std::unique_ptr<AudioProcessor> processor) {
    assert(processor != nullptr);
    if (prepared_) {
        processor->prepare(spec_);
    }
    processors_.push_back(std::move(processor));
}

void EffectChain::insertProcessor(size_t index,
                                  std::unique_ptr<AudioProcessor> processor) {
    assert(processor != nullptr);
    assert(index <= processors_.size());
    if (prepared_) {
        processor->prepare(spec_);
    }
    processors_.insert(processors_.begin() + static_cast<long>(index),
                       std::move(processor));
}

std::unique_ptr<AudioProcessor> EffectChain::removeProcessor(size_t index) {
    assert(index < processors_.size());
    auto it = processors_.begin() + static_cast<long>(index);
    std::unique_ptr<AudioProcessor> removed = std::move(*it);
    processors_.erase(it);
    return removed;
}

void EffectChain::moveProcessor(size_t fromIndex, size_t toIndex) {
    assert(fromIndex < processors_.size());
    assert(toIndex < processors_.size());
    if (fromIndex == toIndex) {
        return;
    }
    std::unique_ptr<AudioProcessor> moved = std::move(processors_[fromIndex]);
    processors_.erase(processors_.begin() + static_cast<long>(fromIndex));
    processors_.insert(processors_.begin() + static_cast<long>(toIndex),
                       std::move(moved));
}

void EffectChain::clear() {
    processors_.clear();
}

AudioProcessor* EffectChain::getProcessor(size_t index) const {
    assert(index < processors_.size());
    return processors_[index].get();
}

}  // namespace muse_dsp
