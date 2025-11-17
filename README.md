# TrunkSDR

A fully functional trunked radio system decoder application that uses RTL-SDR hardware to monitor, decode, and provide real-time playback of trunked radio communications.

## Features

- **Multi-Protocol Support**
  - P25 Phase 1 (APCO Project 25)
  - P25 Phase 2 (TDMA)
  - Motorola SmartNet/SmartZone
  - EDACS (framework included)
  - DMR (framework included)

- **Digital Voice Codecs**
  - IMBE (P25 Phase 1) via mbelib
  - AMBE (P25 Phase 2, DMR)
  - Analog FM demodulation

- **Real-Time Audio**
  - Live playback via PulseAudio
  - Automatic gain control
  - Call queueing and priority management
  - Optional call recording

- **ARM Optimized**
  - Optimized for Raspberry Pi and ARM-based systems
  - NEON SIMD support where available
  - Low memory footprint
  - Efficient multi-threaded design

## Hardware Requirements

### Minimum
- Raspberry Pi 3 Model B+ or equivalent ARM board
- RTL-SDR dongle (RTL2832U-based)
- 1GB RAM
- 4GB storage

### Recommended
- Raspberry Pi 4 Model B (4GB RAM)
- RTL-SDR Blog V3 or V4
- Quality antenna tuned to your frequencies
- Heatsink/cooling for continuous operation

## Quick Start

### 1. Install Dependencies

On Raspberry Pi OS / Debian / Ubuntu:

```bash
sudo ./scripts/install_deps.sh
```

This will install all required libraries including:
- RTL-SDR drivers
- PulseAudio
- mbelib (for digital voice)
- Build tools and dependencies

### 2. Build

```bash
./scripts/build_arm.sh
```

### 3. Configure

Copy the example configuration:

```bash
cp config/config.example.json config/myconfig.json
```

Edit `config/myconfig.json` with your system parameters:
- Control channel frequencies
- System ID / NAC (for P25)
- Talkgroups to monitor
- Audio settings

See [CONFIGURATION.md](docs/CONFIGURATION.md) for detailed configuration options.

### 4. Run

```bash
./build/trunksdr --config config/myconfig.json
```

Or install system-wide:

```bash
cd build
sudo make install
trunksdr --config /path/to/config.json
```

## Usage

### List RTL-SDR Devices

```bash
trunksdr --devices
```

### Set Log Level

```bash
trunksdr --config config.json --log-level debug
```

### Log to File

```bash
trunksdr --config config.json --log-file /var/log/trunksdr.log
```

## Supported Systems

| Protocol | Status | Control Channel | Voice Channel | Notes |
|----------|--------|-----------------|---------------|-------|
| P25 Phase 1 | ✅ Full | C4FM | IMBE | Most widely deployed |
| P25 Phase 2 | 🟡 Partial | C4FM | AMBE (2-slot TDMA) | Framework included |
| SmartNet | ✅ Full | FSK 3600/9600 | Analog FM | Motorola Type II |
| SmartZone | 🟡 Partial | FSK | Analog/Digital | Framework included |
| EDACS | 🟡 Partial | FSK | Analog FM | Framework included |
| DMR | 🟡 Partial | - | AMBE | Framework included |

✅ Full support | 🟡 Partial/Framework | ❌ Not implemented

## Documentation

- [Building and Installation](docs/BUILDING.md)
- [Configuration Guide](docs/CONFIGURATION.md)
- [Protocol Details](docs/PROTOCOLS.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

## Legal Considerations

**IMPORTANT:** This software is intended for monitoring of authorized communications only.

- ✅ **Legal Uses:**
  - Monitoring public safety communications where permitted by law
  - Amateur radio experimentation
  - Educational purposes
  - Authorized security research

- ❌ **Illegal Uses:**
  - Intercepting private communications
  - Monitoring cellular phone calls
  - Using intercepted information for commercial advantage
  - Disclosing intercepted communications (varies by jurisdiction)

**Know your local laws!** In the United States:
- 18 U.S.C. § 2511 - Interception of communications
- 47 U.S.C. § 605 - Unauthorized publication of communications
- State and local regulations may apply

### Codec Licensing

IMBE and AMBE are patented technologies. This software uses mbelib, which implements these codecs. Users are responsible for ensuring they have appropriate licenses. Consider:

1. Using hardware decoders (DV3000, ThumbDV)
2. Monitoring systems using open codecs (Codec2)
3. Consulting legal counsel regarding your use case

## Performance

Typical performance on Raspberry Pi 4 (4GB):

- CPU Usage: 30-50% (single P25 system)
- Memory: ~200MB
- Audio Latency: <500ms
- Dropped Samples: <0.1% with quality USB power

Optimizations:
- Use `SCHED_FIFO` real-time scheduling for audio thread
- Disable unnecessary services (GUI, etc.)
- Use quality power supply (3A minimum)
- Active cooling recommended for 24/7 operation

## Architecture

```
RTL-SDR → I/Q Samples → Demodulator → Protocol Decoder → Call Manager → Audio Out
                          ↓              ↓                 ↓
                       C4FM/FSK      P25/SmartNet    Talkgroup Filter
                                                          ↓
                                                     Codec Decoder
                                                     (IMBE/AMBE)
```

Multi-threaded design:
- Thread 1: SDR I/Q acquisition
- Thread 2: Control channel processing
- Thread 3: Voice channel demodulation (future multi-SDR support)
- Thread 4: Audio codec decoding
- Thread 5: Audio playback

## Contributing

Contributions welcome! Areas of interest:

- Additional protocol implementations (NXDN, DMR enhancements, TETRA)
- Performance optimizations
- Web UI / monitoring interface
- Call recording and database logging
- Multi-SDR support for parallel voice channels
- Hardware codec support (DV3000, etc.)

## Credits

- RTL-SDR: https://github.com/osmocom/rtl-sdr
- mbelib: https://github.com/szechyjs/mbelib
- Inspired by trunk-recorder: https://github.com/robotastic/trunk-recorder
- P25 protocol reference: OP25 project

## License

MIT License - See [LICENSE](LICENSE) file for details.

**This software is provided as-is with no warranty. Users are responsible for compliance with all applicable laws and regulations.**

## Contact

For issues, feature requests, or questions:
- GitHub Issues: [project issues page]
- Documentation: See `docs/` directory

## Disclaimer

This software is for educational and authorized use only. The authors are not responsible for misuse or any legal consequences resulting from the use of this software. Always respect privacy laws and regulations in your jurisdiction.
