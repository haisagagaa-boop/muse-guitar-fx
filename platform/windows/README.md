# Windows — build lokal & CI (B2, Phase 1)

## Status B2

- **Windows TIDAK dapat di-build atau diuji di VM ini** (Linux; tidak ada
  MSVC, MinGW, maupun wine — sesuai temuan Phase 0). Tidak ada klaim
  "Windows berhasil diuji" di B2.
- Yang tersedia di B2: konfigurasi build Windows — workflow GitHub Actions
  (`.github/workflows/windows-build.yml`) dan perintah build lokal di bawah.
- Workflow CI **belum pernah dijalankan**: repository masih lokal (belum ada
  remote GitHub). Menjalankan CI membutuhkan keputusan user: buat repository
  GitHub, tentukan visibilitas (publik = gratis; privat = menit gratis
  terbatas), lalu push branch `main`.

## Build lokal (di mesin Windows, Visual Studio 2022)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

## Build via CI

Push branch `main` ke GitHub — workflow `Windows build` berjalan otomatis
di `windows-latest`. Hasil run pertama harus dilaporkan apa adanya (hijau
atau merah beserta log errornya) sebelum B2 dinyatakan selesai untuk sisi
Windows.
