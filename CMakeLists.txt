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

# Installation
install(TARGETS trunksdr DESTINATION bin)
install(FILES config/config.example.json DESTINATION share/trunksdr)

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
message(STATUS "")
