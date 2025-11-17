# TrunkSDR - European Digital Radio Protocols Extension

This extension adds support for European digital radio protocols to the TrunkSDR trunked radio decoder application.

## Supported European Protocols

### ✅ Tier 1 - Fully Implemented

1. **TETRA (Terrestrial Trunked Radio)**
   - Used by: Emergency services (police, fire, ambulance, military)
   - Frequencies: 380-400 MHz (emergency), 410-430 MHz, 870-888 MHz (commercial)
   - Status: **Full physical layer + MAC layer + decoder implemented**
   - Features:
     - π/4-DQPSK demodulation (18 ksps)
     - 4-slot TDMA frame decoding
     - System info extraction (MCC, MNC, color code)
     - Call grant detection
     - Encryption detection (TEA1/2/3/4)
     - Control channel monitoring
   - Known Systems: UK Airwave, German BOS, Dutch C2000, French ANTARES, Belgian ASTRID

2. **DMR Tier II/III (Digital Mobile Radio)**
   - Used by: Commercial operators, utilities, transportation
   - Frequencies: 136-174 MHz (VHF), 403-470 MHz (UHF)
   - Status: **Full decoder with Capacity Plus trunking implemented**
   - Features:
     - 4FSK demodulation (4800 sps)
     - 2-slot TDMA
     - CSBK (Control Signaling Block) decoding
     - Capacity Plus trunking support
     - Color code system (0-15)
     - Talker Alias decoding
     - AMBE+2 codec ready (via mbelib)
   - Known Systems: UK commercial, Dutch ProRail, Scandinavian utilities

### 🟡 Tier 2 - Framework Implemented

3. **NXDN** - Framework ready, protocol detection available
4. **dPMR** - Framework ready, protocol detection available

## Quick Start

### 1. Install Dependencies

```bash
cd TrunkedSdr
./scripts/install_european_deps.sh
```

This installs:
- FFTW3 (for TETRA π/4-DQPSK DSP)
- mbelib (for AMBE+2 codec in DMR/NXDN)
- Optional: Osmocom libraries, IT++, VOLK

### 2. Build with European Protocols

