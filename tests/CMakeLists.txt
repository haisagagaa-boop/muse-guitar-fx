# tests — smoke test host untuk shared core (tanpa framework test).
# Framework unit test formal (kandidat: Catch2, BSL-1.0) diputuskan di B4
# saat unit test EffectChain ditulis. engine_check di sini adalah program
# verifikasi B3 gaya assert sederhana (seperti smoke test), bukan suite
# unit test formal.

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
