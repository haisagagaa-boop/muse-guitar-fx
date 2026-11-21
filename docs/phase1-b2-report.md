# LAPORAN B2 — STRUKTUR PROYEK & BUILD SYSTEM (GATE B, PHASE 1)

- Tanggal: 2026-11-21 16:10–16:30 WIB
- Repository: `~/workspace/muse-guitar-fx/` (git, branch `main`, commit awal dibuat di akhir B2)
- Backup Gate A: `~/workspace/backups/muse-guitar-fx-phase0-2026-11-21/` (tidak disentuh; diverifikasi utuh di akhir B2)

## A. Status

**Berhasil, dengan satu pengecualian yang dilaporkan apa adanya: CI Windows
belum bisa dijalankan.** Workflow GitHub Actions sudah dibuat dan YAML-nya
tervalidasi, tetapi repository masih lokal (tanpa remote GitHub) sehingga
belum ada run CI apa pun. Menjalankan CI membutuhkan keputusan user (buat
repository GitHub + tentukan visibilitas). Semua target B2 yang lain selesai
dan terverifikasi: struktur proyek, JUCE 8.0.15 sebagai submodule, CMake
host + Android, build Release `-j 2`, dan kedua smoke test lolos.

## B. Perubahan

### Struktur direktori

```
muse-guitar-fx/
├── .github/workflows/windows-build.yml   # CI Windows (GitHub Actions)
├── .gitignore
├── .gitmodules                            # submodule JUCE
├── CMakeLists.txt                         # root: C++17, opsi, subdirektori
├── CMakePresets.json                      # preset host-release + android-arm64-v8a
├── README.md                              # Phase 0 — TIDAK DIUBAH
├── apps/
│   ├── CMakeLists.txt
│   └── host-smoke/                        # console app pembuktian integrasi JUCE
│       ├── CMakeLists.txt
│       └── main.cpp
├── core/                                  # shared DSP core (JUCE-independent)
│   ├── CMakeLists.txt
│   ├── include/muse_dsp/muse_dsp.h
│   └── src/muse_dsp.cpp
├── docs/                                  # dokumen Phase 0 + B1 + B2
│   ├── build.md                           # BARU: cara build host/Android/Windows
│   ├── dependencies.md                    # DIUBAH: status B2 + §1a (deps sistem host)
│   ├── phase1-b1-report.md
│   ├── phase1-recovery-audit.md
│   └── phase1-b2-report.md                # laporan ini
├── platform/
│   ├── android/README.md                  # BARU: konfigurasi awal NDK r28c
│   └── windows/README.md                  # BARU: build lokal VS2022 + CI
├── tests/                                 # smoke test shared core
│   ├── CMakeLists.txt
│   └── muse_dsp_smoke_test.cpp
└── third_party/JUCE/                      # git submodule (pin tag 8.0.15)
```

### File baru (17)

`.gitignore`, `.gitmodules`, `CMakeLists.txt`, `CMakePresets.json`,
`core/CMakeLists.txt`, `core/include/muse_dsp/muse_dsp.h`,
`core/src/muse_dsp.cpp`, `tests/CMakeLists.txt`,
`tests/muse_dsp_smoke_test.cpp`, `apps/CMakeLists.txt`,
`apps/host-smoke/CMakeLists.txt`, `apps/host-smoke/main.cpp`,
`platform/android/README.md`, `platform/windows/README.md`,
`.github/workflows/windows-build.yml`, `docs/build.md`,
`docs/phase1-b2-report.md`.

### File diubah (1)

- `docs/dependencies.md` — status JUCE (difetch di B2), NDK (terinstal di B2),
  CMake-for-Android (belum terinstal), GitHub Actions (workflow dibuat, belum
  dijalankan), plus sub-bab baru §1a (dependency sistem host yang diinstal di
  B2). Dokumen Phase 0 tidak diubah.

### File dihapus

- Tidak ada.

### Repository Git