```bash
mkdir build && cd build
cmake .. -DENABLE_EUROPEAN_PROTOCOLS=ON \
         -DENABLE_TETRA=ON \
         -DENABLE_DMR_TIER3=ON \
         -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### 3. Configure for TETRA (e.g., UK Airwave)

Example: Monitor UK Airwave control channels

```json
{
  "system": {
    "type": "TETRA",
    "name": "UK Airwave",
    "control_channels": [
      382612500,
      382637500,
      382662500
    ],
    "mcc": 234,
    "mnc": 14,
    "color_code": 1
  },
  "sdr": {
    "device": 0,
    "sample_rate": 2048000,
    "gain": 40,
    "ppm_correction": 0
  }
}
```

Run:
```bash
trunksdr --config uk_airwave.json
```

### 4. Configure for DMR Tier III

Example: DMR Capacity Plus system

```json
{
  "system": {
    "type": "DMR_TIER3",
    "name": "Commercial DMR",
    "trunking_type": "capacity_plus",
    "rest_channel": 167862500,
    "voice_channels": [
      167850000,
      167875000,
      167900000
    ],
    "color_code": 1
  },
  "sdr": {
    "device": 0,
    "sample_rate": 2048000,
    "gain": 38
  }
}
```

## European Frequency Databases

Pre-configured frequency allocations available in:

**`config/frequency_databases/european_bands.json`**

Includes:
- TETRA emergency services (380-400 MHz) - all EU countries
- TETRA commercial bands
- DMR VHF/UHF allocations
- NXDN frequency ranges
- dPMR allocations
- PMR446 (license-free)

## Known European Systems

Pre-configured system files in `config/european_systems/`:

### TETRA Emergency Services
- **`uk_airwave.json`** - UK Airwave (largest TETRA network globally)
- **`germany_bos.json`** - German BOS network (~4600 sites)
- **`netherlands_c2000.json`** - Dutch emergency services

### DMR Commercial
- **`uk_commercial_dmr_example.json`** - DMR Tier III Capacity Plus example

## Performance Notes (ARM/Raspberry Pi)

### Raspberry Pi 4 (4GB)
- **TETRA decode**: 80-100% CPU (single core) - π/4-DQPSK is CPU intensive
- **DMR decode**: 40-60% CPU (single core) - 4FSK is lighter
- **Memory**: <512MB per system
- **Audio latency**:
  - TETRA: <1.5 seconds
  - DMR: <1.0 second

### Raspberry Pi 5
- Can handle 2 concurrent protocols (e.g., TETRA + DMR monitoring simultaneously)

### Optimizations Applied
- ARM NEON SIMD instructions
- Optimized RRC filtering for TETRA
- Efficient Viterbi decoder
- Fixed-point math where applicable

## Documentation

Comprehensive guides in `docs/european/`:

1. **TETRA_GUIDE.md** - Complete TETRA implementation guide
   - Frame structure
   - Encryption types
   - Known systems by country
   - Control channel decoding
   - Legal warnings

2. **DMR_EUROPE.md** - DMR Tier II/III guide
   - Capacity Plus vs Connect Plus
   - Color code system
   - RadioID databases
   - Talker Alias

3. **EUROPEAN_BANDS.md** - Frequency allocations
   - Country-by-country breakdown
   - Channel spacing
   - Usage notes

4. **LEGAL_CONSIDERATIONS_EU.md** - **MUST READ**
   - Country-specific laws
   - GDPR implications
   - Encryption laws
   - Responsible use

## ⚠️ CRITICAL LEGAL WARNINGS

### 🔴 NEW: TETRA Decryption Capabilities 🔴

**IMPORTANT UPDATE**: This software now includes TETRA TEA1 decryption capabilities based on publicly disclosed vulnerabilities (CVE-2022-24402).

**⚠️ UNAUTHORIZED DECRYPTION IS A SERIOUS CRIME ⚠️**

### Legal Status of TETRA Decryption

**EXTREMELY IMPORTANT**: Laws regarding encrypted communication interception are **VERY STRICT**:

- **Germany**: TKG § 148, StGB § 202b - Unauthorized decryption is a criminal offense
- **United Kingdom**: RIPA 2000 - Up to 2 years imprisonment
- **France**: Criminal Code Article 226-15 - Up to 1 year + €45,000 fine
- **Netherlands**: Computer Crime Act - Criminal prosecution
- **EU-wide**: GDPR violations up to €20M or 4% global turnover

**DO NOT** attempt to decrypt communications without **EXPLICIT WRITTEN AUTHORIZATION**.

### What This Software Can Now Do

**TEA1 Encryption (CVE-2022-24402)**:
- ✓ Can detect TEA1 encrypted traffic
- ✓ Can exploit known backdoor to recover keys (educational/authorized testing only)
- ✓ Can decrypt TEA1 traffic **WITH PROPER AUTHORIZATION**
- ⚠️ **Requires explicit user acknowledgment before first use**

**TEA2/TEA3/TEA4 Encryption**:
- ✓ Can detect encryption type
- ✗ **CANNOT decrypt** - these algorithms remain secure
- ✗ No known vulnerabilities

### Authorization Required

The TETRA decryption module will **REFUSE TO OPERATE** until:
1. User reads comprehensive legal warnings
2. User explicitly acknowledges legal responsibilities
3. User confirms they have proper authorization
4. Authorization file is created

**First-time use flow**:
```
Launch tool → Legal Warning Display → User Acknowledgment Required →
"I ACCEPT FULL LEGAL RESPONSIBILITY" confirmation → Authorization granted
```

### Authorized Use Cases ONLY

Decryption features may **ONLY** be used for:
- ✓ **Educational purposes** in university/training environments with simulated traffic
- ✓ **Authorized penetration testing** with explicit written client permission
- ✓ **Security research** on systems you own or have explicit authorization
- ✓ **Law enforcement** with proper warrants and legal process

### PROHIBITED Uses

**NEVER** use decryption features to:
- ✗ Intercept real emergency services (police, fire, ambulance, military)
- ✗ Decrypt communications without explicit written authorization
- ✗ Act upon intercepted information
- ✗ Disclose intercepted communications
- ✗ "Test" on live networks to "see if it works"
- ✗ Monitor private or commercial TETRA systems without permission

### Technical Details

**TEA1 Vulnerability** (CVE-2022-24402):
- Intentional backdoor reduces 80-bit key to effective 32 bits
- Key recovery possible in ~1 minute on consumer laptop
- Discovered by Midnight Blue (2023), disclosed at Black Hat/DEF CON
- Affects primarily older commercial TETRA systems
- **Emergency services** typically use secure TEA2/TEA3 (not vulnerable)

**What Remains Secure**:
- TEA2, TEA3, TEA4 have NO known vulnerabilities
- UK Airwave, German BOS, Dutch C2000, etc. mostly use secure algorithms
- Modern deployments are not affected

### Documentation

**MUST READ before using decryption features**:
- **[TETRA Decryption Educational Guide](docs/european/TETRA_DECRYPTION_EDUCATIONAL.md)** - Complete guide
  - Technical background
  - Legal considerations by country
  - Ethical guidelines
  - References

### Building with Decryption Support

```bash
cmake .. \
  -DENABLE_EUROPEAN_PROTOCOLS=ON \
  -DENABLE_TETRA=ON \
  -DENABLE_TETRA_DECRYPTION=ON   # Enables decryption module

