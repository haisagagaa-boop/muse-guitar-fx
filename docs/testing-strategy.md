# MUSE GUITAR FX — Testing Strategy

> Dokumen Phase 0 — 21 November 2026.
> Prinsip: **tidak ada klaim tanpa bukti**. Efek, latensi, dan build hanya
> dinyatakan berfungsi setelah pengujian yang dicatat — bukan karena UI-nya
> tampil atau kodenya ter-compile.

## 1. Tingkat pengujian

| Tingkat | Apa yang diuji | Di mana | Kapan |
|---|---|---|---|
| Unit test DSP | Satu efek/node: respons benar, tanpa crash, tanpa alokasi di callback | VM Linux (GCC + CMake) | Setiap phase yang menyentuh DSP |
| Integration test | Rantai lengkap, urutan pedal, preset save/load, IR load | VM Linux | Phase 1, 5, 9 |
| Render-to-file | Sinyal uji → WAV → ukur (THD, respons frekuensi, gain reduction, decay) | VM Linux | Phase 2–4 |
| Smoke test build | Aplikasi jalan, device enumeration, callback hidup | VM Linux (ALSA) + CI Windows | Setiap phase |
| Uji perangkat fisik | Dengar nyata, latensi, dropout, USB interface, OTG | HP Android + mesin Windows | Phase 7–9 (dan smoke test Phase 1) |
| Uji stabilitas | Bermain 30 menit tanpa dropout/crash | Perangkat fisik | Phase 7–9 |
| Regression | Seluruh efek + preset + build ulang | VM + perangkat fisik | Phase 9 |

## 2. Apa yang BISA diuji di VM ini vs yang TIDAK BISA

### Bisa (VM Linux, terverifikasi tersedia)

- Unit test DSP offline (tanpa audio device) — cara utama menguji efek.
- Render-to-file dan analisis WAV (THD, spektrum, envelope).
- Build Linux + build APK (setelah NDK diinstal) + CI Windows (setelah repo dibuat).
- Preset JSON: round-trip save/load, validasi, migrasi versi.
- IR loader: load WAV, tolak file rusak, batas panjang IR.

### Tidak bisa (VM ini)

- Uji dengar (tidak ada speaker/mic, tidak ada `/dev/snd`).
- Ukur latensi round-trip nyata.
- Uji dropout/crackling nyata.
- Enumerasi USB audio interface (tidak ada interface).
- Uji OTG Android (tidak ada HP terpasang; `adb devices` kosong).
- Build Windows (tidak ada MSVC/MinGW/wine).
- Uji ASIO (tidak ada Windows, tidak ada driver ASIO).

**Konsekuensi:** setiap klaim "efek berfungsi" yang hanya diuji di VM harus
diberi label **"Not yet tested" pada perangkat** sampai uji fisik dilakukan.
Lihat taksonomi §5.

## 3. Kriteria lolos / gagal per jenis pengujian

### 3.1 Unit test DSP (wajib lolos 100%)

- Semua test hijau sebelum phase dinyatakan selesai.
- Test yang flaky (gagal kadang-kadang) diperlakukan sebagai gagal —
  diselidiki, bukan di-retry sampai hijau.
- Setiap efek baru wajib membawa minimal: (a) test bypass = sinyal tidak
  berubah (bit-identical dalam toleransi float); (b) test parameter ekstrem
  (min/maks) tanpa NaN/inf/crash; (c) test 10 detik proses tanpa alokasi
  (bila memungkinkan diukur).

### 3.2 Render-to-file (target terukur)

