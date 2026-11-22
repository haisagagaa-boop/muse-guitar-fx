# Laporan 2.2 — Drive: Overdrive + Distortion (Phase 2)

Tanggal: 2026-11-22 ~17:35–17:55 WIB
Status awal: 2.1 selesai (HEAD `2edcab9`, CI Windows hijau run
45012537979, 7/7); 2.2 disetujui user 2026-11-22 17:35 WIB
("Lanjut ke 2.2").

## A. Status

**Berhasil.**

Dua efek drive selesai dan terverifikasi:

- `OverdriveProcessor` (soft clipping tanh) dan `DistortionProcessor`
  (hard clip asimetris +1,0/−0,65, rentang drive 0–60 dB) tersedia di
  shared core `muse_dsp` (JUCE-independent, C++17, pola
  `AudioProcessor` + `SmoothedParameter` + `EffectChain` yang sudah
  ada).
- **Oversampling 4x (default; bisa 1/2/4) dikelola DI DALAM node**
  (`DriveProcessor`): upsample polyphase + waveshaper + tone filter
  pada rate oversampled + downsample, filter anti-aliasing FIR
  windowed-sinc Hamming 128 tap, cutoff 0,85 × Nyquist base. Semua
  buffer/state dialokasikan di `prepare()` (untuk faktor maks 4x).
- Semua parameter lewat `SmoothedParameter` (atomic, lock-free,
  smoothing 20 ms); `process()` bebas alokasi, bebas mutex, bebas
  file I/O (desain + inspeksi kode, seperti B3).
- Denormal protection tanpa JUCE: flush-to-zero eksplisit pada state
  tone filter + state DC blocker bila |state| < 1e-30 (preseden 2.1).
- Verifikasi: `drive_tests` (Catch2) **16 test cases, 72.278
  assertions, semua lolos**; `ctest` host **8/8 lolos** (7 lama +
  1 baru); core ter-cross-compile ke Android arm64-v8a
  (`drive_tests` ELF64 AArch64, hanya dikompilasi, tidak dijalankan
  — tanpa perangkat); **Windows CI hijau run 45014275274, 8/8**
  (commit `f59020e`).
- Versi core **tetap 0.2.0-b3** (bump versi diputuskan di 2.4).

## B. Perubahan

### File baru (7)

| File | Baris | Isi |
|---|---|---|
| `core/include/muse_dsp/drive_processor.h` | 142 | `DriveProcessor`: kelas dasar drive — oversampling, FIR, tone, DC blocker, parameter; dokumentasi desain lengkap |
| `core/src/drive_processor.cpp` | 278 | Upsample polyphase (dengan kompensasi gain zero-stuffing ×factor), waveshaper + tone pada rate oversampled, downsample, DC blocker, desain FIR |
| `core/include/muse_dsp/overdrive_processor.h` | 43 | `OverdriveProcessor`: tanh, drive 0–30 dB, tone 500–8000 Hz, level ±24 dB |
| `core/src/overdrive_processor.cpp` | 22 | `waveshape()` = tanh |
| `core/include/muse_dsp/distortion_processor.h` | 45 | `DistortionProcessor`: hard clip asimetris, drive 0–60 dB, tone 500–8000 Hz, level ±24 dB |
| `core/src/distortion_processor.cpp` | 30 | `waveshape()` = clamp(x, −0,65, +1,0) |
| `tests/drive_tests.cpp` | 713 | Suite Catch2: 16 test cases, 72.278 assertions (THD, spektrum, aliasing ON vs OFF, tone, bypass, blok, parameter, ekstrem, reset, thread-safety) |

### File diubah (2)

| File | Perubahan | Alasan |
|---|---|---|
| `core/CMakeLists.txt` | Tambah `drive_processor.cpp` + `overdrive_processor.cpp` + `distortion_processor.cpp` ke `muse_dsp` | Sumber efek baru |
| `tests/CMakeLists.txt` | Target `drive_tests` + entri CTest | Suite unit test 2.2 |

### File dihapus

Tidak ada.

