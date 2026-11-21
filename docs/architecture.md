# MUSE GUITAR FX — Arsitektur

> Dokumen Phase 0 — 21 November 2026.
> Status: **dokumen perencanaan saja**. Belum ada satu baris kode pun yang ditulis,
> belum ada dependency yang diunduh, belum ada build yang dijalankan.
> Semua pernyataan "sudah terverifikasi" di bawah ini merujuk pada audit environment
> VM yang dilakukan pada 21 November 2026 (lihat `testing-strategy.md` § Bukti audit).

## 1. Ringkasan keputusan arsitektur

| Keputusan | Pilihan | Alasan singkat |
|---|---|---|
| Bahasa inti DSP | C++17 (naik ke C++20 bila JUCE 9 dipilih) | JUCE mensyaratkan C++17 minimum; C++20 didukung JUCE 8.0.2+ |
| Framework audio/UI | JUCE 8.0.15 (pin) | Satu codebase untuk Windows + Android; CMake; Oboe sudah terbundel di dalamnya |
| Build system | CMake 3.28 (sudah ada di VM) + Gradle untuk shell Android | CMake adalah cara resmi JUCE; Gradle sudah terbukti di VM ini (CYCLOTRACK) |
| Shared DSP core | Library C++ statis `muse_dsp` (JUCE `juce_dsp` + `juce_audio_basics`) | Dipakai identik oleh kedua platform; efek ditulis sekali |
| Shell Windows | JUCE Standalone app (`juce_add_gui_app`) | WASAPI + ASIO lewat `juce_audio_devices` |
| Shell Android | JUCE Android app (Gradle + NDK) | Oboe/AAudio lewat `juce_audio_devices`; UI sama dengan Windows |
| UI | JUCE GUI (shared), tema dark | Satu UI untuk dua platform; alternatif native dicatat di §6 |
| Preset | JSON berversi, validasi skema | Sesuai brief; mudah diimpor/diekspor |
| IR cabinet | WAV loader + `juce::dsp::Convolution` | Tanpa aset berlisensi; IR buatan sendiri atau berlisensi jelas |

**Keputusan yang sengaja ditunda ke Phase 1:** JUCE 8.0.15 vs 9.0.2. Versi 9.0.2
(September 2026) adalah rilis terbaru, tetapi 8.0.15 (Juli 2026, rilis terakhir seri 8.x)
lebih matang dan terdokumentasi. Evaluasi 9.x dilakukan saat Phase 1 dimulai, sebelum
pin di-commit. Jangan menaikkan versi JUCE di tengah phase tanpa alasan teknis.

**Keputusan lisensi yang harus dibuat user sebelum distribusi (bukan sebelum coding):**
JUCE dual GPLv3/komersial (modul inti ISC; modul lain GPLv3 atau komersial —
Personal gratis di bawah pendapatan USD 50 ribu, Indie USD 40/bulan, Pro USD 130/bulan).
ASIO SDK kini dual GPLv3/proprietary (sejak Oktober 2025). Jika binary Windows
me-link ASIO, binary tersebut jatuh di bawah GPLv3 kecuali lisensi komersial Steinberg
diambil. Ini tidak menghalangi development; ini menghalangi *distribusi tertutup*
tanpa keputusan lisensi. Catat keputusannya di `development-roadmap.md` Phase 9.

## 2. Peta modul

