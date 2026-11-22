# Verifikasi Akhir Phase 1 — Windows CI

Laporan akhir Tahap 4 dari instruksi "MUSE — FINAL VERIFICATION PHASE 1:
WINDOWS CI" (2026-11-22). Tahap 1 (audit pra-publikasi) selesai 2026-11-22
~16:30 WIB; Tahap 2 (persiapan GitHub) dan Tahap 3 (Windows CI) selesai
2026-11-22 ~17:09 WIB.

## Ringkasan eksekutif

- **Status akhir Phase 1: SELESAI.** Windows CI pertama yang hijau
  (run 45010775935) membuktikan build + seluruh unit test Phase 1 berjalan
  di Windows (Visual Studio 2022, Release).
- **Repository:** https://github.com/haisagagaa-boop/muse-guitar-fx
- **Visibilitas:** publik (diputuskan user 2026-11-22 16:26 WIB).
- **Lisensi repo:** GNU AGPLv3 (`LICENSE`, 661 baris, teks resmi gnu.org).
- **Commit yang diuji:** `156cb95`
  (`156cb953fa05be878c139723671c6868de76e78d`), branch `main`.
- **Phase 2 TIDAK dimulai** tanpa persetujuan eksplisit user (aturan Gate B).

## Commit yang diuji

Riwayat `main` saat verifikasi (7 commit):

| Commit | Isi |
|---|---|
| `d925e2b` | B2: struktur proyek, CMake, JUCE 8.0.15 (submodule), NDK r28c, CI Windows |
| `a476b56` | B3: fondasi audio engine (interface, EffectChain, parameter atomic+smoothing, mock device) |
| `e9dddc1` | B4: pengujian dan integrasi awal (Catch2, offline render, batasan Android) |
| `f7d3be1` | LICENSE (GNU AGPLv3) — temuan audit Tahap 1 |
| `a517d4f` | Perbaikan CI: runner dipin ke windows-2022 (lihat Kendala 2) |
| `c1838c0` | Perbaikan mock pacing: spin-wait 1 ms terakhir (lihat Kendala 3) |
| `156cb95` | Perbaikan mock pacing Windows: timeBeginPeriod(1) (lihat Kendala 3) |

Working tree bersih saat push; 48 file ter-track (47 + LICENSE). Submodule
JUCE 8.0.15 di `91ad83ae` (URL publik, checkout CI tanpa autentikasi).

## URL dan visibilitas repository

- URL: https://github.com/haisagagaa-boop/muse-guitar-fx
- Visibilitas: **publik**. Akun GitHub: `haisagagaa-boop`.
- Remote lokal: `origin`
  `https://github.com/haisagagaa-boop/muse-guitar-fx.git` (branch `main`
  ter-track).
- Biaya CI: repo publik → GitHub Actions gratis (tidak ada biaya).
  (Untuk referensi: repo privat di GitHub Free mendapat 2.000 menit/bulan;
  workflow ini ~2,5 menit/run.)

## Workflow yang dijalankan

- File: `.github/workflows/windows-build.yml`
- Workflow: **Windows build**, job `build-windows`.
- Runner: `windows-2022` (dipin — lihat Kendala 2).
- Trigger: push ke `main` (juga `pull_request` dan `workflow_dispatch`).
- Langkah: checkout (actions/checkout v4, dipin ke SHA penuh
  `11d5960a326750d5838078e36cf38b85af677262`, `submodules: recursive`)
  → Configure (`cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  -DCMAKE_BUILD_TYPE=Release`) → Build (`cmake --build build --config
  Release --parallel 2`) → Test (`ctest --test-dir build -C Release
  --output-on-failure`).
- JUCE yang dipakai CI: submodule `third_party/JUCE` (8.0.15, sesuai pin B1).

## Hasil Windows build (run hijau)

Run **45010775935** — https://github.com/haisagagaa-boop/muse-guitar-fx/actions/runs/45010775935

- Kesimpulan: **success**. Durasi: 2 menit 23 detik.
  Dimulai 2026-11-22 17:06:20 WIB, selesai 17:08:46 WIB.
- Langkah: Set up job ✓, checkout ✓, Configure ✓, Build ✓, Test ✓.
- Runner image: `windows-2022`.
- Build menghasilkan semua target default, termasuk `muse_dsp`,
  `muse_dsp_smoke_test`, `engine_check`, `effect_chain_tests`,
  `offline_render`, dan `juce_smoke_test` (terbukti dari CTest yang
  menjalankan keenamnya).

## Hasil Windows test (run hijau)

`ctest --test-dir build -C Release --output-on-failure` — **6/6 lolos,
100% tests passed, 0 tests failed**:

| # | Test | Hasil | Waktu |
|---|---|---|---|
| 1 | muse_dsp_smoke_test | Passed | 0,01 dtk |
| 2 | engine_check | Passed | 1,12 dtk |
| 3 | effect_chain_tests | Passed | 0,01 dtk |
| 4 | offline_render_256 | Passed | 0,01 dtk |
| 5 | offline_render_1024 | Passed | 0,01 dtk |
| 6 | juce_smoke_test | Passed | 0,01 dtk |

Catatan: `engine_check` lolos berarti 27 checks, 0 failures, dan blok mock
≥ 80 dalam 0,5 detik (ambang yang sama dengan yang gagal di run 2–3).
Angka pastinya tidak tampil di log karena CTest `--output-on-failure`
hanya menampilkan output tes yang gagal. Verifikasi host (Linux) setelah
perbaikan yang sama: ctest 6/6, `engine_check` 27 checks 0 failures,
blok stabil 94.

## Riwayat run CI (semua run)

| Run | Commit | Hasil | Durasi | Penyebab / catatan |
|---|---|---|---|---|
| 45009798107 | `f7d3be1` | failure (Configure) | 29 dtk | Kendala 2: windows-latest = image VS 2026 |
| 45009915655 | `a517d4f` | failure (Test) | 3 mnt 4 dtk | Kendala 3: engine_check 63 blok |
| 45010340962 | `c1838c0` | failure (Test) | 3 mnt 3 dtk | Kendala 3: engine_check 66 blok |
| **45010775935** | **`156cb95`** | **success** | **2 mnt 23 dtk** | **Verifikasi — 6/6 lolos** |
| 45010815379 | `0282cc9` | success | 3 mnt 1 dtk | Edit judul README oleh user di GitHub (1 baris, tanpa perubahan kode; kode sama dengan `156cb95`) |

Log mentah keempat run tersimpan di
`~/workspace/muse-guitar-fx-ci-logs/run-<id>.log` (di VM, di luar repo).

## Kendala yang ditemukan

### Kendala 1 — Push ditolak: token OAuth tanpa scope `workflow`
- Gejala: repo berhasil dibuat, tetapi `git push` ditolak —
  "refusing to allow an OAuth App to create or update workflow
  `.github/workflows/windows-build.yml` without `workflow` scope".
- Penyebab: token `gh` hasil login device-code pertama hanya punya scope
  `gist`, `read:org`, `repo`.
- Perbaikan: `gh auth refresh -h github.com -s workflow` (kode device
  kedua, dimasukkan user) + `gh auth setup-git` agar git memakai
  kredensial `gh` (tanpa ini push gagal "could not read Username").
  Setelah itu scope token: `gist`, `read:org`, `repo`, `workflow`.
- Catatan: token `gh` tersimpan plain-text di `~/.config/gh/hosts.yml`
  (perilaku standar `gh` di VM ini; jangan dibagikan ke siapa pun).

### Kendala 2 — `windows-latest` kini image VS 2026 (tanpa VS 2022)
- Gejala: run 45009798107 gagal dalam 29 detik di Configure —
  `Generator "Visual Studio 17 2022" could not find any instance of
  Visual Studio.`
- Penyebab: `windows-latest` kini mengarah ke image
  `windows-2025-vs2026` (Version 20261116.315.1), yang hanya membawa
  Visual Studio 2026, bukan 2022.
- Perbaikan: runner dipin ke `windows-2022` (commit `a517d4f`).
  Jangan kembalikan ke `windows-latest` tanpa mengganti generator ke
  "Visual Studio 18 2026" dan memvalidasi ulang JUCE 8.0.15 + CMake
  terhadap toolchain itu.
- Tidak ada test yang diubah atau dihapus untuk kendala ini.

### Kendala 3 — Mock audio device tertinggal dari real-time di Windows
- Gejala: run 2 dan 3 gagal di Test — `engine_check` check 6
  ("blok diproses minimal 80 dalam 0.5 detik"): run 2 = 63 blok,
  run 3 = 66 blok. Lima tes lain lolos di kedua run.
- Diagnosis: BUKAN cacat engine — callback hanya ~26–42 us (jauh di
  bawah durasi blok 5,333 ms = 256 sampel @ 48 kHz), 0 overrun, RMS
  steady-state 0,1768/0,1769 tepat (diharapkan 0,1768). Yang gagal
  adalah pacing mock: `sleep_until` di Windows bangun dengan
  granularitas timer default ~15,6 ms — lebih kasar dari durasi blok
  5,333 ms — sehingga tiap iterasi molor dan deadline yang terlewat
  tidak dikejar (by design, anti-spiral).
