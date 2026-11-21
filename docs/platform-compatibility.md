# MUSE GUITAR FX — Platform Compatibility

> Dokumen Phase 0 — 21 November 2026.
> Semua pernyataan "terverifikasi" di bawah ini berasal dari audit VM pada
> 21 November 2026. Pernyataan tentang perangkat/user ditandai sebagai
> "belum terverifikasi" bila memang belum diuji.

## 1. Ringkasan matriks

| Kemampuan | Windows | Android | Bisa dibangun di VM ini? | Bisa diuji di VM ini? |
|---|---|---|---|---|
| Build DSP core (C++/JUCE) | ✅ MSVC | ✅ NDK (Clang) | ⚠️ Linux dulu (bukti engine); Windows tidak | ❌ dengar: tidak ada audio device |
| Build shell Windows (.exe/installer) | ✅ | — | ❌ tidak ada MSVC/MinGW/wine | ❌ |
| Build APK | — | ✅ Gradle + NDK | ⚠️ setelah NDK diinstal | ❌ tidak ada HP terpasang |
| USB audio interface | ✅ ASIO/WASAPI | ⚠️ OTG, tergantung perangkat | ❌ | ❌ butuh interface fisik |
| Uji latensi/dropout | ✅ | ✅ | ❌ | ❌ butuh perangkat fisik |
| Unit test DSP offline | ✅ | ✅ | ✅ (Linux) | ✅ (render-to-file) |

## 2. Yang bisa dilakukan di VM ini (Linux, terverifikasi)

- **Build dan unit test DSP core** dengan GCC 13.3.0 + CMake 3.28.3
  (keduanya terverifikasi ada). JUCE dapat di-build di Linux (ALSA tersedia
  sebagai header `libasound2-dev`; JUCE Linux memakai ALSA/JACK).
- **Smoke test audio engine** sebagai aplikasi JUCE standalone Linux
  (target ALSA) — hanya untuk membuktikan callback dan chain berjalan,
  bukan untuk menilai kualitas suara (tidak ada `/dev/snd`, tidak ada
  speaker/mic di VM).
- **Render-to-file**: DSP diuji offline dengan menulis output WAV dan
  membandingkannya dengan referensi — ini adalah cara utama menguji efek
  di VM.
- **Build APK**: setelah NDK + CMake-for-Android diinstal lewat
  `sdkmanager` (remote fetch terverifikasi berfungsi), Gradle dapat
  merakit APK di VM ini (pola yang sama sudah terbukti pada proyek
  CYCLOTRACK/English Learning App).
- **Manajemen repo**: git 2.43.0 tersedia.

## 3. Yang TIDAK bisa dilakukan di VM ini

| Keterbatasan | Bukti audit | Dampak |
|---|---|---|
| Tidak ada NDK | `android-sdk/ndk/` tidak ada; `sdkmanager --list_installed` hanya menampilkan build-tools 34.0.0, platform-tools 37.0.1, platform android-34/35 | Shell Android tidak bisa di-build sampai NDK diinstal (Phase 1) |
| Tidak ada CMake-for-Android | Tidak ada `android-sdk/cmake/` | Sama seperti di atas |
| Tidak ada compiler Windows | `x86_64-w64-mingw32-gcc` tidak ditemukan; wine tidak ada; MSVC tidak ada (MSVC memang hanya ada di Windows) | Binary Windows tidak bisa di-build di VM ini |
| Tidak ada perangkat audio | `/dev/snd` tidak ada; `aplay` tidak ada; `lsusb` tidak ada | Tidak ada uji dengar, uji latensi, atau uji dropout di VM |
| Tidak ada HP Android terpasang | `adb devices` kosong | APK tidak bisa diuji di VM; butuh HP fisik |
| Tidak ada USB audio interface | Tidak ada perangkat USB audio yang terdeteksi | Uji input gitar nyata tidak bisa dilakukan di VM |
| RAM terbatas saat audit | 7,7 GB total, 776 MB tersedia saat pengecekan | Build JUCE yang besar (khususnya dengan debug info) dapat melambat/kehabisan memori — pakai build Release dan batasi job paralel (`cmake --build -j 2`) |
| VM di-recycle berkala | Pola yang tercatat di proyek lain: VM di-recycle kira-kira tiap 2 jam | Build panjang harus dapat dilanjutkan (gunakan cache ccache bila memungkinkan; jangan mengandalkan proses yang berjalan berjam-jam tanpa checkpoint) |

