# Laporan 2.1 — Dynamics: Noise Gate + Compressor (Phase 2)

Tanggal: 2026-11-22 ~17:17–17:40 WIB
Status awal: Phase 1 selesai (HEAD `7ac98b3`, CI Windows hijau run
45010775935); 2.1 disetujui user 2026-11-22 17:15 WIB ("Lanjut").

## A. Status

**Berhasil.**

Dua efek dynamics pertama selesai dan terverifikasi:

- `NoiseGateProcessor` dan `CompressorProcessor` tersedia di shared core
  `muse_dsp` (JUCE-independent, C++17, pola `AudioProcessor` +
  `SmoothedParameter` + `EffectChain` yang sudah ada).
- Semua parameter lewat `SmoothedParameter` (atomic, lock-free,
  smoothing 20 ms); `process()` bebas alokasi, bebas mutex, bebas
  file I/O (desain + inspeksi kode, seperti B3).
- Denormal protection tanpa JUCE: flush-to-zero eksplisit pada state
  envelope (lihat B dan F.2).
- Verifikasi: `dynamics_tests` (Catch2) **15 test cases, 72.360
  assertions, semua lolos** (3× pengulangan stabil); `ctest` host
  **7/7 lolos** (6 lama + 1 baru); core ter-cross-compile ke Android
  arm64-v8a (hanya dikompilasi, tidak dijalankan — tanpa perangkat).
- `GainProcessor` **tidak diubah** (tetap prosesor referensi B3).
- Versi core **tetap 0.2.0-b3** (bump versi diputuskan di 2.4).

## B. Perubahan

### File baru (5)

| File | Baris | Isi |
|---|---|---|
| `core/include/muse_dsp/noise_gate_processor.h` | 91 | `NoiseGateProcessor`: threshold/attack/release/range dB, hysteresis 3 dB, dokumentasi desain + denormal |
| `core/src/noise_gate_processor.cpp` | 147 | Envelope follower peak satu-pole (linked stereo) + gain computer hysteresis + gain ramp satu-pole |
| `core/include/muse_dsp/compressor_processor.h` | 99 | `CompressorProcessor`: threshold/ratio/attack/release/makeup/knee, dokumentasi desain + denormal |
| `core/src/compressor_processor.cpp` | 168 | Envelope follower peak satu-pole + gain computer soft knee (kuadrat kontinu) + makeup gain |
| `tests/dynamics_tests.cpp` | 685 | Suite Catch2: 15 test cases, 72.360 assertions (gate, comp, blok, parameter, ekstrem, reset, thread-safety) |

### File diubah (2)

| File | Perubahan | Alasan |
|---|---|---|
| `core/CMakeLists.txt` | Tambah `noise_gate_processor.cpp` + `compressor_processor.cpp` ke `muse_dsp` | Sumber efek baru |
| `tests/CMakeLists.txt` | Target `dynamics_tests` + entri CTest | Suite unit test 2.1 |

### File dihapus

Tidak ada.

### Yang sengaja TIDAK dibuat/diubah di 2.1

- Tidak ada Overdrive/Distortion/EQ (itu 2.2/2.3) dan tidak ada preset
  (itu 2.4).
- `GainProcessor` tidak diubah perilakunya; `muse_dsp.h` /
  `muse_dsp.cpp` tidak disentuh (versi tetap 0.2.0-b3).
- Tidak ada perubahan pada dokumen Phase 0, backup Gate A,
  `docs/phase1-final-verification.md`, `LICENSE`, dan
  `.github/workflows/` (workflow tidak disentuh sama sekali).

### Desain efek (ringkas)

**Noise Gate** (per frame, linked stereo — satu gain untuk semua
channel):
1. Envelope follower peak satu-pole: naik dengan attack, turun dengan
   release; envelope dari |sampel| terbesar di semua channel.
2. Gain computer hysteresis 3 dB: TERBUKA bila envelope > threshold,
   TERTUTUP bila envelope < threshold − 3 dB; di antaranya state
   dipertahankan — mencegah chattering pada sinyal di sekitar threshold.
