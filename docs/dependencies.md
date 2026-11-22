# DEPENDENCY & LISENSI — MUSE GUITAR FX

- Dibuat: Phase 1, sub-tahap B1 (2026-11-21). Dokumen hidup: diperbarui setiap kali dependency ditambah/diubah.
- Aturan: jangan memakai dependency yang lisensinya belum jelas. Keputusan final lisensi produk ditunda sampai sebelum Phase 9 (keputusan user 2026-11-21), tetapi seluruh dependency, versi, lisensi, dan implikasi distribusinya didokumentasikan sejak Phase 1.
- **Koreksi terhadap Phase 0:** dokumen Phase 0 menulis JUCE "dual GPLv3/komersial; Personal gratis di bawah USD 50 ribu". Itu **tidak akurat**. Verifikasi B1 dari beberapa sumber independen (perbandingan framework Sonore, panduan lisensi yang dicek 14 Nov 2026, analisis lisensi proyek JUCE lain) menunjukkan: JUCE sudah dual-licensed **AGPLv3** + EULA komersial sejak JUCE 8, dan tier gratis **Starter** dibatasi **USD 20.000** pendapatan/pendanaan tahunan (bukan 50 ribu). Koreksi ini dicatat di sini dan di laporan B1; dokumen Phase 0 tidak diubah (sesuai aturan), tetapi dokumen ini adalah acuan lisensi yang berlaku mulai B1.

## 1. Dependency inti (dipakai / akan dipakai)

