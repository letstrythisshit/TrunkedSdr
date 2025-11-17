#!/bin/bash
# TrunkSDR Dependency Installation Script
# For Debian/Ubuntu/Raspberry Pi OS

set -e

echo "========================================="
echo "TrunkSDR Dependency Installation"
echo "========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "Cannot detect OS. This script supports Debian-based systems."
    exit 1
fi

echo "Detected OS: $OS $VER"
echo ""

# Update package list
echo "Updating package list..."
apt-get update

# Install build tools
echo "Installing build tools..."
apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    wget \
    curl

# Install RTL-SDR library
echo "Installing RTL-SDR library..."
apt-get install -y \
    librtlsdr-dev \
    libusb-1.0-0-dev \
    rtl-sdr

# Install audio libraries
echo "Installing audio libraries..."
apt-get install -y \
    libpulse-dev \
    pulseaudio \
    libasound2-dev

# Install other dependencies
echo "Installing other dependencies..."
apt-get install -y \
    libjsoncpp-dev \
    libboost-all-dev \
    libcurl4-openssl-dev

# Optional: Install liquid-dsp for advanced DSP
echo "Checking for liquid-dsp..."
if ! dpkg -l | grep -q libliquid-dev; then
    echo "Installing liquid-dsp (this may take a while)..."
    apt-get install -y libliquid-dev || {
        echo "liquid-dsp not available in repos, building from source..."
        cd /tmp
        git clone https://github.com/jgaeddert/liquid-dsp.git
        cd liquid-dsp
        ./bootstrap.sh
        ./configure --enable-simdoverride
        make -j$(nproc)
        make install
        ldconfig
        cd ~
        rm -rf /tmp/liquid-dsp
    }
else
    echo "liquid-dsp already installed"
fi

# Install mbelib for digital voice codecs
echo "Installing mbelib for digital voice decoding..."
if [ ! -f /usr/local/lib/libmbe.so ] && [ ! -f /usr/lib/libmbe.so ]; then
    cd /tmp
    git clone https://github.com/szechyjs/mbelib.git
    cd mbelib
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    make install
    ldconfig
    cd ~
    rm -rf /tmp/mbelib
    echo "mbelib installed successfully"
else
    echo "mbelib already installed"
fi

# Blacklist DVB-T drivers to prevent conflict with RTL-SDR
echo "Blacklisting DVB-T drivers..."
cat > /etc/modprobe.d/blacklist-rtl.conf << EOF
blacklist dvb_usb_rtl28xxu
blacklist rtl2832
blacklist rtl2830
EOF

# Unload DVB-T modules if loaded
rmmod dvb_usb_rtl28xxu 2>/dev/null || true
rmmod rtl2832 2>/dev/null || true

# Create udev rules for RTL-SDR
echo "Creating udev rules for RTL-SDR..."
cat > /etc/udev/rules.d/20-rtlsdr.rules << EOF
# RTL-SDR
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2832", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", MODE="0666"
EOF

udevadm control --reload-rules
udevadm trigger

echo ""
echo "========================================="
echo "Dependency installation complete!"
echo "========================================="
echo ""
echo "Please reconnect your RTL-SDR device if it was already connected."
echo "You can test it with: rtl_test"
echo ""
