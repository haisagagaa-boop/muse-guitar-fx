# Keterbatasan Pengujian Android (Phase 1, B4)

Dokumen ini mencatat dengan jujur apa yang sudah dan belum bisa diuji
untuk Android pada akhir Phase 1. Dibuat di B4 (2026-11-22) sesuai butir
8 ruang lingkup B4.

## Yang sudah terverifikasi

- **Cross-compile shared core ke arm64-v8a berhasil** (B2, B3, B4):
  `libmuse_dsp.a` dan binary pengujian (`muse_dsp_smoke_test`,
  `engine_check`, `effect_chain_tests`, `offline_render`) terverifikasi
  sebagai **ELF 64-bit ARM aarch64** lewat `file` (+ `llvm-readelf` di B2).
- Toolchain: Android NDK **r28c (28.2.13676358)**, Clang 19.0.1,
  `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-26` (minSdk 26,
  sesuai target proyek). CMake host 3.28.3 + toolchain file NDK
  (`$NDK/build/cmake/android.toolchain.cmake`).
- JUCE untuk Android **belum di-build** di Phase 1 — preset
  `android-arm64-v8a` membangun core + pengujian saja
  (`MUSE_BUILD_JUCE_APPS=OFF`); shell JUCE Android (Gradle + NDK)
  adalah pekerjaan Phase 7.

## Yang belum bisa diuji (dan alasannya)

1. **Tidak ada perangkat Android terhubung ke VM.**
   `adb devices` kosong; HP uji (Samsung Galaxy M32, SM-M325FV) milik
   user tidak terhubung ke VM ini. Tidak ada emulator yang dipakai —
   sesuai aturan proyek, emulator tidak diterima sebagai pengganti
   perangkat fisik untuk pengujian akhir.
2. **Tidak ada perangkat audio di VM** (`/dev/snd` kosong). Uji dengar,
   latensi round-trip, dan perilaku USB Audio Interface **tidak dapat**
   dilakukan di VM — wajib di perangkat fisik (Galaxy M32 + USB Audio
   Interface yang belum ditentukan). Tidak ada klaim latensi <10 ms.
3. **Binary pengujian AArch64 hanya dikompilasi, tidak dijalankan.**
   Menjalankan `engine_check` / `effect_chain_tests` di perangkat
   membutuhkan adb + perangkat yang sama — belum tersedia.
4. **Tidak ada APK di Phase 1.** Tidak ada aplikasi Android yang bisa
   diinstal/diuji; aplikasi (shell JUCE Android) baru ada di Phase 7.
5. **CMake-for-Android (paket `cmake` sdkmanager) belum terinstal.**
   Tidak dibutuhkan untuk konfigurasi CMake + NDK saat ini; baru
   dibutuhkan saat build Gradle (Phase 7).

## Cara menjalankan pengujian di perangkat (bila HP sudah terhubung)

```sh
# Dari VM, setelah HP terhubung via adb:
adb push build/android-arm64-v8a/tests/engine_check /data/local/tmp/
adb shell /data/local/tmp/engine_check
adb push build/android-arm64-v8a/tests/effect_chain_tests /data/local/tmp/
adb shell /data/local/tmp/effect_chain_tests
# offline_render menulis WAV ke path yang diberikan sebagai argumen:
adb push build/android-arm64-v8a/tests/offline_render /data/local/tmp/
adb shell /data/local/tmp/offline_render /data/local/tmp/render.wav 256
adb pull /data/local/tmp/render.wav
```

Catatan: binary ini adalah PIE executable AArch64 yang di-link dinamis
terhadap Bionic (`/system/bin/linker64`); tidak membutuhkan root.
`offline_render` menulis file WAV 16-bit PCM stereo 48 kHz yang bisa
didengarkan langsung dari HP.

## Risiko yang tersisa

- Perilaku thread + atomic di ARM64 baru terbukti di tingkat kompilasi,
  bukan eksekusi — ada kemungkinan (kecil) perbedaan perilaku runtime
  yang hanya terlihat di perangkat, mis. penjadwalan thread mock device
  di bawah beban. Pengujian perangkat di Phase 7 harus menjalankan
  ulang seluruh suite di atas di HP.
- Oboe/AAudio (audio Android sesungguhnya) belum disentuh sama sekali di
  Phase 1 — latensi dan stabilitas audio Android adalah risiko Phase 7,
  bukan Phase 1.