```
                        ┌─────────────────────────────────┐
                        │        UI (JUCE GUI, shared)     │
                        │  Home · Pedalboard · Amp Studio  │
                        │  Preset Library · Audio Settings │
                        │  Practice Tools (tuner, metronom)│
                        └──────────────┬──────────────────┘
                                       │ parameter changes
                                       │ (lock-free, lihat §4)
                        ┌──────────────▼──────────────────┐
                        │      Preset System (JSON)        │
                        │  load/save/import/export/search  │
                        │  versioning + validasi parameter │
                        └──────────────┬──────────────────┘
                                       │
                        ┌──────────────▼──────────────────┐
                        │   DSP Engine — muse_dsp (C++)    │
                        │  EffectChain (ordered nodes)     │
                        │  Parameter smoothing · denormal  │
                        │  protection · bypass per-node    │
                        └──────┬───────────────┬──────────┘
                               │               │
              ┌────────────────▼─────┐ ┌───────▼────────────────┐
              │   Effects Library    │ │  Amp & Cabinet Sim     │
              │  Gate · Comp · Limit │ │  Amp: 6 voicing        │
              │  OD · Dist · Fuzz    │ │  Cab: 1x12/2x12/4x12   │
              │  Chorus · Flanger ·  │ │  IR WAV loader         │
              │  Phaser · Tremolo    │ │  (juce::dsp::Convolution)
              │  Delay · Reverb · EQ │ └────────────────────────┘
              │  Wah                 │
              └──────────────────────┘
                        ┌──────────────▼──────────────────┐
                        │   Audio Engine (platform glue)   │
                        │  device mgmt · callback · meter  │
                        │  latency monitor · reconnect     │
                        └──────┬───────────────┬──────────┘
                  Windows: WASAPI/ASIO   Android: Oboe/AAudio
                  (juce_audio_devices)   (juce_audio_devices)
```

### 2.1 Audio Engine (platform glue, tipis)

Tugasnya hanya: membuka device, menjalankan callback real-time, melaporkan meter
dan latensi, menangani device hilang/berganti. **Tidak ada logika efek di sini.**
Di Windows memakai `juce::AudioDeviceManager` (WASAPI default, ASIO bila SDK
terdeteksi saat build). Di Android memakai `juce::AudioDeviceManager` yang di
belakangnya memakai Oboe (sudah terbundel dalam JUCE, Apache 2.0) di atas AAudio.

Konsekuensi audit: tidak ada NDK di VM saat ini, jadi shell Android belum bisa
di-build sampai NDK + CMake-for-Android diinstal lewat `sdkmanager` di Phase 1.
Tidak ada perangkat audio di VM (`/dev/snd` tidak ada), jadi audio engine hanya
bisa diuji lewat unit test offline dan render-to-file di VM; uji dengar nyata
wajib di perangkat fisik (lihat `testing-strategy.md`).

### 2.2 DSP Engine — `muse_dsp`

Library C++ statis, tanpa dependensi UI. Isi:

- **`EffectChain`**: daftar terurut dari node efek. Urutan adalah data, bukan
  tampilan — drag-and-drop di pedalboard mengubah urutan node ini, dan urutan
  inilah yang dieksekusi di callback. Satu rantai, satu arah sinyal.
- **Kontrak node efek** (interface tunggal, dipakai semua efek):
  `prepare(sampleRate, blockSize)` → `process(AudioBuffer)` → `reset()`.
  Setiap node punya daftar parameter bernama dengan rentang + default, dan
  `setParameter()` yang thread-safe (lihat §4).
- **Parameter smoothing**: semua parameter yang dapat berubah saat bermain
  (gain, mix, cutoff, dsb.) melewati `juce::SmoothedValue` / `LinearSmoothedValue`
  agar tidak ada zipper noise / klik saat knob diputar.
- **Denormal protection**: `juce::ScopedNoDenormals` di callback + flush-to-zero
  pada state filter (khususnya reverb/delay dengan feedback panjang).
- **Alokasi**: nol alokasi di callback audio. Semua buffer dialokasikan di
  `prepare()`; preset load dan IR load terjadi di luar callback (lihat
  `audio-engine-design.md`).

### 2.3 Effects Library

Satu node per efek, semua mengikuti kontrak §2.2. Daftar sesuai brief:

| Kelompok | Efek | Catatan implementasi |
|---|---|---|
| Dynamics | Noise Gate, Compressor, Limiter | Envelope follower + lookahead untuk limiter |
| Drive | Overdrive, Distortion, Fuzz | Waveshaper + oversampling (min. 2x, target 4x) untuk menekan aliasing |
| Modulation | Chorus, Flanger, Phaser, Tremolo | LFO + delay line / allpass ter-modulasi |
| Time | Delay, Reverb | Delay: tap tempo + feedback; Reverb: algorithmic (Freeverb-style) |
| Tone | Equalizer, Wah | EQ: biquad peaking/shelf; Wah: bandpass resonan + envelope/LFO |
| Amp | Clean, Blues/Vintage, Crunch, Classic Rock, High Gain, Metal | Tone stack (FMV) + waveshaper per voicing; parameter Gain/Bass/Mid/Treble/Presence/Master |
| Cabinet | 1x12, 2x12, 4x12, IR WAV loader, mic sim, bypass | Konvolusi IR; IR bawaan harus original/berlisensi jelas |

