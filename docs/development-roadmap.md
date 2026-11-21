# MUSE GUITAR FX — Development Roadmap (Phase 0–9)

> Dokumen Phase 0 — 21 November 2026.
> Estimasi di bawah ini adalah **rentang kasar untuk satu developer** dengan
> pekerjaan paruh waktu, berdasarkan kompleksitas tiap phase — bukan janji.
> Estimasi dihitung ulang di akhir setiap phase. Satu phase tidak dimulai
> sebelum phase sebelumnya dinyatakan lolos gerbangnya (lihat §3).

## 0. Gambaran

| Phase | Nama | Estimasi kasar | Ketergantungan |
|---|---|---|---|
| 0 | Audit + arsitektur (dokumen ini) | selesai 21 Nov 2026 | — |
| 1 | Shared audio engine + USB audio I/O | 2–4 minggu | Phase 0; NDK + JUCE |
| 2 | Efek dasar (Gate, Comp, OD, Dist, EQ) | 2–3 minggu | Phase 1 |
| 3 | Modulasi + time (Chorus, Flanger, Phaser, Delay, Reverb) | 3–4 minggu | Phase 2 |
| 4 | Amp + cabinet simulator | 3–5 minggu | Phase 2 (Drive/EQ) |
| 5 | Pedalboard virtual + preset system | 2–4 minggu | Phase 2–4 |
| 6 | Tuner, metronome, practice tools | 1–2 minggu | Phase 1 |
| 7 | Integrasi Android + uji APK | 2–4 minggu | Phase 1–6; NDK; HP + interface |
| 8 | Integrasi Windows + uji executable | 2–3 minggu | Phase 1–6; mesin/CI Windows |
| 9 | Optimasi, regression, release prep | 2–3 minggu | Phase 7–8 |
| **Total** | | **≈ 19–32 minggu** (4,5–8 bulan) | |

Catatan: estimasi di atas mengasumsikan pekerjaan berkelanjutan. Jeda antar-phase,
perubahan scope, dan waktu tunggu perangkat uji tidak termasuk.

## 1. Phase 0 — Repository audit & technical architecture ✅ SELESAI

**Scope:** audit repository (hasil: tidak ada — proyek greenfield), audit environment
VM, tulis 5 dokumen (`architecture.md`, `development-roadmap.md`,
`audio-engine-design.md`, `platform-compatibility.md`, `testing-strategy.md`).

**Hasil audit (terverifikasi 21 Nov 2026):**

- Tidak ada direktori `~/workspace/muse-guitar-fx/`, tidak ada kode guitar/JUCE
  di `~/workspace/`. Proyek dimulai dari nol.
- Toolchain VM: CMake 3.28.3, GCC/G++ 13.3.0, GNU Make 4.3, git 2.43.0,
  Python 3.12.3, Node v24.22.1. **Tidak ada:** clang, ninja, MinGW, wine.
- Android SDK: build-tools 34.0.0, platform-tools 37.0.1, platform android-34
  dan android-35, cmdline-tools, lisensi SDK diterima. **Tidak ada:** NDK,
  CMake-for-Android, emulator/system image.
- JDK Temurin 21.0.13, `~/.gradle` ada, repo Maven lokal ada.
- Audio di VM: tidak ada perangkat audio (`/dev/snd` tidak ada), `libasound2-dev`
  terinstal (header ALSA), `libsndfile1` runtime ada, tidak ada JACK/PulseAudio dev.
- `adb devices`: tidak ada perangkat terpasang.
- JUCE: belum ada di VM. Sumber: `github.com/juce-framework/JUCE`, pin yang
  direkomendasikan 8.0.15 (terakhir seri 8.x, Juli 2026); terbaru 9.0.2
  (September 2026) dievaluasi di Phase 1. Lisensi dual GPLv3/komersial.
- ASIO SDK: dual GPLv3/proprietary sejak Oktober 2025; mirror
  `github.com/audiosdk/asio`; sumber resmi `steinberg.net/developers/asiosdk-open`.
- `sdkmanager --list_installed` terverifikasi; fetch remote repository berfungsi,
  jadi NDK dapat diinstal di Phase 1.

**Gerbang keluar Phase 0:** 5 dokumen selesai + persetujuan user untuk Phase 1.
Tidak ada kode yang ditulis di phase ini (sesuai instruksi).

## 2. Phase 1 — Shared audio engine & USB audio I/O

**Scope:**

- Inisialisasi repo git + struktur folder sesuai `architecture.md` §7.
- Ambil JUCE 8.0.15 (pin; evaluasi 9.0.2 sebelum commit pin).
- Instal Android NDK (pilih versi LTS, mis. r27 — verifikasi saat instalasi)
  + CMake-for-Android lewat `sdkmanager`.
- `muse_dsp` kerangka: `EffectChain` + kontrak node + satu efek passthrough
  (bypass) sebagai bukti rantai hidup.
- Audio engine: device enumeration, open/close, callback real-time,
  input/output meter, latency report, bypass global, reconnect handling.
- Build pertama: aplikasi JUCE standalone Linux (VM, ALSA) sebagai smoke test
  — target ini **bukan** produk, hanya bukti engine jalan.
- Satu unit test offline: render buffer lewat chain dan bandingkan dengan
  referensi (tanpa audio device).

**Dependensi:** Phase 0 selesai; akses internet untuk JUCE + NDK.
**Deliverable:** repo + build Linux jalan + log latensi + unit test hijau.
**Risiko utama:** NDK gagal diinstal lewat proxy VM (pola yang sama pernah
terjadi pada Gradle/Maven di proyek lain — solusinya repo lokal + fetch manual).
**Estimasi:** 2–4 minggu.

