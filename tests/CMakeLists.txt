# tests — smoke test host untuk shared core (tanpa framework test).
# Framework unit test (kandidat: Catch2, BSL-1.0) diputuskan di B3 saat
# unit test EffectChain ditulis. Smoke test ini hanya membuktikan core
# ter-build, ter-link, dan API versinya bekerja.

add_executable(muse_dsp_smoke_test
    muse_dsp_smoke_test.cpp
)

target_link_libraries(muse_dsp_smoke_test PRIVATE muse_dsp)

add_test(NAME muse_dsp_smoke_test COMMAND muse_dsp_smoke_test)