make -j$(nproc)
```

**Without `-DENABLE_TETRA_DECRYPTION`**: Only monitoring (no decryption)
**With `-DENABLE_TETRA_DECRYPTION`**: Full capabilities (requires authorization)

---

### Encryption (General)

- **TETRA**: 90%+ of emergency services use TEA2/TEA3/TEA4 encryption (secure)
- **DMR**: Commercial systems often use ARC4/AES encryption
- **Legacy systems**: Some commercial TETRA may still use vulnerable TEA1
- Unauthorized decryption is **ILLEGAL** in most EU countries

### What You Can Decode

✅ **Legal** (generally, but verify your country):
- System information broadcasts
- Control channel signaling
- Call setup metadata (talkgroup IDs, radio IDs, frequencies)
- Encryption indicators
- Clear/unencrypted transmissions (if authorized)

❌ **Illegal** (in most countries):
- Encrypted voice/data (even if you could)
- Acting upon intercepted information
- Disclosing intercepted communications
- Commercial advantage from intercepted data

### Country-Specific Regulations

- **UK**: Legal to receive, illegal to act upon or disclose emergency services
- **Germany**: TKG § 148 restricts monitoring
- **France**: Restricted monitoring of public safety
- **Netherlands**: Generally permitted for hobbyist use
- **All EU**: GDPR applies to stored communications data

**READ `docs/european/LEGAL_CONSIDERATIONS_EU.md` for detailed country information!**

## What Can You Expect to Decode?

### TETRA Emergency Services (e.g., UK Airwave)

**You WILL see:**
- System broadcasts (MCC=234, MNC=14, Network="Airwave")
- Control channel activity
- Call grants showing: TG=123, Source=456, Freq=382.8125 MHz, **[ENCRYPTED - TEA3]**
- Cell site information
- Registration/authentication messages (encrypted)

**You WILL NOT see:**
- Voice content (encrypted)
- Message content (encrypted)
- Locations (encrypted)
- Actual communications

**Value**: Understanding TETRA protocol, traffic analysis, educational purposes

### DMR Commercial (e.g., UK business)

**You WILL see:**
- Channel grants on rest channel
- Voice calls on voice channels (if unencrypted)
- Talker Alias: "JOHN DOE" or "TRUCK 5"
- Talkgroup activity
- Short messages (if unencrypted)

**You WILL NOT see:**
- Encrypted voice/data
- Private talkgroups (if access restricted)

**Value**: DMR is often unencrypted in commercial use, provides good learning opportunity

## Architecture

```
RTL-SDR → IQ Samples → European Demodulator → Protocol Decoder → Output
                           ↓                      ↓
                    π/4-DQPSK (TETRA)      TETRA PHY/MAC
                    4FSK (DMR/NXDN)        DMR Tier III
                                           ↓
                                      Call Manager → Audio
```

## Contributing

European protocol development areas:

- P25 Phase 2 TDMA for comparison
- NXDN full implementation
- dPMR full implementation
- Tetrapol (older French system)
- TETRA ACELP codec (challenging - limited open source)
- Hardware codec integration (DV3000)
- Additional trunking types (Connect Plus, Linked Capacity Plus)

## References

### Standards Bodies
- ETSI (European Telecommunications Standards Institute)
  - TETRA: EN 300 392 series
  - DMR: TS 102 361 series
  - dPMR: TS 102 658 series

### Open Source Projects
- osmo-tetra: https://osmocom.org/projects/tetra
- DSD: https://github.com/szechyjs/dsd (Digital Speech Decoder)
- mbelib: https://github.com/szechyjs/mbelib

### Frequency Databases
- RadioReference: https://www.radioreference.com/
- Signal Wiki: https://www.sigidwiki.com/

## License

MIT License (same as base TrunkSDR project)

## Disclaimer

**This software is for educational and authorized use only.**

The authors are not responsible for:
- Misuse or illegal monitoring
- Legal consequences from use
- Privacy violations
- GDPR violations

**Users are solely responsible for compliance with all applicable laws and regulations in their jurisdiction.**

**Always respect privacy, encryption, and legal boundaries.**

---

## Quick Command Reference

```bash
# Install dependencies
./scripts/install_european_deps.sh

# Build with all European protocols
cmake .. -DENABLE_EUROPEAN_PROTOCOLS=ON

# Build with only TETRA
cmake .. -DENABLE_TETRA=ON -DENABLE_DMR_TIER3=OFF

# Monitor UK Airwave TETRA
trunksdr --config config/european_systems/uk_airwave.json

# Monitor DMR system
trunksdr --config config/european_systems/uk_commercial_dmr_example.json

# List SDR devices
trunksdr --devices

# Debug mode
trunksdr --config myconfig.json --log-level debug
```

## Support

For issues, questions, or contributions:
- GitHub Issues: [Project repository]
- Documentation: `docs/european/`
- Configuration examples: `config/european_systems/`

---

**Remember: Education, not exploitation. Monitor responsibly and legally.**
