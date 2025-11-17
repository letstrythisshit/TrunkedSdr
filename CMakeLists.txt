cmake_minimum_required(VERSION 3.16)
project(TrunkSDR VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

# ARM-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(STATUS "Detected ARM processor: ${CMAKE_SYSTEM_PROCESSOR}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=native")

    # Enable NEON if available
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mfpu=neon" HAS_NEON)
    if(HAS_NEON)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon")
        message(STATUS "ARM NEON optimizations enabled")
    endif()
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# RTL-SDR
pkg_check_modules(RTLSDR REQUIRED librtlsdr)

# PulseAudio
pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)

# JsonCpp
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Boost (optional but recommended)
find_package(Boost COMPONENTS system filesystem thread)

# mbelib (optional - for IMBE/AMBE decoding)
find_library(MBELIB mbe)
if(MBELIB)
    message(STATUS "Found mbelib: ${MBELIB}")
    add_definitions(-DHAVE_MBELIB)
else()
    message(WARNING "mbelib not found - digital voice codecs will use stubs")
endif()

# European protocol options
option(ENABLE_EUROPEAN_PROTOCOLS "Enable European digital radio protocols (TETRA, DMR, NXDN, dPMR)" ON)
option(ENABLE_TETRA "Enable TETRA decoder" ON)
option(ENABLE_TETRA_DECRYPTION "Enable TETRA TEA1 decryption (CVE-2022-24402) - Educational/Authorized Use Only" OFF)
option(ENABLE_DMR_TIER3 "Enable DMR Tier III decoder" ON)
option(ENABLE_NXDN "Enable NXDN decoder" ON)
option(ENABLE_DPMR "Enable dPMR decoder" ON)

# Optional European-specific dependencies
if(ENABLE_TETRA)
    pkg_check_modules(FFTW3 fftw3f)
    if(FFTW3_FOUND)
        message(STATUS "Found FFTW3: ${FFTW3_LIBRARIES}")
        add_definitions(-DHAVE_FFTW3)
    else()
        message(WARNING "FFTW3 not found - TETRA will use basic DSP")
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${RTLSDR_INCLUDE_DIRS}
    ${PULSEAUDIO_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp

    # SDR
    src/sdr/rtlsdr_source.cpp

    # DSP
    src/dsp/fsk_demod.cpp
    src/dsp/c4fm_demod.cpp

    # Decoders
    src/decoders/p25_decoder.cpp
    src/decoders/smartnet_decoder.cpp

    # Codecs
    src/codecs/imbe_codec.cpp

    # Audio
    src/audio/audio_output.cpp
    src/audio/call_manager.cpp

    # Trunking
    src/trunking/trunk_controller.cpp

    # Utils
    src/utils/config_parser.cpp
)

# European protocol sources
if(ENABLE_EUROPEAN_PROTOCOLS)
    list(APPEND SOURCES
        # Enhanced DSP for European protocols
        src/dsp/fsk4_demod.cpp
    )

    if(ENABLE_TETRA)
        list(APPEND SOURCES
            src/dsp/dqpsk_demod.cpp
            src/european/tetra/tetra_phy.cpp
            src/european/tetra/tetra_decoder.cpp
        )
        add_definitions(-DENABLE_TETRA)
        message(STATUS "TETRA decoder enabled")

        if(ENABLE_TETRA_DECRYPTION)
            list(APPEND SOURCES
                src/european/tetra/tetra_crypto.cpp
            )
            add_definitions(-DENABLE_TETRA_DECRYPTION)
            message(STATUS "⚠️  TETRA DECRYPTION ENABLED - Educational/Authorized Use Only")
            message(STATUS "⚠️  WARNING: Unauthorized interception is ILLEGAL")
            message(STATUS "⚠️  Users are SOLELY RESPONSIBLE for legal compliance")
        endif()
    endif()

    if(ENABLE_DMR_TIER3)
        list(APPEND SOURCES
            src/european/dmr/dmr_decoder.cpp
        )
        add_definitions(-DENABLE_DMR_TIER3)
        message(STATUS "DMR Tier III decoder enabled")
    endif()

    if(ENABLE_NXDN)
        add_definitions(-DENABLE_NXDN)
        message(STATUS "NXDN decoder enabled (framework)")
    endif()

    if(ENABLE_DPMR)
        add_definitions(-DENABLE_DPMR)
        message(STATUS "dPMR decoder enabled (framework)")
    endif()
