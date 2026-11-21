# LAPORAN B1 — PERSIAPAN LINGKUNGAN & DEPENDENCY (GATE B, PHASE 1)

- Tanggal: 2026-11-21 16:07–16:15 WIB
- Repository: `~/workspace/muse-guitar-fx/`
- Backup Gate A: `~/workspace/backups/muse-guitar-fx-phase0-2026-11-21/` (tidak disentuh)

## A. Status

**Berhasil.** Lingkungan siap untuk B2 dengan dua catatan yang sudah punya jalan keluar (lihat F): instalasi NDK harus lewat unduhan langsung (bukan `sdkmanager --list`), dan RAM VM ketat sehingga build harus `-j 2` Release. Tidak ada blocker yang menghentikan B2.

## B. Perubahan

### File baru
| File | Alasan |
|---|---|
| `docs/dependencies.md` | Dokumen dependency + lisensi yang diwajibkan sejak Phase 1 (nama, versi, sumber, lisensi, implikasi distribusi, status instalasi). Termasuk evaluasi JUCE 8.0.15 vs 9.0.2 dan koreksi lisensi JUCE terhadap Phase 0. |
| `docs/phase1-b1-report.md` | Laporan ini (format A–G sesuai prompt Gate B). |

### File yang diubah
- Tidak ada. Dokumen Phase 0 tidak disentuh.

### File yang dihapus
- Tidak ada.

### Backup Gate A
- Tidak dihapus, dipindah, atau ditimpa. Diverifikasi masih utuh: arsip 22.006 byte + `MANIFEST.sha256` + checksum arsip + `BACKUP-REPORT.md` masih ada di `~/workspace/backups/muse-guitar-fx-phase0-2026-11-21/`.

## C. Dependency

Rincian lengkap ada di `docs/dependencies.md`. Ringkasan:

| Nama | Versi | Sumber | Lisensi | Status instalasi |
|---|---|---|---|---|
| JUCE | **8.0.15** (dipin) | `github.com/juce-framework/JUCE`, tag `8.0.15` | AGPLv3 atau EULA komersial (Starter gratis ≤ USD 20.000/tahun) | Belum difetch — fetch di B2 (git submodule) |
| Oboe | 1.10.0 (terbundel JUCE) | Terbawa JUCE | Apache 2.0 | Terbawa JUCE |
| CMake (host) | 3.28.3 | Paket sistem | BSD 3-clause | Terinstal, terverifikasi |
| CMake-for-Android | Terbaru dari sdkmanager (B2) | `sdkmanager` | BSD 3-clause | Belum terinstal |
| GCC / G++ | 13.3.0 | Paket sistem | GPLv3 + Runtime Exception | Terinstal, terverifikasi |
| Android NDK | **r28c (28.2.13676358)** direkomendasikan; alternatif r29 / LTS r27d | `dl.google.com` (unduhan langsung) | Android SDK License Agreement | Belum terinstal — instal di B2 |
| JDK Temurin | 21.0.13+9 LTS | Sudah ada di VM | GPLv2 + Classpath Exception | Terinstal, terverifikasi |
| Android SDK | build-tools 34.0.0, platform-tools 37.0.1, platform android-34/35 | Sudah ada di VM | Android SDK License Agreement | Terinstal, terverifikasi |
| ALSA libasound2-dev | 1.2.11 | Paket sistem | LGPL-2.1 | Terinstal, terverifikasi |
| GitHub Actions | `windows-latest` | Workflow dibuat di B2 | — (layanan CI) | Belum dibuat |
| ASIO SDK | — | Tidak difetch (Phase 8) | Dual GPLv3/proprietary (Okt 2025) | Rencana Phase 8 |

**Keputusan versi JUCE: 8.0.15.** 9.0.2 dievaluasi dan ditolak untuk saat ini: 9.0.2 sudah bukan patch 9.x terbaru (9.0.5 sudah ada), fitur barunya (variable font, SVG, performa OpenGL) tidak dibutuhkan DSP core baru, dan ada breaking change `ThreadPool::addJob`. Evaluasi ulang JUCE 9.x dijadwalkan Phase 7/8.

**Koreksi lisensi penting:** Phase 0 menulis JUCE "dual GPLv3; Personal gratis di bawah USD 50 ribu". Verifikasi B1 (beberapa sumber independen) menunjukkan yang benar: **AGPLv3** + EULA komersial sejak JUCE 8, tier gratis **Starter ≤ USD 20.000** pendapatan/pendanaan tahunan. Koreksi ini dicatat di `docs/dependencies.md`; dokumen Phase 0 tidak diubah, tetapi dokumen ini adalah acuan lisensi yang berlaku.

## D. Build