### Yang sengaja TIDAK dibuat/diubah di 2.2

- Tidak ada EQ (itu 2.3) dan tidak ada preset (itu 2.4).
- `GainProcessor`, `NoiseGateProcessor`, `CompressorProcessor` tidak
  diubah; `muse_dsp.h` / `muse_dsp.cpp` tidak disentuh (versi tetap
  0.2.0-b3).
- Tidak ada perubahan pada dokumen Phase 0, backup Gate A,
  `docs/phase1-final-verification.md`, `docs/phase2-2.1-report.md`,
  `LICENSE`, dan `.github/workflows/` (workflow tidak disentuh sama
  sekali).

## C. Dependency

**Tidak ada dependency baru.**

- JUCE 8.0.15 (submodule, sudah ada sejak B2) tidak dipakai oleh core
  drive — core tetap JUCE-independent.
- Catch2 v3.7.1 (vendored, sudah ada sejak B4) dipakai suite
  `drive_tests`; tidak ada penambahan dependency test.
- Tidak ada paket apt yang perlu diinstal ulang di 2.2: `configure`
  host dan Android langsung berhasil (header X11 dari B3 masih ada —
  VM belum di-recycle sejak B3).

## D. Build

- **Host** (`cmake --build --preset host-release --parallel 2`):
  berhasil, tanpa warning/error baru. `drive_tests` ter-link.
- **Android** (`cmake --build --preset android-arm64-v8a --parallel
  2`): berhasil; `build/android-arm64-v8a/tests/drive_tests`
  terverifikasi **ELF 64-bit LSB, ARM aarch64** (BuildID
  `32eb28cd…`). Hanya dikompilasi, tidak dijalankan (tanpa perangkat,
  tanpa `/dev/snd`).
- **Windows CI** (GitHub Actions, workflow tidak diubah): run
  **45014275274** atas commit `f59020e`, status **success**,
  durasi ~3m01s (17:49–17:52 WIB), **8/8 tes lolos** termasuk
  `drive_tests` (1,08 detik). Satu-satunya anotasi: peringatan
  deprecation Node.js 20 pada `actions/checkout` (bukan dari kode
  kita; sama seperti run-run sebelumnya).
- Commit: **`f59020e`** (9 file, +1288 baris), identitas repo-lokal
  `Bex <bex@local>`, push ke `origin main` berhasil.

## E. Test

Semua angka di bawah diukur offline (Goertzel, tanpa FFT), sine
input 0,5 @ 48 kHz, jendela ukur steady-state 38.400 sampel (0,8
detik, bilangan bulat periode sehingga tanpa leakage). THD =
harmonik 2..8. Tidak ada uji dengar (tidak ada perangkat audio di
VM).

### E.1 THD naik monoton sesuai drive (sine 1 kHz)

| Drive | Overdrive THD | Distortion THD |
|---|---|---|
| 0 dB | 1,85% | 0,0000076% (~0) |
| 6 dB | 6,31% | 12,55% |
| 12 dB | 16,21% | 26,24% |
| 18 dB | 27,64% | — |
| 24 dB | 34,21% | 36,51% |
| 30 dB | 36,53% | — |
| 36 dB | — | 37,33% |
| 48 dB | — | 37,44% |
| 60 dB | — | 37,44% |

- Overdrive: monoton naik; THD(0 dB) 1,85% < 5%; THD(30 dB) 36,53%
  > 20% dan > 10× THD(0 dB) (19,7×). Lolos.
- Distortion: monoton naik; THD(0 dB) ~0% < 0,5% (di bawah ambang
  clip: input 0,5 < 0,65, nyaris tanpa distorsi — karakter khas hard
  clip vs tanh); THD(60 dB) 37,44% > 20%. Lolos.
- Fundamental overdrive pada drive 0 dB terjaga: **0,4686** dari
  input 0,5 (toleransi ±0,12). DC offset −2,8e-07. Lolos.

### E.2 Spektrum harmonik — karakter kedua efek berbeda

