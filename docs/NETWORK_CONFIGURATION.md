# Network Configuration Guide - WiFi vs Thread

## Overview

This project supports both **WiFi** and **Thread** network interfaces for Matter communication, depending on the chip you're using:

| Chip | WiFi | Thread | Recommended |
|------|------|--------|-------------|
| ESP32-C6 | ✅ Yes | ✅ Yes* | WiFi (default) |
| ESP32-C3 | ✅ Yes | ❌ No | WiFi |
| ESP32-S3 | ✅ Yes | ❌ No | WiFi |
| ESP32-H2 | ❌ No | ✅ Yes | Thread |

\* ESP32-C6 can support both, but only **one at a time** (not simultaneously)

## Current Configuration

### ESP32-C6 (WiFi Mode) - Default
```ini
[env:esp32-c6-devkitc-1]
build_flags =
    -D CONFIG_ENABLE_MATTER_OVER_WIFI=1
    -D CHIP_DEVICE_CONFIG_ENABLE_WIFI=1
```

This is the **recommended configuration** for ESP32-C6 because:
- ✅ Easier commissioning (uses existing WiFi network)
- ✅ No need for Thread Border Router
- ✅ Works with standard home routers
- ✅ Better range and throughput

### ESP32-H2 (Thread Mode)
```ini
[env:esp32h2-devkitm-1]
build_flags =
    -D CONFIG_ENABLE_MATTER_OVER_THREAD=1
    -D CHIP_DEVICE_CONFIG_ENABLE_THREAD=1
```

ESP32-H2 **only supports Thread** (no WiFi radio):
- ✅ Lower power consumption
- ✅ Mesh networking capability
- ❌ Requires Thread Border Router (HomePod Mini, Apple TV 4K, etc.)

## How to Build

### For ESP32-C6 with WiFi (Default)
```bash
# Build for ESP32-C6 with WiFi
~/.platformio/penv/bin/platformio run -e esp32-c6-devkitc-1

# Upload to ESP32-C6
~/.platformio/penv/bin/platformio run -e esp32-c6-devkitc-1 -t upload
```

### For ESP32-H2 with Thread
```bash
# Build for ESP32-H2 with Thread
~/.platformio/penv/bin/platformio run -e esp32h2-devkitm-1

# Upload to ESP32-H2
~/.platformio/penv/bin/platformio run -e esp32h2-devkitm-1 -t upload
```

## Switching Between WiFi and Thread on ESP32-C6

If you want to use **Thread instead of WiFi** on ESP32-C6:

### Option 1: Modify Build Flags
Edit [platformio.ini](../platformio.ini):

```ini
[env:esp32-c6-devkitc-1]
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D CONFIG_ENABLE_MATTER_OVER_THREAD=1  ; Change to Thread
    -D CHIP_DEVICE_CONFIG_ENABLE_THREAD=1  ; Enable Thread
    # Remove or comment out WiFi flags:
    # -D CONFIG_ENABLE_MATTER_OVER_WIFI=1
    # -D CHIP_DEVICE_CONFIG_ENABLE_WIFI=1
```

### Option 2: Create Separate Environment
Add a new environment in [platformio.ini](../platformio.ini):

```ini
[env:esp32-c6-thread]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-c6-devkitc-1
framework = arduino
board_build.partitions = huge_app.csv
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D CONFIG_ENABLE_MATTER_OVER_THREAD=1
    -D CHIP_DEVICE_CONFIG_ENABLE_THREAD=1
monitor_speed = 115200
upload_flags =
    --before=default_reset
    --after=hard_reset
```

Then build with:
```bash
~/.platformio/penv/bin/platformio run -e esp32-c6-thread
```

## How the Code Detects Network Type

The implementation uses conditional compilation to automatically detect and configure the correct network interface:

### In MatterMultiSpeedFan.cpp
```cpp
#if CONFIG_ENABLE_MATTER_OVER_WIFI || defined(CONFIG_IDF_TARGET_ESP32C6)
  log_i("Configuring Matter over WiFi");
#elif CONFIG_ENABLE_MATTER_OVER_THREAD || defined(CONFIG_IDF_TARGET_ESP32H2)
  log_i("Configuring Matter over Thread (WiFi disabled)");
#endif
```