- `git init -b main` (baru di B2; sebelumnya bukan git repo). Identitas commit
  lokal: `Bex <bex@local>` (sama seperti proyek CYCLOTRACK di VM ini).
- Commit awal dibuat di akhir B2 (semua file di atas + gitlink JUCE).
- Remote: **tidak ada** (`git remote -v` kosong) — repo masih lokal.

### Prinsip pemisahan platform (B2)

- `core/` (muse_dsp) **tidak bergantung pada JUCE, UI, atau platform apa pun**
  (C++17 murni) — satu kode dipakai host dan Android.
- JUCE dipakai oleh app shell (`apps/`) sebagai framework audio/UI.
- B2 tidak berisi efek DSP apa pun dan tidak berisi UI aplikasi — core hanya
  API versi; EffectChain dan efek dikerjakan di B3 / Phase 2+.

## C. Dependency — versi & checksum

| Nama | Versi | Checksum / bukti | Status |
|---|---|---|---|
| JUCE | 8.0.15 | Git submodule `third_party/JUCE`, tag `8.0.15`, commit `91ad83ae34a81e0833b1a2b0866f54846370ae53` (shallow clone, 110 MB); macro `JUCE_MAJOR_VERSION 8`, `MINOR 0`, `BUILDNUMBER 15` terverifikasi di `modules/juce_core/system/juce_StandardHeader.h` | Difetch di B2 |
| Android NDK | r28c (28.2.13676358) | ZIP `android-ndk-r28c-linux.zip` 722.261.334 byte, **SHA-1 `a7b54a5de87fecd125a17d54f73c446199e72a64`** (cocok dengan android/ndk wiki); `source.properties` Pkg.Revision 28.2.13676358; Clang 19.0.1 | Terinstal di B2 |
| CMake (host) | 3.28.3 | — | Terinstal (B1) |
| GCC / G++ | 13.3.0 | — | Terinstal (B1) |
| actions/checkout (CI) | v4 | Commit SHA penuh `11d5960a326750d5838078e36cf38b85af677262` (dipin di workflow, bukan tag bergerak) | Dipin di B2 |
| libcurl4-openssl-dev | 8.5.0 | pkg-config `libcurl` 8.5.0 | Diinstal di B2 |
| libxrandr-dev | 2:1.5.2 | pkg-config `xrandr` 1.5.2 | Diinstal di B2 |
| libxinerama-dev | 2:1.1.4 | pkg-config `xinerama` 1.1.4 | Diinstal di B2 |
| libxcursor-dev | 1:1.2.1 | pkg-config `xcursor` 1.2.1 | Diinstal di B2 |
| libxrender-dev | 1:0.9.10 | pkg-config `xrender` 0.9.10 | Diinstal di B2 |
| libxcomposite-dev | 1:0.4.5 | pkg-config `xcomposite` 0.4.5 | Diinstal di B2 |
| libxfixes-dev | 1:6.0.0 | pkg-config `xfixes` 6.0.0 | Diinstal di B2 |
| libfreetype6-dev | 2.13.2 | pkg-config `freetype2` 26.1.20 | Diinstal di B2 |
| libfontconfig1-dev | 2.15.0 | pkg-config `fontconfig` 2.15.0 | Diinstal di B2 |
| ASIO SDK | — | — | **Tidak diunduh** (sesuai instruksi; Phase 8) |
| CMake-for-Android | — | — | Belum terinstal (baru dibutuhkan saat build Gradle, Phase 7) |

## D. Build — perintah yang dijalankan & hasil aktual

Semua build Release, paralel maksimal `-j 2` (sesuai instruksi).

### 1. Host configure

```
cmake --preset host-release
```

- Hasil: **berhasil** (135,4 detik — sebagian besar waktu membangun `juceaide`,
  helper CMake JUCE yang dikompilasi saat configure).