- Overdrive (simetris, tanh): harmonik ke-2 **−226,2 dB**,
  ke-4 **−226,2 dB** rel. fundamental — praktis nol (ambang
  < −50 dB). Lolos.
- Distortion (asimetris): harmonik ke-2 **−21,9 dB** rel.
  fundamental pada drive 12 dB (ambang > −30 dB). Lolos.
  Terukur per drive: 6 dB → −19,8 dB; 12 dB → −21,9 dB;
  24 dB → −33,2 dB; 36 dB → −43,4 dB; 48–60 dB → −145 dB
  (lihat F.2 — harmonik genap memuncak pada drive moderat).

### E.3 Aliasing — oversampling ON (4x) vs OFF (1x), sine 5 kHz

Rasio alias = energi gabungan bin alias {1,3,7,11,13,17,21,23} kHz
rel. fundamental 5 kHz.

| Kasus | ON (4x) | OFF (1x) | Selisih |
|---|---|---|---|
| Distortion, drive 24 dB | **−45,07 dB** | −14,29 dB | 30,8 dB |
| Distortion, drive 60 dB (maks) | **−25,96 dB** | −14,29 dB | 11,7 dB |
| Overdrive, drive 30 dB (maks) | **−43,65 dB** | (tidak diukur; tanh alias rendah) | — |

- Distorsi 24 dB: ON < −40 dB, OFF > −20 dB, selisih ≥ 15 dB.
  Lolos (margin ON 5,1 dB).
- Distorsi 60 dB: ON < −20 dB, OFF > −20 dB, selisih ≥ 10 dB.
  Lolos (margin ON 6,0 dB) — ambang worst-case yang menjepit
  perilaku pada drive maks secara jujur, bukan menyembunyikannya
  (lihat F.3).
- Overdrive 30 dB: ON < −40 dB. Lolos — **tetapi margin hanya
  3,65 dB** (lihat F.4).

### E.4 Tone control

Overdrive, sine 8 kHz: magnitudo tone 500 Hz = 0,0184; tone
8000 Hz = 0,2083 — selisih **21,1 dB** (ambang > 10 dB). Lolos.

### E.5 Bypass, ukuran blok, perubahan parameter, ekstrem, reset, thread-safety

- Bypass overdrive dan distortion = input (fundamental 0,5 ±1e-6).
  Lolos.
- Output **bit-identik (maxDiff = 0)** di blok 64/128/256/512/1024
  (chain OD + Dist, perubahan drive di sampel 2048). Lolos.
- Perubahan drive di tengah render (OD 6 → 24 dB di sampel 2048):
  THD **6,30% → 34,21%** (rasio 5,43×, ambang > 3×). Lolos.
- Parameter ekstrem (OD 30 dB/tone 500/level +24 dB + Dist
  60 dB/tone 8000/level +24 dB, sine 0,9 + sunyi 0,5 dtk):
  semua 72.000 sampel finite (tanpa NaN/inf); max |output| **16,11**
  (ambang < 20, diturunkan dari desain: clip ≤ 1,0 × level +24 dB
  = 15,85); bagian sunyi peak < 1e-6 (state ter-flush); DC offset
  **3,3e-05** (ambang < 0,05 — DC blocker bekerja). Lolos.
- Reset: setelah diproses keras lalu `reset()`, sunyi tetap nol
  (OD dan Dist); drive Dist kembali ke nilai awal 24 dB. Lolos.
- Thread-safety: 200× perubahan drive/tone/level dari control
  thread saat MockAudioDevice berjalan — tanpa crash, parameter
  konvergen ke nilai akhir. Lolos (pola `engine_check` B3).

## F. Risiko dan kendala

### F.1 Bug gain upsampling — ditemukan dan diperbaiki di 2.2

