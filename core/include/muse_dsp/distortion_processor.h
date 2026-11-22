// DistortionProcessor — distorsi hard clipping asimetris (Phase 2.2).
//
// Hard clipping dengan ambang asimetris: positif dijepit di +1,0,
// negatif di -0,65. Karakter JELAS berbeda dari Overdrive (tanh):
//   - Clipping KERAS (bukan saturasi halus): begitu sinyal melewati
//     ambang, output langsung datar — spektrum harmonik meluruh lambat
//     (~1/n, seperti gelombang kotak), jauh lebih terang dan agresif
//     daripada tanh.
//   - ASIMETRIS: sisi positif dan negatif dijepit pada level berbeda,
//     sehingga fungsi transfer tidak ganjil — muncul harmonik GENAP
//     (terutama harmonik ke-2) yang tidak ada pada overdrive simetris.
//     Inilah pembeda yang terukur antara keduanya, bukan sekadar angka
//     drive yang berbeda.
//   - Pada drive rendah (sinyal di bawah ambang) efek hampir tidak
//     bekerja sama sekali (THD ~ 0) — berbeda dari tanh yang selalu
//     sedikit nonlinear.
//
// Parameter: drive 0..60 dB (pre-gain, rentang LEBIH BESAR dari
// overdrive 0..30 dB), tone 500..8000 Hz (cutoff lowpass),
// level -24..+24 dB (output gain). Default: drive 24 dB, tone 4000 Hz,
// level 0 dB. Oversampling default 4x (lihat DriveProcessor) —
// penting di sini karena hard clipping menghasilkan harmonik paling
// kuat (kasus terburuk untuk aliasing).

#pragma once

#include "muse_dsp/drive_processor.h"

namespace muse_dsp {

class DistortionProcessor : public DriveProcessor {
public:
    // driveDb: 0..60 dB (default 24). toneHz: 500..8000 Hz (default
    // 4000). levelDb: -24..+24 dB (default 0). oversamplingFactor:
    // 1, 2, atau 4 (default 4).
    DistortionProcessor(float driveDb = 24.0f, float toneHz = 4000.0f,
                        float levelDb = 0.0f, int oversamplingFactor = 4);

    const char* getName() const override { return "Distortion"; }

protected:
    float waveshape(float x) const noexcept override;
};

}  // namespace muse_dsp