## 4. Opsi build & uji Windows (jujur)

VM ini Linux. Tidak ada jalan pintas yang mengubah itu. Tiga opsi:

### Opsi A — Mesin Windows fisik (direkomendasikan untuk Phase 8)

- Visual Studio 2022/2026 (MSVC) + CMake + JUCE + ASIO SDK.
- Kelebihan: jalur yang didukung penuh JUCE dan Steinberg; ASIO dapat
  diuji dengan interface nyata.
- Kekurangan: butuh akses ke mesin Windows (milik user atau mesin sewa).
- **Keputusan user yang dibutuhkan sebelum Phase 8:** mesin Windows apa
  yang dipakai, dan siapa yang menjalankannya.

### Opsi B — GitHub Actions (runner `windows-latest`, direkomendasikan untuk CI)

- Build otomatis di cloud setiap push: MSVC tersedia, JUCE di-fetch,
  ASIO SDK di-fetch dari mirror GPLv3 (`github.com/audiosdk/asio`) atau
  dari Steinberg.
- Kelebihan: tidak butuh mesin Windows; build dapat direproduksi; gratis
  untuk repo publik (kuota menit terbatas untuk repo privat).
- Kekurangan: **tidak bisa menguji audio nyata** (tidak ada interface di
  runner); hanya membuktikan "build sukses + unit test lolos". Uji dengar
  tetap butuh Opsi A.
- Catatan lisensi: ASIO SDK di bawah GPLv3 boleh di-fetch di CI; binary
  hasil build yang me-link ASIO jatuh di bawah GPLv3 (lihat
  `architecture.md` §1).

### Opsi C — Cross-compile MinGW dari Linux (tidak direkomendasikan)

- MinGW tidak terinstal di VM (harus diinstal), JUCE + MinGW bukan jalur
  yang didukung penuh, dan ASIO (COM-based) bermasalah di MinGW.
- Hanya dipertimbangkan bila Opsi A dan B sama-sama tidak tersedia, dan
  hasilnya dinyatakan "build eksperimental, bukan rilis".

**Rekomendasi Phase 0:** Opsi B untuk build/CI sejak Phase 1 (supaya setiap
perubahan langsung terbukti bisa di-build di Windows), Opsi A untuk uji
fungsional Phase 8. Opsi C ditolak kecuali keadaan memaksa.

## 5. Android — risiko kompatibilitas spesifik

| Risiko | Penjelasan | Mitigasi |
|---|---|---|
| USB OTG tidak didukung semua HP | USB host mode tergantung perangkat + kernel; HP lama/anggaran kadang menonaktifkan | Uji di HP target sejak Phase 1 (bukan Phase 7); catat model HP yang didukung |
| Interface USB tidak dikenali Android | Tidak semua interface USB Audio Class 2.0 dikenali; beberapa butuh driver vendor | Pilih interface yang sudah terbukti UAC2-compliant; uji enumerasi device di Phase 1 |
| Sample rate terkunci | Banyak HP Android mengunci output 48 kHz; input USB bisa 44.1/48 kHz tergantung interface | Engine selalu memakai rate aktual device; DSP di-`prepare()` ulang; tampilkan rate aktual |
| Buffer minimum besar | `AAudio` low-latency path tidak tersedia di semua perangkat; buffer 64/128 sering ditolak, fallback ke 256/512 | Desain engine menerima ukuran buffer berapa pun; target 64/128 adalah "bila tersedia", bukan janji |
| Latensi tidak bisa dijamin | Latensi round-trip Android sangat bervariasi (5–50+ ms) tergantung HP + interface + mode | Ukur dan laporkan latensi per perangkat; jangan klaim angka latensi tanpa pengukuran |
| Izin USB | Android meminta izin USB setiap kali interface dicolok (atau per aplikasi bila diingat) | Tangani permission flow di shell Android; uji cabut-colok |
| Daya | Interface USB menarik daya dari HP; beberapa HP mematikan OTG saat baterai rendah | Catat sebagai known limitation; sarankan powered USB hub bila perlu |
| HP uji belum ditentukan | Sampai Phase 0 selesai, belum ada keputusan HP + interface apa | **Keputusan user sebelum Phase 7** (idealnya sebelum Phase 1 agar smoke test dilakukan di perangkat yang sama) |

