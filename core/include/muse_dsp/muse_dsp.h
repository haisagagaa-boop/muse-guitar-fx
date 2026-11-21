// muse_dsp — shared DSP core (JUCE-independent).
//
// B2 (Phase 1): fondasi build saja — API versi. EffectChain, parameter
// thread-safe, dan efek DSP dikerjakan di B3 / Phase 2 dan seterusnya.

#pragma once

namespace muse_dsp {

// Versi semantic shared core. Naik mengikuti perubahan core, bukan app.
inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

// Contoh: "0.1.0-b2".
const char* getVersionString();

}  // namespace muse_dsp
