# TETRA Decryption Educational Guide

**⚠️ CRITICAL: This document is for EDUCATIONAL PURPOSES ONLY ⚠️**

**LEGAL WARNING:** Unauthorized interception and decryption of communications is **ILLEGAL** in most jurisdictions. This guide is provided solely for:
- Academic education in cryptography and radio security
- Authorized penetration testing with written permission
- Security research on systems you own or control
- Understanding vulnerabilities to improve security

**Users are SOLELY RESPONSIBLE for compliance with all applicable laws.**

---

## Table of Contents

1. [Introduction](#introduction)
2. [TETRA:BURST Vulnerabilities](#tetraburst-vulnerabilities)
3. [TEA1 Backdoor (CVE-2022-24402)](#tea1-backdoor-cve-2022-24402)
4. [Technical Implementation](#technical-implementation)
5. [Using the Decryption Tools](#using-the-decryption-tools)
6. [Legal Considerations by Jurisdiction](#legal-considerations-by-jurisdiction)
7. [Ethical Guidelines](#ethical-guidelines)
8. [References and Further Reading](#references-and-further-reading)

---

## Introduction

### What is TETRA?

**TETRA** (Terrestrial Trunked Radio) is a professional digital mobile radio standard used globally by:
- Emergency services (police, fire, ambulance)
- Military and defense organizations
- Transportation systems
- Utilities and critical infrastructure

TETRA provides:
- Encrypted voice and data communications
- Trunked radio operation
- Group calling capabilities
- Robust coverage and reliability

### TETRA Encryption Algorithms

TETRA supports multiple encryption algorithms:

| Algorithm | Key Size | Status | Vulnerability |
|-----------|----------|--------|---------------|
| **TEA1** | 80 bits (nominal) | **VULNERABLE** | CVE-2022-24402: Intentional backdoor reduces effective key to 32 bits |
| **TEA2** | 80 bits | **SECURE** | No known vulnerabilities |
| **TEA3** | 128 bits | **SECURE** | No known vulnerabilities |
| **TEA4** | 256 bits | **SECURE** | No known vulnerabilities |

### Why This Matters

The discovery of vulnerabilities in TEA1 has significant implications:
1. **Commercial systems** often use TEA1 due to export restrictions on stronger algorithms
2. **Older deployments** may still rely on TEA1
3. **Critical infrastructure** communications may be at risk
4. **Security awareness** is essential for upgrading vulnerable systems

---

## TETRA:BURST Vulnerabilities

### Discovery Timeline

- **December 2021**: Midnight Blue discovers vulnerabilities, reports to Dutch NCSC
- **December 2021 - July 2023**: Responsible disclosure period (1.5 years)
- **July 24, 2023**: Public disclosure of TETRA:BURST
- **August 2023**: Technical details presented at Black Hat, DEF CON, and USENIX Security

### The Five Vulnerabilities

Midnight Blue identified **five vulnerabilities**, two deemed critical:

#### CVE-2022-24402 (Critical) - TEA1 Backdoor
- **Severity**: Critical (CVSS score: 9.8)
- **Description**: Intentional backdoor in TEA1 algorithm
- **Impact**: 80-bit key effectively reduced to 32 bits
- **Exploitation**: Key recovery possible in ~1 minute on consumer laptop
- **Quote from researchers**: *"The vulnerability in the TEA1 cipher is obviously the result of intentional weakening."*

#### CVE-2022-24401 (High) - Authentication Bypass
- **Severity**: High
- **Description**: Weakness in air interface encryption key derivation
- **Impact**: Possible impersonation attacks

#### Three Additional Vulnerabilities (Medium severity)
- Various implementation and protocol weaknesses
- Details available in Midnight Blue's published research

### What Is NOT Vulnerable

**Important**: TEA2, TEA3, and TEA4 are **NOT vulnerable** to these attacks and remain secure.

---

## TEA1 Backdoor (CVE-2022-24402)

### Technical Background

TEA1 was designed with an **intentional backdoor** that allows key recovery:

1. **Nominal Key Size**: 80 bits (should provide 2^80 security level)
2. **Actual Key Size**: Effectively 32 bits (2^32 security level)
3. **Key Derivation**: The 80-bit key is derived from a 32-bit seed in a predictable manner
4. **Brute Force**: 2^32 keys can be tested in reasonable time

### Attack Complexity

**Time Complexity**: O(2^32) operations (approximately 4 billion attempts)

**Hardware Requirements**: Consumer-grade laptop or desktop
- Modern CPU can test ~100 million keys/second
- Estimated time: **30-90 seconds** for average case
- Worst case: **~2 minutes**

**Software Requirements**:
- Implementation of TEA1 algorithm (reverse-engineered from public sources)
- Known-plaintext or ciphertext-only attack capability

### Attack Methodology

#### Ciphertext-Only Attack
1. Capture TEA1 encrypted TETRA burst
2. Iterate through 2^32 possible keys
3. For each key:
   - Decrypt the ciphertext
   - Verify if plaintext matches TETRA structure
4. Valid key identified by correct plaintext structure

#### Known-Plaintext Attack (Faster)
1. Obtain known plaintext-ciphertext pair
2. Iterate through 2^32 possible keys
3. For each key:
   - Encrypt known plaintext
   - Compare with captured ciphertext
4. Matching key found when ciphertext matches

### Verification of Decryption

Valid TETRA plaintext has specific characteristics:
- **PDU Type**: First byte contains valid MAC PDU identifier (0x00-0x0F)
- **Field Structure**: Known bit patterns for TETRA MAC layer
- **CRC**: Valid cyclic redundancy check
- **Protocol Consistency**: Values within expected ranges

---

## Technical Implementation

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  TETRA Decryption System                     │
└─────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴─────────────┐
                │                           │
        ┌───────▼────────┐        ┌────────▼────────┐
        │ Main Decoder   │        │  Interceptor    │
        │   (TrunkSDR)   │        │   (Standalone)  │
        └───────┬────────┘        └────────┬────────┘
                │                           │
                └──────────┬────────────────┘
                           │
                  ┌────────▼──────────┐
                  │  TETRA Crypto     │
                  │   Module          │
                  └────────┬──────────┘
                           │
            ┌──────────────┼──────────────┐
            │              │              │
     ┌──────▼─────┐  ┌────▼─────┐  ┌────▼──────┐
     │ Encryption │  │   Key    │  │  Legal    │
     │  Detection │  │ Recovery │  │  Checker  │
     └────────────┘  └──────────┘  └───────────┘
```

### Components

#### 1. TETRA Crypto Module (`tetra_crypto.h/cpp`)
- Encryption algorithm detection
- TEA1 key recovery (exploiting CVE-2022-24402)
- TEA1 decryption
- Key caching for performance

#### 2. Interceptor Tool (`tetra_decrypt_interceptor`)
- Standalone program for decryption tasks
- Live mode: Real-time interception from RTL-SDR
- File mode: Decrypt captured traffic
- Key management and caching

#### 3. Legal Compliance Checker
- **MANDATORY** user acknowledgment before use
- Creates authorization file after acknowledgment
- Displays comprehensive legal warnings
- Can be revoked by deleting authorization file

### Code Structure

```cpp
// Example usage of TETRA crypto module
#include "tetra_crypto.h"

TETRACrypto crypto;

// 1. Check authorization (REQUIRED)
if (!TETRACryptoLegalChecker::checkAuthorization()) {
    // User declined or no authorization
    exit(1);
}

// 2. Detect encryption type
TETRAEncryptionAlgorithm algo = crypto.detectEncryption(burst_data, length);

if (algo == TETRAEncryptionAlgorithm::TEA1) {
    // 3. Attempt key recovery
    auto key_result = crypto.recoverTEA1Key(burst_data, length);

    if (key_result.success) {
        // 4. Decrypt with recovered key
        auto decrypt_result = crypto.decryptTEA1(
            burst_data, length, key_result.recovered_key
        );

        if (decrypt_result.success) {
            // Use decrypted plaintext
            processPlaintext(decrypt_result.plaintext);
        }
    }
}
```

---

## Using the Decryption Tools

### Prerequisites

1. **Legal Authorization**: You MUST have explicit authorization to intercept and decrypt communications
2. **Hardware**: RTL-SDR dongle (for live monitoring)
3. **Software**: TrunkSDR compiled with TETRA decryption support

### Building with Decryption Support

```bash
cd TrunkedSdr
mkdir build && cd build

cmake .. \
  -DENABLE_EUROPEAN_PROTOCOLS=ON \
  -DENABLE_TETRA=ON \
  -DENABLE_TETRA_DECRYPTION=ON \
  -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)
sudo make install
```

### Using the Interceptor Tool

#### Example 1: Decrypt Captured File

```bash
# Process a captured TETRA file with automatic key recovery
tetra_decrypt_interceptor \
  --mode file \
  --input captured_traffic.bin \
  --output decrypted_traffic.bin \
  --auto-recover \
  --key-cache keys.cache

# First run: Displays legal warning, requires acknowledgment
# Subsequent runs: Uses cached authorization
```

#### Example 2: Live Monitoring (Educational Lab Only)

```bash
# Monitor a test TETRA network in controlled environment
tetra_decrypt_interceptor \
  --mode live \
  --frequency 382612500 \
  --mcc 234 \
  --mnc 14 \
  --auto-recover \
  --key-cache keys.cache \
  --output live_decrypt.bin
```

### First-Time Use Flow

```
┌─────────────────────────────────────────────────────┐
│  Launch tetra_decrypt_interceptor                   │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
         ┌───────────────┐
         │ Authorization  │  ◄── Check ~/.trunksdr_tetra_crypto_authorized
         │   Check       │
         └───────┬───────┘
                 │
         ┌───────┴────────┐
         │ Not Found      │ Found ──────────┐
         ▼                                   │
┌─────────────────┐                         │
│ Display Legal   │                         │
│    Warning      │                         │
└────────┬────────┘                         │
         │                                   │
         ▼                                   │
  ┌──────────────┐                          │
  │ User Prompt  │                          │
  │  (yes/no)    │                          │
  └──────┬───────┘                          │
         │                                   │
    ┌────┴────┐                             │
    │   No    │ Yes                         │
    ▼         ▼                             │
┌────────┐ ┌─────────────────┐             │
│  Exit  │ │ Confirmation    │             │
└────────┘ │ "I ACCEPT..."   │             │
           └────────┬────────┘             │
                    │                       │
             ┌──────┴──────┐               │
             │ No          │ Yes           │
             ▼             ▼               │
          ┌────────┐  ┌──────────────┐    │
          │  Exit  │  │ Create Auth  │    │
          └────────┘  │    File      │    │
                      └──────┬───────┘    │
                             │             │
                             └─────┬───────┘
                                   │
                                   ▼
                         ┌──────────────────┐
                         │ Start Operation  │
                         └──────────────────┘
```

### Key Cache Management

The key cache stores recovered TEA1 keys for faster subsequent decryption:

```bash
# View cached keys
cat keys.cache

# Format: network_id,talkgroup,key_hex
# Example entries:
234014,1001,0xABCD1234
234014,1002,0x12345678

# Clear key cache (force re-recovery)
rm keys.cache
```

---

## Legal Considerations by Jurisdiction

### ⚠️ CRITICAL: Know Your Local Laws ⚠️

Laws regarding radio interception vary significantly by country. **This is not legal advice** - consult a lawyer before using these tools.

### United States

**Relevant Laws**:
- **18 U.S.C. § 2511** - Interception and disclosure of wire, oral, or electronic communications
- **18 U.S.C. § 2701** - Unlawful access to stored communications
- **47 U.S.C. § 605** - Unauthorized publication or use of communications

**Key Points**:
- Interception of radio communications is generally illegal without consent
- Even if you can receive, **acting upon** intercepted information is illegal
- Disclosure of intercepted communications is illegal
- Penalties: Up to 5 years imprisonment, fines

**Legal Uses**:
- ✓ Monitoring amateur radio (with proper license)
- ✓ Authorized penetration testing with written permission
- ✓ Law enforcement with proper warrant
- ✓ Educational demonstrations in controlled environments (simulated traffic)

### United Kingdom

**Relevant Laws**:
- **Regulation of Investigatory Powers Act 2000 (RIPA)**
- **Wireless Telegraphy Act 2006**
- **Computer Misuse Act 1990**

**Key Points**:
- Legal to **receive** radio transmissions (including encrypted)
- **Illegal** to act upon intercepted information
- **Illegal** to disclose intercepted communications
- Penalties: Up to 2 years imprisonment

**Special Case - Airwave (Emergency Services)**:
- Monitoring control channels: Generally tolerated for educational purposes
- Decrypting voice: **ILLEGAL** under RIPA
- Using information: **ILLEGAL** under RIPA

### European Union

**Relevant Regulations**:
- **GDPR (General Data Protection Regulation)** - Applies to intercepted personal data
- **Directive 2002/58/EC** (ePrivacy Directive) - Electronic communications privacy
- **National laws** - Each member state has specific interception laws

**Key Points**:
- GDPR applies to any personal data obtained through interception
- Penalties can be severe: Up to €20 million or 4% of global turnover
- Unauthorized decryption often violates both criminal and data protection law

**By Country**:

| Country | Receiving Legal? | Decryption Legal? | Disclosure Legal? | Notes |
|---------|------------------|-------------------|-------------------|-------|
| Germany | ✓ (Generally) | ✗ | ✗ | TKG § 148 restricts monitoring |
| France | ~ (Restricted) | ✗ | ✗ | Monitoring public safety restricted |
| Netherlands | ✓ (Hobbyist) | ✗ | ✗ | Generally tolerant for hobbyists |
| Belgium | ✓ | ✗ | ✗ | Must not act on information |
| Spain | ✓ | ✗ | ✗ | Disclosure prohibited |

### Australia

**Relevant Laws**:
- **Telecommunications (Interception and Access) Act 1979**
- **Radiocommunications Act 1992**

**Key Points**:
- Receiving encrypted communications without authorization is illegal
- Penalties: Up to 2 years imprisonment

### Other Jurisdictions

Most jurisdictions have similar restrictions. **Always check local laws before use.**

---

## Ethical Guidelines

### Responsible Use Principles

Even when legally authorized, ethical considerations apply:

#### 1. Authorization First
- ✓ Always obtain **written authorization** before testing
- ✓ Define clear scope of testing
- ✓ Document all activities
- ✗ Never test "just to see if it works"

#### 2. Minimize Harm
- ✓ Use only for improving security
- ✓ Report vulnerabilities responsibly
- ✗ Never interfere with emergency communications
- ✗ Never use information obtained through interception

#### 3. Responsible Disclosure
- ✓ Report vulnerabilities to system owners
- ✓ Allow time for remediation before public disclosure
- ✓ Coordinate with security teams
- ✗ Never publish exploits without vendor notification

#### 4. Educational Purpose
- ✓ Use for learning about security vulnerabilities
- ✓ Demonstrate in controlled environments
- ✓ Use simulated traffic when possible
- ✗ Never use real emergency services traffic for demonstrations

### Example Authorized Use Cases

#### ✓ Acceptable: University Research Lab
```
Scenario: University professor teaching radio security course
Authorization: Simulated TETRA network in shielded lab environment
Purpose: Demonstrate vulnerabilities to students
Legal: Yes - no real communications intercepted
Ethical: Yes - educational purpose, controlled environment
```

#### ✓ Acceptable: Penetration Testing
```
Scenario: Security consultant testing client's TETRA system
Authorization: Written contract with client, defined scope
Purpose: Assess security of client's communications
Legal: Yes - explicit authorization from system owner
Ethical: Yes - improving client's security
```

#### ✗ Unacceptable: "Research" on Live Network
```
Scenario: Individual testing tool on local emergency services
Authorization: None
Purpose: "Just curious" / "proving it works"
Legal: NO - unauthorized interception
Ethical: NO - no legitimate purpose, could interfere with emergency services
```

---

## References and Further Reading

### Primary Research

1. **Midnight Blue - TETRA:BURST**
   - Website: https://www.midnightblue.nl/research/tetraburst
   - Published: July 24, 2023
   - Authors: Carlo Meijer, Wouter Bokslag, Jos Wetzels

2. **CVE-2022-24402**
   - NVD: https://nvd.nist.gov/vuln/detail/CVE-2022-24402
   - CVSS Score: 9.8 (Critical)
   - Description: TEA1 intentional backdoor

### Conference Presentations

3. **Black Hat USA 2023**
   - "TETRA:BURST - Breaking TETRA Radio Encryption"
   - August 2023, Las Vegas

4. **USENIX Security 2023**
   - Peer-reviewed academic presentation
   - August 2023

5. **DEF CON 31**
   - Public security conference presentation
   - August 2023

### Standards and Specifications

6. **ETSI EN 300 392** - TETRA Standard
   - Part 1: General network design
   - Part 2: Air Interface (AI)
   - Part 7: Security

7. **ETSI EN 300 392-7** - TETRA Security
   - Encryption algorithms (classified sections)
   - Key management
   - Authentication

### Legal Resources

8. **NIST Cybersecurity Framework**
   - Guidelines for authorized security testing

9. **OWASP Testing Guide**
   - Penetration testing methodologies

10. **Council of Europe Convention on Cybercrime**
    - International legal framework

### Technical Background

11. **"On the Security of TEA"** - Various Academic Papers
    - Cryptanalysis of TEA family algorithms

12. **RTL-SDR Documentation**
    - https://www.rtl-sdr.com/
    - Hardware for TETRA reception

13. **Osmocom TETRA Project**
    - https://osmocom.org/projects/tetra
    - Open-source TETRA protocol analysis

### Security Best Practices

14. **NIST SP 800-115** - Technical Guide to Information Security Testing and Assessment

15. **PTES (Penetration Testing Execution Standard)**
    - http://www.pentest-standard.org/

---

## Conclusion

The TETRA:BURST vulnerabilities represent a significant security issue affecting critical infrastructure worldwide. Understanding these vulnerabilities is essential for:

1. **Security Professionals**: Assessing risk in deployed systems
2. **System Operators**: Planning upgrades from TEA1 to secure algorithms
3. **Researchers**: Advancing radio security knowledge
4. **Students**: Learning about cryptographic vulnerabilities

**However, this knowledge comes with serious legal and ethical responsibilities:**

- ✓ Use only with proper authorization
- ✓ Respect privacy and laws
- ✓ Contribute to improving security
- ✗ Never intercept real communications without authorization
- ✗ Never disclose intercepted information
- ✗ Never interfere with emergency services

**The goal of this software is EDUCATION and AUTHORIZED SECURITY RESEARCH to make communication systems more secure, not to enable illegal surveillance.**

---

## Disclaimer

**This document and associated software are provided for EDUCATIONAL PURPOSES ONLY.**

The authors and contributors:
- **DO NOT** condone illegal interception of communications
- **DO NOT** take responsibility for misuse of this information
- **DO NOT** provide legal advice

**Users are SOLELY RESPONSIBLE for:**
- Compliance with all applicable laws
- Obtaining proper authorization
- Ethical use of these tools
- Any legal consequences of their actions

**If you have any doubt about the legality of your intended use, DO NOT PROCEED. Consult a lawyer first.**

---

**Last Updated**: 2025-11-17
**Version**: 1.0
**For**: TrunkSDR TETRA Decryption Module
