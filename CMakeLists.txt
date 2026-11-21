# MUSE Guitar FX — root build
#
# Shared C++ DSP core (muse_dsp) + JUCE 8.0.15 (git submodule, tag 8.0.15)
# + host smoke tests. Android app shell: Phase 7. Windows app shell: Phase 8.
#
# Build host (Release, maks. -j 2 karena RAM VM terbatas):
#   cmake --preset host-release
#   cmake --build --preset host-release --parallel 2
#   ctest --preset host-release
#
# Android (hanya cross-compile shared core, tanpa JUCE app):
#   ANDROID_NDK_HOME=$HOME/workspace/android-sdk/ndk/28.2.13676358 \
#   cmake --preset android-arm64-v8a
#   cmake --build --preset android-arm64-v8a --parallel 2

cmake_minimum_required(VERSION 3.22)

project(MuseGuitarFX
    VERSION 0.1.0
    DESCRIPTION "Muse Guitar FX — multi-effects processor (shared DSP core + JUCE)"
    LANGUAGES C CXX)

# JUCE 8 membutuhkan C++17 (C++20 hanya bila Windows MIDI Services aktif).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opsi build.
option(MUSE_BUILD_TESTS "Build host smoke tests (core, tanpa framework)" ON)
option(MUSE_BUILD_JUCE_APPS "Build JUCE app targets (host-smoke)" ON)

# Release di VM ini memakai -O2, bukan default CMake -O3: menghemat puncak
# pemakaian RAM saat mengompilasi JUCE (RAM VM ~7,7 GB, tersedia < 1 GB).
# Tetap konfigurasi Release (optimasi aktif + NDEBUG).
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)
endif()

# JUCE 8.0.15 sebagai git submodule (pin: tag 8.0.15, commit tercatat di gitlink).
# Hanya ditambahkan bila app JUCE dibangun; shared core TIDAK bergantung pada JUCE.
if(MUSE_BUILD_JUCE_APPS)
    add_subdirectory(third_party/JUCE)
endif()

add_subdirectory(core)

if(MUSE_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

if(MUSE_BUILD_JUCE_APPS)
    add_subdirectory(apps)
endif()
