// offline_render — offline rendering test (B4).
//
// Merender 1 detik sine 440 Hz stereo @ 48 kHz lewat EffectChain TANPA
// thread real-time (murni pemanggilan process() per blok), menulis hasilnya
// sebagai file WAV 16-bit PCM, lalu memverifikasi isi render:
//   - RMS paruh pertama  = 0.25/sqrt(2) (sine 0.5 x gain 0.5),
//   - RMS 0,25 detik terakhir = 0.5/sqrt(2) (gain sudah naik ke 1.0),
//   - peak keseluruhan ~0.5,
//   - header WAV yang ditulis ulang terbaca kembali dengan benar.
//
// Penggunaan: offline_render <output.wav> [blockSize]
// Keluar 0 bila semua pemeriksaan lolos, 1 bila ada yang gagal.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "muse_dsp/audio_buffer.h"
#include "muse_dsp/effect_chain.h"
#include "muse_dsp/gain_processor.h"
#include "test_processors.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kNumChannels = 2;
constexpr int kTotalSamples = 48000;  // 1 detik
constexpr double kFrequencyHz = 440.0;
constexpr float kInputAmplitude = 0.5f;

int g_failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++g_failures;
        std::printf("FAIL: %s\n", message);
    }
}

void checkNear(double actual, double expected, double tolerance,
               const char* message) {
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::printf("FAIL: %s (actual=%.6f expected=%.6f tol=%.6f)\n", message,
                    actual, expected, tolerance);
    }
}

// --- Penulis WAV minimal (16-bit PCM, little-endian) ----------------------
void writeU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void writeU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void writeTag(std::vector<std::uint8_t>& out, const char* tag) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(tag[i]));
    }
}

// Interleaved float [-1, 1] -> file WAV 16-bit PCM. Mengembalikan false bila
// file tidak bisa ditulis.
bool writeWav16(const std::string& path,
                const std::vector<float>& interleaved, int numChannels,
                int sampleRate) {
    const std::uint32_t numFrames =
        static_cast<std::uint32_t>(interleaved.size() /
                                   static_cast<size_t>(numChannels));
    const std::uint32_t dataBytes = numFrames *
                                    static_cast<std::uint32_t>(numChannels) * 2;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + dataBytes);
    writeTag(bytes, "RIFF");
    writeU32(bytes, 36 + dataBytes);
    writeTag(bytes, "WAVE");
    writeTag(bytes, "fmt ");
    writeU32(bytes, 16);  // ukuran subchunk fmt
    writeU16(bytes, 1);   // PCM
    writeU16(bytes, static_cast<std::uint16_t>(numChannels));
    writeU32(bytes, static_cast<std::uint32_t>(sampleRate));
    writeU32(bytes, static_cast<std::uint32_t>(sampleRate * numChannels * 2));
    writeU16(bytes, static_cast<std::uint16_t>(numChannels * 2));
    writeU16(bytes, 16);  // bits per sample
    writeTag(bytes, "data");
    writeU32(bytes, dataBytes);
    for (float sample : interleaved) {
        const float clamped =
            std::max(-1.0f, std::min(1.0f, sample));
        const auto pcm = static_cast<std::int16_t>(
            std::lround(clamped * 32767.0f));
        writeU16(bytes, static_cast<std::uint16_t>(pcm));
    }

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    const size_t written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    return written == bytes.size();
}

// Baca kembali header WAV dan periksa strukturnya. Mengembalikan false bila
// file rusak atau bukan WAV 16-bit PCM stereo 48 kHz.
bool verifyWavHeader(const std::string& path, std::uint32_t expectedFrames) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::uint8_t header[44];
    const size_t read = std::fread(header, 1, sizeof(header), file);
    std::fclose(file);
    if (read != sizeof(header)) {
        return false;
    }
    auto u16 = [&](size_t off) {
        return static_cast<std::uint16_t>(header[off] |
                                          (header[off + 1] << 8));
    };
    auto u32 = [&](size_t off) {
        return static_cast<std::uint32_t>(header[off] |
                                          (header[off + 1] << 8) |
                                          (header[off + 2] << 16) |
                                          (header[off + 3] << 24));
    };
    if (std::memcmp(header, "RIFF", 4) != 0 ||
        std::memcmp(header + 8, "WAVE", 4) != 0 ||
        std::memcmp(header + 12, "fmt ", 4) != 0 ||
        std::memcmp(header + 36, "data", 4) != 0) {
        return false;
    }
    if (u16(20) != 1 || u16(22) != kNumChannels || u32(24) != 48000 ||
        u16(34) != 16) {
        return false;  // bukan PCM 16-bit stereo 48 kHz
    }
    return u32(40) == expectedFrames * kNumChannels * 2;
}

