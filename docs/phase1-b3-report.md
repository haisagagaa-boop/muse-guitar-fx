# Laporan B3 — Fondasi Audio Engine (Phase 1, Gate B)

Tanggal: 2026-11-22 ~12:34–13:00 WIB
Status awal: B2 selesai (commit `d925e2b`), repo bersih, B3 disetujui user
2026-11-22 12:34 WIB ("Lanjut ke B3").

## A. Status

**Berhasil.**

Fondasi audio engine selesai dan terverifikasi:

- Audio processing interface (`AudioProcessor` + `ProcessSpec`) dan
  `AudioBuffer` tersedia di shared core `muse_dsp` (JUCE-independent).
- `EffectChain` tersedia sebagai sumber kebenaran urutan efek — urutan
  eksplisit, reorder benar-benar mengubah hasil pemrosesan.
- Parameter thread-safe (atomic, lock-free) + smoothing ramp linear
  (20 ms default) — perubahan gain tidak menimbulkan klik.
- Audio thread terpisah dari UI/control thread (mock device menjalankan
  chain di thread-nya sendiri); callback bebas alokasi, bebas mutex
  blocking, bebas file I/O (berdasarkan desain + inspeksi kode — lihat F.2).
- Mock audio device tersedia: input sine, merekam output, mengukur waktu
  callback dan overrun.
- Verifikasi: `engine_check` **27 checks lolos**, `ctest` host **3/3 lolos**,
  core ter-cross-compile ke Android arm64-v8a (hanya dikompilasi, tidak
  dijalankan — tanpa perangkat).

## B. Perubahan

### File baru (11)

| File | Isi |
|---|---|
| `core/include/muse_dsp/audio_buffer.h` + `core/src/audio_buffer.cpp` | Buffer audio antar-prosesor; dialokasikan sekali saat prepare, dipakai ulang tiap blok |
| `core/include/muse_dsp/audio_processor.h` | Interface `AudioProcessor` (`prepare`/`process`/`reset`/`getName`), `ProcessSpec`, bypass atomic |
| `core/include/muse_dsp/smoothed_parameter.h` | `SmoothedParameter`: target `std::atomic<float>` (lock-free), ramp linear per frame |
| `core/include/muse_dsp/gain_processor.h` + `core/src/gain_processor.cpp` | `GainProcessor` — prosesor referensi B3 (bukan efek gitar), gain di-smoothing per frame |
| `core/include/muse_dsp/effect_chain.h` + `core/src/effect_chain.cpp` | `EffectChain`: urutan eksplisit, `add`/`insert`/`remove`/`move`/`clear`, lewati prosesor yang di-bypass |
| `core/include/muse_dsp/mock_audio_device.h` + `core/src/mock_audio_device.cpp` | `MockAudioDevice`: thread audio sendiri, input sine 440 Hz, rekam output channel 0, hitung blok/overrun/waktu callback maks |
| `tests/engine_check.cpp` | Program verifikasi B3 (7 kelompok uji, 27 checks) |

### File diubah (6)

| File | Perubahan | Alasan |
|---|---|---|
| `core/CMakeLists.txt` | Tambah 4 sumber baru; `find_package(Threads)` + link `Threads::Threads` (PUBLIC) | Mock device memakai `std::thread` |
| `core/include/muse_dsp/muse_dsp.h` | Versi core 0.1.0 → **0.2.0** | Perubahan core B3 |
| `core/src/muse_dsp.cpp` | Version string `"0.1.0-b2"` → `"0.2.0-b3"` | Penanda B3 |
| `tests/CMakeLists.txt` | Tambah target `engine_check` + CTest; catatan framework unit test formal pindah ke B4 | Verifikasi B3 |
| `tests/muse_dsp_smoke_test.cpp` | Ekspektasi versi 0.1.0-b2 → 0.2.0-b3 | Ikut versi core |
| `docs/dependencies.md` | §1a: koreksi nama paket FreeType (`libfreetype-dev`, bukan `libfreetype6-dev`); catatan paket sistem hilang saat VM di-recycle + perintah instal ulang | Temuan B3 (lihat F.1) |

### File dihapus

Tidak ada.

### Yang sengaja TIDAK dibuat di B3

- Tidak ada efek gitar (Overdrive, Distortion, dll.) — itu Phase 2.
  `GainProcessor` hanya prosesor referensi untuk membuktikan plumbing.
- Tidak ada UI aplikasi.
- Tidak ada framework unit test formal (Catch2 diputuskan di B4);
  `engine_check` adalah program verifikasi gaya assert seperti smoke test B2.
- Tidak ada perubahan pada dokumen Phase 0 dan backup Gate A.

### Verifikasi tidak berubah