- Catatan configure: `Found libcurl, version 8.5.0`; `webkit2gtk-4.0` dan
  `gtk+-x11-3.0` tidak ditemukan — tidak berbahaya (hanya dibutuhkan bila
  `JUCE_WEB_BROWSER` aktif; tidak aktif di B2).

### 2. Host build

```
cmake --build --preset host-release --parallel 2
```

- Hasil: **berhasil** (63 detik). Target yang ter-build: `muse_dsp`
  (`libmuse_dsp.a`, 1.634 byte), `muse_dsp_smoke_test` (ELF x86-64, 16.120
  byte), `juce_smoke_test` (3.449.392 byte, di
  `build/host-release/apps/host-smoke/juce_smoke_test_artefacts/Release/`).
- Modul JUCE yang terkompilasi: `juce_core`, `juce_audio_basics`,
  `juce_dsp`, `juce_audio_formats` (dependensi `juce_dsp`).
- Tidak ada OOM meski RAM tersedia hanya ~596 MB saat build (swap 1 GB
  sebagai bantalan; Release memakai `-O2`, bukan `-O3` default CMake —
  lihat `CMakeLists.txt`).

### 3. Android configure + build (shared core saja)

```
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

- Configure: **berhasil** (1,6 detik) — compiler Clang 19.0.1 dari NDK r28c.
- Build: **berhasil** (< 1 detik) — `libmuse_dsp.a` (3.354 byte) +
  `muse_dsp_smoke_test` untuk `arm64-v8a`.
- Verifikasi arsitektur: `file` + `llvm-readelf` memastikan binary
  `muse_dsp_smoke_test` adalah `ELF 64-bit LSB pie executable, ARM aarch64`.
- Ini HANYA membuktikan toolchain NDK + shared core berhasil
  di-cross-compile. Bukan APK dan bukan pengujian perangkat.

## E. Test

### Host — kedua smoke test lolos

```
./build/host-release/tests/muse_dsp_smoke_test
  muse_dsp version: 0.1.0-b2
  muse_dsp smoke test: PASS        (exit 0)

./build/host-release/apps/host-smoke/juce_smoke_test_artefacts/Release/juce_smoke_test
  MUSE Guitar FX — JUCE integration smoke test
  muse_dsp version: 0.1.0-b2
  JUCE version: 8.0.15
  juce::dsp::Gain: 1.0 * 0.5 = 0.500000
  JUCE smoke test: PASS            (exit 0)
```

```
ctest --preset host-release
  1/2 muse_dsp_smoke_test ... Passed
  2/2 juce_smoke_test ..... Passed
  100% tests passed, 0 tests failed out of 2