- Perbaikan bertahap (tanpa mengubah ambang tes 80 blok):
  1. `c1838c0` — spin-wait 1 ms terakhir sebelum deadline. Tidak
     cukup (63 → 66 blok): bila tidur bangun SETELAH deadline, spin
     sudah tidak ada gunanya.
  2. `156cb95` — `timeBeginPeriod(1)` selama device berjalan
     (cara standar aplikasi audio di Windows) + tetap spin-wait.
     Dengan resolusi timer 1 ms, tidur bangun tepat waktu dan spin
     menutup sisanya. Hasil: run 4 hijau, `engine_check` lolos.
- Lingkup perubahan: hanya `MockAudioDevice` (infrastruktur pengujian)
  + link `winmm` di `core/CMakeLists.txt`, didokumentasikan sebagai
  satu-satunya pengecualian platform di core (lihat
  `core/include/muse_dsp/mock_audio_device.h`). Tidak ada perubahan
  pada EffectChain, parameter, atau tes mana pun — ambang 80 blok
  tetap, tidak diturunkan.

## Temuan audit Tahap 1 (sudah ditindaklanjuti)

Audit pra-publikasi 2026-11-22 ~16:30 WIB (read-only, sebelum push apa pun):

1. Git bersih: HEAD `e9dddc1` saat itu, 3 commit, 47 file ter-track,
   tanpa build artifact.
2. Secret/personal data: BERSIH — tidak ada API key, token, password,
   atau credential di seluruh commit; identitas git hanya
   `Bex <bex@local>`. Satu-satunya penyebut perangkat adalah model
   "Samsung Galaxy M32 (SM-M325FV)" di dokumentasi (bukan data pribadi).
3. Lisensi: JUCE AGPLv3/komersial dan Catch2 BSL-1.0 terdokumentasi di
   `docs/dependencies.md`; ASIO belum diambil (ditunda ke Phase 8).
   **Temuan: repo belum punya file LICENSE** → diperbaiki commit
   `f7d3be1` (AGPLv3) sebelum push.
4. Backup Gate A utuh: tar.gz SHA-256 `499a08ae…7c5f` cocok, manifest
   7/7 OK, file-file Phase 0 identik dengan repo.
5. Dokumentasi lengkap (13 dokumen saat audit).
6. Workflow YAML valid; JUCE submodule publik (checkout CI tanpa auth).
7. **Visibilitas saat audit belum diputuskan** → diputuskan user:
   publik (16:26 WIB). Koneksi GitHub via tautan Muse gagal (tautan
   membuka sesi Muse lain); diselesaikan lewat `gh` CLI device-code
   flow (lihat Kendala 1).

## Peringatan yang tersisa (bukan kegagalan)

- Anotasi di setiap run: "Node.js 20 is deprecated... actions/checkout
  ... forced to run on Node.js 24". Hanya peringatan — checkout tetap
  berjalan. Pin SHA checkout dipertahankan sesuai keputusan B2
  (reproducibility); migrasi ke checkout v5 (Node 24) adalah
  pekerjaan housekeeping, bukan bagian Phase 1.
- Token `gh` tersimpan plain-text di VM (lihat Kendala 1).

## Batasan yang tetap berlaku (dari B4)

- Verifikasi ini membuktikan host build + unit test di Windows.
  BUKAN bukti uji perangkat fisik, latensi round-trip, atau audio USB
  — semua itu tetap menunggu perangkat (Galaxy M32, USB audio
  interface) dan fase yang sesuai.
- Android: cross-compile saja (AArch64, tidak dijalankan); belum ada
  APK di Phase 1; JUCE Android shell adalah Phase 7.
- Tidak ada klaim latensi < 10 ms sebelum pengujian round-trip fisik.

## Status akhir Phase 1

**SELESAI.** Gate A (backup) + Gate B (B1–B4) + verifikasi Windows CI
hijau (run 45010775935, commit `156cb95`, 6/6 tes lolos di Windows).
Semua gerbang yang disyaratkan user terpenuhi dengan bukti aktual
(bukan klaim).

Langkah berikutnya HANYA atas persetujuan eksplisit user: **Phase 2**.
Jangan memulai Phase 2 dari dokumen ini.

---
*Dokumen ini di-commit setelah run 45010775935 hijau. Setelah run hijau,
user mengubah judul README langsung di GitHub (commit `0282cc9`, 1 baris,
tanpa perubahan kode); run 45010815379 atas commit itu juga hijau.
Dokumen ini di-rebase di atas commit tersebut sebelum push; push-nya
memicu satu run lagi (perubahan dokumen saja, kode sama dengan commit
yang diuji di atas).*