- Keenam file Phase 0 cocok dengan baseline SHA-256 audit recovery
  (README `b7ff2f0b…9c88`, architecture `c60f69a3…34cc`,
  audio-engine-design `05625ab6…9cf8`, development-roadmap
  `e745c682…250251`, platform-compatibility `2d24eefd…a204c`,
  testing-strategy `9285c9cb…57bb`).
- Backup Gate A utuh: `muse-guitar-fx-phase0-2026-11-21.tar.gz`
  SHA-256 `499a08ae…67c5f`, `gzip -t` OK.

## C. Dependency

Tidak ada dependency baru di B3.

| Nama | Versi | Sumber | Lisensi | Status |
|---|---|---|---|---|
| JUCE | 8.0.15 (dipin) | git submodule `third_party/JUCE`, commit `91ad83ae…` | AGPLv3 / EULA komersial | Sudah ada sejak B2, tidak berubah |
| Android NDK | r28c (28.2.13676358) | `~/workspace/android-sdk/ndk/` | Android SDK License Agreement | Sudah ada sejak B2, tidak berubah |
| CMake | 3.28.3 | sistem | BSD 3-clause | Sudah ada |
| GCC/G++ | 13.3.0 | sistem | GPL | Sudah ada |
| C++ stdlib (`<thread>`, `<atomic>`, `<chrono>`) | C++17 | toolchain | — (bagian toolchain) | Dipakai mock device + parameter; tanpa dependency tambahan |
| Paket `-dev` sistem (X11, FreeType, Fontconfig, libcurl) | Sama seperti B2 (FreeType: `libfreetype-dev` 2.13.2+dfsg-1ubuntu0.2) | apt minimal (`~/workspace/.cache/apt-minimal/`) | Beragam (X11/MIT, FTL/GPL, dsb. — build-time saja) | **Diinstal ulang di B3** karena hilang saat VM di-recycle (lihat F.1) |
| ASIO SDK | — | — | Dual GPLv3/proprietary (Okt 2025) | **Tidak diambil** — tetap ditunda ke Phase 8 |

## D. Build

### Host (Linux, Release, `-j 2`)

Perintah:

```sh
cmake --preset host-release
cmake --build --preset host-release --parallel 2
```

Hasil:

- Configure pertama **gagal**: `juceaide` gagal dibangun —
  `fatal error: X11/extensions/Xrandr.h: No such file or directory`.
  Penyebab: paket `-dev` sistem hilang karena VM di-recycle sejak B2.
  Diatasi dengan instal ulang 9 paket lewat apt minimal (perintah lengkap
  di `docs/dependencies.md` §1a); configure ulang **berhasil (2,3 detik)**.
- Build **berhasil** (muse_dsp + engine_check + muse_dsp_smoke_test +
  juce_smoke_test). Tidak ada error, tidak ada OOM.

### Android (cross-compile, arm64-v8a)

Perintah:

```sh
export ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358
cmake --preset android-arm64-v8a
cmake --build --preset android-arm64-v8a --parallel 2
```

Hasil: configure 1,8 detik OK; build **berhasil**. `libmuse_dsp.a`,
`engine_check`, dan `muse_dsp_smoke_test` terverifikasi sebagai
**ELF 64-bit ARM aarch64** (`file`). Hanya dikompilasi — **tidak dijalankan**
(tanpa perangkat Android terhubung).

### Windows CI

Tidak dijalankan di B3 (bagian B4; repo masih lokal tanpa remote GitHub).

## E. Test

### ctest host-release — 3/3 LULUS

| Test | Hasil |
|---|---|
| `muse_dsp_smoke_test` | Lulus (versi core 0.2.0-b3) |
| `engine_check` | Lulus — **27 checks, 0 failures** |
| `juce_smoke_test` | Lulus (JUCE 8.0.15; Gain 1.0 × 0.5 = 0.5) |

### Rincian `engine_check` (7 kelompok)

1. **Urutan chain (urutan pemanggilan):** 1,2,3 → `move(0,2)` jadi 2,3,1 →
   `remove(1)` jadi 2,1 → `insert(0,9)` jadi 9,2,1. Lolos.
2. **Urutan chain (matematika audio):** DC offset +0,5 lalu Gain ×2 pada
   input 1,0 = **3,0**; dibalik (Gain lalu DC) = **2,5**. Urutan benar-benar
   mengubah hasil (tidak komutatif). Lolos.
3. **Bypass:** Gain 0 yang di-bypass membiarkan sinyal 1,0; yang aktif
   menghasilkan 0,0. Lolos.
4. **Smoothing:** ramp 20 ms @ 48 kHz = 960 sampel. Sampel pertama
   1/960, tengah 0,5, akhir tepat 1,0, monoton naik, tanpa overshoot. Lolos.
