# TrunkSDR Configuration Guide

Complete reference for configuring TrunkSDR.

## Table of Contents

- [Configuration File Format](#configuration-file-format)
- [SDR Configuration](#sdr-configuration)
- [System Configuration](#system-configuration)
- [Talkgroup Configuration](#talkgroup-configuration)
- [Audio Configuration](#audio-configuration)
- [Protocol-Specific Settings](#protocol-specific-settings)
- [Examples](#examples)

## Configuration File Format

TrunkSDR uses JSON format for configuration. The configuration file has four main sections:

```json
{
  "sdr": { ... },
  "system": { ... },
  "talkgroups": { ... },
  "audio": { ... }
}
```

## SDR Configuration

Controls RTL-SDR hardware settings.

```json
"sdr": {
  "device_index": 0,
  "sample_rate": 2048000,
  "gain": "auto",
  "ppm_correction": 0,
  "frequency_correction": 0
}
```

### Parameters

**device_index** (integer, default: 0)
- RTL-SDR device index (0 for first device)
- Use `trunksdr --devices` to list available devices
- For multiple dongles, use different indices

**sample_rate** (integer, default: 2048000)
- Sample rate in Hz
- Recommended values: 1024000, 2048000, 2400000
- Higher rates = more bandwidth but more CPU usage
- P25: 2048000 works well
- SmartNet: 1024000 sufficient

**gain** (string or number)
- `"auto"`: Automatic gain control
- Number (0-50): Manual gain in dB
- Recommended: Start with `"auto"`, tune manually if needed
- Check available gains: `rtl_test`

**ppm_correction** (integer, default: 0)
- Frequency correction in parts-per-million
- Most RTL-SDR dongles need 0-60 ppm correction
- Determine your dongle's PPM with `rtl_test` or `kalibrate-rtl`
- Positive or negative values accepted

**frequency_correction** (integer, default: 0)
- Additional fine frequency correction in Hz
- Usually not needed if ppm_correction is set correctly

### Finding Your PPM Correction

Method 1: Using rtl_test
```bash
rtl_test -p
# Let it run for 5 minutes, note the PPM error
```

Method 2: Using known strong signal
```bash
# Tune to a known frequency and note the offset
# PPM = (measured_freq - actual_freq) / actual_freq * 1e6
```

## System Configuration

Defines the trunked radio system parameters.

```json
"system": {
  "type": "p25",
  "name": "My P25 System",
  "system_id": "0x001",
  "nac": "0x293",
  "wacn": "0xBEE00",
  "control_channels": [
    851012500,
    851037500,
    851062500
  ],
  "modulation": "c4fm"
}
```

### Parameters

**type** (string, required)
- System type identifier
- Options:
  - `"p25"` or `"p25_phase1"` - APCO P25 Phase 1
  - `"p25_phase2"` - APCO P25 Phase 2
  - `"smartnet"` - Motorola SmartNet
  - `"smartzone"` - Motorola SmartZone
  - `"edacs"` - EDACS
  - `"dmr"` - DMR Tier 3
  - `"nxdn"` - NXDN

**name** (string, optional)
- Friendly name for the system
- Used in logs and displays

**system_id** (string/integer, P25 only)
- P25 System ID (hexadecimal or decimal)
- Find in RadioReference.com or by scanning

**nac** (string/integer, P25 required)
- Network Access Code (12-bit value)
- Hexadecimal (with 0x prefix) or decimal
- Critical for P25 - must match your system
- Find on RadioReference.com

**wacn** (string/integer, P25 optional)
- Wide Area Communications Network ID
- Used for multi-site systems

**control_channels** (array of numbers, required)
- List of control channel frequencies in Hz
- System will scan these to find active control channel
- Minimum 1 channel required
- Find on RadioReference.com or by scanning

**modulation** (string, optional)
- Modulation type (auto-detected from system type)
- Options: `"c4fm"`, `"fsk"`, `"gmsk"`, `"qpsk"`

### Finding System Parameters

**RadioReference.com:**
1. Search for your location
2. Find the trunked system
3. Note: System Type, Control Channels, NAC/System ID

**Scanning:**
```bash
# Use rtl_power or other tools to find active frequencies
rtl_power -f 851M:862M:25k -g 40 -i 10s scan.csv
```

## Talkgroup Configuration

Controls which talkgroups to monitor and how to handle them.

```json
"talkgroups": {
  "enabled": [100, 101, 102, 200],
  "priority": {
    "100": 10,
    "101": 9,
    "200": 5
  },
  "labels": {
    "100": "Police Dispatch",
    "101": "Police Tactical",
    "200": "Fire Dispatch"
  }
}
```

### Parameters

**enabled** (array of integers)
- List of talkgroup IDs to monitor
- Empty array `[]` = monitor all talkgroups
- Only listed talkgroups will be decoded and played

**priority** (object, optional)
- Talkgroup ID to priority mapping
- Priority range: 0-255 (higher = more important)
- Used when multiple calls are active
- Default priority: 5

**labels** (object, optional)
- Talkgroup ID to friendly name mapping
- Used in logs and future UI
- Purely informational

### Talkgroup Priority System

When multiple calls are active simultaneously:
1. Higher priority calls are played first
2. Lower priority calls are queued
3. Equal priority: first-come-first-served

Example priorities:
- Emergency: 10
- Dispatch: 8
- Tactical: 6
- General: 5
- Maintenance: 3

## Audio Configuration

Controls audio output and recording.

```json
"audio": {
  "output_device": "default",
  "codec": "imbe",
  "sample_rate": 8000,
  "record_calls": false,
  "recording_path": "/var/lib/trunksdr/recordings"
}
```

### Parameters

**output_device** (string, default: "default")
- PulseAudio output device name
- `"default"`: System default audio output
- List devices: `pactl list sinks short`

**codec** (string, auto-detected)
- Voice codec type
- Options:
  - `"imbe"` - P25 Phase 1
  - `"ambe"` - P25 Phase 2, DMR
  - `"analog"` - FM analog
- Usually auto-detected from system type

**sample_rate** (integer, default: 8000)
- Audio output sample rate in Hz
- P25/Digital: 8000 Hz
- Analog: 8000 or 16000 Hz

**record_calls** (boolean, default: false)
- Enable call recording to disk
- Creates WAV files in recording_path

**recording_path** (string)
- Directory for recorded audio files
- Must be writable by user running trunksdr
- Files named: `{talkgroup}_{timestamp}.wav`

## Protocol-Specific Settings

### P25 Phase 1

```json
{
  "system": {
    "type": "p25",
    "nac": "0x293",
    "wacn": "0xBEE00",
    "system_id": "0x001",
    "control_channels": [851012500, 851037500]
  },
  "audio": {
    "codec": "imbe"
  }
}
```

**Required:** `nac`, `control_channels`
**Optional:** `wacn`, `system_id`

### Motorola SmartNet

```json
{
  "system": {
    "type": "smartnet",
    "control_channels": [851012500],
    "baud_rate": 3600
  },
  "audio": {
    "codec": "analog"
  }
}
```

**baud_rate options:**
- `3600` - SmartNet Type II (older)
- `9600` - SmartNet Type II (newer)

### P25 Phase 2

```json
{
  "system": {
    "type": "p25_phase2",
    "nac": "0x293",
    "control_channels": [851012500]
  },
  "audio": {
    "codec": "ambe"
  }
}
```

Requires mbelib with AMBE support.

## Examples

### Example 1: Basic P25 System

```json
{
  "sdr": {
    "device_index": 0,
    "sample_rate": 2048000,
    "gain": "auto",
    "ppm_correction": 52
  },
  "system": {
    "type": "p25",
    "name": "County Public Safety",
    "nac": "0x293",
    "control_channels": [
      851012500,
      851037500
    ]
  },
  "talkgroups": {
    "enabled": [100, 101, 200],
    "priority": {
      "100": 10
    },
    "labels": {
      "100": "Police",
      "101": "Fire",
      "200": "EMS"
    }
  },
  "audio": {
    "output_device": "default",
    "record_calls": false
  }
}
```

### Example 2: SmartNet with Recording

```json
{
  "sdr": {
    "device_index": 0,
    "sample_rate": 1024000,
    "gain": 40.0,
    "ppm_correction": 0
  },
  "system": {
    "type": "smartnet",
    "name": "City SmartNet",
    "control_channels": [851012500],
    "baud_rate": 3600
  },
  "talkgroups": {
    "enabled": []
  },
  "audio": {
    "output_device": "default",
    "record_calls": true,
    "recording_path": "/home/pi/recordings"
  }
}
```

### Example 3: Multi-Priority P25

```json
{
  "sdr": {
    "device_index": 0,
    "sample_rate": 2048000,
    "gain": 42.0,
    "ppm_correction": 56
  },
  "system": {
    "type": "p25",
    "nac": "0x293",
    "system_id": "0x001",
    "wacn": "0xBEE00",
    "control_channels": [
      851012500,
      851037500,
      851062500
    ]
  },
  "talkgroups": {
    "enabled": [100, 101, 102, 200, 201, 300],
    "priority": {
      "100": 10,
      "101": 9,
      "102": 8,
      "200": 7,
      "201": 6,
      "300": 5
    },
    "labels": {
      "100": "Police Dispatch",
      "101": "Police Tac 1",
      "102": "Police Tac 2",
      "200": "Fire Dispatch",
      "201": "Fire Operations",
      "300": "EMS Dispatch"
    }
  },
  "audio": {
    "output_device": "default",
    "codec": "imbe",
    "sample_rate": 8000,
    "record_calls": true,
    "recording_path": "/mnt/usb/recordings"
  }
}
```

## Configuration Best Practices

1. **Start Simple**
   - Begin with minimal configuration
   - Add features incrementally
   - Test each change

2. **PPM Calibration**
   - Always calibrate your RTL-SDR
   - PPM drift affects reception quality

3. **Gain Settings**
   - Start with auto gain
   - If poor performance, try manual gain
   - Too much gain = worse performance

4. **Talkgroup Filtering**
   - Monitor specific talkgroups to reduce CPU load
   - Empty `enabled` array = monitor everything

5. **Recording**
   - Ensure sufficient disk space
   - Recordings can grow quickly
   - Consider rotating old files

## Validation

Validate your configuration:

```bash
# TrunkSDR will report configuration errors on startup
./trunksdr --config myconfig.json --log-level debug
```

Common configuration errors:
- Missing required fields (`nac`, `control_channels`)
- Invalid frequencies (must be in Hz)
- Malformed JSON
- Invalid file paths

## Environment Variables

Override configuration with environment variables:

```bash
export TRUNKSDR_DEVICE_INDEX=1
export TRUNKSDR_LOG_LEVEL=debug
./trunksdr --config config.json
```

## Next Steps

- [Return to README](../README.md)
- [Protocol Details](PROTOCOLS.md)
- [Troubleshooting](TROUBLESHOOTING.md)