**Aturan aset:** dilarang memakai model amp atau impulse response berlisensi tanpa
izin. IR bawaan dibuat sendiri (rekaman) atau dari sumber berlisensi jelas.
Sampai IR bawaan ada, cabinet sim memakai IR sintetis/placeholder yang diberi
label jelas `placeholder` dan statusnya dicatat "UI prototype only / Not implemented".

### 2.4 Preset System

- Format JSON dengan field `version` (dimulai dari 1). Loader menolak atau
  memigrasi file dengan versi yang tidak dikenal — tidak pernah crash.
- Satu preset = urutan node + seluruh nilai parameter + metadata (nama, kategori,
  favorit, tanggal). Kategori awal: Clean, Blues, Rock, Hard Rock, Metal, Ambient,
  Experimental.
- Operasi: create, save, rename, delete, import, export, duplicate, search,
  favorite — semua lewat satu `PresetManager`, tanpa logika preset tersebar di UI.
- Validasi: setiap parameter di-clamp ke rentangnya saat load; parameter yang
  tidak dikenal diabaikan dengan peringatan (bukan error fatal).

### 2.5 Practice Tools

- **Chromatic tuner**: deteksi pitch dari input (autocorrelation + FFT,
  `juce::dsp::FFT`), tampil di halaman Practice Tools. Bukan efek dalam rantai.
- **Metronome + tap tempo**: click dijadwalkan sample-accurate dari audio
  callback (bukan timer UI), agar tidak drift.
- **Meter**: input/output level + clipping indicator, dihitung di callback,
  ditampilkan di UI lewat nilai atomik (bukan lewat callback UI).

### 2.6 UI — 6 halaman

Home, Pedalboard, Amp Studio, Preset Library, Audio Settings, Practice Tools.
Desain dark modern, satu design language. Layout mobile-first untuk Android;
layout desktop tidak dipaksakan ke layar HP (panel dan pedalboard disusun ulang,
bukan diperkecil).

## 3. Aliran sinyal (signal flow)

```
Gitar → USB Interface → [Input gain] → Noise Gate → Drive → Wah → EQ →
Modulation → Amp Sim → Cabinet Sim → Delay → Reverb → [Master + Limiter] →
Output device
```

- Urutan default di atas hanya titik awal. Pengguna bebas menyusun ulang
  semua node (kecuali input gain dan master/limiter yang tetap di ujung).
- Bypass bersifat per-node dan global (bypass seluruh rantai = sinyal kering).
- Tuner mengambil sinyal dari input (pre-chain) dan tidak memengaruhi output.
- Metronome dicampur ke output setelah master, dengan level sendiri.

## 4. Threading & komunikasi UI ↔ audio

- **Satu-satunya penulis** di callback audio adalah `EffectChain::process()`.
- UI tidak pernah memanggil fungsi DSP secara langsung. Perubahan parameter
  dikirim lewat antrian lock-free (FIFO single-producer/single-consumer) atau
  `std::atomic<float>` per parameter + smoothing di sisi audio.
- Metering: callback menulis nilai level ke atomik; UI membaca dengan timer
  (~15–30 Hz), tidak pernah memblokir callback.
- Load preset / IR: dibaca dan diparse di thread UI/background, lalu diterapkan
  ke rantai lewat pertukaran atomik pointer (double-buffer) di batas blok,
  bukan di tengah blok.
- Lihat detail di `audio-engine-design.md` §5–§6.

## 5. Pembagian shared core vs platform code

