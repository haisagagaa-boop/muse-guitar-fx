// Smoke test shared core (B2): tanpa framework, tanpa JUCE.
// Keluar non-nol bila ada pemeriksaan yang gagal.

#include <cstdio>
#include <cstring>

#include "muse_dsp/muse_dsp.h"

int main() {
    const char* version = muse_dsp::getVersionString();
    std::printf("muse_dsp version: %s\n", version);

    if (version == nullptr || std::strlen(version) == 0) {
        std::printf("FAIL: version string kosong\n");
        return 1;
    }
    if (muse_dsp::kVersionMajor != 0 || muse_dsp::kVersionMinor != 2 ||
        muse_dsp::kVersionPatch != 0) {
        std::printf("FAIL: versi core bukan 0.2.0\n");
        return 1;
    }
    if (std::strcmp(version, "0.2.0-b3") != 0) {
        std::printf("FAIL: version string bukan 0.2.0-b3\n");
        return 1;
    }

    std::printf("muse_dsp smoke test: PASS\n");
    return 0;
}