### In main.cpp
```cpp
#if CONFIG_ENABLE_MATTER_OVER_WIFI || defined(CONFIG_IDF_TARGET_ESP32C6)
  Serial.println("Network: WiFi (802.11)");
#elif CONFIG_ENABLE_MATTER_OVER_THREAD || defined(CONFIG_IDF_TARGET_ESP32H2)
  Serial.println("Network: Thread (802.15.4)");
#endif
```

## Commissioning Differences

### WiFi Mode (ESP32-C6)

1. **Prerequisites:**
   - iPhone/iPad with iOS 16.1+ or HomePod Mini/Apple TV 4K
   - Active 2.4GHz WiFi network (5GHz not supported by Matter)

2. **Commissioning Process:**
   - Device creates BLE advertisement
   - Apple Home scans QR code via BLE
   - Device receives WiFi credentials from iPhone
   - Device connects to WiFi network
   - Commissioning completes over WiFi

3. **Serial Output:**
```
Network: WiFi (802.11)
Matter node created with WiFi support
Manual pairing code: 34970112332
QR code URL: https://...
```

### Thread Mode (ESP32-H2 or ESP32-C6)

1. **Prerequisites:**
   - iPhone/iPad with iOS 16.1+
   - **Thread Border Router** (HomePod Mini, Apple TV 4K 2nd gen or later)
   - Border Router must be on same network as iPhone

2. **Commissioning Process:**
   - Device creates BLE advertisement
   - Apple Home scans QR code via BLE
   - Device receives Thread credentials from iPhone
   - Device joins Thread network via Border Router
   - Commissioning completes over Thread

3. **Serial Output:**
```
Network: Thread (802.15.4)
Matter node created with Thread support
Manual pairing code: 34970112332
QR code URL: https://...
```

## Troubleshooting

### WiFi Mode Issues

