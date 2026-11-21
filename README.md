# MUSE GUITAR FX — Guitar Multi-Effects Processor (Android & Windows)

> **Status: Phase 0 selesai (21 November 2026).**
> Proyek greenfield — belum ada kode, belum ada dependency yang diunduh,
> belum ada build. Phase 1 menunggu persetujuan user.

## Dokumen Phase 0

| Dokumen | Isi |
|---|---|
| `docs/architecture.md` | Arsitektur modul, aliran sinyal, shared core vs platform code |
| `docs/development-roadmap.md` | Phase 0–9, scope, dependensi, estimasi kasar |
| `docs/audio-engine-design.md` | Desain audio engine real-time (callback, latensi, thread safety) |
| `docs/platform-compatibility.md` | Matriks kompatibilitas Android vs Windows + opsi build Windows |
| `docs/testing-strategy.md` | Strategi pengujian, kriteria lolos/gagal, taksonomi status, bukti audit |

## Ringkasan audit (21 Nov 2026)

- **Repository:** tidak ada — proyek dimulai dari nol.
- **Keputusan arsitektur:** shared DSP core C++ + JUCE 8.0.15 (pin), CMake, preset JSON berversi.
- **Yang sudah ada di VM:** CMake 3.28.3, GCC 13.3.0, Android SDK (build-tools 34.0.0, platform android-34/35), JDK Temurin 21.0.13, git 2.43.0.
- **Yang belum ada (dibutuhkan Phase 1):** JUCE, Android NDK, CMake-for-Android, ASIO SDK (build Windows), HP + USB audio interface untuk uji, akses build/uji Windows.
- **Ruang web "Efek Gitar" (artefak efek-gitar) adalah prototipe browser yang terpisah** — bukan proyek ini, tidak disentuh.

## Aturan

- Jangan mulai Phase 1 tanpa persetujuan user.
- Jangan klaim efek/build berfungsi sebelum diuji (lihat `docs/testing-strategy.md` §5).
- Jangan ubah kode/konfigurasi/deployment di luar scope phase yang sedang berjalan.
