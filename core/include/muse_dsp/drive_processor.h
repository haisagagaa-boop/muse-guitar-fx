// DriveProcessor — kelas dasar untuk efek drive (Phase 2.2).
//
// Menangani semua yang sama antara Overdrive dan Distortion, sehingga kedua
// efek hanya berbeda pada waveshaper-nya:
//   - Oversampling 4x (default; bisa 1/2/4): upsample polyphase +
//     waveshaper + tone filter pada rate oversampled + downsample.
//     Filter anti-aliasing: FIR windowed-sinc (Hamming, 128 tap,
//     cutoff 0,85 x Nyquist base). Dikelola DI DALAM node; semua buffer
//     dan state dialokasikan di prepare() (untuk faktor maks 4x).
//   - Tone: lowpass satu-pole pasca-waveshaper (cutoff = parameter tone),
//     dijalankan pada rate oversampled.
//   - DC blocker: high-pass satu-pole 20 Hz di output (base rate), per
//     channel. Distortion asimetris menghasilkan offset DC (clip
//     +1,0/-0,65); tanpa blocker, DC itu mengalir ke efek berikutnya
//     dalam chain dan menggeser titik kerjanya. Cutoff 20 Hz jauh di
//     bawah nada gitar terendah (E2 = 82 Hz) sehingga tidak terdengar;
//     overdrive (simetris, tanpa DC) tidak terpengaruh secara berarti.
//   - Parameter drive/tone/level lewat SmoothedParameter (atomic,
//     lock-free, smoothing 20 ms) — aman diubah dari control thread.
//
// Latensi tambahan oversampling: FIR linear-phase 128 tap menambah
// (128-1)/2 sampel @ rate oversampled per filter; upsample + downsample
// total ~127 sampel @ 4x = ~31,75 sampel @ base rate (~0,66 ms @ 48 kHz).
// Konstan, tidak dikompensasi (di bawah 1 ms, tidak terdengar).
//
// Denormal protection (TANPA JUCE — core tidak boleh bergantung JUCE):
// satu-satunya state rekursif adalah tone filter satu-pole; state-nya
// di-flush ke nol bila |state| < 1e-30 (preseden 2.1). Delay line FIR
// bukan rekursif (isinya sampel input masa lalu) sehingga tidak butuh
// flush. SmoothedParameter meng-clamp tepat di akhir ramp.
//
// Kontrak thread: sama seperti prosesor lain — prepare()/reset() hanya
// saat audio thread berhenti; parameter dan bypass aman kapan pun.
// Perubahan oversampling factor aman dari control thread (hanya membaca
// atomic; koefisien untuk faktor 2 dan 4 sudah dihitung di prepare),
// tetapi dapat menimbulkan transient singkat karena delay line berisi
// data rate lama — sebaiknya diubah saat device berhenti.

#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

#include "muse_dsp/audio_processor.h"
#include "muse_dsp/smoothed_parameter.h"

namespace muse_dsp {

class DriveProcessor : public AudioProcessor {
public:
    // driveDb: pre-gain sebelum waveshaper (dB). toneHz: cutoff lowpass
    // tone (Hz). levelDb: output gain (dB). oversamplingFactor: 1, 2,
    // atau 4 (default 4). Rentang drive/level diberikan kelas turunan
    // (overdrive dan distortion berbeda); tone selalu 500..8000 Hz.
    DriveProcessor(float driveDb, float toneHz, float levelDb,
                   int oversamplingFactor, float driveMinDb,
                   float driveMaxDb, float levelMinDb, float levelMaxDb);

    void prepare(const ProcessSpec& spec) override;
    void process(AudioBuffer& buffer) override;
    void reset() override;

    // Thread-safe: dari control/UI thread kapan pun (atomic, lock-free).
    void setDriveDb(float db) noexcept;
    void setToneHz(float hz) noexcept;
    void setLevelDb(float db) noexcept;
    // Faktor oversampling (1, 2, atau 4; di luar itu di-clamp ke yang
    // terdekat). Lihat catatan transient di atas.
    void setOversamplingFactor(int factor) noexcept;
    float getDriveDb() const noexcept { return driveDb_.getTarget(); }
    float getToneHz() const noexcept { return toneHz_.getTarget(); }
    float getLevelDb() const noexcept { return levelDb_.getTarget(); }
    int getOversamplingFactor() const noexcept {
        return oversamplingFactor_.load(std::memory_order_relaxed);
    }

protected:
    // Waveshaper — satu-satunya yang berbeda antar efek drive. Dipanggil
    // sekali per sub-sampel oversampled, per channel. Harus memoryless,
    // deterministik, dan finite untuk input finite.
    virtual float waveshape(float x) const noexcept = 0;

    // Batas clamp drive/level — diisi kelas turunan (dipakai setter dan
    // konstruktor). Tone selalu 500..8000 Hz.
    float driveMinDb_ = 0.0f;
    float driveMaxDb_ = 30.0f;
    float levelMinDb_ = -24.0f;
    float levelMaxDb_ = 24.0f;
    float initialDriveDb_ = 0.0f;
    float initialToneHz_ = 4000.0f;
    float initialLevelDb_ = 0.0f;

    SmoothedParameter driveDb_;
    SmoothedParameter toneHz_;
    SmoothedParameter levelDb_;

private:
    // Panjang FIR anti-aliasing (tap) pada rate oversampled. 128 tap,
    // kelipatan 4 dan 2 (faktor 2 dan 4 membagi habis).
    static constexpr int kFirLength = 128;
    // Ambang flush-to-zero state tone filter (denormal protection).
    static constexpr float kDenormalFlushLevel = 1e-30f;
    // Faktor oversampling maksimum (buffer dialokasi untuk ini).
    static constexpr int kMaxFactor = 4;

    // Koefisien FIR windowed-sinc (Hamming) untuk satu faktor.
    // cutoff ternormalisasi = 0,85 / (2 * faktor) siklus/sampel @ rate
    // oversampled (0,85 x Nyquist base).
    static std::vector<float> designFirCoefficients(int factor);

    // Satu-pole lowpass: y += a * (x - y). a dari cutoff dan rate.
    static float onePoleCoefficient(float cutoffHz, double rate) noexcept;

    std::atomic<int> oversamplingFactor_{4};
    double sampleRate_ = 48000.0;
    int numChannels_ = 2;

    // Koefisien FIR per faktor (dihitung di prepare()).
    std::vector<float> firFactor2_;
    std::vector<float> firFactor4_;

    // Delay line upsample: sampel base terakhir per channel (ring,
    // kapasitas kFirLength/2 = 64 — cukup untuk faktor 2; faktor 4
    // hanya memakai 32 entri terbaru).
    std::vector<std::vector<float>> upHistory_;
    std::vector<int> upWritePos_;
    // Delay line downsample: sampel oversampled terakhir per channel
    // (ring, kapasitas kFirLength = 128).
    std::vector<std::vector<float>> downHistory_;
    std::vector<int> downWritePos_;
    // State tone filter satu-pole per channel (rate oversampled).
    std::vector<float> toneState_;
    // State DC blocker (high-pass satu-pole 20 Hz, base rate):
    // input dan output sebelumnya per channel.
    std::vector<float> dcBlockerPrevIn_;
    std::vector<float> dcBlockerPrevOut_;
    float dcBlockerCoeff_ = 0.0f;  // R = exp(-2*pi*20/sampleRate)
    static constexpr float kDcBlockerCutoffHz = 20.0f;
};

}  // namespace muse_dsp
