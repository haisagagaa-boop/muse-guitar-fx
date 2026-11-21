# MUSE GUITAR FX — Audio Engine Design

> Dokumen Phase 0 — 21 November 2026.
> Desain ini berbasis JUCE (`juce_audio_devices`, `juce_dsp`, `juce_audio_basics`)
> dan hasil audit VM 21 November 2026. Belum ada kode yang ditulis.

## 1. Tujuan desain

Audio engine harus memenuhi semua ini, dalam urutan prioritas:

1. **Tidak pernah dropout / crackling** pada target yang didukung.
2. **Tidak pernah memblokir** callback audio (tidak ada lock, alokasi, I/O file,
   atau panggilan sistem yang bisa tidur di dalam callback).
3. **Latensi round-trip serendah yang diizinkan perangkat** (target uji:
   buffer 64/128/256/512 pada 44.1/48 kHz).
4. **Parameter berubah mulus** (tidak ada klik/zipper noise saat knob diputar
   atau preset diganti).
5. **Aman saat device dicabut / berganti** (reconnect tanpa crash).

## 2. Arsitektur callback

Satu callback audio, satu arah sinyal, tanpa percabangan tersembunyi:

```
AudioDeviceManager (JUCE)
  └─ audioDeviceIOCallback (real-time thread)
       ├─ 1. Ambil input buffer (mono/stereo dari interface)
       ├─ 2. Input gain + meter input (peak/RMS → atomik)
       ├─ 3. Tuner tap (copy ringan, pre-chain, tanpa efek samping)
       ├─ 4. EffectChain::process (semua node, sesuai urutan)
       ├─ 5. Master gain + limiter + meter output + clip flag
       ├─ 6. Campur metronome click (bila aktif)
       └─ 7. Tulis output buffer
```

**Kontrak callback (keras, tanpa pengecualian):**

- Tidak ada `new`/`malloc`, tidak ada `std::vector::push_back`, tidak ada
  `std::string`, tidak ada lock/mutex, tidak ada I/O file, tidak ada
  panggilan UI, tidak ada `printf`/logging.
- Semua buffer dan state dialokasikan di `prepareToPlay()` / konstruktor.
- Semua parameter dibaca dari atomik atau FIFO lock-free (lihat §5).

**Pemisahan dari UI:** audio engine tidak bergantung pada proses rendering UI.
UI berjalan di message thread JUCE; audio di real-time thread. Keduanya hanya
berkomunikasi lewat atomik dan FIFO.

## 3. Device management

| Fungsi | Perilaku |
|---|---|
| Enumerate | Daftar input/output dari `AudioDeviceManager::getAvailableDeviceTypes()`; nama device ditampilkan apa adanya |
| Open | Minta sample rate + buffer yang diminta user; **selalu baca kembali nilai aktual** dari device (device boleh menolak dan memberi nilai lain) |
| Sample rate | Target 44.1 kHz dan 48 kHz; engine bekerja pada rate aktual device, DSP di-`prepare()` ulang bila rate berubah |
| Buffer size | Target 64/128/256/512 bila tersedia; tampilkan ukuran aktual; bila device memberi ukuran lain, terima dan laporkan — jangan memaksakan |
| Channel | Input mono (gitar) atau stereo; output stereo; konversi mono→stereo di input stage |
| Reconnect | `audioDeviceError()` / device hilang → stop callback dengan aman, tampilkan status "device terputus", tawarkan reconnect; tidak pernah crash; tidak pernah auto-reconnect diam-diam ke device yang salah |
| Bypass | Bypass global (kering) dan bypass per-node; bypass tidak menghentikan callback — sinyal tetap mengalir |

**Windows:** WASAPI adalah default (selalu tersedia). ASIO hanya diaktifkan
bila ASIO SDK terdeteksi saat build (lihat `platform-compatibility.md` §3).
**Android:** Oboe (terbundel di JUCE) di atas AAudio; bila AAudio tidak
didukung perangkat, Oboe jatuh ke OpenSL ES secara otomatis.

## 4. Buffer, sample rate & latensi

- **Ukuran blok internal DSP = ukuran blok callback.** Tidak ada buffering
  tambahan di dalam chain (menambah latensi). Satu-satunya pengecualian:
  oversampling pada drive/amp (internal, 2x–4x, dikelola di dalam node).
- **Latensi yang dilaporkan** = input latency + output latency dari device
  (`getInputLatencyInSamples()` / `getOutputLatencyInSamples()`) + ukuran
  buffer, ditampilkan dalam milidetik dan sampel. Angka ini diukur, bukan
  ditebak.
- **Perubahan sample rate / buffer saat berjalan:** hentikan stream, panggil
  `prepare()` ulang pada seluruh chain, mulai lagi. Jangan mengubah rate
  di tengah blok.
