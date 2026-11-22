// DistortionProcessor — implementasi (Phase 2.2).
//
// Waveshaper: hard clip asimetris (lihat header). process() diwarisi
// dari DriveProcessor (oversampling + tone + level); di sini hanya
// waveshaper-nya.

#include "muse_dsp/distortion_processor.h"

#include <algorithm>

namespace muse_dsp {

namespace {

// Ambang jepit: positif +1,0, negatif -0,65 (asimetris — lihat header).
constexpr float kClipPositive = 1.0f;
constexpr float kClipNegative = -0.65f;

}  // namespace

DistortionProcessor::DistortionProcessor(float driveDb, float toneHz,
                                         float levelDb, int oversamplingFactor)
    : DriveProcessor(driveDb, toneHz, levelDb, oversamplingFactor, 0.0f,
                     60.0f, -24.0f, 24.0f) {}

float DistortionProcessor::waveshape(float x) const noexcept {
    return std::max(kClipNegative, std::min(kClipPositive, x));
}

}  // namespace muse_dsp
