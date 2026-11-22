// OverdriveProcessor — implementasi (Phase 2.2).
//
// Waveshaper: tanh — soft clipping simetris (lihat header untuk alasan
// pemilihan). process() diwarisi dari DriveProcessor (oversampling +
// tone + level); di sini hanya waveshaper-nya.

#include "muse_dsp/overdrive_processor.h"

#include <cmath>

namespace muse_dsp {

OverdriveProcessor::OverdriveProcessor(float driveDb, float toneHz,
                                       float levelDb, int oversamplingFactor)
    : DriveProcessor(driveDb, toneHz, levelDb, oversamplingFactor, 0.0f,
                     30.0f, -24.0f, 24.0f) {}

float OverdriveProcessor::waveshape(float x) const noexcept {
    return static_cast<float>(std::tanh(static_cast<double>(x)));
}

}  // namespace muse_dsp