endif()

# Executable
add_executable(trunksdr ${SOURCES})

# Link libraries
target_link_libraries(trunksdr
    ${RTLSDR_LIBRARIES}
    ${PULSEAUDIO_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

if(MBELIB)
    target_link_libraries(trunksdr ${MBELIB})
endif()

if(Boost_FOUND)
    target_link_libraries(trunksdr ${Boost_LIBRARIES})
endif()

if(ENABLE_TETRA AND FFTW3_FOUND)
    target_link_libraries(trunksdr ${FFTW3_LIBRARIES})
    target_include_directories(trunksdr PRIVATE ${FFTW3_INCLUDE_DIRS})
endif()

# TETRA Decryption Interceptor Tool (optional standalone tool)
if(ENABLE_TETRA_DECRYPTION)
    add_executable(tetra_decrypt_interceptor
        src/tools/tetra_decrypt_interceptor.cpp
        src/european/tetra/tetra_crypto.cpp
        src/european/tetra/tetra_phy.cpp
        src/european/tetra/tetra_decoder.cpp
        src/dsp/dqpsk_demod.cpp
    )

    target_link_libraries(tetra_decrypt_interceptor
        Threads::Threads
    )

    if(FFTW3_FOUND)
        target_link_libraries(tetra_decrypt_interceptor ${FFTW3_LIBRARIES})
        target_include_directories(tetra_decrypt_interceptor PRIVATE ${FFTW3_INCLUDE_DIRS})
    endif()

    install(TARGETS tetra_decrypt_interceptor DESTINATION bin)
    message(STATUS "tetra_decrypt_interceptor tool will be built")
endif()

# Installation
install(TARGETS trunksdr DESTINATION bin)
install(FILES config/config.example.json DESTINATION share/trunksdr)

if(ENABLE_EUROPEAN_PROTOCOLS)
    install(DIRECTORY config/european_systems/ DESTINATION share/trunksdr/european_systems)
    install(DIRECTORY config/frequency_databases/ DESTINATION share/trunksdr/frequency_databases)
    install(DIRECTORY docs/european/ DESTINATION share/doc/trunksdr/european)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "TrunkSDR Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  RTL-SDR: ${RTLSDR_LIBRARIES}")
message(STATUS "  PulseAudio: ${PULSEAUDIO_LIBRARIES}")
message(STATUS "  JsonCpp: ${JSONCPP_LIBRARIES}")
if(MBELIB)
    message(STATUS "  mbelib: ${MBELIB}")
else()
    message(STATUS "  mbelib: NOT FOUND (digital voice will use stubs)")
endif()
if(ENABLE_EUROPEAN_PROTOCOLS)
    message(STATUS "  European Protocols: ENABLED")
    if(ENABLE_TETRA)
        message(STATUS "    TETRA: ENABLED")
        if(ENABLE_TETRA_DECRYPTION)
            message(STATUS "    TETRA Decryption: ⚠️  ENABLED (Educational/Authorized Only)")
        else()
            message(STATUS "    TETRA Decryption: DISABLED (monitoring only)")
        endif()
    endif()
    if(ENABLE_DMR_TIER3)
        message(STATUS "    DMR Tier III: ENABLED")
    endif()
    if(ENABLE_NXDN)
        message(STATUS "    NXDN: ENABLED (framework)")
    endif()
    if(ENABLE_DPMR)
        message(STATUS "    dPMR: ENABLED (framework)")
    endif()
endif()
message(STATUS "")
