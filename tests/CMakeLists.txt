# tests — pengujian host untuk shared core.
#
# Framework unit test formal (diputuskan di B4): Catch2 v3.7.1, file
# amalgamated yang di-vendor di ../third_party/catch2/ (BSL-1.0, test-only,
# tidak di-link ke binary aplikasi). Lihat third_party/catch2/README.md
# untuk sumber, versi, dan SHA-256.
#
# engine_check adalah program verifikasi B3 gaya assert sederhana (bukan
# suite unit test formal) dan tetap dipertahankan sebagai lapisan
# verifikasi terpisah.

# Catch2 amalgamated sebagai static lib (main() disediakan Catch2).
add_library(catch2_amalgamated STATIC
    ../third_party/catch2/catch_amalgamated.cpp
)

target_include_directories(catch2_amalgamated PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/catch2
)

# Catch2 memakai __android_log_write (liblog) untuk debug output di Android;
# tanpa ini, link effect_chain_tests untuk arm64-v8a gagal (B4).
if(ANDROID)
    target_link_libraries(catch2_amalgamated PUBLIC log)
endif()

add_executable(muse_dsp_smoke_test
    muse_dsp_smoke_test.cpp
)

target_link_libraries(muse_dsp_smoke_test PRIVATE muse_dsp)

add_test(NAME muse_dsp_smoke_test COMMAND muse_dsp_smoke_test)

# Verifikasi fondasi audio engine (B3): urutan chain, bypass, smoothing,
# atomic lock-free, mock device, dan thread-safety.
add_executable(engine_check
    engine_check.cpp
)

target_link_libraries(engine_check PRIVATE muse_dsp)

add_test(NAME engine_check COMMAND engine_check)

# Unit test formal EffectChain (B4, Catch2): urutan, bypass, perubahan
# parameter, dan kesamaan output di semua ukuran blok relevan.
add_executable(effect_chain_tests
    effect_chain_tests.cpp
)

target_link_libraries(effect_chain_tests PRIVATE muse_dsp catch2_amalgamated)

add_test(NAME effect_chain_tests COMMAND effect_chain_tests)

# Unit test formal Dynamics (Phase 2.1, Catch2): Noise Gate +
# Compressor — atenuasi, gain reduction, attack/release, soft knee,
# pumping, ukuran blok, perubahan parameter, ekstrem, reset,
# thread-safety dengan MockAudioDevice.
add_executable(dynamics_tests
    dynamics_tests.cpp
)

target_link_libraries(dynamics_tests PRIVATE muse_dsp catch2_amalgamated)

add_test(NAME dynamics_tests COMMAND dynamics_tests)

# Offline rendering test (B4): render 1 detik sine lewat EffectChain tanpa
# thread real-time, tulis WAV 16-bit PCM, verifikasi RMS/peak + header WAV.
add_executable(offline_render
    offline_render.cpp
)

target_link_libraries(offline_render PRIVATE muse_dsp)

add_test(NAME offline_render_256
    COMMAND offline_render ${CMAKE_CURRENT_BINARY_DIR}/offline-render-256.wav 256)
add_test(NAME offline_render_1024
    COMMAND offline_render ${CMAKE_CURRENT_BINARY_DIR}/offline-render-1024.wav 1024)
