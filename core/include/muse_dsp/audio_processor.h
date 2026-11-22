// AudioProcessor — interface pemrosesan audio (B3).
//
// Setiap prosesor (efek, chain, gain) mengimplementasikan interface ini:
//   prepare() — dipanggil sekali saat stream disiapkan (alokasi & reset di sini).
//   process() — dipanggil tiap blok audio; HARUS bebas alokasi, bebas mutex
//               blocking, bebas file I/O, dan deterministik.
//   reset()   — kembalikan state internal ke kondisi awal (tanpa alokasi).
//
// Kontrak thread:
// - prepare()/reset() dan perubahan struktur (tambah/hapus/pindah prosesor)
//   hanya dilakukan saat audio thread berhenti.
// - Parameter (lihat smoothed_parameter.h) dan bypass aman diubah kapan pun
//   dari thread mana pun (atomic, lock-free).

#pragma once

#include <atomic>

namespace muse_dsp {

class AudioBuffer;

// Spesifikasi stream yang dipakai prepare().
struct ProcessSpec {
    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    int numChannels = 2;
};

class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    virtual void prepare(const ProcessSpec& spec) = 0;
    virtual void process(AudioBuffer& buffer) = 0;
    virtual void reset() = 0;
    virtual const char* getName() const = 0;

    // Bypass aman diubah dari thread mana pun (atomic, lock-free). Bila
    // bypassed, EffectChain melewati prosesor ini tanpa memanggil process().
    void setBypassed(bool bypassed) noexcept {
        bypassed_.store(bypassed, std::memory_order_relaxed);
    }
    bool isBypassed() const noexcept {
        return bypassed_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<bool> bypassed_{false};
};

}  // namespace muse_dsp
