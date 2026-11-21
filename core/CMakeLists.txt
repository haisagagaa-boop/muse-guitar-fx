# muse_dsp — shared DSP core (JUCE-independent, C++17 murni).
#
# Prinsip: core TIDAK bergantung pada JUCE, UI, atau platform apa pun, sehingga
# satu kode dipakai host (Linux/Windows) dan Android (NDK). JUCE dipakai oleh
# app shell (lihat apps/) sebagai framework audio/UI, bukan oleh core.
# B2 hanya berisi fondasi: API versi. EffectChain dan efek (B3, Phase 2+)
# belum ada di sini — jangan diklaim sudah ada.

add_library(muse_dsp STATIC
    src/muse_dsp.cpp
)

target_include_directories(muse_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Peringatan sebagai error untuk kode core baru (menjaga kualitas sejak awal).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(muse_dsp PRIVATE -Wall -Wextra)
endif()

set_target_properties(muse_dsp PROPERTIES
    OUTPUT_NAME "muse_dsp"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