```

### Android

- Binary `arm64-v8a` terverifikasi arsitekturnya (lihat D.3). **Tidak
  dijalankan** — tidak ada perangkat Android terhubung (`adb devices`
  kosong); pengujian perangkat menunggu Phase 7.

### CI Windows

- **Belum dijalankan** (lihat §G dan bagian "Hasil CI Windows" di bawah).

## Hasil CI Windows

**Tidak ada hasil — workflow belum pernah berjalan.** Alasan: repository
masih lokal tanpa remote GitHub. Yang sudah dilakukan di B2:

- Workflow `.github/workflows/windows-build.yml` dibuat: job
  `build-windows` di `windows-latest`, checkout dengan `submodules:
  recursive`, configure Visual Studio 2022 x64 Release, build `--parallel 2`,
  `ctest -C Release`.
- YAML divalidasi dengan Python `yaml.safe_load`: valid, 1 job, 4 langkah,
  `actions/checkout` dipin ke SHA commit penuh.
- Visibilitas repository diperiksa: `git remote -v` kosong — belum ada
  repository GitHub, jadi belum ada keputusan publik vs privat dan belum ada
  biaya CI yang timbul (biaya $0 sejauh ini karena tidak ada run).
- Yang dibutuhkan untuk menjalankan CI: (1) buat repository GitHub,
  (2) tentukan visibilitas (publik = gratis; privat = menit gratis terbatas),
  (3) push branch `main`. **Jangan klaim "CI Windows lolos" sebelum run
  pertama benar-benar terlihat hijau di GitHub.**

## F. Kendala dan risiko

1. **JUCE membangun `juceaide` saat configure dan `juceaide` butuh header
   X11** (`Xrandr.h` dll.) — configure pertama gagal karena
   `libxrandr-dev` dkk. belum terinstal. Diselesaikan dengan menginstal 9
   paket `-dev` yang didokumentasikan JUCE (lihat C). Bukan masalah kode.
2. **`apt-get update` penuh terlalu lambat** (ratusan MB daftar paket;
   dihentikan setelah ~10 menit pada 375 MB daftar parsial). Diganti update
   minimal (suite `noble` + `noble-updates`, komponen `main` + `universe`,
   tanpa Translation): 16,7 MB dalam 10 detik. Konfigurasi apt sistem tidak
   diubah; sources sementara di `~/workspace/.cache/apt-minimal/`.
3. **`/tmp` hanya 512 MB (tmpfs).** Unduhan NDK pertama gagal di 511 MB
   (`curl: (23) Failure writing output`) karena `/tmp` penuh. Diselesaikan:
   file parsial dipindah ke `~/workspace/android-sdk/downloads/`, unduhan
   dilanjutkan (`curl -C -`), SHA-1 cocok. Pelajaran dicatat di
   `docs/build.md`: jangan mengunduh file besar ke `/tmp`.
4. **RAM ketat** (~596 MB tersedia saat build host; swap 1 GB). Build
   berhasil dengan `-j 2` + `-O2` tanpa OOM. Bila build JUCE di masa depan
   kehabisan memori, turunkan ke `-j 1` (masih dalam batas "maksimal -j 2").
5. **Submodule JUCE: `git submodule add --branch 8.0.15` gagal** (tag bukan
   branch; `origin/8.0.15 is not a commit`). Diselesaikan dengan clone
   biasa (`--depth 1 --branch 8.0.15`) + `.gitmodules` manual +
   `git submodule absorbgitdirs`. Hasil akhir terverifikasi sebagai
   submodule yang benar (`.git` berupa gitfile; `git submodule status`
   menunjukkan commit pin).
6. **Rilis NDK terbaru berubah:** per Nov 2026 LTS terbaru adalah r30
   (30.0.16248370); r28c kini tercantum di "Unsupported Downloads".
   r28c tetap dipakai sesuai instruksi B2 — masih dapat diunduh dan
   SHA-1-nya resmi. Evaluasi ulang ke r29/r30 hanya bila ada alasan teknis.
7. **Tidak ada toolchain Windows di VM** (sesuai Phase 0) dan **tidak ada
   perangkat audio** (`/dev/snd` tidak ada; `adb` kosong). Build Windows
   hanya lewat CI; uji dengar/latensi hanya di perangkat fisik.
8. **Lisensi JUCE (AGPLv3)** tetap berlaku seperti dicatat di B1 — belum ada
   keputusan final (sebelum Phase 9).

## G. Keputusan

- **Apakah B3 layak dimulai? Ya.**
  - Fondasi build terbukti di kedua sisi: host (core + JUCE ter-link dan
    bekerja offline) dan Android (core ter-cross-compile ke `arm64-v8a`).
  - Yang masih terbuka dan TIDAK menghambat B3: run pertama CI Windows
    (butuh keputusan repo GitHub + visibilitas), CMake-for-Android
    (Phase 7), USB audio interface (belum ditentukan; B3 memakai mock +
    offline), keputusan lisensi final produk (sebelum Phase 9).
  - B3 (fondasi audio engine: interface pemrosesan, EffectChain, parameter
    thread-safe + smoothing, mock audio device) dapat dimulai setelah
    persetujuan user.
- **Langkah berikutnya menunggu persetujuan user:** B3. Saya berhenti di
  sini sesuai aturan Gate B.