Run pertama `drive_tests`: 10/16 lolos, 5 gagal. Kegagalan paling
mendasar: fundamental overdrive pada drive 0 dB terukur **0,1238**
dari input 0,5 — tepat 1/4. Penyebab: filter interpolasi
polyphase. Jumlah tap tiap fase ~1/factor dari total (filter DC
gain 1), jadi sinyal upsampled melemah 1/factor oleh zero-stuffing;
filter interpolasi standar harus memiliki DC gain = factor.
Perbaikan: `upSampled *= factor` setelah penjumlahan polyphase
(`core/src/drive_processor.cpp`). Setelah perbaikan, fundamental
0,4686 dan semua tes THD/alias bergeser ke titik kerja yang benar.
Tidak ada ambang yang dilemahkan untuk menutupi kegagalan ini —
bug-nya diperbaiki di implementasi.

### F.2 Harmonik genap distorsi lenyap pada drive ekstrem — fisika, bukan bug

Draf awal tes mengukur harmonik ke-2 pada drive 60 dB dan mendapat
**−145 dB** (gagal ambang > −30 dB). Diagnosis: pada drive
ekstrem, output waveshaper saturasi apa pun mendekati gelombang
dua level duty 50%, yang harmonik genapnya ~0 secara matematis —
bahkan dengan level clip asimetris (+1,0/−0,65), karena yang
menciptakan harmonik genap adalah asimetri *bentuk* (slope) di
daerah transisi, bukan asimetri level jenuh. Pengukuran membuktikan:
harmonik ke-2 memuncak pada drive moderat (−19,8 dB @ 6 dB,
−21,9 dB @ 12 dB) lalu meluruh (−33,2 dB @ 24 dB, −145 dB @
48 dB). Tes diperbaiki menjadi drive 12 dB (titik "crunch"
realistis) dengan ambang > −30 dB, dan alasannya didokumentasikan
di komentar tes. Saturator halus alternatif
(x/(1+|x|^8)^(1/8), asimetris 0,65) diukur IDENTIK pada drive
48 dB — jadi bukan masalah bentuk waveshaper.

### F.3 Aliasing pada drive maks tidak mencapai −40 dB — batas fisika 4x

Pada drive 60 dB, aliasing distorsi 4x terukur −25,96 dB (bin
dominan: 3 kHz −30,4 dB, 7 kHz −31,8 dB, 1 kHz −33,9 dB).
Penyebab: hard clipper menghasilkan harmonik ~1/n; harmonik di
atas Nyquist rate 4x (96 kHz) melipat DI DALAM domain 4x ke
frekuensi rendah (harmonik ke-37/39 dari 5 kHz → 7/3 kHz)
SEBELUM filter downsample sempat bekerja — filter hanya bisa
meredam komponen yang memang berada di atas 24 kHz dalam domain
4x. Ini batas fisika oversampling 4x untuk hard clipper, bukan
cacat implementasi: saturator halus (sudut membulat, harmonik
meluruh lebih cepat) diukur IDENTIK −25,81 dB pada drive 48 dB,
karena pada drive dalam outputnya sama-sama mendekati gelombang
dua level. Yang dilakukan: (a) tes utama memakai drive 24 dB
(ambang −40 dB, terukur −45,07 dB); (b) tes kedua menjepit
worst-case 60 dB dengan ambang −20 dB (terukur −25,96 dB),
sehingga regresi tetap terdeteksi. Opsi perbaikan nyata —
oversampling 8x — di luar cakupan 2.2 (target desain 4x);
keputusan ada di 2.4/Phase 3 bila user menginginkannya.
Dalam praktik, alias −26 dB berada 26 dB di bawah fundamental
(tidak dominan) dan termasking saat bermain.

### F.4 Margin paling tipis: alias overdrive 30 dB (3,65 dB)

Alias overdrive naik curam antara 24 dB (−70,26 dB) dan 30 dB
(−43,65 dB): pada drive maks, tanh jenuh penuh (input 0,5 ×
31,6 ≈ 15,8) sehingga output mendekati gelombang kotak dan
lantai aliasnya sama seperti hard clip. Pengukuran deterministik
(input dan kode sama → hasil sama setiap run), jadi margin 3,65 dB
atas ambang −40 dB dapat diterima, tetapi ini margin paling tipis
di suite — bila implementasi berubah (mis. FIR diganti), tes ini
yang pertama memberi sinyal.

