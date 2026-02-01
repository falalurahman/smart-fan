# Build Guide - ESP32-C6 vs ESP32-H2

This project supports two ESP32 chip variants with different network configurations.

## Quick Reference

| Feature | ESP32-C6 | ESP32-H2 |
|---------|----------|----------|
| **Network** | WiFi (802.11) | Thread (802.15.4) |
| **Border Router** | Not required | **Required** (HomePod Mini, Apple TV 4K) |
| **Environment** | `esp32-c6-devkitc-1` | `esp32h2-devkitm-1` |
| **Recommended For** | Most users, easy setup | Advanced users, mesh networking |
| **Commissioning** | Via WiFi + BLE | Via Thread + BLE |
| **Power** | ~100mA active | ~10-20mA active |

## Building for ESP32-C6 (WiFi)

### Build
```bash
~/.platformio/penv/bin/platformio run -e esp32-c6-devkitc-1
```

### Upload
```bash
~/.platformio/penv/bin/platformio run -e esp32-c6-devkitc-1 -t upload
```

### Monitor
```bash
~/.platformio/penv/bin/platformio device monitor -e esp32-c6-devkitc-1
```

### Requirements
- ESP32-C6 DevKit-C1 board
- 2.4GHz WiFi network (5GHz not supported)
- iPhone/iPad with iOS 16.1+ or HomePod/Apple TV

### Expected Serial Output
```
===========================================
Matter Smart Fan - WiFi Configuration
Network: WiFi (802.11)
Chip: ESP32-C6 (Dual-band WiFi + BLE)
===========================================
Connecting to Hyperoptic Fibre 91B3
...
WiFi connected
IP address: 192.168.x.x
```

## Building for ESP32-H2 (Thread)

### Build
```bash
~/.platformio/penv/bin/platformio run -e esp32h2-devkitm-1
```

### Upload
```bash
~/.platformio/penv/bin/platformio run -e esp32h2-devkitm-1 -t upload
```

### Monitor
```bash
~/.platformio/penv/bin/platformio device monitor -e esp32h2-devkitm-1
```

### Requirements
- ESP32-H2 DevKit-M1 board
- **Thread Border Router** (HomePod Mini or Apple TV 4K 2nd gen+)
- iPhone/iPad with iOS 16.1+
- Border Router must be on same network as iPhone

### Expected Serial Output
```
===========================================
Matter Smart Fan - Thread Configuration
Network: Thread (802.15.4)
Chip: ESP32-H2 (Thread/Zigbee only)
===========================================
```

### Thread Border Router Setup
1. Open Apple Home app
2. Go to Home Settings → Thread Network
3. Verify you see "Thread Border Router: [Your Device Name]"
4. If not visible, you don't have a compatible device

**Compatible Thread Border Routers:**
- HomePod Mini ($99)
- Apple TV 4K (2nd generation or later, 2021+)
- Some newer HomePod models

## GPIO Pin Configuration

Both environments use the same GPIO pins:

| Function | GPIO Pin | Direction |
|----------|----------|-----------|
| Boot Button | 9 | Input (Pull-up) |
| Fan Speed Control | 4 | Output |
| Speed Input - Low | 5 | Input (Pull-up) |
| Speed Input - Medium | 6 | Input (Pull-up) |
| Speed Input - High | 7 | Input (Pull-up) |
| Oscillation Control | 18 | Output |
| Oscillation Input | 19 | Input (Pull-up) |

## Switching Between Configurations

### Option 1: Build Specific Environment
```bash
# Build for WiFi (ESP32-C6)
pio run -e esp32-c6-devkitc-1

# Build for Thread (ESP32-H2)
pio run -e esp32h2-devkitm-1
```

### Option 2: Set Default Environment
Edit `platformio.ini` and add at the top:
```ini
[platformio]
default_envs = esp32-c6-devkitc-1  ; or esp32h2-devkitm-1
```

Then you can build without specifying `-e`:
```bash
pio run
pio run -t upload
```

## Clean Build (Recommended After Switching)