## 3. Phase 2 — Basic DSP effects

**Scope:** Noise Gate, Compressor, Overdrive, Distortion, EQ (3-band + parametric
dasar). Parameter smoothing, denormal protection, oversampling pada drive.
Setiap efek: unit test offline (sinyal uji → ukur gain reduction / THD /
respons frekuensi) + uji dengar di perangkat bila memungkinkan.

**Dependensi:** Phase 1 (chain + callback).
**Deliverable:** 5 efek lolos unit test + preset awal "Clean" dan "Rock".
**Estimasi:** 2–3 minggu.

## 4. Phase 3 — Modulation & time effects

**Scope:** Chorus, Flanger, Phaser, Tremolo, Delay (tap tempo), Reverb
(algoritmik). Delay line + LFO + feedback; reverb dengan decay dan mix.

**Dependensi:** Phase 2 (pola node + smoothing sudah terbukti).
**Deliverable:** 6 efek lolos unit test + preset "Ambient".
**Estimasi:** 3–4 minggu.

## 5. Phase 4 — Amplifier & cabinet simulator

**Scope:** 6 voicing amp (Clean, Blues/Vintage, Crunch, Classic Rock, High Gain,
Metal) dengan Gain/Bass/Mid/Treble/Presence/Master; cabinet 1x12/2x12/4x12;
IR WAV loader + mic sim + bypass. IR bawaan harus original/berlisensi jelas;
sampai ada, pakai placeholder berlabel jelas.

**Dependensi:** Phase 2 (Drive + EQ adalah fondasi amp sim).
**Deliverable:** amp + cab lolos uji dengar + preset per voicing.
**Risiko utama:** kualitas amp sim adalah penentu apakah aplikasi terdengar
"profesional" — alokasikan waktu tuning paling besar di sini; jangan klaim
voicing "mirip amp X" (itu klaim merek dan kualitas yang tidak teruji).
**Estimasi:** 3–5 minggu.

## 6. Phase 5 — Virtual pedalboard & preset system

**Scope:** pedalboard drag-and-drop (urutan benar-benar mengubah urutan DSP),
tambah/hapus/aktifkan efek, simpan/muat preset JSON berversi, import/export,
duplicate, search, favorite, 7 kategori awal.

**Dependensi:** Phase 2–4 (semua node harus ada agar pedalboard bermakna).
**Deliverable:** pedalboard + preset manager lolos uji (termasuk uji: urutan
diubah → hasil audio berubah; preset disimpan → dimuat kembali identik).
**Estimasi:** 2–4 minggu.

## 7. Phase 6 — Tuner, metronome & practice tools

**Scope:** chromatic tuner (pitch detection), metronome + tap tempo
(sample-accurate), level meter + clipping indicator, audio device settings.

**Dependensi:** Phase 1 (input + callback).
**Deliverable:** tuner akurat ±2 sen pada rentang gitar; metronome stabil.
**Estimasi:** 1–2 minggu.

## 8. Phase 7 — Android integration & APK testing

**Scope:** shell Android (Gradle + NDK + JUCE Android), USB OTG/host permission
dan device handling, pengaturan sample rate/buffer per perangkat, APK debug
+ release, uji di HP fisik + USB audio interface.

**Dependensi:** Phase 1–6; NDK; HP Android + USB audio interface yang
kompatibel (harus diputuskan sebelum phase ini — lihat
`platform-compatibility.md` §5).
**Deliverable:** APK terinstal di HP, interface dikenali, latensi terukur,
tidak ada dropout dalam uji stabilitas 30 menit.
**Risiko utama:** fragmentasi Android — sample rate dan buffer minimum
berbeda per perangkat; interface USB tertentu tidak dikenali Android.
**Estimasi:** 2–4 minggu.

## 9. Phase 8 — Windows integration & executable testing

**Scope:** shell Windows (WASAPI + ASIO bila SDK tersedia), pemilihan
input/output device, sample rate/buffer, installer (NSIS/Inno Setup),
uji di mesin Windows dengan ASIO interface dan dengan onboard audio (WASAPI).

**Dependensi:** Phase 1–6; akses ke mesin Windows atau CI Windows
(lihat `platform-compatibility.md` §4).
**Deliverable:** installer/executable jalan di Windows, ASIO dikenali bila
driver tersedia, WASAPI sebagai fallback.
**Estimasi:** 2–3 minggu.

## 10. Phase 9 — Performance optimization, regression & release

**Scope:** profiling (CPU per efek, latensi round-trip), optimasi hot path,
regression test seluruh efek + preset, perbaikan bug dari Phase 7–8,
keputusan lisensi final (JUCE + ASIO), dokumentasi rilis, versioning.

**Dependensi:** Phase 7–8.
**Deliverable:** release candidate + laporan regression + keputusan lisensi
tertulis.
**Estimasi:** 2–3 minggu.

## 11. Aturan gerbang antar-phase

1. Setiap phase berakhir dengan laporan: scope, file yang berubah, tes yang
   dilakukan, hasil tes, known issues, sisa tugas.
2. Phase berikutnya tidak dimulai sebelum laporan phase sebelumnya diterima
   user. Ini berlaku terutama untuk Phase 0 → 1 (persetujuan eksplisit).
3. Status setiap fitur dicatat dengan taksonomi di `testing-strategy.md` §5:
   Implemented / Partially implemented / UI prototype only / Not implemented /
   Tested on device / Not yet tested.
4. Dilarang mengklaim efek "berfungsi" sebelum diuji lewat audio engine
   (bukan hanya karena UI-nya tampil).
5. Dilarang menghapus/mengganti sistem existing tanpa alasan teknis yang jelas
   dan dicatat.
