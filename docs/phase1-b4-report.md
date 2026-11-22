# Laporan B4 — Pengujian dan Integrasi Awal (Phase 1, Gate B)

Tanggal: 2026-11-22 ~13:33–13:45 WIB
Status awal: B3 selesai (commit `a476b56`), repo bersih, B4 disetujui user
2026-11-22 13:33 WIB ("Lanjut ke B4").

## A. Status

**Berhasil, dengan satu butir yang tidak dapat dijalankan (CI Windows —
lihat E dan F.3).**

Semua pengujian yang bisa dijalankan di VM ini lolos:

- Unit test formal EffectChain (Catch2 v3.7.1): **34 assertions dalam
  8 test cases, semua lolos** — urutan, bypass, perubahan parameter,
  dan kesamaan output di ukuran blok 64/128/256/512/1024.
- Offline rendering test: render 1 detik sine 440 Hz stereo @ 48 kHz
  lewat EffectChain, tulis WAV 16-bit PCM (192.044 byte), verifikasi
  RMS/peak + header WAV — **lolos di blok 256 dan 1024**.
- `ctest` host: **6/6 lolos** (termasuk 3 pengujian B3 yang tidak berubah).
- Cross-compile Android arm64-v8a: semua target berhasil, terverifikasi
  ELF AArch64 (hanya dikompilasi, tidak dijalankan — tanpa perangkat).
- Keterbatasan pengujian Android didokumentasikan
  (`docs/android-testing-limitations.md`).
- **CI Windows TIDAK dapat dijalankan**: repo masih lokal tanpa remote
  GitHub, dan keputusan visibilitas repo (publik/privat) belum dijawab
  user. Workflow sudah dibuat (B2) dan YAML-nya tervalidasi ulang di B4 —
  tetapi tidak ada satu pun run CI yang terjadi. Ini dilaporkan apa adanya,
  bukan diklaim berhasil.

## B. Perubahan

### File baru (6)

| File | Isi |
|---|---|
| `third_party/catch2/catch_amalgamated.hpp` (524.559 byte) + `catch_amalgamated.cpp` (422.559 byte) + `README.md` | Catch2 v3.7.1 di-vendor (file amalgamated resmi); README mencatat sumber, versi, SHA-256 |
| `tests/test_processors.h` | Prosesor khusus tes (`OrderRecorderProcessor`, `DcOffsetProcessor`) — dipindah dari `engine_check.cpp` agar dipakai bersama suite Catch2 |
| `tests/effect_chain_tests.cpp` | Unit test formal EffectChain (Catch2): 8 test cases, 34 assertions |
| `tests/offline_render.cpp` | Offline rendering test + penulis WAV 16-bit PCM minimal |
| `docs/android-testing-limitations.md` | Keterbatasan pengujian Android (butir 8 B4) + cara menjalankan pengujian di perangkat bila HP sudah terhubung |

### File diubah (3)

| File | Perubahan | Alasan |
|---|---|---|
| `tests/CMakeLists.txt` | Target `catch2_amalgamated` (static lib), `effect_chain_tests`, `offline_render` + 2 entri CTest; link `log` di Android (lihat F.2); komentar framework diperbarui | Framework unit test diputuskan di B4: Catch2 v3.7.1 |
| `tests/engine_check.cpp` | Prosesor khusus tes dipindah ke `test_processors.h` (include + `using`) | Hindari duplikasi dengan suite Catch2; perilaku tidak berubah (ctest B3 tetap lolos) |
| `docs/dependencies.md` | Baris 11: Catch2 3.7.1, BSL-1.0, sumber, SHA-256, alasan pemilihan versi | Dokumentasi dependency baru |

### File dihapus

Tidak ada.

### Verifikasi tidak berubah

- Keenam file Phase 0 cocok dengan baseline SHA-256 audit recovery
  (README `b7ff2f0b…9c88`, architecture `c60f69a3…34cc`,
  audio-engine-design `05625ab6…9cf8`, development-roadmap
  `e745c682…250251`, platform-compatibility `2d24eefd…a204c`,
  testing-strategy `9285c9cb…57bb`).
- Backup Gate A utuh: `muse-guitar-fx-phase0-2026-11-21.tar.gz`
  SHA-256 `499a08ae…7c5f`, `gzip -t` OK.
- Versi core tidak berubah di B4 (tetap **0.2.0-b3**) — B4 hanya menambah
  pengujian, bukan kode core.

## C. Dependency

Satu dependency baru di B4; sisanya tidak berubah.