| Efek | Yang diukur | Lolos bila |
|---|---|---|
| Noise Gate | Atenuasi saat sinyal di bawah threshold | ≥ 40 dB atenuasi, tanpa chattering pada sinyal uji |
| Compressor | Gain reduction pada sinyal di atas threshold | GR sesuai rasio ±1 dB, tanpa pumping berlebihan |
| Overdrive/Distortion | THD + spektrum harmonik | THD naik sesuai drive; tidak ada aliasing dominan di atas Nyquist (cek spektrum) |
| EQ | Respons frekuensi | Peak/shelf pada frekuensi yang diminta ±1 dB |
| Chorus/Flanger/Phaser | Modulasi delay/fase | LFO mengubah delay/fase sesuai rate; tidak ada klik saat sweep |
| Delay | Waktu tunda + feedback | Delay sesuai ms yang diminta ±1 ms; feedback stabil (tidak runaway) |
| Reverb | RT60 (waktu decay) | RT60 sesuai parameter decay ±20% |
| Amp sim | Respons tone stack + karakter drive | Tone stack mengubah spektrum sesuai knob; tidak ada DC offset / clipping tak disengaja |
| Cabinet IR | Konvolusi benar | Output = konvolusi input dengan IR (bandingkan dengan referensi offline) |

### 3.3 Uji perangkat fisik (Phase 7–8)

- **Fungsional:** gitar masuk lewat interface → suara keluar terproses;
  setiap efek terdengar mengubah suara; urutan pedal memengaruhi hasil;
  preset tersimpan dan termuat kembali.
- **Latensi:** ukur round-trip (metode loopback bila memungkinkan);
  catat angka per perangkat + buffer. Tidak ada angka target global —
  targetnya adalah "serendah yang diizinkan perangkat" dan "stabil".
- **Stabilitas 30 menit:** bermain terus-menerus 30 menit: 0 crash,
  0 dropout yang terdengar, counter xrun = 0 (atau dicatat bila > 0
  beserta penyebabnya).
- **Cabut-colok:** cabut interface saat bermain → tidak crash; colok lagi
  → dapat reconnect.
- **Baterai/thermal (Android):** catat bila HP panas atau throttling
  memengaruhi audio.

### 3.4 Uji build

- Build Linux (VM), build Windows (CI/mesin), build APK (VM + Gradle)
  harus hijau di setiap phase yang mengubah kode native.
- Peringatan compiler baru (warning) dicatat; warning di kode baru
  diperlakukan sebagai temuan, bukan diabaikan.

## 4. Perangkat & alat uji yang dibutuhkan

| Kebutuhan | Status | Keterangan |
|---|---|---|
| HP Android (uji) | Belum diputuskan | Kandidat: Galaxy M32 milik user — dukungan OTG/interface **belum terverifikasi** |
| USB audio interface | Belum diputuskan | Harus UAC2-compliant agar dikenali Android; model diputuskan sebelum Phase 7 (idealnya Phase 1) |
| Gitar listrik + kabel | Belum diputuskan | Untuk uji dengar nyata |
| Mesin Windows | Belum diputuskan | Atau CI Windows (GitHub Actions) untuk build; mesin fisik untuk uji dengar ASIO |
| Alat ukur latensi | Belum ada | Metode loopback (kabel output→input) + perekaman; dicatat prosedurnya di Phase 7/8 |
| DAW / penganalisis spektrum (opsional) | Belum ada | Untuk verifikasi render-to-file; dapat diganti skrip Python + `libsndfile` (runtime sudah ada di VM) |

## 5. Taksonomi status (wajib dipakai di semua laporan phase)

Setiap fitur/efek dicatat dengan **satu** status berikut — tidak boleh ada
status lain, tidak boleh ada status ganda:

| Status | Arti | Bukti yang dibutuhkan |
|---|---|---|
| `Implemented` | Kode selesai dan unit test lolos | Log test hijau |
| `Partially implemented` | Sebagian jalan, sebagian belum | Daftar sub-fitur yang jalan vs belum |
| `UI prototype only` | UI tampil, logika/audio belum ada | Screenshot UI + catatan "belum ada logika" |
| `Not implemented` | Belum dikerjakan | — |
| `Tested on device` | Sudah diuji di perangkat fisik | Perangkat + tanggal + hasil (termasuk latensi/xrun) |
| `Not yet tested` | Belum diuji (di perangkat atau di VM) | — |