**Perangkat uji yang sudah ada di tangan user (belum terverifikasi untuk proyek ini):**
Samsung Galaxy M32 (SM-M325FV) — dipakai untuk CYCLOTRACK dan English Learning
App. Apakah M32 mendukung USB OTG + interface audio adalah **belum terverifikasi**
dan harus diuji di Phase 1 (colok interface → cek apakah Android mengenalinya),
bukan diasumsikan.

## 6. Windows — risiko kompatibilitas spesifik

| Risiko | Penjelasan | Mitigasi |
|---|---|---|
| ASIO butuh driver vendor | ASIO hanya jalan bila interface punya driver ASIO; onboard audio umumnya tidak punya | WASAPI sebagai fallback selalu tersedia; ASIO adalah peningkatan, bukan syarat |
| Lisensi ASIO (GPLv3) | Binary Windows yang me-link ASIO jatuh di bawah GPLv3 kecuali lisensi komersial Steinberg | Keputusan lisensi sebelum distribusi (lihat `architecture.md` §1); pisahkan build ASIO dan non-ASIO bila perlu |
| WASAPI exclusive vs shared | Shared mode menambah latensi; exclusive mode mengambil alih device | Beri pilihan mode di Audio Settings; default shared (aman), exclusive sebagai opsi |
| Sample rate/buffer device | Sama seperti Android: device boleh menolak | Selalu baca nilai aktual; tampilkan apa adanya |
| Installer | Butuh NSIS/Inno Setup + code signing bila ingin distribusi serius | Phase 8; code signing adalah keputusan/biaya terpisah |

## 7. Dependency yang harus dipenuhi sebelum Phase 1

| Dependency | Cara memperoleh | Status saat ini |
|---|---|---|
| JUCE 8.0.15 (pin) | `git clone github.com/juce-framework/JUCE` + checkout tag, atau CMake FetchContent | ❌ Belum ada |
| Android NDK (versi LTS, mis. r27) | `sdkmanager "ndk;<versi>"` | ❌ Belum ada (remote fetch berfungsi) |
| CMake-for-Android | `sdkmanager "cmake;<versi>"` | ❌ Belum ada |
| ASIO SDK (untuk build Windows) | Mirror GPLv3 `github.com/audiosdk/asio` atau Steinberg resmi | ❌ Belum ada (hanya dibutuhkan di build Windows) |
| GitHub repo + CI (direkomendasikan) | Buat repo + workflow Windows/Linux/Android | ❌ Belum ada |
| HP Android + USB audio interface (uji) | Diputuskan user | ❌ Belum diputuskan |
| Mesin Windows atau CI Windows (uji/build) | Diputuskan user | ❌ Belum diputuskan |

## 8. Keputusan yang menunggu user

1. HP Android + USB audio interface apa yang dipakai untuk pengujian?
2. Build Windows: mesin Windows fisik, GitHub Actions, atau keduanya?
3. Model lisensi produk akhir (mempengaruhi GPLv3 vs komersial — lihat
   `architecture.md` §1). Tidak menghambat Phase 1, tetapi harus diputuskan
   sebelum Phase 9.