| Nama | Versi | Sumber | Lisensi | Status |
|---|---|---|---|---|
| **Catch2** | **3.7.1** (dipin, diputuskan di B4) | File amalgamated resmi dari rilis GitHub `catchorg/Catch2`, tag `v3.7.1` (commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14`) | **BSL-1.0** (permisif) | **Di-vendor di B4** — `third_party/catch2/`; SHA-256 hpp `91d47e2d…0a159b`, cpp `450fc5f2…cdb9ba`; macro versi terverifikasi 3.7.1. Test-only, tidak di-link ke binary aplikasi. |
| JUCE | 8.0.15 | git submodule (B2) | AGPLv3 / EULA komersial | Tidak berubah |
| Android NDK | r28c | `~/workspace/android-sdk/ndk/` | Android SDK License Agreement | Tidak berubah |
| CMake / GCC | 3.28.3 / 13.3.0 | sistem | BSD 3-clause / GPL+exception | Tidak berubah |
| ASIO SDK | — | — | Dual GPLv3/proprietary | **Tidak diambil** — tetap ditunda ke Phase 8 |

Catatan pemilihan versi Catch2: tag terbaru saat itu adalah v3.15.1,
tetapi rilis GitHub-nya **tidak menyertakan file amalgamated (404)** —
v3.7.1 adalah versi terbaru yang artefaknya tersedia sebagai file
amalgamated resmi. Keputusan ini dicatat di
`third_party/catch2/README.md` dan `docs/dependencies.md`.

## D. Build

### Host (Linux, Release, `-j 2`)

Perintah:

```sh
cmake --preset host-release
cmake --build --preset host-release --parallel 2
```

Hasil: configure OK; build **berhasil** (termasuk kompilasi Catch2
amalgamated ~35 detik total). Tidak ada error, tidak ada OOM.
Berbeda dengan B3, kali ini **tidak perlu instal ulang paket `-dev`**
(paket yang diinstal ulang di B3 masih ada — belum ada recycle VM
di antara B3 dan B4).

### Android (cross-compile, arm64-v8a)

Perintah:

```sh
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

Hasil: configure OK; build sempat **gagal di link `effect_chain_tests`**
— `undefined symbol: __android_log_write`. Penyebab: Catch2 memakai
`__android_log_write` (liblog Android) untuk debug output. Diatasi
dengan `target_link_libraries(catch2_amalgamated PUBLIC log)` bila
`ANDROID` di `tests/CMakeLists.txt` (perbaikan satu baris, tidak
mengubah kode). Build ulang **berhasil**. Semua binary pengujian
(`muse_dsp_smoke_test`, `engine_check`, `effect_chain_tests`,
`offline_render`) terverifikasi **ELF 64-bit ARM aarch64**. Hanya
dikompilasi — **tidak dijalankan** (tanpa perangkat).

### Windows CI

Tidak dijalankan — lihat E dan F.3.

## E. Test

### ctest host-release — 6/6 LULUS

| Test | Hasil |
|---|---|
| `muse_dsp_smoke_test` | Lulus (versi core 0.2.0-b3) |
| `engine_check` | Lulus — 27 checks (B3, tidak berubah) |
| `effect_chain_tests` | Lulus — **34 assertions dalam 8 test cases** |
| `offline_render_256` | Lulus — WAV 192.044 byte, RMS/peak sesuai |
| `offline_render_1024` | Lulus — WAV 192.044 byte, RMS/peak sesuai |
| `juce_smoke_test` | Lulus (JUCE 8.0.15) |

### Rincian `effect_chain_tests` (Catch2, 8 test cases)

1. Urutan eksplisit (panggil): 1,2,3 → move(0,2) → 2,3,1 → remove(1) →
   2,1 → insert(0,9) → 9,2,1; `clear()` mengosongkan chain.
2. Urutan mengubah hasil audio: DC lalu Gain = 3,0; Gain lalu DC = 2,5.
3. Bypass: Gain 0 di-bypass + DC 0,25 → 1,25; aktif → 0,25.
4. Chain kosong melewatkan audio apa adanya (stereo, 0,75).
5. Smoothing: ramp 20 ms @ 48 kHz — awal 1/960, tengah 0,5,
   akhir tepat 1,0, monoton, tanpa overshoot.
6. Perubahan parameter di tengah render: gain 0,5 → 1,0 di sampel 1024;
   sampel 1024 mulai naik, sampel 2047 tepat 1,0.
7. **Kesamaan output di semua ukuran blok (64/128/256/512/1024):**
   render sine + DC + perubahan gain di sampel 2048 (kelipatan semua
   ukuran blok) — perbedaan maks antar ukuran blok **< 1e-6** (praktis
   identik; smoothing bekerja per-sampel sehingga tidak bergantung pada
   ukuran blok).
8. Reset chain mengembalikan state prosesor (gain kembali ke nilai awal
   1,0).

Catatan: perubahan target di tengah render sengaja diletakkan di sampel
2048 — kelipatan semua ukuran blok yang diuji — agar terjadi di posisi
yang sama apapun ukuran bloknya. (Pelajaran dari penulisan awal yang
memakai sampel 2400 — bukan kelipatan 64/128/256/512/1024 — sehingga
perubahan tidak akan pernah terpicu di sebagian ukuran blok. Diperbaiki
sebelum build pertama.)

### Rincian `offline_render` (blok 256 dan 1024)

Render 1 detik sine 440 Hz stereo @ 48 kHz lewat EffectChain (Gain 0,5 →
1,0 di tengah render), tanpa thread real-time:

- RMS paruh pertama: **0,1768** (diharapkan 0,25/√2) — lolos.
- RMS kuartal terakhir: **0,3536** (diharapkan 0,5/√2) — lolos.
- Peak: **0,5000** (diharapkan ~0,5) — lolos.
- File WAV ditulis dan header-nya dibaca kembali (RIFF/WAVE/fmt/data,
  PCM 16-bit stereo 48 kHz, 48.000 frame) — lolos.
- Verifikasi independen dengan modul `wave` Python: stereo, 48 kHz,
  48.000 frame, 16-bit — cocok.

### Yang belum dapat dilakukan

- **CI Windows: TIDAK dijalankan.** Repo masih lokal tanpa remote GitHub
  (`git remote -v` kosong); keputusan visibilitas repo (publik/privat)
  belum dijawab user meski sudah ditanyakan 3× (22 Nov pagi, laporan B3,
  dan pesan B4). Workflow `.github/workflows/windows-build.yml` sudah
  ada sejak B2 dan YAML-nya **tervalidasi ulang di B4** (job
  `build-windows`, `runs-on: windows-latest`), tetapi validasi YAML
  bukan run CI. **Tidak ada klaim CI lolos.**
- Uji dengar / latensi round-trip di perangkat fisik — tidak ada
  perangkat audio di VM, tidak ada HP terhubung. **Tidak ada klaim
  latensi <10 ms.**
- Pengujian di perangkat Android — hanya cross-compile; cara
  menjalankannya di HP didokumentasikan di
  `docs/android-testing-limitations.md`.

## F. Risiko dan kendala

1. **CI Windows belum pernah jalan.** Ini satu-satunya butir B4 yang
   tidak selesai — terhalang keputusan user (visibilitas repo), bukan
   masalah teknis. Workflow siap; begitu repo dibuat dan di-push, run
   pertama akan terlihat di tab Actions GitHub.
2. **Catch2 di Android butuh liblog.** Link `effect_chain_tests`
   untuk arm64-v8a sempat gagal (`__android_log_write`); diatasi dengan
   link `log` di CMake (B4). Ini peringatan bahwa dependency pengujian
   pun punya kebutuhan platform — dicatat di `tests/CMakeLists.txt`.
3. **Klaim "callback bebas alokasi" masih desain + inspeksi** (seperti
   dicatat di B3) — B4 tidak menambah allocation counter. Uji beban
   (mock berjalan lama + pantau RSS) masih bisa ditambahkan bila user
   menginginkannya.
4. **Perubahan struktur chain TIDAK real-time safe** (seperti dicatat
   di B3) — tidak berubah di B4.
5. **Paket `-dev` sistem bisa hilang lagi saat VM di-recycle** (temuan
   B3) — kali ini tidak terjadi karena belum ada recycle di antara B3
   dan B4; perintah instal ulang tetap terdokumentasi di
   `docs/dependencies.md` §1a.
6. **Tidak ada efek gitar dan tidak ada UI di Phase 1** — sesuai
   ruang lingkup; efek sesungguhnya adalah Phase 2.

## G. Keputusan

- **Gate B selesai secara substansi, kecuali run CI Windows pertama
  yang terhalang keputusan user.** Semua fondasi Phase 1 ada dan
  teruji: struktur proyek, build system, shared core, EffectChain,
  fondasi audio processing, mock device, unit test + offline render
  test, lisensi dan dependency terdokumentasi, semua perubahan tercatat.
- **Phase 2 TIDAK boleh dimulai tanpa persetujuan eksplisit user**
  (aturan Gate B).
- Syarat yang belum terpenuhi:
  1. Keputusan visibilitas repo GitHub (publik = gratis; privat = menit
     gratis terbatas) + pembuatan repo + push `main` — baru setelah itu
     CI Windows pertama bisa dijalankan dan hasilnya dilaporkan.
  2. Persetujuan eksplisit user untuk Phase 2 (bila ingin lanjut).

**BERHENTI — menunggu keputusan user. Jangan mulai Phase 2.**
