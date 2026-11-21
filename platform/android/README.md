# Android — konfigurasi awal (B2, Phase 1)

## Status B2

- NDK **r28c (28.2.13676358)** terinstal di `~/workspace/android-sdk/ndk/28.2.13676358`
  (SHA-1 ZIP terverifikasi: `a7b54a5de87fecd125a17d54f73c446199e72a64`).
- Toolchain file NDK tersedia:
  `~/workspace/android-sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake`
- Yang dibuktikan B2: **shared core (`muse_dsp`) berhasil di-cross-compile
  untuk `arm64-v8a`** memakai toolchain file tersebut (preset
  `android-arm64-v8a`). Tanpa JUCE app, tanpa Gradle, tanpa APK.
- Yang BELUM ada (bukan klaim): APK, app shell Android (JUCE), pengujian di
  perangkat (Galaxy M32), pengujian USB audio. App shell Android + APK
  dikerjakan di Phase 7 sesuai roadmap.

## Cara build (konfigurasi awal)

```sh
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

Preset memakai `ANDROID_ABI=arm64-v8a` dan `ANDROID_PLATFORM=android-26`
(minSdk 26, sesuai target proyek) dan menonaktifkan target JUCE
(`MUSE_BUILD_JUCE_APPS=OFF`) — JUCE Android membutuhkan app shell Gradle
yang baru dibuat di Phase 7.

## Catatan instalasi NDK

- `sdkmanager --list` gagal di VM ini (proxy tunneling Java) — lihat
  `docs/dependencies.md` §4. NDK diinstal lewat unduhan langsung dari
  `dl.google.com` + verifikasi SHA-1 + ekstraksi manual ke
  `~/workspace/android-sdk/ndk/28.2.13676358`.
- Jangan mengunduh ulang NDK bila direktori di atas sudah ada dan
  `source.properties` menunjukkan `Pkg.Revision = 28.2.13676358`.
- CMake-for-Android (paket `cmake` sdkmanager) belum terinstal; untuk
  konfigurasi B2 dipakai CMake host 3.28.3 + toolchain file NDK. Paket
  `cmake` sdkmanager baru dibutuhkan saat build Gradle (Phase 7).