**Problem**: Device won't commission
- ✅ Ensure 2.4GHz WiFi is enabled (5GHz-only networks won't work)
- ✅ Check WiFi signal strength (device must be in range)
- ✅ Verify no MAC filtering on router
- ✅ Try disabling AP isolation on guest networks

**Problem**: Device commissioned but shows "No Response"
- ✅ Check if device is on same network as iPhone
- ✅ Verify multicast/mDNS is enabled on router
- ✅ Try restarting router

### Thread Mode Issues

**Problem**: Device won't commission
- ✅ Verify Thread Border Router is set up
  - Open Home app → Home Settings → Thread Network
  - Should show "Thread Border Router: [Your Device]"
- ✅ Ensure Border Router is powered on and connected
- ✅ Check if Border Router and iPhone are on same WiFi network

**Problem**: Device commissioned but shows "No Response"
- ✅ Move device closer to Border Router
- ✅ Restart Border Router
- ✅ Check Thread network in Home app

**Problem**: No Thread Border Router available
- ❌ You **cannot use Thread mode** without a Border Router
- ✅ Options:
  1. Buy HomePod Mini ($99)
  2. Buy Apple TV 4K 2nd gen or later ($129+)
  3. Use WiFi mode on ESP32-C6 instead

## Network Interface Comparison

| Feature | WiFi | Thread |
|---------|------|--------|
| **Range** | 50-100m | 10-30m per hop, mesh extends range |
| **Power** | ~100mA active | ~10-20mA active |
| **Latency** | <100ms | 100-300ms |
| **Throughput** | Up to 150Mbps (Matter uses <1Mbps) | ~250kbps |
| **Mesh** | No native mesh | Yes, automatic mesh |
| **Commissioning** | Via WiFi credentials | Via Thread credentials |
| **Border Router** | Not required | **Required** |
| **Chip Support** | ESP32, C3, S3, C6 | ESP32-H2, ESP32-C6 |

## Recommendations

### When to Use WiFi (ESP32-C6)
- ✅ You don't have a Thread Border Router
- ✅ You want easier setup and commissioning
- ✅ Device has constant power supply (not battery)
- ✅ You need better range and throughput
- ✅ You're building prototype/development device

### When to Use Thread (ESP32-H2 or ESP32-C6)
- ✅ You have a HomePod Mini or Apple TV 4K
- ✅ You want lower power consumption (battery devices)
- ✅ You're building a mesh network of devices
- ✅ You want better reliability in crowded WiFi environments
- ✅ You're building commercial IoT products

## Advanced: Dual-Mode Support (Future)

Matter specification allows devices to support both WiFi and Thread, but the ESP-IDF and Arduino frameworks currently require choosing one at build time. This is a hardware limitation of how the radios are managed.

To support both modes in the same product, you would need to:
1. Build two separate firmware images (one WiFi, one Thread)
2. Provide user interface to select mode
3. Flash appropriate firmware based on selection

This is beyond the scope of this project but is technically possible.

## Build Flags Reference

### WiFi-Specific Flags
```cpp
-D CONFIG_ENABLE_MATTER_OVER_WIFI=1      // Enable Matter WiFi transport
-D CHIP_DEVICE_CONFIG_ENABLE_WIFI=1      // Enable WiFi in CHIP stack
-D CONFIG_ENABLE_WIFI_STATION=1           // ESP-IDF WiFi station mode
```

### Thread-Specific Flags
```cpp
-D CONFIG_ENABLE_MATTER_OVER_THREAD=1    // Enable Matter Thread transport
-D CHIP_DEVICE_CONFIG_ENABLE_THREAD=1    // Enable Thread in CHIP stack
-D CONFIG_OPENTHREAD_ENABLED=1            // Enable OpenThread stack
-D CONFIG_ENABLE_WIFI_STATION=0           // Disable WiFi (H2 only)
```

### Chip Detection Flags (Auto-detected)
```cpp
CONFIG_IDF_TARGET_ESP32C6   // Defined for ESP32-C6
CONFIG_IDF_TARGET_ESP32H2   // Defined for ESP32-H2
CONFIG_IDF_TARGET_ESP32     // Defined for ESP32
CONFIG_IDF_TARGET_ESP32C3   // Defined for ESP32-C3
CONFIG_IDF_TARGET_ESP32S3   // Defined for ESP32-S3
```

## Testing Checklist

### WiFi Mode (ESP32-C6)
- [ ] Build succeeds with WiFi flags
- [ ] Serial output shows "Network: WiFi (802.11)"
- [ ] Device appears in Apple Home during commissioning
- [ ] Device connects to WiFi network
- [ ] Fan controls work from Apple Home
- [ ] State persists after reopening Apple Home
- [ ] Device reconnects after power cycle

### Thread Mode (ESP32-H2)
- [ ] Build succeeds with Thread flags
- [ ] Serial output shows "Network: Thread (802.15.4)"
- [ ] Thread Border Router is visible in Home app
- [ ] Device appears during commissioning
- [ ] Device joins Thread network
- [ ] Fan controls work from Apple Home
- [ ] State persists after reopening Apple Home
- [ ] Device reconnects after power cycle

## References

- [Matter Specification - Network Interfaces](https://csa-iot.org/developer-resource/specifications-download-request/)
- [ESP-Matter WiFi Configuration](https://github.com/espressif/esp-matter/blob/main/docs/en/developing.rst)
- [ESP-Matter Thread Configuration](https://deepwiki.com/espressif/esp-matter/7.2-thread-configuration)
- [Apple Home Architecture](https://developer.apple.com/documentation/homekit/architecture)
- [Thread Group - What is Thread?](https://www.threadgroup.org/What-is-Thread)

## Summary

This implementation provides flexible network configuration:

- ✅ **WiFi mode** (default for ESP32-C6) - Easy setup, no Border Router needed
- ✅ **Thread mode** (ESP32-H2 or optional for C6) - Lower power, mesh networking
- ✅ **Conditional compilation** - Automatically detects chip and configures correctly
- ✅ **Easy switching** - Change build flags to switch between WiFi and Thread
- ✅ **Multi-platform** - Supports ESP32, C3, S3, C6, H2 with appropriate configurations

Choose the network interface that best fits your use case and hardware!