- **Denormal protection:** `juce::ScopedNoDenormals` di awal callback; node
  dengan state rekursif (reverb, filter) menambahkan flush-to-zero pada
  state-nya. Denormal adalah penyebab klasik CPU spike yang terlihat seperti
  "dropout misterius" pada tail reverb/delay.

## 5. Parameter smoothing & thread safety

- Setiap parameter yang dapat berubah saat bermain melewati
  `juce::SmoothedValue<float>` (atau `LinearSmoothedValue`) dengan waktu
  smoothing ~20–50 ms (dapat diatur per parameter; parameter gain lebih cepat,
  parameter filter lebih lambat).
- **Jalur perubahan parameter:** UI → `setParameter()` menulis ke
  `std::atomic<float>` target → callback membaca atomik dan memberi nilai
  target ke smoother → smoother menghasilkan nilai per-sampel/blok.
  UI tidak pernah menulis langsung ke state DSP.
- **Perubahan struktural** (tambah/hapus/pindah node, ganti preset, ganti IR):
  dilakukan di luar callback. Rantai memakai double-buffer: chain baru
  dirakit di thread background, lalu pointer chain aktif ditukar secara atomik
  di batas blok. Chain lama dibebaskan setelah satu blok berlalu (bukan di
  dalam callback).
- **Metering:** callback menulis peak/RMS ke `std::atomic<float>`; UI membaca
  dengan timer 15–30 Hz. Clip indicator adalah latch atomik yang di-reset
  dari UI.

## 6. Efek & node — kontrak yang ditegakkan engine

Setiap node efek wajib memenuhi kontrak ini; engine tidak memberi pengecualian:

```cpp
// Kontrak (ditulis ulang dalam C++ saat Phase 1; ini spesifikasinya)
prepare(sampleRate, blockSize);   // alokasi semua buffer & state di sini
process(AudioBuffer<float>&);     // tanpa alokasi, tanpa lock, tanpa I/O
reset();                          // bersihkan state (dipanggil saat bypass lama / preset ganti)
getParameters();                  // daftar parameter bernama + rentang + default
```

- **Oversampling drive/amp:** waveshaper dijalankan pada 2x (minimum) atau 4x
  (target) dengan filter anti-aliasing; tanpa oversampling, distorsi
  menghasilkan aliasing yang terdengar sebagai bunyi "kasar digital".
- **Delay/Reverb:** delay line dialokasikan sekali di `prepare()` dengan
  ukuran maksimum (mis. 2 detik); perubahan waktu delay tidak pernah
  merealokasi. Reverb memakai struktur algoritmik (mis. Freeverb-style)
  dengan parameter decay, mix, dan damping.
- **Convolution (cabinet IR):** `juce::dsp::Convolution` dengan latensi
  yang dilaporkan; IR dimuat di luar callback; panjang IR dibatasi
  (mis. maks. 500 ms / ~24.000 sampel @48 kHz) agar beban CPU terkendali.

## 7. Tuner & metronome

- **Tuner:** mengambil salinan input pre-chain. Deteksi pitch dengan
  autocorrelation + konfirmasi FFT (`juce::dsp::FFT`). Akurasi target ±2 sen
  pada rentang E2–E6. Tuner tidak memengaruhi sinyal output.
- **Metronome:** click dijadwalkan berdasarkan posisi sampel (sample-accurate),
  bukan timer UI. Tap tempo menghitung BPM dari ketukan pengguna. Level click
  diatur terpisah dari master.

## 8. Error handling & kondisi gagal

| Kondisi | Respons engine |
|---|---|
| Device tidak ditemukan saat start | Status error jelas; aplikasi tetap jalan (UI + preset); tidak crash |
| Device dicabut saat bermain | Callback berhenti aman; status "terputus"; tombol reconnect |
| Sample rate/buffer ditolak device | Pakai nilai aktual device; tampilkan nilai aktual (jangan tampilkan nilai yang diminta seolah-olah dipakai) |
| Buffer underrun (xrun) | Hitung dan tampilkan counter xrun; jangan sembunyikan — xrun adalah data uji |
| IR/preset rusak | Tolak dengan pesan; chain tetap pada preset terakhir yang valid |
| CPU overload (callback melebihi budget blok) | Deteksi via pengukuran waktu callback; tampilkan peringatan; saran: naikkan buffer |

## 9. Yang TIDAK dilakukan engine ini

- Tidak ada plugin VST/AU (aplikasi standalone dulu; plugin adalah produk
  terpisah dan butuh keputusan lisensi sendiri).
- Tidak ada perekaman audio (di luar scope brief; bisa ditambah setelah Phase 9).
- Tidak ada pemrosesan di thread UI.
- Tidak ada "mode demo" yang memalsukan meter atau latensi — angka yang
  ditampilkan adalah angka yang diukur.
