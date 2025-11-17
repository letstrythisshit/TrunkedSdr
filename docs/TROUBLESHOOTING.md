# TrunkSDR Troubleshooting Guide

Solutions to common problems.

## Table of Contents

- [Installation Issues](#installation-issues)
- [RTL-SDR Problems](#rtl-sdr-problems)
- [Audio Issues](#audio-issues)
- [Decoding Problems](#decoding-problems)
- [Performance Issues](#performance-issues)
- [Build Errors](#build-errors)

## Installation Issues

### "No RTL-SDR devices found"

**Symptoms:** Application can't detect RTL-SDR dongle

**Solutions:**

1. **Check USB connection:**
   ```bash
   lsusb | grep Realtek
   ```
   Should show: `Realtek Semiconductor Corp. RTL2838 DVB-T`

2. **Check permissions:**
   ```bash
   # Add udev rules
   sudo ./scripts/install_deps.sh

   # Or manually:
   sudo usermod -a -G plugdev $USER
   # Log out and back in
   ```

3. **Unload DVB-T drivers:**
   ```bash
   sudo rmmod dvb_usb_rtl28xxu
   sudo rmmod rtl2832

   # Blacklist permanently
   echo "blacklist dvb_usb_rtl28xxu" | sudo tee -a /etc/modprobe.d/blacklist-rtl.conf
   ```

4. **Test with rtl_test:**
   ```bash
   rtl_test -t
   ```

### "mbelib not found"

**Symptoms:** Builds but no digital voice

**Solution:**
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

### Permission Denied Errors

**Symptoms:** Can't access /dev/bus/usb devices

**Solution:**
```bash
# Add user to plugdev group
sudo usermod -a -G plugdev $USER

# Create udev rule
sudo cat > /etc/udev/rules.d/20-rtlsdr.rules << EOF
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

# Log out and back in
```

## RTL-SDR Problems

### Poor Reception / No Signal Lock

**Symptoms:** Can't lock onto control channel

**Solutions:**

1. **Check antenna:**
   - Verify antenna is connected
   - Use antenna tuned for frequency range
   - Try different antenna positions

2. **Verify frequency:**
   ```bash
   # Use rtl_fm to verify signal exists
   rtl_fm -f 851.0125M -M fm -s 48k | aplay -r 48k -f S16_LE
   ```

3. **Adjust PPM correction:**
   ```bash
   # Test PPM error
   rtl_test -p

   # Update config.json with measured PPM
   "ppm_correction": 52
   ```

4. **Try manual gain:**
   ```json
   "gain": 42.0
   ```

   Test different values: 20, 30, 40, 45

5. **Check frequency is correct:**
   - Verify on RadioReference.com
   - Ensure frequency is in Hz (e.g., 851012500, not 851.0125)

### Dropped Samples

**Symptoms:** "Failed to write data" or sample overruns

**Solutions:**

1. **Use USB 2.0 port (not USB 3.0):**
   - RTL-SDR works better on USB 2.0
   - Avoid USB hubs if possible

2. **Reduce sample rate:**
   ```json
   "sample_rate": 1024000
   ```

3. **Check USB power:**
   - Use powered USB hub
   - Quality power supply (3A for Pi 4)

4. **Disable USB autosuspend:**
   ```bash
   # Temporarily
   echo -1 | sudo tee /sys/module/usbcore/parameters/autosuspend

   # Permanently - add to /etc/rc.local
   echo "options usbcore autosuspend=-1" | sudo tee /etc/modprobe.d/usb-autosuspend.conf
   ```

### Frequency Drift

**Symptoms:** Lock lost after time, frequency seems off

**Solutions:**

1. **Calibrate PPM:**
   Use kalibrate-rtl or rtl_test -p

2. **Temperature:** RTL-SDR drifts when hot
   - Add heatsink
   - Allow warm-up time
   - Consider TCXO-equipped dongle (RTL-SDR Blog V3/V4)

3. **Update PPM in config:**
   ```json
   "ppm_correction": 56
   ```

## Audio Issues

### No Audio Output

**Symptoms:** Everything works but no audio

**Solutions:**

1. **Check PulseAudio:**
   ```bash
   # Is PulseAudio running?
   pulseaudio --check
   echo $?  # Should be 0

   # Start if not running
   pulseaudio --start
   ```

2. **List audio devices:**
   ```bash
   pactl list sinks short
   ```

   Update config.json:
   ```json
   "output_device": "alsa_output.usb-C-Media_Electronics_Inc._USB_Audio_Device-00.analog-stereo"
   ```

3. **Check volume:**
   ```bash
   pactl set-sink-volume @DEFAULT_SINK@ 65536  # 100%
   pactl set-sink-mute @DEFAULT_SINK@ 0        # Unmute
   ```

4. **Test with aplay:**
   ```bash
   speaker-test -t wav -c 1
   ```

5. **Check ALSA instead:**
   If PulseAudio problematic, modify code to use ALSA directly

### Choppy/Distorted Audio

**Symptoms:** Audio plays but sounds bad

**Solutions:**

1. **Increase audio buffer:**
   Modify AUDIO_BUFFER_FRAMES in types.h:
   ```cpp
   constexpr size_t AUDIO_BUFFER_FRAMES = 320;  // Increase from 160
   ```

2. **Check CPU usage:**
   ```bash
   top -p $(pgrep trunksdr)
   ```
   If >80%, reduce load (see Performance Issues)

3. **Verify signal strength:**
   Weak signal = poor audio quality

4. **Check sample rate:**
   ```json
   "sample_rate": 8000
   ```

### Audio Delays

**Symptoms:** Significant lag between transmission and playback

**Solutions:**

1. **Normal latency:** 200-500ms is expected
   - Demodulation takes time
   - Decoding takes time
   - Audio buffering needed

2. **Reduce if excessive (>1s):**
   - Reduce audio buffer size
   - Increase CPU performance
   - Check for USB issues

## Decoding Problems

### Can't Lock Control Channel

**Symptoms:** "Searching for sync" or never locks

**Solutions:**

1. **Verify NAC (P25):**
   ```json
   "nac": "0x293"  // Must match system
   ```

   Find correct NAC on RadioReference.com

2. **Check system type:**
   ```json
   "type": "p25"  // Correct?
   ```

3. **Verify control channel frequency:**
   - Use rtl_fm to listen
   - Should hear continuous data
   - Check RadioReference for current frequencies

4. **Increase debug logging:**
   ```bash
   trunksdr --config config.json --log-level debug
   ```

   Look for:
   - Frame sync messages
   - NAC mismatches
   - Demodulation errors

5. **Try different sample rate:**
   ```json
   "sample_rate": 2400000
   ```

### No Call Grants Decoded

**Symptoms:** Locks control channel but no voice grants

**Solutions:**

1. **Check talkgroup filter:**
   ```json
   "enabled": []  // Empty = monitor all
   ```

2. **Verify talkgroups are active:**
   - Check RadioReference or scanner
   - System may be quiet

3. **Check grant callback:**
   Enable debug logging to see if grants detected:
   ```bash
   --log-level debug
   ```

4. **Verify protocol decoder:**
   P25 TSBK opcodes should appear in logs

### Voice Not Following Grants

**Symptoms:** Sees grants but doesn't tune to voice

**Solutions:**

1. **Single SDR limitation:**
   - Current version monitors control channel only
   - Voice following requires second SDR or fast retuning
   - Multi-SDR support planned

2. **Check frequency table:**
   - P25 uses Identifier Updates
   - Ensure frequency table populated

## Performance Issues

### High CPU Usage

**Symptoms:** TrunkSDR uses >80% CPU

**Solutions:**

1. **Optimize compiler flags:**
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -mcpu=native"
   ```

2. **Reduce sample rate:**
   ```json
   "sample_rate": 1024000
   ```

3. **Enable ARM NEON:**
   Verify NEON used:
   ```bash
   cmake .. 2>&1 | grep NEON
   ```

4. **Disable other services:**
   ```bash
   sudo systemctl stop bluetooth
   sudo systemctl stop avahi-daemon
   # etc.
   ```

5. **Use headless mode:**
   - Disable X11/desktop
   - Run from SSH

6. **Overclock (Raspberry Pi):**
   ```bash
   # Edit /boot/config.txt
   over_voltage=6
   arm_freq=2000
   ```

   **WARNING:** Requires good cooling

### Memory Issues

**Symptoms:** Out of memory errors

**Solutions:**

1. **Add swap:**
   ```bash
   sudo dd if=/dev/zero of=/swapfile bs=1M count=2048
   sudo chmod 600 /swapfile
   sudo mkswap /swapfile
   sudo swapon /swapfile
   ```

2. **Reduce buffer sizes:**
   Edit types.h:
   ```cpp
   constexpr size_t DEFAULT_BUFFER_SIZE = 128 * 1024;
   ```

3. **Close other applications**

### System Freezes

**Symptoms:** Raspberry Pi becomes unresponsive

**Solutions:**

1. **Better power supply:**
   - Raspberry Pi 4: 3A minimum
   - Poor power = instability

2. **Cooling:**
   - Add heatsink
   - Add fan
   - Monitor temperature:
     ```bash
     vcgencmd measure_temp
     ```

3. **Reduce load:**
   - Lower sample rate
   - Don't overclock
   - Disable GUI

## Build Errors

### CMake Can't Find Libraries

**Error:** `Could not find librtlsdr`

**Solution:**
```bash
sudo apt-get update
sudo apt-get install librtlsdr-dev pkg-config
pkg-config --modversion librtlsdr
```

### Compiler Out of Memory

**Error:** `internal compiler error: Killed`

**Solution:**
```bash
# Use single job
make -j1

# Add swap
sudo dd if=/dev/zero of=/swapfile bs=1M count=2048
sudo mkswap /swapfile
sudo swapon /swapfile
```

### Linker Errors

**Error:** `undefined reference to...`

**Solution:**
```bash
# Update library cache
sudo ldconfig

# Verify libraries installed
ldconfig -p | grep rtl
ldconfig -p | grep pulse
ldconfig -p | grep mbe
```

## Getting Help

### Useful Debug Information

When reporting issues, include:

1. **System info:**
   ```bash
   uname -a
   cat /proc/cpuinfo | grep -i "model name"
   cat /proc/meminfo | grep MemTotal
   ```

2. **RTL-SDR info:**
   ```bash
   rtl_test -t
   lsusb | grep Realtek
   ```

3. **Build info:**
   ```bash
   cmake .. 2>&1 | grep "TrunkSDR Configuration"
   ```

4. **Debug log:**
   ```bash
   trunksdr --config config.json --log-level debug 2>&1 | tee debug.log
   ```

5. **Configuration:**
   - Sanitized config.json (remove sensitive info)
   - System type, frequencies

### Log Levels

```bash
--log-level debug    # Verbose, everything
--log-level info     # Normal operation
--log-level warning  # Warnings only
--log-level error    # Errors only
```

### Community Resources

- GitHub Issues
- RadioReference.com forums
- RTL-SDR subreddit
- Scanner enthusiast forums

## Known Limitations

Current version:

1. **Single SDR only** - Voice following requires manual implementation
2. **P25 Phase 1 only** - Phase 2 framework present but incomplete
3. **No encryption support** - Cannot decode encrypted transmissions
4. **No web UI** - Command-line only
5. **Limited recording** - Basic WAV recording only

These are planned for future releases.

## Next Steps

If still having issues:

1. Review [Configuration Guide](CONFIGURATION.md)
2. Check [Protocol Details](PROTOCOLS.md)
3. Verify [Building Instructions](BUILDING.md)
4. Ask for help with debug log