5. **Atomic lock-free:** `std::atomic<float>` dan `std::atomic<bool>`
   (bypass) keduanya lock-free. Lolos.
6. **Mock device:** 0,5 detik berjalan → **94 blok, 0 overrun, waktu
   callback maks 42,8 µs** (durasi blok 5.333 µs). RMS output steady-state
   **0,1768** — tepat sama dengan harapan 0,25/√2 (sine 0,5 × gain 0,5).
   Lolos.
7. **Thread-safety:** control thread mengubah gain dan bypass 300× saat
   mock berjalan — tanpa crash/deadlock; nilai akhir konvergen ke 0,5
   setelah ramp. Perubahan struktur saat device berhenti (tambah DC offset)
   bekerja: 1,0×0,5+0,25 = 0,75. Lolos.

### Satu kegagalan awal (bukan bug engine)

Saat run pertama, 26/27 checks lolos. Yang gagal: tengah ramp 0,500003 vs
0,5 dengan toleransi 1e-6 — galat akumulasi float32 setelah 480 penjumlahan
step (bukan kesalahan engine; nilai akhir ramp tetap tepat 1,0 karena
di-clamp di akhir ramp). Diperbaiki dengan melonggarkan toleransi tengah
ramp ke **1e-4** (masih ketat: 0,02%), dengan alasan terdokumentasi di
komentar kode. Re-run: **27/27 lolos, ctest 3/3 lolos**.

### Yang belum dapat dilakukan

- Uji dengar / latensi round-trip di perangkat fisik — tidak ada perangkat
  audio di VM (`/dev/snd` kosong), tidak ada HP terhubung. **Tidak ada
  klaim latensi <10 ms.**
- Uji Android di perangkat — hanya cross-compile (lihat D).
- CI Windows — bagian B4.
- Offline render test formal + unit test dengan framework — bagian B4.

## F. Risiko dan kendala

1. **Paket sistem hilang saat VM di-recycle.** Paket `-dev` yang diinstal
   B2 sudah hilang saat configure B3; instal ulang berhasil dan perintahnya
   didokumentasikan di `docs/dependencies.md` §1a. Ini akan terulang setiap
   kali VM di-recycle sebelum build JUCE pertama — mitigasi jangka panjang
   (ekstrak header `.deb` ke home dan arahkan CMake ke sana) belum
   dievaluasi.
2. **Klaim "callback bebas alokasi" adalah desain + inspeksi kode, bukan
   pengukuran runtime.** Alokasi hanya terjadi di `prepare()`/`start()`
   (`AudioBuffer::resize`, `recorded_.assign`, vector prosesor); `process()`
   hanya iterasi + perkalian; tidak ada mutex di jalur audio (hanya atomic).
   Belum ada allocation counter runtime. Bila diperlukan, B4 dapat
   menambah uji beban (mock berjalan lama + pantau RSS).
3. **Perubahan struktur chain TIDAK real-time safe.** `add`/`remove`/`move`
   hanya saat audio thread berhenti — didokumentasikan di
   `effect_chain.h`. Bila Phase 2 butuh reorder pedal saat audio berjalan,
   perlu mekanisme penukaran atomik (mis. double-buffered chain) — belum
   diimplementasikan.
4. **Waktu smoothing hanya diubah saat berhenti** (bukan parameter
   real-time) — keputusan desain, didokumentasikan di
   `smoothed_parameter.h`.
5. **Mock device bukan pengganti driver audio nyata.** Timing-nya memakai
   `sleep_until` dan bergantung pada penjadwal OS; tidak ada jaminan
   latensi. Overrun dihitung, tetapi hasil 0 overrun di VM bukan bukti
   latensi perangkat nyata.
6. **Belum ada efek gitar.** Hanya `GainProcessor` referensi; efek
   sesungguhnya adalah Phase 2.
7. **Repo masih lokal, tanpa remote GitHub; CI Windows belum pernah jalan.**
   Keputusan visibilitas repo (publik/privat) ditanyakan ke user 22 Nov
   pagi dan belum dijawab — dibutuhkan sebelum run CI pertama di B4.

## G. Keputusan

- **B4 layak dimulai, dengan persetujuan user.**
- Syarat yang belum terpenuhi:
  1. Persetujuan eksplisit user untuk B4.
  2. Keputusan visibilitas repo GitHub (publik = gratis; privat = menit
     gratis terbatas) sebelum CI Windows pertama dijalankan di B4.
  3. Framework unit test formal (kandidat Catch2, BSL-1.0) diputuskan di B4
     saat unit test EffectChain ditulis.

**BERHENTI — menunggu persetujuan user sebelum B4. Jangan mulai Phase 2.**
