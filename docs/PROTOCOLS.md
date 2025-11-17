# Trunking Protocol Reference

Technical details of supported trunking protocols.

## Table of Contents

- [P25 Phase 1](#p25-phase-1)
- [P25 Phase 2](#p25-phase-2)
- [Motorola SmartNet](#motorola-smartnet)
- [EDACS](#edacs)
- [DMR Tier 3](#dmr-tier-3)
- [Protocol Comparison](#protocol-comparison)

## P25 Phase 1

APCO Project 25 Phase 1 is the most widely deployed digital public safety standard in North America.

### Technical Specifications

- **Modulation:** C4FM (4-level FSK)
- **Symbol Rate:** 4800 symbols/second
- **Voice Codec:** IMBE (Improved Multiband Excitation)
- **Audio Sample Rate:** 8000 Hz
- **Channel Bandwidth:** 12.5 kHz
- **Control Channel:** Continuous
- **Error Correction:** Golay, Hamming, BCH

### Frame Structure

**Data Unit Structure:**
```
Frame Sync (48 bits) → NID (64 bits) → Data → Error Correction
```

**Network ID (NID):**
- NAC: 12 bits (Network Access Code)
- DUID: 4 bits (Data Unit ID)
- Additional fields: 48 bits

### Data Unit IDs (DUID)

- `0x0` - Header Data Unit
- `0x3` - Terminator Data Unit
- `0x5` - Logical Link Data Unit 1 (LDU1)
- `0x7` - Trunking Signaling Block (TSBK)
- `0xA` - Logical Link Data Unit 2 (LDU2)
- `0xC` - Packet Data Unit (PDU)

### Trunking Signaling Blocks (TSBK)

**Group Voice Grant (0x00):**
```
Opcode (6) | Options (8) | Service (8) | Frequency (12) | Group (16) | Source (24)
```

**Unit-to-Unit Grant (0x04):**
```
Opcode (6) | Options (8) | Service (8) | Frequency (12) | Target (24) | Source (24)
```

**Identifier Update (0x3C):**
- Provides frequency table for the system
- Maps channel IDs to actual frequencies

### NAC (Network Access Code)

- 12-bit identifier
- Filters out other systems
- Range: 0x000 - 0xFFF
- Common values: 0x293, 0x2F7, 0xF7E

### WACN (Wide Area Communications Network)

- 20-bit identifier
- Identifies multi-site systems
- Used for roaming

### Finding P25 System Parameters

**RadioReference.com:**
1. Search your location
2. Note NAC, WACN, System ID
3. List all control channels

**Using OP25:**
```bash
# Monitor and decode control channel
op25-rx.py -f 851.0125e6 -T trunk.tsv -V
```

### Implementation Notes

- Frame sync requires bit error tolerance (allow 4 errors)
- IMBE codec requires mbelib
- Talkgroup grants are time-sensitive
- Voice channels may change mid-call

## P25 Phase 2

Extension of P25 with TDMA for increased capacity.

### Technical Specifications

- **Modulation:** H-CPM (HCPM) or H-DQPSK
- **Symbol Rate:** 6000 symbols/second
- **Voice Codec:** AMBE+2
- **Channel Bandwidth:** 12.5 kHz
- **Slots:** 2 (TDMA)
- **Capacity:** 2 voice channels per RF channel

### Differences from Phase 1

- TDMA multiplexing (2 slots)
- Different voice codec (AMBE+2 vs IMBE)
- Higher symbol rate
- More complex demodulation

### Slot Structure

```
Slot 0: Voice Channel A
Slot 1: Voice Channel B
```

Each slot:
- Duration: ~60ms
- Voice frames: 88 bits
- Interleaved with opposite slot

### Implementation Status

TrunkSDR provides framework for Phase 2:
- Control channel decoding: Supported
- TDMA demodulation: Partial
- AMBE+2 codec: Via mbelib

## Motorola SmartNet

Legacy trunking system, still widely deployed.

### Technical Specifications

- **Modulation:** FSK (Binary)
- **Baud Rates:** 3600 or 9600 bps
- **Voice:** Analog FM
- **Channel Bandwidth:** 25 kHz (legacy) or 12.5 kHz
- **Control Channel:** Continuous
- **Frame Rate:** 75ms per OSW

### Outbound Signaling Word (OSW)

**Structure (76 bits):**
```
Sync (16) | Address (10) | Group (3) | Command (11) | CRC (16) | Status (20)
```

**Sync Pattern:**
- Standard: `0x5555`
- Bit sync critical for decoding

### Command Types

- `0x300` - Group Voice Channel Grant
- `0x308` - Private Call Grant
- `0x310` - Status/Data
- `0x2F0` - Idle

### Address Field

- 10 bits = 1024 talkgroups
- Fleet/Subfleet mapping
- Dynamic ID allocation

### Frequency Calculation

```
Frequency = Base + (Channel * Spacing)
```

Typical:
- Base: 851.0125 MHz
- Spacing: 25 kHz
- Channel: from OSW

### SmartZone

Enhanced version of SmartNet:
- Multi-site capability
- Better roaming
- Dynamic channel assignment
- Compatible control channel

### Implementation Notes

- CRC-16 validation critical
- Baud rate auto-detection useful
- Voice is analog FM (easier than digital)

## EDACS

Enhanced Digital Access Communications System.

### Technical Specifications

- **Modulation:** FSK
- **Baud Rates:** 9600 or 4800 bps
- **Voice:** Analog or ProVoice digital
- **Variants:** Wide, Narrow, ProVoice

### Message Structure

**Standard EDACS:**
```
Sync (40 bits) | Command (12) | Address (20) | CRC
```

**Wide/Narrow:**
- Wide: 25 kHz spacing
- Narrow: 12.5 kHz spacing

### ProVoice

Motorola's digital voice codec for EDACS:
- **Codec:** VSELP (Vector Sum Excited Linear Prediction)
- **Proprietary:** Difficult to decode
- **Quality:** ~3.6 kbps voice

### Implementation Notes

- Less common than P25 or SmartNet
- ProVoice decoding very difficult
- Framework provided in TrunkSDR

## DMR Tier 3

Digital Mobile Radio - ETSI standard.

### Technical Specifications

- **Modulation:** 4FSK
- **Symbol Rate:** 4800 symbols/second
- **Voice Codec:** AMBE+2
- **Channel Bandwidth:** 12.5 kHz
- **Slots:** 2 (TDMA)
- **Variants:** Tier 2 (conventional), Tier 3 (trunked)

### Trunking Modes

**Capacity Plus:**
- Single-site trunking
- Rest channel carries control

**Connect Plus:**
- Multi-site trunking
- Dedicated control channel

**Linked Capacity Plus:**
- Multiple sites
- IP-based linking

### Color Codes

- 16 color codes (0-15)
- Similar to P25 NAC
- Prevents interference

### Slot Types

- Voice: AMBE+2 encoded speech
- Data: Short/Long messages
- CSBK: Control Signaling Block

### Implementation Notes

- Complex TDMA synchronization
- AMBE+2 requires mbelib or hardware decoder
- DMR framework partially implemented

## Protocol Comparison

| Feature | P25 Phase 1 | P25 Phase 2 | SmartNet | EDACS | DMR |
|---------|-------------|-------------|----------|-------|-----|
| Modulation | C4FM | H-DQPSK | FSK | FSK | 4FSK |
| Voice Codec | IMBE | AMBE+2 | Analog | Analog/ProVoice | AMBE+2 |
| Bandwidth | 12.5 kHz | 12.5 kHz | 25 kHz | 25/12.5 kHz | 12.5 kHz |
| TDMA | No | Yes (2 slot) | No | No | Yes (2 slot) |
| Open Standard | Yes | Yes | No | No | Yes |
| Deployment | Very High | Medium | High | Low | Medium |

### Decoding Difficulty

**Easy:**
- SmartNet (analog voice)
- EDACS Wide (analog voice)

**Medium:**
- P25 Phase 1

**Hard:**
- P25 Phase 2 (TDMA)
- DMR (TDMA)
- ProVoice (proprietary)

## Finding System Information

### RadioReference.com

Best source for:
- System type
- Control channels
- NAC/System ID
- Talkgroup lists
- Locations

### Scanner Databases

- Sentinel (Uniden)
- ARC (GRE/RadioShack)
- Proscan databases

### Signal Analysis

**Using rtl_power:**
```bash
rtl_power -f 850M:870M:25k -g 40 -i 1m output.csv
```

Analyze spectrum for continuous carriers (control channels).

**Using trunk-recorder:**
Study control channel traffic to identify system type.

### Field Identification

**P25:**
- Continuous control channel
- C4FM modulation (sounds like modem)
- Regular bursts every ~200ms

**SmartNet:**
- Repeating 75ms bursts
- FSK modulation
- Slower than P25

**DMR:**
- Synchronized bursts
- 4FSK modulation
- TDMA structure visible

## Decoding Tools Comparison

| Tool | Protocols | Platform | Voice Decode |
|------|-----------|----------|--------------|
| TrunkSDR | P25, SmartNet, partial others | ARM/Linux | Yes (mbelib) |
| trunk-recorder | P25 | Linux | Yes |
| OP25 | P25 | Linux | Yes |
| SDRTrunk | Many | Java/Cross | Partial |
| DSDPlus | Many | Windows | Yes (paid) |

## References

**P25:**
- TIA-102 Standard (official spec)
- OP25 Project: https://github.com/boatbod/op25

**SmartNet:**
- Scanner enthusiast documentation
- Uniden scanner manuals

**EDACS:**
- GE-Ericsson documentation (hard to find)
- Scanner forums

**DMR:**
- ETSI TS 102 361 (official spec)
- DMR Association: https://www.dmrassociation.org/

## Next Steps

- [Configuration Guide](CONFIGURATION.md)
- [Troubleshooting](TROUBLESHOOTING.md)
- [Building from Source](BUILDING.md)