### F.5 Dua kesalahan draf tes (bukan implementasi)

- Jendela ukur THD "sebelum" perubahan drive (draf: sampel
  480–4320) melewati titik perubahan (2048) sehingga mengukur
  segmen non-stasioner dan menggelembungkan THD "sebelum" menjadi
  11,16% (vs 6,30% yang benar). Diperbaiki ke sampel 128–2048
  (1.920 sampel = 40 periode @ 1 kHz, bilangan bulat).
- Ambang max |output| ekstrem (draf: < 10,0) salah hitung: level
  +24 dB pada dua efek seri memang mengizinkan ~15,85 (clip 1,0 ×
  15,85). Diperbaiki ke < 20,0 dengan penurunan dari desain.
  Keduanya didokumentasikan di komentar tes.

### F.6 Batasan yang masih berlaku

- Tidak ada uji dengar dan tidak ada uji latensi round-trip (tidak
  ada perangkat audio di VM; uji dengar hanya di perangkat fisik,
  Galaxy M32 + USB audio interface, sesuai keputusan Phase 1).
- Latensi tambahan oversampling ~0,66 ms @ 48 kHz (FIR 128 tap,
  up + down) tidak dikompensasi — di bawah 1 ms, tidak terdengar.
- Perubahan faktor oversampling dari control thread aman secara
  data (atomic) tetapi dapat menimbulkan transient singkat karena
  delay line berisi data rate lama — sebaiknya diubah saat device
  berhenti (didokumentasikan di header).

## G. Keputusan

1. **Overdrive = tanh** — soft clipping simetris, C-infinity,
   monoton, terbatas ±1; harmonik ganjil saja (terukur: ke-2/ke-4
   −226 dB); THD naik halus 1,85% → 36,53% pada 0–30 dB.
2. **Distortion = hard clip asimetris clamp(x, −0,65, +1,0)** —
   karakter jelas berbeda dari overdrive (bukan duplikat dengan
   angka berbeda): THD ~0 di bawah ambang clip, naik curam ke
   37,44%; harmonik genap jelas hadir pada drive moderat
   (−21,9 dB @ 12 dB); rentang drive lebih besar (0–60 dB vs
   0–30 dB). Alternatif saturator halus ditolak berdasarkan
   pengukuran (identik pada drive dalam — lihat F.2/F.3).
3. **Oversampling 4x default (target desain), 2x minimum, 1x
   untuk pembuktian** — dikelola di dalam node; faktor 2 dan 4
   memakai FIR masing-masing (koefisien dihitung di `prepare()`).
   FIR windowed-sinc Hamming 128 tap, cutoff 0,85 × Nyquist base.
4. **Tone = lowpass satu-pole pasca-waveshaper pada rate
   oversampled** (500–8000 Hz), seperti desain audio-engine.
5. **DC blocker high-pass satu-pole 20 Hz di output** (base rate,
   per channel) — wajib karena clip asimetris menghasilkan offset
   DC; terukur DC 3,3e-05 pada kondisi ekstrem. Cutoff jauh di
   bawah E2 = 82 Hz.
6. **Denormal protection mengikuti preseden 2.1**: flush-to-zero
   eksplisit (< 1e-30) pada state tone filter + state DC blocker,
   tanpa JUCE.
7. **Ambang tes ditetapkan dari pengukuran, bukan dari harapan**:
   semua ambang di atas memiliki margin atas nilai terukur
   (alias distorsi 24 dB: 5,1 dB; alias maks: 6,0 dB; alias OD:
   3,65 dB — lihat F.4). Tidak ada ambang yang dilemahkan untuk
   menutupi kegagalan — yang gagal diperbaiki implementasinya
   (F.1) atau tesnya dibetulkan karena salah mengukur (F.2, F.5).
8. **Versi core tetap 0.2.0-b3** di 2.2; bump versi diputuskan di
   2.4 (sesuai rencana sub-gerbang).
9. **BERHENTI — 2.3 (EQ) tidak dimulai tanpa persetujuan
   eksplisit user.**