| Kode | Lokasi (rencana) | Platform |
|---|---|---|
| `muse_dsp` (semua efek, amp, cab, chain, preset logic) | `src/dsp/` | Shared — sama persis di Windows & Android |
| Audio engine glue (JUCE AudioDeviceManager wrapper) | `src/audio/` | Shared (JUCE) + sedikit kode kondisional per platform |
| UI (JUCE Components) | `src/ui/` | Shared (JUCE) |
| Shell Windows (main, installer config) | `platforms/windows/` | Windows |
| Shell Android (Gradle, manifest, JNI bridge JUCE, USB permission) | `platforms/android/` | Android |
| Preset JSON + IR bawaan | `assets/` | Shared |

**Prinsip:** bila sebuah bug hanya muncul di satu platform, penyebabnya dicari
dulu di platform glue — bukan dengan meng-cabang DSP core. DSP core tidak
boleh mengandung `#ifdef ANDROID` / `#ifdef WINDOWS` kecuali untuk alasan
yang didokumentasikan.

## 6. Alternatif yang dipertimbangkan dan ditolak (untuk Phase 0)

1. **UI React Native / WebView** — ditolak untuk Phase 1–6. Menambah JNI bridge
   + runtime JS di jalur yang tidak butuh itu, dan pedalboard custom lebih
   mudah digambar native JUCE. Bisa dievaluasi ulang setelah Phase 6 bila UI
   JUCE terbukti menghambat.
2. **UI native Kotlin (Compose) di Android + JUCE hanya untuk audio** — layak
   secara teknis (pengalaman tim: CYCLOTRACK memakai Kotlin/Compose), tetapi
   menggandakan pekerjaan UI (dua UI untuk dua platform). Ditunda: bila di
   Phase 7 UI JUCE di Android terasa buruk, opsi ini dibuka kembali dengan
   JNI bridge ke `muse_dsp` yang sama — DSP core tidak berubah.
3. **Oboe standalone (tanpa JUCE)** — tidak perlu; Oboe sudah terbundel di
   JUCE. Mengambil Oboe terpisah hanya menambah surface perawatan.
4. **PortAudio / RtAudio sebagai pengganti JUCE audio** — tidak perlu;
   `juce_audio_devices` sudah mencakup WASAPI/ASIO/Oboe dan satu API untuk
   keduanya.
5. **Menulis DSP dari nol tanpa JUCE** — ditolak; JUCE memberi audio device,
   DSP primitives (`juce_dsp`), dan GUI dalam satu paket. Menulis ulang
   device handling Android (AAudio) + Windows (WASAPI/ASIO) adalah pekerjaan
   berbulan-bulan yang tidak menambah nilai ke efek gitarnya.

## 7. Struktur repository yang diusulkan (dibuat di Phase 1, bukan sekarang)

```
muse-guitar-fx/
├── CMakeLists.txt            # root: JUCE via FetchContent/submodule + muse_dsp
├── README.md
├── docs/                     # dokumen Phase 0 ini
├── src/
│   ├── dsp/                  # muse_dsp: chain, effects, amp, cab, preset logic
│   ├── audio/                # audio engine glue
│   └── ui/                   # JUCE GUI, 6 halaman
├── platforms/
│   ├── windows/              # shell + installer (NSIS/Inno Setup)
│   └── android/              # Gradle, AndroidManifest, USB permission
├── assets/
│   ├── presets/              # preset JSON bawaan
│   └── ir/                   # impulse response WAV (original/berlisensi jelas)
└── tests/
    ├── dsp_tests/            # unit test DSP (offline render, tanpa audio device)
    └── app_tests/            # integration test
```

## 8. Keputusan yang menunggu user (bukan blocker Phase 1)

1. **Model lisensi produk akhir**: open source GPLv3 (gratis, konsisten dengan
   JUCE GPL + ASIO GPL) atau komersial (butuh lisensi JUCE berbayar + lisensi
   ASIO Steinberg). Keputusan ini memengaruhi Phase 9, bukan Phase 1.
2. **Apakah Windows build boleh memakai GitHub Actions** (runner Windows gratis
   untuk repo publik) atau harus mesin Windows fisik — lihat
   `platform-compatibility.md` §4.
3. **Perangkat uji Android**: HP apa + USB audio interface apa yang dipakai
   untuk Phase 7. Tanpa ini, Phase 7 tidak bisa dinyatakan lolos.
