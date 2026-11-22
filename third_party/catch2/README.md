# Catch2 — vendored (test-only)

Framework unit test untuk `tests/` (diputuskan di B4).

- **Versi:** 3.7.1 (tag `v3.7.1`, commit
  `fa43b77429ba76c462b1898d6cd2f2d7a9416b14`)
- **Sumber:** file amalgamated resmi dari rilis GitHub
  `catchorg/Catch2` —
  `https://github.com/catchorg/Catch2/releases/download/v3.7.1/catch_amalgamated.hpp`
  dan `.../catch_amalgamated.cpp`
- **Lisensi:** Boost Software License 1.0 (BSL-1.0) — permisif; header
  lisensi ada di kedua file. Dependency test-only: tidak di-link ke
  binary aplikasi, tidak didistribusikan bersama produk.
- **SHA-256 (terverifikasi saat pengunduhan 2026-11-22):**
  - `catch_amalgamated.hpp` (524.559 byte):
    `91d47e2ddfb2949df22ff582d6c292554184da743e11d3811dcdd06b5b0a159b`
  - `catch_amalgamated.cpp` (422.559 byte):
    `450fc5f213a87668f0f48c91a2afa4f4226186fef453d36e6fa3bc184ccdb9ba`

Catatan pemilihan versi: tag terbaru saat itu adalah v3.15.1, tetapi rilis
GitHub-nya tidak menyertakan file amalgamated (404) — v3.7.1 adalah versi
terbaru yang file amalgamated-nya tersedia sebagai artefak rilis resmi.
Bila suatu saat perlu naik versi, unduh pasangan file dari tag yang sama
dan perbarui SHA-256 di sini serta di `docs/dependencies.md`.
