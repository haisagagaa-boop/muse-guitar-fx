// OverdriveProcessor — overdrive soft clipping (Phase 2.2).
//
// Soft clipping simetris dengan tanh: penguatan drive mendorong sinyal
// ke daerah saturasi tanh yang halus, menghasilkan harmonik GANJIL yang
// meluruh cepat (karakter "tube" yang hangat). Karena tanh simetris
// (tanh(-x) = -tanh(x)), tidak ada harmonik genap.
//
// Alasan memilih tanh (bukan hard clip atau polinomial):
//   - C-infinity (mulus di semua turunan) — tanpa sudut pada kurva
//     transfer, sehingga spektrum harmonik meluruh cepat dan tidak
//     terdengar "kasar digital".
//   - Monoton dan terbatas (±1) — tidak pernah NaN/inf; gain berlebih
//     hanya memperdalam saturasi, bukan menambah level.
//   - Perilaku pada sinyal kecil hampir linear (tanh(x) ~= x - x^3/3),
//     jadi pada drive rendah efek hampir bersih dan THD naik mulus
//     seiring drive.
//
// Parameter: drive 0..30 dB (pre-gain), tone 500..8000 Hz (cutoff
// lowpass), level -24..+24 dB (output gain). Default: drive 12 dB,
// tone 4000 Hz, level 0 dB. Oversampling default 4x (lihat
// DriveProcessor).

#pragma once

#include "muse_dsp/drive_processor.h"

namespace muse_dsp {

class OverdriveProcessor : public DriveProcessor {
public:
    // driveDb: 0..30 dB (default 12). toneHz: 500..8000 Hz (default
    // 4000). levelDb: -24..+24 dB (default 0). oversamplingFactor:
    // 1, 2, atau 4 (default 4).
    OverdriveProcessor(float driveDb = 12.0f, float toneHz = 4000.0f,
                       float levelDb = 0.0f, int oversamplingFactor = 4);

    const char* getName() const override { return "Overdrive"; }

protected:
    float waveshape(float x) const noexcept override;
};

}  // namespace muse_dsp
