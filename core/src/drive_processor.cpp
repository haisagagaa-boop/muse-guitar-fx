// DriveProcessor — implementasi kelas dasar efek drive (Phase 2.2).
//
// process() bebas alokasi/mutex/file I/O: hanya membaca atomic target,
// aritmetika float per (sub-)sampel, dan state internal yang sudah
// dialokasi di prepare(). Tidak ada panggilan sistem di jalur audio.
//
// Oversampling (lihat header untuk desain): upsample polyphase FIR +
// waveshaper + tone filter pada rate oversampled + downsample FIR.
// Koefisien FIR dihitung di prepare() (bukan di process()).

#include "muse_dsp/drive_processor.h"

#include <algorithm>
#include <cmath>

#include "muse_dsp/audio_buffer.h"

namespace muse_dsp {

namespace {

constexpr double kPi = 3.14159265358979323846;

float clamp(float value, float low, float high) noexcept {
    return std::max(low, std::min(high, value));
}

// sinc(x) = sin(pi*x) / (pi*x); sinc(0) = 1.
double sinc(double x) noexcept {
    if (std::fabs(x) < 1e-12) {
        return 1.0;
    }
    return std::sin(kPi * x) / (kPi * x);
}

}  // namespace

DriveProcessor::DriveProcessor(float driveDb, float toneHz, float levelDb,
                               int oversamplingFactor, float driveMinDb,
                               float driveMaxDb, float levelMinDb,
                               float levelMaxDb)
    : driveMinDb_(driveMinDb),
      driveMaxDb_(driveMaxDb),
      levelMinDb_(levelMinDb),
      levelMaxDb_(levelMaxDb),
      initialDriveDb_(clamp(driveDb, driveMinDb, driveMaxDb)),
      initialToneHz_(clamp(toneHz, 500.0f, 8000.0f)),
      initialLevelDb_(clamp(levelDb, levelMinDb, levelMaxDb)),
      driveDb_(initialDriveDb_, 0.02),
      toneHz_(initialToneHz_, 0.02),
      levelDb_(initialLevelDb_, 0.02),
      oversamplingFactor_(oversamplingFactor == 2
                              ? 2
                              : (oversamplingFactor <= 1 ? 1 : 4)) {}

// static
std::vector<float> DriveProcessor::designFirCoefficients(int factor) {
    // Windowed-sinc, jendela Hamming, panjang kFirLength tap.
    // Cutoff: 0,85 x Nyquist base. Dalam siklus/sampel @ rate
    // oversampled: fc = 0,85 / (2 * faktor).
    const double fc = 0.85 / (2.0 * static_cast<double>(factor));
    const double middle = (kFirLength - 1) / 2.0;
    std::vector<float> coeff(static_cast<size_t>(kFirLength), 0.0f);
    double sum = 0.0;
    for (int n = 0; n < kFirLength; ++n) {
        const double x = static_cast<double>(n) - middle;
        const double window =
            0.54 - 0.46 * std::cos(2.0 * kPi * static_cast<double>(n) /
                                   static_cast<double>(kFirLength - 1));
        const double value = 2.0 * fc * sinc(2.0 * fc * x) * window;
        coeff[static_cast<size_t>(n)] = static_cast<float>(value);
        sum += value;
    }
    // Normalisasi: gain DC = 1 (jumlah koefisien = 1).
    const float invSum = static_cast<float>(1.0 / sum);
    for (float& c : coeff) {
        c *= invSum;
    }
    return coeff;
}

// static
float DriveProcessor::onePoleCoefficient(float cutoffHz, double rate) noexcept {
    if (cutoffHz <= 0.0f || rate <= 0.0) {
        return 1.0f;  // langsung sampai (tanpa filtering)
    }
    return static_cast<float>(1.0 - std::exp(-2.0 * kPi *
                                             static_cast<double>(cutoffHz) /
                                             rate));
}

void DriveProcessor::prepare(const ProcessSpec& spec) {
    sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : 48000.0;
    numChannels_ = spec.numChannels > 0 ? spec.numChannels : 2;
    driveDb_.prepare(sampleRate_);
    toneHz_.prepare(sampleRate_);
    levelDb_.prepare(sampleRate_);

    firFactor2_ = designFirCoefficients(2);
    firFactor4_ = designFirCoefficients(4);

    // Delay line upsample: kapasitas kFirLength/2 = 64 sampel base
    // (faktor 2 memakai 64, faktor 4 memakai 32 entri terbaru).
    upHistory_.assign(static_cast<size_t>(numChannels_),
                      std::vector<float>(kFirLength / 2, 0.0f));
    upWritePos_.assign(static_cast<size_t>(numChannels_), 0);
    // Delay line downsample: kapasitas kFirLength = 128 sampel
    // oversampled.
    downHistory_.assign(static_cast<size_t>(numChannels_),
                        std::vector<float>(kFirLength, 0.0f));
    downWritePos_.assign(static_cast<size_t>(numChannels_), 0);
    toneState_.assign(static_cast<size_t>(numChannels_), 0.0f);
    dcBlockerPrevIn_.assign(static_cast<size_t>(numChannels_), 0.0f);
    dcBlockerPrevOut_.assign(static_cast<size_t>(numChannels_), 0.0f);
    dcBlockerCoeff_ = static_cast<float>(
        std::exp(-2.0 * kPi * static_cast<double>(kDcBlockerCutoffHz) /
                 sampleRate_));
}

void DriveProcessor::process(AudioBuffer& buffer) {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int factor = oversamplingFactor_.load(std::memory_order_relaxed);
    const std::vector<float>& fir =
        (factor == 2) ? firFactor2_ : firFactor4_;
    const int tapsPerPhase = kFirLength / (factor > 1 ? factor : 1);

    for (int sample = 0; sample < numSamples; ++sample) {
        // Parameter yang di-smoothing per frame (target dibaca dari atomic).
        const float driveDb = driveDb_.getNextValue();
        const float toneHz = toneHz_.getNextValue();
        const float levelDb = levelDb_.getNextValue();
        const float driveGain =
            static_cast<float>(std::pow(10.0, driveDb * 0.05));
        const float levelGain =
            static_cast<float>(std::pow(10.0, levelDb * 0.05));
        // Koefisien tone dihitung dari rate oversampled (atau base rate
        // bila faktor 1); diperbarui sekali per sampel base.
        const float toneCoeff = onePoleCoefficient(
            toneHz, sampleRate_ * static_cast<double>(factor));

        for (int channel = 0; channel < numChannels; ++channel) {
            float* data = buffer.getWritePointer(channel);
            const float input = data[sample];
            float output = 0.0f;

            if (factor <= 1) {
                // Tanpa oversampling: waveshaper + tone langsung pada
                // base rate (dipakai tes perbandingan aliasing).
                const float shaped = waveshape(input * driveGain);
                float& state = toneState_[static_cast<size_t>(channel)];
                state += toneCoeff * (shaped - state);
                if (std::fabs(state) < kDenormalFlushLevel) {
                    state = 0.0f;
                }
                output = state * levelGain;
            } else {
                // --- Upsample (polyphase) ---
                // Dorong sampel base ke delay line (ring, kapasitas 64).
                std::vector<float>& up = upHistory_[static_cast<size_t>(channel)];
                int& upPos = upWritePos_[static_cast<size_t>(channel)];
                up[static_cast<size_t>(upPos)] = input;
                upPos = (upPos + 1) & (kFirLength / 2 - 1);
                // upPos sekarang menunjuk slot SETELAH sampel terbaru;
                // sampel terbaru (x[m]) ada di (upPos - 1).
                const int upMask = kFirLength / 2 - 1;

                std::vector<float>& down =
                    downHistory_[static_cast<size_t>(channel)];
                int& downPos = downWritePos_[static_cast<size_t>(channel)];
                float& state = toneState_[static_cast<size_t>(channel)];

                for (int phase = 0; phase < factor; ++phase) {
                    // y[factor*m + phase] = sum_k fir[factor*k + phase]
                    //                          * x[m - k].
                    float upSampled = 0.0f;
                    for (int k = 0; k < tapsPerPhase; ++k) {
                        // +2*kapasitas agar indeks tidak pernah negatif
                        // sebelum & mask (portabel, tidak mengandalkan
                        // two's complement pada bilangan negatif).
                        upSampled += fir[static_cast<size_t>(factor * k + phase)] *
                                     up[static_cast<size_t>(
                                         (upPos - 1 - k + kFirLength) &
                                         upMask)];
                    }
                    // Waveshaper + tone filter pada rate oversampled.
                    // Kompensasi gain zero-stuffing: jumlah tap tiap fase
                    // ~1/factor dari total (filter DC gain 1), jadi sinyal
                    // upsampled melemah 1/factor; filter interpolasi standar
                    // memiliki DC gain = factor. Tanpa perkalian ini gain
                    // end-to-end = 1/factor (terukur: fundamental 0,124
                    // dari input 0,5 pada factor 4).
                    upSampled *= static_cast<float>(factor);
                    const float shaped = waveshape(upSampled * driveGain);
                    state += toneCoeff * (shaped - state);
                    if (std::fabs(state) < kDenormalFlushLevel) {
                        state = 0.0f;
                    }
                    // Dorong ke delay line downsample (ring, kapasitas 128).
                    down[static_cast<size_t>(downPos)] = state;
                    downPos = (downPos + 1) & (kFirLength - 1);
                }

                // --- Downsample (decimate) ---
                // y[m] = sum_n fir[n] * w[factor*m - n]. Sampel terbaru
                // di delay line adalah w[factor*m + factor - 1], jadi
                // w[factor*m - n] berada (factor - 1 + n) slot di
                // belakangnya, yaitu indeks (downPos - factor - n).
                float downSampled = 0.0f;
                for (int n = 0; n < kFirLength; ++n) {
                    // +2*kapasitas agar indeks tidak pernah negatif
                    // sebelum & mask (lihat komentar di upsample).
                    downSampled += fir[static_cast<size_t>(n)] *
                                   down[static_cast<size_t>(
                                       (downPos - factor - n +
                                        2 * kFirLength) &
                                       (kFirLength - 1))];
                }
                output = downSampled * levelGain;
            }

            // DC blocker (high-pass satu-pole 20 Hz, base rate):
            // y = x - xPrev + R * yPrev. Menghilangkan offset DC dari
            // clipping asimetris (lihat header); sinyal gitar (>= 82 Hz)
            // tidak terpengaruh.
            float& dcPrevIn =
                dcBlockerPrevIn_[static_cast<size_t>(channel)];
            float& dcPrevOut =
                dcBlockerPrevOut_[static_cast<size_t>(channel)];
            const float blocked =
                output - dcPrevIn + dcBlockerCoeff_ * dcPrevOut;
            dcPrevIn = output;
            dcPrevOut = blocked;
            // Denormal protection: state rekursif di-flush ke nol.
            if (std::fabs(dcPrevOut) < kDenormalFlushLevel) {
                dcPrevOut = 0.0f;
            }
            data[sample] = blocked;
        }
    }
}

void DriveProcessor::reset() {
    driveDb_.reset(initialDriveDb_);
    toneHz_.reset(initialToneHz_);
    levelDb_.reset(initialLevelDb_);
    for (auto& history : upHistory_) {
        std::fill(history.begin(), history.end(), 0.0f);
    }
    std::fill(upWritePos_.begin(), upWritePos_.end(), 0);
    for (auto& history : downHistory_) {
        std::fill(history.begin(), history.end(), 0.0f);
    }
    std::fill(downWritePos_.begin(), downWritePos_.end(), 0);
    std::fill(toneState_.begin(), toneState_.end(), 0.0f);
    std::fill(dcBlockerPrevIn_.begin(), dcBlockerPrevIn_.end(), 0.0f);
    std::fill(dcBlockerPrevOut_.begin(), dcBlockerPrevOut_.end(), 0.0f);
}

void DriveProcessor::setDriveDb(float db) noexcept {
    driveDb_.setTarget(clamp(db, driveMinDb_, driveMaxDb_));
}

void DriveProcessor::setToneHz(float hz) noexcept {
    toneHz_.setTarget(clamp(hz, 500.0f, 8000.0f));
}

void DriveProcessor::setLevelDb(float db) noexcept {
    levelDb_.setTarget(clamp(db, levelMinDb_, levelMaxDb_));
}

void DriveProcessor::setOversamplingFactor(int factor) noexcept {
    // Hanya 1, 2, atau 4; nilai lain dibulatkan ke yang terdekat.
    const int clamped = (factor <= 1) ? 1 : (factor == 2 ? 2 : 4);
    oversamplingFactor_.store(clamped, std::memory_order_relaxed);
}

}  // namespace muse_dsp
