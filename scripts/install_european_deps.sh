#!/bin/bash

# TrunkSDR European Protocols - Dependency Installation Script
# For ARM Debian (Raspberry Pi OS, Debian, Ubuntu on ARM)

set -e  # Exit on error

echo "=========================================="
echo "TrunkSDR European Protocols Dependencies"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Please do not run as root. Use sudo when prompted."
    exit 1
fi

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    OS_VERSION=$VERSION_ID
else
    echo "Cannot detect OS. This script is for Debian-based systems."
    exit 1
fi

echo "Detected OS: $OS $OS_VERSION"
echo ""

# Update package lists
echo "Updating package lists..."
sudo apt-get update

echo ""
echo "Installing base dependencies (from main TrunkSDR)..."
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    librtlsdr-dev \
    libusb-1.0-0-dev \
    libpulse-dev \
    libjsoncpp-dev \
    libboost-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev

echo ""
echo "Installing European protocol dependencies..."

# FFTW3 for TETRA DSP
echo "  - Installing FFTW3 (for TETRA π/4-DQPSK demodulation)..."
sudo apt-get install -y libfftw3-dev

# mbelib for AMBE+2 codec (DMR, NXDN)
echo "  - Installing mbelib (for DMR/NXDN AMBE+2 voice codec)..."
if ! dpkg -l | grep -q libmbe-dev; then
    echo "    mbelib not in repos, building from source..."
    cd /tmp
    if [ -d mbelib ]; then
        rm -rf mbelib
    fi
    git clone https://github.com/szechyjs/mbelib.git
    cd mbelib
    mkdir -p build && cd build
    cmake ..
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    cd ~
    echo "    mbelib installed successfully"
else
    sudo apt-get install -y libmbe-dev
fi

# Optional: Osmocom libraries for TETRA (if available)
echo "  - Checking for Osmocom TETRA libraries (optional)..."
if sudo apt-cache search libosmocore-dev | grep -q libosmocore-dev; then
    echo "    Installing Osmocom libraries..."
    sudo apt-get install -y \
        libosmocore-dev \
        libosmocoding-dev \
        libosmo-dsp-dev || echo "    Some Osmocom packages not available, continuing..."
else
    echo "    Osmocom libraries not in repos (optional, skipping)"
fi

# IT++ for advanced DSP (optional)
echo "  - Installing IT++ (optional, for advanced DSP)..."
sudo apt-get install -y libitpp-dev || echo "    IT++ not available, skipping"

# VOLK for SIMD optimization (optional)
echo "  - Installing VOLK (optional, for SIMD optimization)..."
sudo apt-get install -y libvolk2-dev || echo "    VOLK not available, skipping"

echo ""
echo "=========================================="
echo "Installation Summary"
echo "=========================================="
echo ""
echo "Required dependencies:"
echo "  ✓ Build tools (cmake, gcc, etc.)"
echo "  ✓ RTL-SDR libraries"
echo "  ✓ PulseAudio libraries"
echo "  ✓ JsonCpp"
echo "  ✓ Boost"
echo "  ✓ FFTW3 (for TETRA)"
echo ""

# Check mbelib
if ldconfig -p | grep -q libmbe; then
    echo "  ✓ mbelib (for AMBE+2 codec)"
else
    echo "  ✗ mbelib NOT FOUND - DMR/NXDN voice will not work"
fi

# Check optional
if dpkg -l | grep -q libosmocore-dev; then
    echo "  ✓ Osmocom libraries (optional)"
else
    echo "  ○ Osmocom libraries not installed (optional)"
fi

if dpkg -l | grep -q libitpp-dev; then
    echo "  ✓ IT++ (optional)"
else
    echo "  ○ IT++ not installed (optional)"
fi

echo ""
echo "=========================================="
echo "Next Steps"
echo "=========================================="
echo ""
echo "1. Build TrunkSDR with European protocols:"
echo "   cd /path/to/TrunkedSdr"
echo "   mkdir build && cd build"
echo ""
echo "   # Standard build (monitoring only):"
echo "   cmake .. -DENABLE_EUROPEAN_PROTOCOLS=ON"
echo "   make -j\$(nproc)"
echo ""
echo "   # WITH TETRA decryption (educational/authorized use ONLY):"
echo "   # ⚠️  WARNING: Unauthorized use is ILLEGAL"
echo "   cmake .. -DENABLE_EUROPEAN_PROTOCOLS=ON -DENABLE_TETRA_DECRYPTION=ON"
echo "   make -j\$(nproc)"
echo ""
echo "2. Configure your system:"
echo "   - For TETRA: See docs/european/TETRA_GUIDE.md"
echo "   - For DMR: See docs/european/DMR_EUROPE.md"
echo "   - Frequency databases: config/frequency_databases/european_bands.json"
echo "   - System configs: config/european_systems/"
echo ""
echo "3. TETRA Decryption (optional - educational/authorized use ONLY):"
echo "   - See docs/european/TETRA_REALTIME_DECRYPTION.md for setup"
echo "   - See docs/european/TETRA_DECRYPTION_EDUCATIONAL.md for legal info"
echo "   - ⚠️  Requires explicit legal authorization"
echo "   - ⚠️  Unauthorized use punishable by law"
echo ""
echo "4. Legal considerations:"
echo "   - READ docs/european/LEGAL_CONSIDERATIONS_EU.md"
echo "   - Know your local laws!"
echo "   - NEVER intercept without authorization"
echo ""
echo "Installation complete!"
echo ""