**Aturan:**

- `Implemented` **tidak** berarti `Tested on device`. Keduanya dicatat
  terpisah: sebuah efek bisa `Implemented` + `Not yet tested` (di perangkat).
- Dilarang menulis `Implemented` untuk efek yang hanya tampil di UI.
- Dilarang menulis `Tested on device` berdasarkan unit test VM.
- Laporan setiap phase memuat tabel status seluruh fitur.

## 6. Regression (Phase 9)

- Seluruh unit test + integration test dijalankan ulang dari nol (clean build).
- Seluruh preset bawaan dimuat dan di-render 5 detik; tidak boleh ada
  crash/NaN.
- Seluruh efek diuji bypass on/off.
- Build Linux + Windows + APK diuji ulang.
- Daftar known issues diperbarui; issue yang belum selesai dicatat dengan
  jelas, bukan disembunyikan.

## 7. Bukti audit Phase 0 (21 November 2026)

Perintah yang dijalankan dan hasilnya (diringkas; output lengkap ada di
log kerja Phase 0):

| Perintah / pemeriksaan | Hasil |
|---|---|
| `ls ~/workspace/muse-guitar-fx/` | Tidak ada (proyek greenfield) |
| `ls -d ~/workspace/*guitar* ~/workspace/*juce*` | Tidak ada |
| `cmake --version` | 3.28.3 |
| `gcc/g++ --version` | 13.3.0 (Ubuntu 24.04) |
| `clang`, `ninja` | Tidak ditemukan |
| `git --version` | 2.43.0 |
| `python3 --version`, `node --version` | 3.12.3, v24.22.1 |
| `ls ~/workspace/android-sdk/` | build-tools, cmdline-tools, licenses, platform-tools, platforms |
| `sdkmanager --list_installed` | build-tools 34.0.0; platform-tools 37.0.1; platform android-34, android-35. **Tidak ada NDK/CMake/emulator** |
| `sdkmanager --list \| grep ndk\|cmake` | Kosong (tidak ada NDK/CMake) |
| `ls ~/workspace/android-sdk/ndk/` | Tidak ada |
| JDK Temurin | 21.0.13 (terverifikasi `java -version`) |
| `~/.gradle`, repo Maven lokal | Ada |
| `find ... -iname '*juce*'` / `'*oboe*'` | Tidak ada di workspace/opt |
| MinGW (`x86_64-w64-mingw32-gcc`), wine | Tidak ditemukan |
| `dpkg -l \| grep libasound2-dev` | Terinstal 1.2.11 |
| `pkg-config --exists alsa` | OK; jack/pulse tidak ada |
| `ls /dev/snd/`, `aplay`, `lsusb` | Tidak ada perangkat audio / perintah tidak ditemukan |
| `adb devices` | Daftar kosong (tidak ada HP terpasang) |
| `df -h ~` | 145 GB bebas |
| `nproc`, `free` | 4 CPU, 7,7 GB RAM (776 MB tersedia saat cek) |
| JUCE (pencarian web) | Repo `github.com/juce-framework/JUCE`; terbaru 9.0.2 (Sep 2026); 8.0.15 terminal 8.x (Jul 2026); CMake ≥3.15; dual GPLv3/komersial; Oboe terbundel (Apache 2.0) |
| ASIO SDK (pencarian web) | Dual GPLv3/proprietary sejak Okt 2025; mirror `github.com/audiosdk/asio`; resmi `steinberg.net/developers/asiosdk-open` |

## 8. Yang tidak diuji di Phase 0 (batas audit ini)

- Kualitas suara efek apa pun (belum ada efek).
- Latensi perangkat apa pun (belum ada perangkat).
- Instalasi NDK/JUCE (sengaja tidak dilakukan — Phase 0 dilarang mengubah
  konfigurasi/deployment; instalasi dilakukan di Phase 1).
- Kompatibilitas HP/interface spesifik (menunggu keputusan perangkat uji).