3. Gain ramp satu-pole menuju 0 dB (terbuka) atau range dB (tertutup),
   laju attack saat membuka dan release saat menutup — tanpa klik.
   Default: threshold −45 dB, attack 1 ms, release 100 ms, range −80 dB.

**Compressor** (per frame, linked stereo):
1. Envelope follower peak satu-pole (attack/release), sama seperti gate.
2. Gain computer soft knee (dB): di bawah knee GR = 0; di atas knee
   GR = (level − threshold) × (1 − 1/ratio); di dalam knee GR mengikuti
   kurva kuadrat yang kontinu (nilai dan turunan cocok di kedua ujung
   knee). Knee 0 = hard knee (dijaga dari pembagian nol).
3. Gain diterapkan = −GR + makeup gain; attack/release berasal dari
   envelope follower — tanpa lompatan, tanpa pumping pada sinyal steady.
   Default: threshold −20 dB, ratio 4:1, attack 10 ms, release 100 ms,
   makeup 0 dB, knee 6 dB.

**Denormal protection (tanpa JUCE):** envelope adalah state rekursif
yang meluruh eksponensial menuju nol saat input sunyi; tanpa perlakuan
ia masuk rentang denormal (CPU spike). Mekanisme: flush-to-zero
eksplisit — envelope di bawah 1e-30 (≈ −600 dB, jauh di bawah ambang
dengar) langsung di-nol-kan, tiap frame. Tidak ada instruksi platform
(FTZ/DAZ via CSR), tidak ada DC offset pada sinyal. State gain gate
bergerak menuju target dan di-clamp tepat di target (tidak ada ekor
asimtotik/denormal); `SmoothedParameter` juga meng-clamp tepat di akhir
ramp. Mekanisme didokumentasikan di kedua header prosesor.

**Rentang parameter (di-clamp di setter/konstruktor):** gate threshold
−100..0 dB, attack 0,01..1000 ms, release 1..5000 ms, range −100..0 dB;
comp threshold −60..0 dB, ratio 1..20, attack 0,01..1000 ms,
release 1..5000 ms, makeup −12..+24 dB, knee 0..20 dB.

### Verifikasi tidak berubah

- Keenam file Phase 0 cocok dengan baseline SHA-256 audit recovery
  (tidak dihitung ulang di 2.1 — tidak ada file Phase 0 yang disentuh;
  `git status` hanya menunjukkan 2 file diubah + 5 file baru di atas).
- Backup Gate A tidak disentuh:
  `~/workspace/backups/muse-guitar-fx-phase0-2026-11-21/` (tidak ada
  perintah baca/tulis ke direktori itu di 2.1).
- `docs/phase1-final-verification.md`, `LICENSE`, dan
  `.github/workflows/` tidak disentuh.

## C. Dependency

Tidak ada dependency baru di 2.1.

| Nama | Versi | Sumber | Lisensi | Status |
|---|---|---|---|---|
| JUCE | 8.0.15 (dipin) | git submodule `third_party/JUCE`, commit `91ad83ae…` | AGPLv3 / EULA komersial | Tidak berubah (core tidak memakai JUCE) |
| Catch2 | 3.7.1 | `third_party/catch2/` (di-vendor, B4) | BSL-1.0 | Tidak berubah; dipakai suite baru |
| Android NDK | r28c | `~/workspace/android-sdk/ndk/` | Android SDK License Agreement | Tidak berubah |
| CMake / GCC | 3.28.3 / 13.3.0 | sistem | BSD 3-clause / GPL+exception | Tidak berubah |
| C++ stdlib (`<cmath>`, `<atomic>`, `<thread>`) | C++17 | toolchain | — (bagian toolchain) | Dipakai efek + tes; tanpa dependency tambahan |
| ASIO SDK | — | — | Dual GPLv3/proprietary | **Tidak diambil** — tetap ditunda ke Phase 8 |

## D. Build

### Host (Linux, Release, `-j 2`)

Perintah:

```sh
cmake --preset host-release
cmake --build --preset host-release --parallel 2
```