Always do a clean build when switching between environments:
```bash
# For ESP32-C6
pio run -e esp32-c6-devkitc-1 -t clean
pio run -e esp32-c6-devkitc-1

# For ESP32-H2
pio run -e esp32h2-devkitm-1 -t clean
pio run -e esp32h2-devkitm-1
```

## Troubleshooting

### ESP32-C6 (WiFi) Issues

**Problem:** "WiFi connection failed"
- ✅ Verify 2.4GHz WiFi is enabled (5GHz won't work)
- ✅ Check WiFi credentials in `main.cpp` lines 23-25
- ✅ Ensure WiFi signal is strong enough
- ✅ Check router allows new devices

**Problem:** "Device shows 'No Response' in Apple Home"
- ✅ Verify device and iPhone are on same WiFi network
- ✅ Check multicast/mDNS is enabled on router
- ✅ Restart router if needed

### ESP32-H2 (Thread) Issues

**Problem:** "Cannot commission device"
- ✅ Verify Thread Border Router is set up (Home Settings → Thread Network)
- ✅ Ensure Border Router is powered on and connected
- ✅ Check iPhone and Border Router are on same WiFi network

**Problem:** "Device commissioned but shows 'No Response'"
- ✅ Move device closer to Border Router (Thread range: 10-30m per hop)
- ✅ Restart Border Router
- ✅ Check Thread network status in Home app

**Problem:** "No Thread Border Router available"
- ❌ ESP32-H2 **requires** a Thread Border Router
- ✅ Options:
  1. Buy HomePod Mini ($99)
  2. Buy Apple TV 4K 2nd gen or later ($129+)
  3. Use ESP32-C6 with WiFi instead

## Commissioning Steps

### Both Platforms (WiFi and Thread)

1. **Flash Firmware**
   ```bash
   pio run -e [environment] -t upload
   ```

2. **Monitor Serial Output**
   ```bash
   pio device monitor -e [environment]
   ```
   Copy the manual pairing code or QR code URL

3. **Open Apple Home**
   - Tap "+" → Add Accessory
   - Scan QR code or enter pairing code manually

4. **Complete Setup**
   - Name the fan
   - Assign to a room
   - Configure automations if desired

5. **Test Functionality**
   - Speed control (Off, Low, Medium, High)
   - Oscillation toggle
   - Physical button inputs
   - Matter controller commands

## Memory Usage

Typical build sizes:

**ESP32-C6 (WiFi):**
- RAM: ~43-45% (140-145 KB / 327 KB)
- Flash: ~74-76% (2.3-2.4 MB / 3.1 MB)

**ESP32-H2 (Thread):**
- RAM: Similar to C6
- Flash: Similar to C6 (Thread stack vs WiFi stack are comparable)

## Configuration Files

The following files are shared between both environments:
- `src/main.cpp` - Main application logic
- `src/MatterMultiSpeedFan.cpp` - Matter fan implementation
- `src/MatterMultiSpeedFan.h` - Fan class definition

Network-specific code is automatically selected via preprocessor directives:
```cpp
#if CONFIG_ENABLE_MATTER_OVER_WIFI
  // WiFi-specific code (ESP32-C6)
#elif CONFIG_ENABLE_MATTER_OVER_THREAD
  // Thread-specific code (ESP32-H2)
#endif
```

## Recommended Choice

**Choose ESP32-C6 (WiFi) if:**
- ✅ You don't have a Thread Border Router
- ✅ You want easier setup and commissioning
- ✅ Device has constant power supply
- ✅ You're prototyping or developing

**Choose ESP32-H2 (Thread) if:**
- ✅ You have a HomePod Mini or Apple TV 4K
- ✅ You want lower power consumption (battery devices)
- ✅ You're building a mesh network of devices
- ✅ You want better reliability in crowded WiFi environments

## Additional Resources

- [Matter Specification](https://csa-iot.org/developer-resource/specifications-download-request/)
- [ESP-Matter Documentation](https://docs.espressif.com/projects/esp-matter/en/latest/)
- [Thread Group - What is Thread?](https://www.threadgroup.org/What-is-Thread)
- [Apple Home Architecture](https://developer.apple.com/documentation/homekit/architecture)
- Project docs: `docs/NETWORK_CONFIGURATION.md`
