# host-smoke — console app pembuktian integrasi JUCE 8.0.15.
#
# Membuktikan: JUCE ter-link (juce_core, juce_audio_basics, juce_dsp),
# macro versi JUCE benar, dan satu prosesor juce::dsp memproses buffer
# offline dengan hasil yang benar. Bukan UI, bukan efek gitar.
#
# Catatan Linux: NEEDS_CURL tidak diaktifkan — JUCE_USE_CURL=0 sehingga
# build host tidak membutuhkan libcurl dev (lihat JUCE docs/Linux
# Dependencies.md: libcurl4-openssl-dev hanya perlu bila JUCE_USE_CURL=1).
# JUCE_WEB_BROWSER juga tidak diaktifkan (tidak ada modul GUI di sini).

juce_add_console_app(juce_smoke_test)

target_sources(juce_smoke_test PRIVATE main.cpp)

target_link_libraries(juce_smoke_test PRIVATE
    muse_dsp
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
)

# Daftarkan ke CTest supaya `ctest` menjalankan kedua smoke test.
add_test(NAME juce_smoke_test COMMAND juce_smoke_test)