Hasil: configure OK (1,5 detik, juceaide jalan — paket `-dev` sistem
masih ada, tidak perlu instal ulang); build **berhasil** (muse_dsp +
semua target tes termasuk `dynamics_tests`). **Tidak ada warning**
compiler pada kode baru (`-Wall -Wextra`, diverifikasi dengan rebuild
file baru + grep warning/error = kosong). Tidak ada error, tidak ada OOM.

### Android (cross-compile, arm64-v8a)

Perintah:

```sh
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

Hasil: configure OK; build **berhasil** — termasuk `dynamics_tests`.
`build/android-arm64-v8a/tests/dynamics_tests` terverifikasi **ELF
64-bit ARM aarch64**. Tidak ada kegagalan link baru (Catch2 sudah
me-link `log` sejak B4; core tidak menambah simbol platform).
Hanya dikompilasi — **tidak dijalankan** (tanpa perangkat).

### Windows CI

Dijalankan setelah commit + push (lihat E).

## E. Test

### ctest host-release — 7/7 LULUS

| Test | Hasil |
|---|---|
| `muse_dsp_smoke_test` | Lulus (versi core 0.2.0-b3, tidak berubah) |
| `engine_check` | Lulus — 27 checks (B3, tidak berubah) |
| `effect_chain_tests` | Lulus — 34 assertions dalam 8 test cases (B4, tidak berubah) |
| `dynamics_tests` | Lulus — **15 test cases, 72.360 assertions** (baru, 2.1) |
| `offline_render_256` | Lulus (B4, tidak berubah) |
| `offline_render_1024` | Lulus (B4, tidak berubah) |
| `juce_smoke_test` | Lulus (JUCE 8.0.15, tidak berubah) |

### Rincian `dynamics_tests` (Catch2, 15 test cases)

**Noise Gate (5):**

1. **Atenuasi di bawah threshold:** sine −50 dB, threshold −40 dB,
   range −80 dB → atenuasi **≥ 70 dB** (syarat ≥ 40 dB); varian
   range −60 dB → atenuasi **60 dB ± 2 dB**. Lolos.
2. **Sinyal di atas threshold lewat:** sine −20 dB → peak output
   0,1 ± 0,5 dB (gain 0 dB). Lolos.
3. **Tanpa chattering di dekat threshold:** sine −41 dB (1 dB di bawah
   threshold) selama 1 detik — 10 jendela 50 ms di 0,5 detik terakhir
   semuanya teratenuasi ≥ 40 dB dengan spread < 1 dB (tidak ada
   toggling buka/tutup). Lolos.
4. **On/off bergantian bersih:** 4 siklus 400 ms ON (−20 dB) /
   400 ms OFF (−60 dB), release 50 ms — 100 ms terakhir tiap periode
   ON = input ± 0,5 dB; 100 ms terakhir tiap periode OFF atenuasi
   ≥ 40 dB; level 8 jendela dalam tiap periode OFF monoton turun
   (tidak ada pembukaan kembali). Lolos.
5. **Bypass:** gate di-bypass, sine −50 dB → output = input
   (± 1e-6). Lolos.

**Compressor (5):**

6. **GR sesuai rasio ± 1 dB (DC, hard knee):** −10 dB, T = −20 dB —
   ratio 4:1 → GR **7,5 dB**; ratio 2:1 → GR **5 dB**; di bawah
   threshold (−30 dB) → GR **0 ± 0,5 dB**. Lolos.
7. **Soft knee:** knee 12 dB, T = −20 dB, R = 4:1 — di −26 dB GR = 0;
   di −20 dB GR = **1,125 dB**; di −14 dB GR = **4,5 dB**
   (masing-masing ± 0,5 dB). Lolos.
8. **Attack/release mengikuti konstanta waktu (DC bertahap):**
   attack 10 ms — GR setelah 1 τ = **~60%** dari akhir (rentang uji
   40–80%), setelah 5 τ ≥ 95%; release 100 ms — GR setelah 1 τ
   ~14% (batas ≤ 40%), setelah 5 τ = 0% (batas ≤ 5%). Sesuai
   signature satu-pole. Lolos.
9. **Tanpa pumping:** sine −10 dB steady 1 detik — GR stabil
   (spread antar-jendela 0 dB, batas < 0,5 dB); setelah step down,
   GR turun monoton (50 sampel tiap 10 ms, toleransi 0,05 dB).
   Catatan: GR steady sine terukur 6,12 dB, bukan 7,5 dB rumus peak —
   itu benar untuk topologi ini (envelope satu-pole mengikuti puncak
   sine yang telah dihaluskan, bukan puncak sesaat); akurasi rumus
   GR diverifikasi eksak dengan DC di test 6. Lolos.
10. **Bypass + makeup gain:** bypass → output = input; makeup +6 dB
    di bawah threshold (GR = 0) → output = input × 2 (± 0,5 dB). Lolos.

**Umum (5):**

11. **Kesamaan output di semua ukuran blok (64/128/256/512/1024):**
    chain Gate + Compressor, sine 440 Hz, perubahan threshold
    kompresor di sampel 2048 — perbedaan maks antar ukuran blok
    **< 1e-6**. Lolos.
12. **Perubahan parameter di tengah render:** threshold kompresor
    −20 dB → −40 dB di sampel 24064 (DC −10 dB) — GR 7,5 dB →
    **22,5 dB** setelah settle (± 1 dB). Lolos.
13. **Parameter ekstrem tanpa NaN/inf:** gate (−100 dB / 0,01 ms /
    5000 ms / −100 dB) + comp (−60 dB / 20:1 / 0,01 ms / 5000 ms /
    +24 dB / knee 20 dB), sine 0,9 selama 1 detik + sunyi 0,5 detik
    (jalur flush-to-zero) — semua 72.000 sampel finite, max |output|
    < 10; bagian sunyi tetap nol (< 1e-6). Lolos.
14. **Thread-safety (mock device):** chain [Gate, Compressor]
    berjalan di mock device sementara control thread mengubah
    threshold gate, ratio, dan threshold kompresor 200× — tanpa
    crash/deadlock; nilai akhir konvergen (threshold gate −35 dB,
    ratio 6, threshold comp −25 dB); output rekaman finite. Lolos.
15. **Reset:** gate yang terbuka → reset → sinyal sunyi tertutup
    seperti baru (atenuasi ≥ 40 dB); kompresor → reset → sampel
    pertama GR ≈ 0 lalu steady GR kembali 7,5 dB. Lolos.

### Stabilitas (flakiness)

`dynamics_tests` dijalankan **3× berturut-turut** setelah perbaikan
terakhir — ketiganya hijau (72.360 assertions, 15 test cases).
`ctest` penuh dijalankan **2×** — keduanya 7/7 hijau.

### Dua kegagalan awal (bukan bug engine — keduanya bug/ekspektasi tes)

1. **GR steady sine 6,12 dB vs harapan 7,5 dB** (test 9): ekspektasi
   tes salah — envelope satu-pole tidak menyamai puncak sine (lihat
   test 9 di atas). Diperbaiki dengan mengubah asersi menjadi
   stabilitas + kompresi aktif; akurasi rumus tetap diuji eksak
   dengan DC (test 6). **Tidak ada perubahan kode efek.**
2. **Perubahan threshold tengah render tidak terpicu** (test 12):
   pemicu di sampel 24000 — bukan kelipatan ukuran blok 256, sehingga
   callback `onBlock` tidak pernah melihatnya (pelajaran B4 soal
   sampel 2048). Diperbaiki ke sampel 24064 (= 94 × 256). **Tidak ada
   perubahan kode efek.**

### Windows CI

Run **45012301976** — https://github.com/haisagagaa-boop/muse-guitar-fx/actions/runs/45012301976

- Kesimpulan: **success**. Durasi: 2 menit 34 detik. Dimulai
  2026-11-22 17:24:38 WIB, selesai 17:27:15 WIB. Commit yang diuji:
  `b31a41e` (`b31a41e870589f3e90fdfaa3120bf4c9eb1e0c31`), branch `main`,
  workflow "Windows build", runner windows-2022 (VS 2022, Release).
- Langkah: Set up job ✓, checkout ✓, Configure ✓, Build ✓, Test ✓.
- `ctest`: **7/7 lolos, 100% tests passed** — `muse_dsp_smoke_test`
  (0,01 dtk), `engine_check` (1,11 dtk), `effect_chain_tests`
  (0,01 dtk), `dynamics_tests` (**0,73 dtk**), `offline_render_256`,
  `offline_render_1024`, `juce_smoke_test` (masing-masing 0,01 dtk).
- Log mentah tersimpan di
  `~/workspace/muse-guitar-fx-ci-logs/run-45012301976.log` (di VM,
  di luar repo).
- Anotasi yang tersisa (bukan kegagalan): peringatan deprecasi Node.js
  20 pada actions/checkout (sama seperti Phase 1 — checkout tetap
  berjalan; migrasi ke checkout v5 adalah housekeeping, bukan 2.1).

### Yang belum dapat dilakukan

- Uji dengar / latensi round-trip di perangkat fisik — tidak ada
  perangkat audio di VM, tidak ada HP terhubung. **Tidak ada klaim
  latensi < 10 ms dan tidak ada klaim kualitas suara.**
- Pengujian di perangkat Android — hanya cross-compile; cara
  menjalankannya di HP didokumentasikan di
  `docs/android-testing-limitations.md`.
- Status efek (taksonomi `testing-strategy.md` §5): Noise Gate dan
  Compressor = `Implemented` + `Not yet tested` (di perangkat).

## F. Risiko dan kendala

1. **Envelope peak satu-pole bukan peak-hold instan.** Pada sinyal
   periodik (sine), envelope steady-state berada di bawah puncak
   (6,12 dB vs 7,5 dB rumus peak pada sine −10 dB) — perilaku yang
   benar untuk topologi ini, tetapi perlu dipahami saat tuning:
   threshold efektif sedikit lebih tinggi dari angka yang tertera
   untuk sinyal periodik. Bila tuning di perangkat nanti menunjukkan
   gate/comp terasa "lambat", opsi yang sudah dipertimbangkan adalah
   detector peak-hold instan + smoothing pada gain (bukan pada
   envelope) — belum diimplementasikan, karena desain saat ini lolos
   semua kriteria 2.1.
2. **Klaim "callback bebas alokasi" masih desain + inspeksi** (seperti
   dicatat di B3/B4) — `process()` kedua efek hanya membaca atomic
   dan aritmetika float per frame; tidak ada alokasi/mutex/file I/O.
   Belum ada allocation counter runtime.
3. **Perubahan struktur chain TIDAK real-time safe** (seperti dicatat
   di B3) — tidak berubah di 2.1; parameter dan bypass tetap aman
   kapan pun.
4. **Paket `-dev` sistem bisa hilang lagi saat VM di-recycle** (temuan
   B3) — kali ini tidak terjadi (configure host langsung OK);
   perintah instal ulang tetap terdokumentasi di
   `docs/dependencies.md` §1a.
5. **Tidak ada uji dengar.** Semua verifikasi 2.1 adalah offline
   deterministik. Kualitas musikal gate/comp (mis. gate terlalu
   agresif pada decay gitar, comp terasa "memompa" pada permainan
   nyata) hanya bisa dinilai di perangkat fisik (Phase 7).
6. **Hysteresis gate 3 dB adalah nilai tetap internal** (bukan
   parameter). Bila tuning nanti butuh hysteresis yang bisa diatur,
   perlu dijadikan parameter — dicatat sebagai calon perbaikan 2.4
   atau Phase 9, bukan bagian 2.1.

## G. Keputusan

- **2.1 selesai dan terverifikasi** (host 7/7, Android cross-compile
  OK, CI Windows hijau run 45012301976 — 7/7 lolos di Windows).
- **BERHENTI — menunggu persetujuan user sebelum 2.2.** Jangan mulai
  2.2 (Overdrive + Distortion) tanpa perintah eksplisit.
- Syarat yang belum terpenuhi: persetujuan eksplisit user untuk 2.2.

**BERHENTI — menunggu persetujuan user sebelum 2.2. Jangan mulai 2.2.**
