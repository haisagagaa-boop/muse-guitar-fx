# muse_dsp — shared DSP core (JUCE-independent, C++17 murni).
#
# Prinsip: core TIDAK bergantung pada JUCE, UI, atau platform apa pun, sehingga
# satu kode dipakai host (Linux/Windows) dan Android (NDK). JUCE dipakai oleh
# app shell (lihat apps/) sebagai framework audio/UI, bukan oleh core.
# B2 hanya berisi fondasi: API versi. EffectChain dan efek (B3, Phase 2+)
# belum ada di sini — jangan diklaim sudah ada.

add_library(muse_dsp STATIC
    src/muse_dsp.cpp
    src/audio_buffer.cpp
    src/gain_processor.cpp
    src/effect_chain.cpp
    src/mock_audio_device.cpp
)

target_include_directories(muse_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# MockAudioDevice memakai std::thread — butuh Threads (pthread di Linux).
find_package(Threads REQUIRED)
target_link_libraries(muse_dsp PUBLIC Threads::Threads)

# Satu-satunya pengecualian platform di core: MockAudioDevice menaikkan
# resolusi timer Windows (timeBeginPeriod, winmm) agar pacing blok presisi.
# Terbatas pada mock (infrastruktur pengujian), bukan DSP — lihat
# mock_audio_device.h. Di non-Windows tidak ada dependensi tambahan.
if(WIN32)
    target_link_libraries(muse_dsp PRIVATE winmm)
endif()

# Peringatan sebagai error untuk kode core baru (menjaga kualitas sejak awal).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(muse_dsp PRIVATE -Wall -Wextra)
endif()

set_target_properties(muse_dsp PROPERTIES
    OUTPUT_NAME "muse_dsp"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
