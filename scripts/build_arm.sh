#!/bin/bash
# TrunkSDR Build Script for ARM (Native compilation)

set -e

echo "========================================="
echo "TrunkSDR ARM Build Script"
echo "========================================="
echo ""

# Detect processor
ARCH=$(uname -m)
echo "Architecture: $ARCH"

if [[ ! "$ARCH" =~ ^(arm|aarch64) ]]; then
    echo "Warning: This doesn't appear to be an ARM system"
    echo "Detected: $ARCH"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
echo "Building TrunkSDR..."
NPROC=$(nproc)
echo "Using $NPROC parallel jobs"

make -j$NPROC

echo ""
echo "========================================="
echo "Build complete!"
echo "========================================="
echo ""
echo "Binary location: build/trunksdr"
echo ""
echo "To install system-wide, run:"
echo "  sudo make install"
echo ""
echo "To run from build directory:"
echo "  ./trunksdr --config ../config/config.example.json"
echo ""
