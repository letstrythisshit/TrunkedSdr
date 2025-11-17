# Building TrunkSDR

Complete guide for building TrunkSDR on ARM-based Debian systems.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Dependency Installation](#dependency-installation)
- [Building from Source](#building-from-source)
- [Cross-Compilation](#cross-compilation)
- [Installation](#installation)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Hardware

- ARM-based system (Raspberry Pi 3/4, Orange Pi, etc.)
- At least 1GB RAM (4GB recommended)
- 4GB free disk space
- RTL-SDR USB dongle

### Software

- Debian-based Linux distribution (Raspberry Pi OS, Ubuntu, Debian)
- Internet connection for downloading dependencies

## Dependency Installation

### Automated Installation (Recommended)

Run the provided installation script:

```bash
sudo ./scripts/install_deps.sh
```

This script will:
1. Update package lists
2. Install build tools (gcc, cmake, etc.)
3. Install RTL-SDR libraries
4. Install audio libraries (PulseAudio)
5. Install mbelib for digital voice decoding
6. Configure udev rules for RTL-SDR
7. Blacklist DVB-T kernel modules

### Manual Installation

If you prefer manual installation or the script fails:

```bash
# Update package database
sudo apt-get update

# Install build essentials
sudo apt-get install -y build-essential cmake git pkg-config

# Install RTL-SDR
sudo apt-get install -y librtlsdr-dev libusb-1.0-0-dev rtl-sdr

# Install audio libraries
sudo apt-get install -y libpulse-dev pulseaudio

# Install other dependencies
sudo apt-get install -y libjsoncpp-dev libboost-all-dev

# Build and install mbelib
cd /tmp
git clone https://github.com/szechyjs/mbelib.git
cd mbelib
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### Verify Dependencies

```bash
# Check RTL-SDR
rtl_test -t

# Check libraries
pkg-config --modversion librtlsdr
pkg-config --modversion libpulse-simple
pkg-config --modversion jsoncpp

# Check mbelib
ldconfig -p | grep mbe
```

## Building from Source

### Quick Build

Use the provided build script:

```bash
./scripts/build_arm.sh
```

### Manual Build

```bash
# Create build directory
mkdir -p build
cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (use all CPU cores)
make -j$(nproc)

# Optionally run tests
# make test

# Binary will be in build/trunksdr
```

### Build Output

After successful build:
```
build/
├── trunksdr          # Main executable
├── CMakeCache.txt
└── ...
```

## Build Configuration

### Build Types

**Release (Default)** - Optimized for performance:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

**Debug** - For development and debugging:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

**RelWithDebInfo** - Release with debug symbols:
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### ARM Optimizations

The build system automatically detects ARM processors and applies optimizations:

- Native CPU tuning (`-mcpu=native`)
- NEON SIMD instructions (if available)
- Architecture-specific flags

To override:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -mcpu=cortex-a72"
```

### Disable Optimizations

For maximum compatibility (slower):
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O2"
```

## Installation

### System-wide Installation

```bash
cd build
sudo make install
```

Default installation paths:
- Binary: `/usr/local/bin/trunksdr`
- Config example: `/usr/local/share/trunksdr/config.example.json`

### Custom Installation Prefix

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/trunksdr
sudo make install
```

### Running Without Installation

```bash
# From build directory
./trunksdr --config ../config/config.example.json

# Or from project root
./build/trunksdr --config config/config.example.json
```

## Cross-Compilation

For building on x86_64 for ARM targets:

### Set up Cross-Compilation Toolchain

```bash
# On Ubuntu/Debian
sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# For 64-bit ARM
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### Create Toolchain File

Create `arm-toolchain.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# For 32-bit ARM
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Or for 64-bit ARM (aarch64)
# set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
# set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

### Build

```bash
mkdir build-arm
cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake
make -j$(nproc)
```

### Transfer to ARM Device

```bash
scp trunksdr pi@raspberrypi:~/
scp ../config/config.example.json pi@raspberrypi:~/config.json
```

## Build Options

### With Debug Symbols

```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Verbose Build Output

```bash
make VERBOSE=1
```

### Clean Build

```bash
rm -rf build
mkdir build
cd build
cmake ..
make
```

## Platform-Specific Notes

### Raspberry Pi

**Raspberry Pi 3:**
- Use swap if only 1GB RAM available
- Compilation may take 20-30 minutes
- Consider cross-compiling on faster machine

**Raspberry Pi 4:**
- Optimal performance with 4GB+ RAM
- Native compilation takes 5-10 minutes
- Can compile with `-j4` safely

**Raspberry Pi Zero/Zero W:**
- Not recommended due to limited CPU
- If attempted, use `-j1` and enable swap

### Orange Pi / Rock Pi

Similar to Raspberry Pi 4, adjust `-j` flag based on CPU cores.

## Troubleshooting

### CMake Can't Find RTL-SDR

```bash
# Ensure pkg-config can find it
pkg-config --cflags --libs librtlsdr

# If not found, install librtlsdr-dev
sudo apt-get install librtlsdr-dev
```

### Compiler Out of Memory

Reduce parallel jobs:
```bash
make -j1
```

Or increase swap:
```bash
sudo dd if=/dev/zero of=/swapfile bs=1M count=2048
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### mbelib Not Found

The application will still build but digital voice codecs won't work.

To fix:
```bash
cd /tmp
git clone https://github.com/szechyjs/mbelib.git
cd mbelib
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```

Then rebuild TrunkSDR.

### Link Errors

```bash
# Update library cache
sudo ldconfig

# Check library paths
echo $LD_LIBRARY_PATH
```

### ARM NEON Not Detected

Check CPU features:
```bash
cat /proc/cpuinfo | grep -i neon
```

If NEON is available but not detected, manually enable:
```bash
cmake .. -DCMAKE_CXX_FLAGS="-mfpu=neon"
```

## Performance Testing

After building, test performance:

```bash
# CPU usage monitoring
top -p $(pgrep trunksdr)

# Detailed profiling
perf record -g ./trunksdr --config config.json
perf report
```

## Next Steps

After successful build:

1. [Configure your system](CONFIGURATION.md)
2. Test with RTL-SDR: `rtl_test -t`
3. Run TrunkSDR: `./trunksdr --config config.json`
4. Check [Troubleshooting](TROUBLESHOOTING.md) if issues arise

## Additional Resources

- CMake documentation: https://cmake.org/documentation/
- ARM GCC optimization: https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
- RTL-SDR Wiki: https://osmocom.org/projects/rtl-sdr/wiki