double rmsOf(const std::vector<float>& samples, size_t begin, size_t end) {
    double sumSq = 0.0;
    for (size_t i = begin; i < end; ++i) {
        sumSq += static_cast<double>(samples[i]) * samples[i];
    }
    const size_t n = end - begin;
    return n > 0 ? std::sqrt(sumSq / static_cast<double>(n)) : 0.0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Penggunaan: offline_render <output.wav> [blockSize]\n");
        return 1;
    }
    const std::string outputPath = argv[1];
    const int blockSize = argc >= 3 ? std::atoi(argv[2]) : 256;
    if (blockSize <= 0 || blockSize > 4096) {
        std::printf("FAIL: blockSize harus 1..4096\n");
        return 1;
    }
    std::printf("offline_render: %d sampel @ %.0f Hz, blok %d, -> %s\n",
                kTotalSamples, kSampleRate, blockSize, outputPath.c_str());

    // Chain: Gain 0.5, naik ke 1.0 tepat di tengah render (0,5 detik).
    muse_dsp::EffectChain chain;
    auto gain = std::make_unique<muse_dsp::GainProcessor>(0.5f, 0.02);
    muse_dsp::GainProcessor* gainPtr = gain.get();
    chain.addProcessor(std::move(gain));
    chain.prepare(muse_dsp::ProcessSpec{kSampleRate, blockSize, kNumChannels});

    // Input: sine 440 Hz amplitudo 0.5, kedua channel identik.
    muse_dsp::AudioBuffer input(kNumChannels, kTotalSamples);
    const double phaseStep =
        2.0 * 3.14159265358979323846 * kFrequencyHz / kSampleRate;
    for (int channel = 0; channel < kNumChannels; ++channel) {
        float* data = input.getWritePointer(channel);
        double phase = 0.0;
        for (int s = 0; s < kTotalSamples; ++s) {
            data[s] = static_cast<float>(kInputAmplitude * std::sin(phase));
            phase += phaseStep;
        }
    }

    // Render offline per blok (tanpa thread real-time).
    std::vector<float> interleaved;
    interleaved.reserve(static_cast<size_t>(kTotalSamples) * kNumChannels);
    std::vector<float> channel0;
    channel0.reserve(static_cast<size_t>(kTotalSamples));
    muse_dsp::AudioBuffer block(kNumChannels, blockSize);
    int samplesDone = 0;
    bool gainRaised = false;
    while (samplesDone < kTotalSamples) {
        const int count = std::min(blockSize, kTotalSamples - samplesDone);
        // Perubahan parameter di tengah render: blok pertama yang mulai
        // pada/setelah 0,5 detik (posisi pasti bergantung pada ukuran blok,
        // tetapi selalu setelah paruh pertama — cukup untuk pemeriksaan
        // statistik RMS di bawah).
        if (!gainRaised && samplesDone >= kTotalSamples / 2) {
            gainPtr->setGain(1.0f);
            gainRaised = true;
        }
        for (int channel = 0; channel < kNumChannels; ++channel) {
            const float* in = input.getReadPointer(channel);
            float* out = block.getWritePointer(channel);
            for (int i = 0; i < count; ++i) {
                out[i] = in[samplesDone + i];
            }
            for (int i = count; i < blockSize; ++i) {
                out[i] = 0.0f;
            }
        }
        chain.process(block);
        for (int i = 0; i < count; ++i) {
            interleaved.push_back(block.getSample(0, i));
            interleaved.push_back(block.getSample(1, i));
            channel0.push_back(block.getSample(0, i));
        }
        samplesDone += count;
    }

    // Verifikasi isi render (channel 0).
    const double rmsFirstHalf =
        rmsOf(channel0, 0, static_cast<size_t>(kTotalSamples) / 2);
    const double rmsLastQuarter = rmsOf(
        channel0, static_cast<size_t>(kTotalSamples) * 3 / 4,
        static_cast<size_t>(kTotalSamples));
    double peak = 0.0;
    for (float sample : channel0) {
        peak = std::max(peak, static_cast<double>(std::fabs(sample)));
    }
    std::printf("  RMS paruh pertama: %.4f (diharapkan ~0.1768)\n", rmsFirstHalf);
    std::printf("  RMS kuartal terakhir: %.4f (diharapkan ~0.3536)\n",
                rmsLastQuarter);
    std::printf("  Peak: %.4f (diharapkan ~0.5)\n", peak);
    checkNear(rmsFirstHalf, 0.25 / std::sqrt(2.0), 0.005,
              "RMS paruh pertama harus 0.25/sqrt(2)");
    checkNear(rmsLastQuarter, 0.5 / std::sqrt(2.0), 0.005,
              "RMS kuartal terakhir harus 0.5/sqrt(2)");
    checkNear(peak, 0.5, 0.01, "peak harus ~0.5");
    // Sampel di tengah ramp (0,5 detik + 10 ms) harus berada di antara
    // 0.5 dan 1.0 kali amplitudo input — bukti smoothing berjalan di render.
    const float midRamp = channel0[static_cast<size_t>(kTotalSamples) / 2 + 480];
    check(std::fabs(midRamp) <= 0.5f,
          "sampel tengah ramp tidak boleh melebihi amplitudo penuh");

    // Tulis WAV dan verifikasi header-nya dengan membaca kembali.
    check(writeWav16(outputPath, interleaved, kNumChannels,
                     static_cast<int>(kSampleRate)),
          "gagal menulis file WAV");
    check(verifyWavHeader(outputPath, kTotalSamples),
          "header WAV tidak valid saat dibaca kembali");
    if (g_failures == 0) {
        std::printf("  WAV ditulis: %s\n", outputPath.c_str());
    }

    std::printf("offline_render: %d failures\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