- Perintah yang dijalankan di B1: hanya perintah pemeriksaan (bukan build).
  - `cmake --version` → 3.28.3
  - `g++ --version` / `gcc --version` → 13.3.0
  - `git --version` → 2.43.0
  - `java -version` (JAVA_HOME Temurin) → 21.0.13+9 LTS
  - `sdkmanager --list_installed` → build-tools 34.0.0, platform-tools 37.0.1, platform android-34/35
  - `curl` ke `dl.google.com` dan `github.com` → host dapat dijangkau
  - `sha256sum` 6 file Phase 0 → cocok dengan baseline audit recovery
  - `ls /dev/snd`, `adb devices`, `pkg-config --modversion alsa` → lihat F
- Hasil: **tidak ada build di B1** (sesuai scope — B1 hanya persiapan dan evaluasi). Tidak ada error build karena tidak ada yang di-build.

## E. Test

- Test yang dijalankan: tidak ada test kode di B1. Yang dilakukan adalah **verifikasi lingkungan**:
  - Hash SHA-256 keenam file Phase 0 cocok persis dengan baseline audit recovery (repository utuh).
  - Backup Gate A utuh (arsip + manifest + checksum + laporan masih ada).
  - Toolchain host terverifikasi versinya (lihat D).
  - Keterjangkauan jaringan untuk fetch dependency terverifikasi via curl (`github.com` 200; `dl.google.com` merespons).
- Test yang belum dapat dilakukan: semua test audio (offline render, unit test EffectChain, smoke test ALSA) — baru ada di B3/B4 setelah kode ditulis. Uji perangkat fisik tetap menunggu Galaxy M32 + USB audio interface yang belum ditentukan.

## F. Risiko dan kendala

1. **sdkmanager remote list gagal (temuan baru B1).** `sdkmanager --list` gagal dengan `java.util.NoSuchElementException` di `HttpURLConnection.doTunneling` — proxy Java gagal saat mengambil daftar paket remote. `--list_installed` tetap berfungsi. Dampak: instalasi NDK lewat `sdkmanager install "ndk;..."` kemungkinan gagal dengan cara yang sama. **Jalan keluar untuk B2 (sudah terverifikasi jalurnya): unduh ZIP NDK langsung dari `dl.google.com` via curl, lalu ekstrak manual ke `~/workspace/android-sdk/ndk/<versi>/`.** Klaim Phase 0 bahwa "remote fetch terverifikasi berfungsi" dikoreksi: hanya berlaku untuk `--list_installed` dan unduhan langsung, bukan daftar remote sdkmanager.
2. **RAM ketat.** 7,7 GB total, ~767 MB tersedia saat pengecekan. Build JUCE di B2 wajib Release dan `-j 2`; bila build kehabisan memori, kurangi ke `-j 1`.
3. **Tidak ada perangkat audio di VM.** `/dev/snd` tidak ada; `adb devices` kosong (diverifikasi ulang di B1). Konsekuensi tidak berubah dari Phase 0: smoke test ALSA di B4 hanya bisa membuktikan engine start/stop dengan bersih via mock device dan gagal anggun tanpa hardware; uji dengar/latensi tetap di perangkat fisik.
4. **Lisensi JUCE (AGPLv3).** Bila produk akhir dirilis di bawah AGPLv3, seluruh source aplikasi harus dibuka (termasuk klausul Affero untuk akses jaringan). Bila closed-source dan pendapatan > USD 20.000/tahun, wajib lisensi Indie/Pro. Keputusan final tetap ditunda sampai sebelum Phase 9 (keputusan user), tetapi fakta ini memengaruhi arsitektur distribusi — dicatat sejak B1.
5. **ASIO belum difetch.** Sesuai instruksi: tidak diambil sebelum kebutuhan dan implikasi lisensinya jelas. Tidak menghambat B2–B7; baru relevan di Phase 8 (Windows).
6. **Visibilitas repo GitHub belum diputuskan.** Berpengaruh pada biaya CI Windows (gratis untuk repo publik; menit gratis terbatas untuk privat). Diputuskan sebelum workflow Windows pertama dijalankan di B2.

## G. Keputusan

- **Apakah B2 layak dimulai? Ya, dengan syarat:**
  1. Instalasi NDK di B2 memakai jalur unduhan langsung `dl.google.com` + ekstraksi manual (bukan `sdkmanager --list`/`install` yang gagal).
  2. Build host di B2 memakai Release + `-j 2` karena RAM ketat.
  3. JUCE dipin pada **8.0.15** via git submodule.
  4. NDK yang diinstal: **r28c (28.2.13676358)**; bila unduhan r28c bermasalah, fallback ke r29 (29.0.14206865) atau LTS r27d.
- **Syarat yang belum terpenuhi:** tidak ada yang menghalangi B2. Yang masih terbuka dan tidak menghambat B2: visibilitas repo GitHub (diputuskan sebelum CI Windows dijalankan), keputusan lisensi final produk (sebelum Phase 9), USB audio interface untuk uji (belum ditentukan; pengujian B3/B4 memakai mock + offline).
- **Langkah berikutnya menunggu persetujuan user:** B2 (struktur proyek + CMake + integrasi JUCE + CI Windows). Saya berhenti di sini sesuai aturan Gate B.