| # | Nama | Versi | Sumber | Lisensi | Implikasi distribusi | Status (B1) |
|---|---|---|---|---|---|---|
| 1 | JUCE | **8.0.15** (dipin, diputuskan di B1) | `github.com/juce-framework/JUCE`, tag `8.0.15` | **AGPLv3** atau EULA komersial (Starter gratis ≤ USD 20.000 pendapatan/pendanaan/tahun; Indie USD 40/user/bulan atau USD 800 perpetual ≤ USD 300.000; Pro USD 175/user/bulan atau USD 3.500 perpetual, tanpa batas) | Bila dirilis di bawah AGPLv3: seluruh source aplikasi harus dibuka di bawah AGPLv3, termasuk saat diakses lewat jaringan (klausul Affero). Bila closed-source dan pendapatan > USD 20.000/tahun: wajib lisensi Indie/Pro. Keputusan final sebelum Phase 9. Splash screen tidak lagi diwajibkan sejak JUCE 8. | **Difetch di B2** — git submodule `third_party/JUCE`, tag `8.0.15`, commit `91ad83ae34a81e0833b1a2b0866f54846370ae53` (shallow clone, 110 MB). Macro versi terverifikasi: 8.0.15. |
| 2 | Oboe (audio Android) | 1.10.0 (terbundel di JUCE 8.0.15) | Tergabung dalam JUCE (`juce_audio_devices` memakai Oboe di Android) | Apache 2.0 | Permisif; cukup sertakan pemberitahuan lisensi. Tidak ada kewajiban membuka source. | Terbawa JUCE — tidak ada fetch terpisah |
| 3 | CMake (host) | 3.28.3 | Paket sistem (Ubuntu) | BSD 3-clause | Build tool saja; tidak ikut terdistribusi dalam aplikasi. | Terinstal, terverifikasi |
| 4 | CMake-for-Android | Terbaru yang ditawarkan sdkmanager (diputuskan saat instalasi di B2) | `sdkmanager` (paket `cmake`) | BSD 3-clause | Build tool saja. | **Belum terinstal** — untuk konfigurasi B2 dipakai CMake host 3.28.3 + toolchain file NDK. Paket `cmake` sdkmanager baru dibutuhkan saat build Gradle (Phase 7). |
| 5 | GCC / G++ (host) | 13.3.0 | Paket sistem (Ubuntu) | GPLv3 + GCC Runtime Library Exception | Compiler saja; exception mengizinkan distribusi binary tanpa kewajiban GPL atas kode aplikasi. | Terinstal, terverifikasi |
| 6 | Android NDK | **r28c (28.2.13676358)** — direkomendasikan B1; alternatif r29 (29.0.14206865, stabil terbaru Okt 2025) atau LTS r27d (27.3.13750724, didukung s.d. pertengahan 2027) | `dl.google.com` (unduhan langsung) atau `sdkmanager` (paket `ndk`) | Android SDK License Agreement (proprietary Google) | Toolchain saja; tidak ada kewajiban membuka source aplikasi. Wajib menerima lisensi SDK (sudah diterima untuk paket yang terinstal). | **Terinstal di B2**: `~/workspace/android-sdk/ndk/28.2.13676358`, `source.properties` Pkg.Revision 28.2.13676358, Clang 19.0.1. ZIP `android-ndk-r28c-linux.zip` 722.261.334 byte, **SHA-1 terverifikasi `a7b54a5de87fecd125a17d54f73c446199e72a64`** (sumber: android/ndk wiki, Unsupported Downloads). Catatan: per Nov 2026 r30 (30.0.16248370) sudah menjadi LTS terbaru — r28c tetap dipakai sesuai instruksi B2; evaluasi ulang bila diperlukan. |
| 7 | JDK Temurin | 21.0.13+9 LTS | `~/workspace/english-learning-app/toolchain/jdk/` (sudah ada) | GPLv2 + Classpath Exception | Build tool saja. | Terinstal, terverifikasi |
| 8 | Android SDK (build-tools 34.0.0, platform-tools 37.0.1, platform android-34/35) | Lihat kolom versi | `~/workspace/android-sdk/` (sudah ada) | Android SDK License Agreement (proprietary Google) | Toolchain saja; lisensi sudah diterima. | Terinstal, terverifikasi |
| 9 | ALSA — libasound2-dev | 1.2.11 | Paket sistem (Ubuntu) | LGPL-2.1 | Library sistem Linux untuk smoke test host; dynamic linking tidak memaksa aplikasi menjadi LGPL. | Terinstal, terverifikasi |
| 10 | GitHub Actions (`windows-latest`) | Runner GitHub (versi berjalan) | `github.com` — workflow di `.github/workflows/` (dibuat di B2) | — (layanan CI; tidak ada dependency kode) | Repositori publik: gratis. Repositori privat: menit gratis terbatas, selebihnya berbayar. Visibilitas repo belum diputuskan — diputuskan sebelum workflow Windows pertama dijalankan. | Workflow dibuat di B2 (`.github/workflows/windows-build.yml`, checkout dipin ke SHA commit penuh). **Belum pernah dijalankan** — repo masih lokal, tanpa remote GitHub. |
| 11 | Catch2 (framework unit test) | **3.7.1** (dipin, diputuskan di B4) | File amalgamated resmi dari rilis GitHub `catchorg/Catch2`, tag `v3.7.1` (commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14`) | **Boost Software License 1.0 (BSL-1.0)** — permisif | Test-only: hanya di-link ke binary pengujian (`effect_chain_tests`), tidak ikut terdistribusi bersama produk. Cukup pertahankan header lisensi pada file yang di-vendor. | **Di-vendor di B4** — `third_party/catch2/catch_amalgamated.hpp` (524.559 byte, SHA-256 `91d47e2d…0a159b`) + `catch_amalgamated.cpp` (422.559 byte, SHA-256 `450fc5f2…cdb9ba`); macro versi terverifikasi: 3.7.1. Alasan v3.7.1: tag terbaru saat itu v3.15.1, tetapi rilisnya tidak menyertakan file amalgamated (404) — v3.7.1 adalah versi terbaru yang artefaknya tersedia. |

## 1a. Dependency sistem host yang diinstal di B2 (build JUCE di Linux)

JUCE membangun `juceaide` saat configure — `juceaide` memakai `juce_gui_basics`
sehingga membutuhkan header X11 + FreeType + Fontconfig. Paket-paket ini
diinstal di B2 dari Ubuntu noble (+ noble-updates), dan merupakan dependency
build yang didokumentasikan JUCE sendiri (`docs/Linux Dependencies.md`):

| Paket | Versi terinstal | Kebutuhan |
|---|---|---|
| libxrandr-dev | 2:1.5.2 | X11 Randr (juce_gui_basics) |
| libxinerama-dev | 2:1.1.4 | X11 Xinerama (juce_gui_basics) |
| libxcursor-dev | 1:1.2.1 | X11 XCursor (juce_gui_basics) |
| libxrender-dev | 1:0.9.10 | X11 Render (juce_gui_basics) |
| libxcomposite-dev | 1:0.4.5 | X11 Composite (juce_gui_basics) |
| libxfixes-dev | 1:6.0.0 | X11 Fixes (juce_gui_basics) |
| libfreetype-dev | 2.13.2+dfsg-1ubuntu0.2 | Font rendering (juce_graphics) |
| libfontconfig1-dev | 2.15.0 | Font config (juce_graphics) |
| libcurl4-openssl-dev | 8.5.0 | libcurl (juce_core, bila JUCE_USE_CURL=1 — di B2 tidak diaktifkan) |

Catatan instalasi: `apt-get update` penuh terlalu lambat di VM ini (ratusan MB
daftar paket; dihentikan setelah ~10 menit) — diganti update minimal (hanya
suite `noble` + `noble-updates`, komponen `main` + `universe`, tanpa
Translation) lewat sources file sementara di `~/workspace/.cache/apt-minimal/`;
konfigurasi apt sistem tidak diubah.

**Koreksi nama paket (B3):** nama paket FreeType yang benar adalah
`libfreetype-dev` (2.13.2+dfsg-1ubuntu0.2); `libfreetype6-dev` hanya nama
transisi dan tidak terinstal. Header yang dipakai JUCE sama
(`/usr/include/freetype2/freetype/freetype.h`).

**Paket sistem tidak bertahan saat VM di-recycle (temuan B3).** Saat configure
B3 (2026-11-22), build `juceaide` gagal dengan `fatal error:
X11/extensions/Xrandr.h: No such file or directory` — hampir semua paket
`-dev` di atas sudah hilang dari sistem karena VM di-recycle sejak B2
(21 Nov), sementara direktori home (repo, NDK, cache apt minimal) tetap utuh.
Perintah instal ulang yang sama dipakai lagi di B3 dan berhasil (semua 9
paket kembali terinstal; `libxrender-dev` ternyata sudah ada di image dasar):
`sudo apt-get -o Dir::Etc::sourcelist=$HOME/workspace/.cache/apt-minimal/ubuntu-min.sources -o Dir::Etc::sourceparts=$HOME/workspace/.cache/apt-minimal/empty.d -o Dir::State::lists=$HOME/workspace/.cache/apt-lists-min -o Acquire::Languages=none install -y libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev libxfixes-dev libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev`
(ikuti dengan `update` minimal yang sama bila daftar paket sudah usang).
Implikasi: setiap build JUCE pertama setelah recycle VM harus menginstal
ulang paket-paket ini dulu — dicatat di sini agar tidak perlu didiagnosis
ulang. Jangka panjang, opsi yang belum dievaluasi: mengekstrak header
`.deb` ke dalam home dan mengarahkan CMake ke sana.

## 2. Dependency yang SENGAJA TIDAK dipakai di Phase 1

| Nama | Alasan | Status |
|---|---|---|
| ASIO SDK | Hanya dibutuhkan untuk build Windows (Phase 8). Lisensi dual GPLv3/proprietary sejak Okt 2025 (mirror `github.com/audiosdk/asio`, resmi `steinberg.net/developers/asiosdk-open`). Binary Windows yang me-link ASIO jatuh di bawah GPLv3 kecuali lisensi komersial Steinberg diambil. **Tidak difetch di B1** sesuai instruksi; diverifikasi ulang saat dibutuhkan. | Rencana Phase 8 |
| Oboe standalone | Sudah terbundel di JUCE (Oboe 1.10.0 di JUCE 8.0.15). Fetch terpisah hanya menambah risiko versi ganda. | Tidak diperlukan |
| PortAudio / RtAudio | Ditolak di Phase 0 (JUCE sudah mencakup manajemen device + Oboe/ASIO/WASAPI). | Ditolak |
| Framework UI React Native / native Kotlin + JNI | Ditolak/ditunda di Phase 0; UI JUCE dipakai untuk kedua platform di Phase 1–6. | Ditunda (opsi Phase 7) |
| Library JSON eksternal (mis. nlohmann/json) | Tidak diperlukan: JUCE sudah punya `juce::JSON`/`juce::var` untuk preset JSON (Phase 5). | Tidak diperlukan |
| Framework unit test (Catch2 / GoogleTest) | Belum dipilih. Diputuskan di B3 saat unit test ditulis. Kandidat utama: Catch2 (BSL-1.0, permisif). Bukan dependency Phase 1 sampai diputuskan. | Belum diputuskan |

## 3. Evaluasi JUCE 8.0.15 vs 9.0.2 (keputusan B1)

| Aspek | JUCE 8.0.15 | JUCE 9.0.2 |
|---|---|---|
| Rilis | 21 Jul 2026 — rilis terakhir seri 8.x | 7 Sep 2026 |
| Status saat ini | Seri 8 sudah selesai; tidak ada patch 8.x baru setelah 8.0.15 | Seri 9 sudah berjalan; patch terbaru 9.0.5 (sudah ada saat B1). 9.0.2 bukan patch 9.x terbaru |
| Perubahan utama | Oboe 1.10.0; perbaikan scaling GUI Windows; min. target Windows 1607 | MP3AudioFormat aktif by default; performa OpenGL; perbaikan CoreAudio & VST3 hosting; SECURITY.md + SBOM |
| Breaking change | — | `ThreadPool::addJob` hanya menerima satu tipe callable (`void` atau `ThreadPoolJob::JobStatus`) |
| Persyaratan | CMake 3.15+, C++17+, g++ 7.0+ / NDK 26 (Android) | CMake 3.22+, C++17 (C++20 bila Windows MIDI Services aktif), NDK 26 (Android) |
| Fokus seri | Stabil, matang | Fitur UI (variable font, SVG lunasvg), embedded Linux, performa |
| Lisensi | AGPLv3 atau EULA komersial (Starter ≤ USD 20.000) | Sama dengan JUCE 8 (AGPLv3 / EULA, harga sama) |

**Keputusan B1: pin JUCE 8.0.15.** Alasan:
1. Proyek ini greenfield — DSP core baru tidak membutuhkan fitur UI JUCE 9 (variable font, SVG) dan tidak memakai `ThreadPool::addJob` yang berubah di 9.0.2.
2. 8.0.15 adalah ujung seri 8.x yang stabil; tidak ada kejutan patch berikutnya.
3. 9.0.2 sudah bukan patch 9.x terbaru (9.0.5 sudah ada); memin 9.0.2 berarti memin versi yang sudah lewat.
4. Persyaratan toolchain lebih rendah (CMake 3.15+ vs 3.22+) — CMake 3.28.3 dan GCC 13.3.0 memenuhi keduanya, tetapi margin 8.0.15 lebih besar.
5. Oboe 1.10.0 sudah termasuk di 8.0.15 — kebutuhan audio Android terpenuhi tanpa Oboe standalone.
6. Evaluasi ulang JUCE 9.x dijadwalkan di Phase 7/8 atau saat ada fitur 9.x yang benar-benar dibutuhkan; bila pindah, pin patch 9.0.x terbaru saat itu, bukan 9.0.2.

**Cara pin (B2):** git submodule pada tag `8.0.15` di `third_party/JUCE` (atau direktori setara). Alasan: reproduksibel, bekerja offline setelah clone, ramah CI. Alternatif FetchContent (unduh saat configure) ditolak untuk B2 karena bergantung pada jaringan setiap kali configure bersih.

## 4. Catatan lingkungan yang memengaruhi B2

1. **sdkmanager remote list gagal saat B1.** `sdkmanager --list` (daftar paket remote) gagal dengan `java.util.NoSuchElementException` di `HttpURLConnection.doTunneling` — penangan proxy Java gagal. `--list_installed` tetap berfungsi. **Jalur instalasi NDK di B2: unduh langsung dari `dl.google.com` via curl (terverifikasi dapat dijangkau: HTTP 404 pada path direktori = host merespons; `github.com` HTTP 200) lalu ekstrak manual ke `~/workspace/android-sdk/ndk/<versi>/`.** Klaim Phase 0 bahwa "remote fetch terverifikasi berfungsi" tidak berlaku untuk daftar remote sdkmanager — dikoreksi di sini.
2. **RAM ketat.** 7,7 GB total; ~767 MB tersedia saat pengecekan B1. Build JUCE di B2: konfigurasi Release dan `-j 2` (sesuai catatan Phase 0).
3. **Tidak ada perangkat audio di VM** (`/dev/snd` tidak ada; `adb devices` kosong) — diverifikasi ulang di B1. Semua uji dengar/latensi tetap menunggu perangkat fisik.
4. **Disk longgar:** 145 GB bebas.
