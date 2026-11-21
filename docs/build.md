# Cara Build — MUSE Guitar FX

Dokumen ini menjelaskan cara membangun proyek pada tahap B2 (Phase 1).
Dependency, versi, lisensi, dan implikasi distribusi: lihat
`docs/dependencies.md`. Arsitektur: lihat `docs/architecture.md`.

## Prasyarat (terverifikasi di B1)

| Kebutuhan | Versi | Keterangan |
|---|---|---|
| CMake | 3.28.3 | Host (Linux). Minimal 3.22 (persyaratan JUCE). |
| GCC / G++ | 13.3.0 | Host (Linux). |
| JDK Temurin | 21.0.13 | Untuk Android SDK tools. `JAVA_HOME=$HOME/workspace/english-learning-app/toolchain/jdk` |
| Android SDK | build-tools 34.0.0, platform-tools 37.0.1, platform android-34/35 | `~/workspace/android-sdk/` |
| Android NDK | r28c (28.2.13676358) | `~/workspace/android-sdk/ndk/28.2.13676358` (diinstal di B2) |
| JUCE | 8.0.15 | Git submodule `third_party/JUCE` (pin tag `8.0.15`) |
| Git | 2.43.0 | Termasuk submodule JUCE (clone dengan `--recurse-submodules` atau `git submodule update --init`) |

## Clone (setelah repository ada di GitHub)

```sh
git clone --recurse-submodules <url-repo>
```

Bila clone tanpa `--recurse-submodules`:

```sh
git submodule update --init --recursive
```

## Build host (Linux)

```sh
cmake --preset host-release
cmake --build --preset host-release --parallel 2
ctest --preset host-release
```

Hasil: `build/host-release/` berisi `libmuse_dsp.a`, `muse_dsp_smoke_test`,
dan `juce_smoke_test` (console app pembuktian integrasi JUCE).

## Cross-compile shared core untuk Android (arm64-v8a)

```sh
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

Hasil: `build/android-arm64-v8a/lib/libmuse_dsp.a` (arm64-v8a).
Ini HANYA membuktikan toolchain NDK + shared core berhasil di-cross-compile.
Bukan APK, bukan pengujian perangkat (Phase 7).

## Build Windows

- Lokal (mesin Windows + Visual Studio 2022): lihat
  `platform/windows/README.md`.
- CI: workflow `.github/workflows/windows-build.yml` berjalan otomatis saat
  push ke GitHub. **Belum pernah dijalankan** (repo masih lokal) — lihat
  laporan B2.

## Keterbatasan lingkungan (VM ini)

1. **RAM ketat** (~7,7 GB total, < 1 GB tersedia saat pengecekan): selalu
   build Release dan **maksimal `-j 2`**. Build JUCE memakai `-O2`
   (bukan `-O3` default CMake) untuk menghemat puncak RAM — lihat
   `CMakeLists.txt`.
2. **`/tmp` hanya 512 MB (tmpfs).** Jangan mengunduh file besar ke `/tmp`
   (unduhan NDK 722 MB pernah gagal karena ini). Pakai
   `~/workspace/android-sdk/downloads/` atau direktori di bawah `~/workspace/`.
3. **`sdkmanager --list` gagal** (proxy tunneling Java). Instalasi NDK
   dilakukan via unduhan langsung `dl.google.com` + verifikasi SHA-1.
   Jangan andalkan `sdkmanager install` tanpa menguji dulu.
4. **Tidak ada perangkat audio di VM** (`/dev/snd` tidak ada; `adb devices`
   kosong). Uji dengar/latensi hanya bisa dilakukan di perangkat fisik.
5. **Tidak ada toolchain Windows di VM.** Build Windows hanya lewat CI
   GitHub Actions atau mesin Windows fisik.
