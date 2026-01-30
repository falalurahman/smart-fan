# MatterMultiSpeedFan Implementation Guide

## Overview

This guide explains the new `MatterMultiSpeedFan` implementation which provides:

1. **Multi-Speed Control**: Discrete speed levels (Off, Low, Medium, High) instead of percentage-based control
2. **Rock/Oscillation Support**: Built-in support for fan oscillation using Matter's RockSupport and RockSetting attributes

## Comparison: Percentage vs Multi-Speed

### Original Implementation (MatterFan / MatterDimmablePlugin)

```cpp
MatterFan fan;
fan.begin(0);  // Initialize with 0% speed
fan.setSpeedPercent(75);  // Set to 75%

// Uses these Matter attributes:
// - PercentSetting (0-100)
// - PercentCurrent (0-100)
```

**Issues**:
- Controllers can set any percentage value (0-100)
- Physical fans often only support discrete speeds
- Requires mapping percentages to discrete levels
- No native oscillation support

### New Implementation (MatterMultiSpeedFan)

```cpp
MatterMultiSpeedFan fan;
fan.begin(3, ROCK_LEFT_RIGHT);  // 3 speed levels, supports left-right rocking
fan.setSpeed(FAN_SPEED_HIGH);  // Set to high speed (level 3)
fan.setRockSetting(ROCK_LEFT_RIGHT);  // Enable oscillation

// Uses these Matter attributes:
// - SpeedMax (defines maximum speed level)
// - SpeedSetting (0 to SpeedMax)
// - SpeedCurrent (0 to SpeedMax)
// - RockSupport (bitmap of supported directions)
// - RockSetting (bitmap of active directions)
```

**Advantages**:
- Controllers see discrete speed options (Off, Low, Medium, High)
- Matches physical fan capabilities
- Native oscillation support via Rock attributes
- No ambiguous percentage-to-speed mapping

## Matter Specification Details

### FanControl Cluster Attributes

| Attribute | ID | Type | Description |
|-----------|-----|------|-------------|
| SpeedMax | 0x0004 | uint8 | Maximum speed level (read-only) |
| SpeedSetting | 0x0005 | uint8 | Desired speed (0 to SpeedMax) |
| SpeedCurrent | 0x0006 | uint8 | Current speed (0 to SpeedMax) |
| RockSupport | 0x0007 | bitmap8 | Supported rock directions (read-only) |
| RockSetting | 0x0008 | bitmap8 | Active rock directions |

### RockSupport Bitmap (0x0007)

| Bit | Value | Description |
|-----|-------|-------------|
| 0 | 0x01 | Supports left-right rocking |
| 1 | 0x02 | Supports up-down rocking |
| 2 | 0x04 | Supports circular/round rocking |
| 3-7 | - | Reserved |

### RockSetting Bitmap (0x0008)

| Bit | Value | Description |
|-----|-------|-------------|
| 0 | 0x01 | Left-right rocking enabled |
| 1 | 0x02 | Up-down rocking enabled |
| 2 | 0x04 | Circular/round rocking enabled |
| 3-7 | - | Reserved |

## Usage Examples

### Basic Setup

```cpp
#include <MatterMultiSpeedFan.h>

MatterMultiSpeedFan fan;

void setup() {
  // Initialize with 3 speed levels (0=Off, 1=Low, 2=Medium, 3=High)
  // and support for left-right oscillation
  fan.begin(3, ROCK_LEFT_RIGHT);

  // Register callbacks
  fan.onChangeSpeed(onSpeedChange);
  fan.onChangeRock(onRockChange);

  Matter.begin();
}

bool onSpeedChange(uint8_t speed) {
  Serial.printf("Speed changed to: %d\r\n", speed);
  // Update your physical fan hardware here
  return true;  // Return true on success
}

bool onRockChange(uint8_t rockSetting) {
  Serial.printf("Rock setting: 0x%02X\r\n", rockSetting);
  // Update your oscillation motor here
  return true;  // Return true on success
}
```

### Setting Speed Levels

```cpp
// Set to low speed
fan.setSpeed(FAN_SPEED_LOW);

// Set to high speed
fan.setSpeed(FAN_SPEED_HIGH);

// Turn off
fan.setSpeed(FAN_SPEED_OFF);

// Or use convenience methods
fan.setOnOff(true);   // Turn on (sets to low if currently off)
fan.setOnOff(false);  // Turn off
fan.toggle();         // Toggle on/off
```

### Controlling Oscillation

```cpp
// Enable left-right oscillation
fan.setRockSetting(ROCK_LEFT_RIGHT);

// Disable oscillation
fan.setRockSetting(0);

// Check if oscillating
if (fan.isRocking()) {
  Serial.println("Fan is oscillating");
}

// Get current rock setting
uint8_t rockSetting = fan.getRockSetting();

// Get supported rock directions
uint8_t rockSupport = fan.getRockSupport();
```

### Button Control Example

```cpp
void loop() {
  // Button press detected
  if (buttonPressed()) {
    uint8_t currentSpeed = fan.getSpeed();
    uint8_t newSpeed;

    // Cycle through speeds: OFF -> LOW -> MEDIUM -> HIGH -> OFF
    switch (currentSpeed) {
      case FAN_SPEED_OFF:
        newSpeed = FAN_SPEED_LOW;
        break;
      case FAN_SPEED_LOW:
        newSpeed = FAN_SPEED_MEDIUM;
        break;
      case FAN_SPEED_MEDIUM:
        newSpeed = FAN_SPEED_HIGH;
        break;
      default:
        newSpeed = FAN_SPEED_OFF;
        break;
    }

    // Update Matter attributes (triggers reports to controllers)
    fan.setSpeed(newSpeed);

    // Update physical hardware
    setPhysicalFanSpeed(newSpeed);
  }
}
```

## Controller Behavior

### Apple Home

Apple Home displays fans with discrete speed options:

- **Off**: Speed level 0
- **Low**: Speed level 1 (33%)
- **Medium**: Speed level 2 (66%)
- **High**: Speed level 3 (100%)

When you use `MatterMultiSpeedFan`:
- Apple Home shows 4 distinct speed options (including Off)
- No slider for arbitrary percentages
- Oscillation appears as a separate toggle (if supported)
- State is retained when closing/reopening the app (if properly implemented)

### Home Assistant

Home Assistant shows:

```yaml
fan.smart_fan:
  state: "on"
  speed: 2
  percentage: 66
  preset_mode: null
  oscillating: true
```

- `speed`: Raw speed level (0-3)
- `percentage`: Calculated as `(speed / speedMax) * 100`
- `oscillating`: True if any rock bit is set

## Key Implementation Details

### Attribute Updates

**When speed changes locally (button/sensor)**:

```cpp
// ✅ CORRECT - Updates Matter attributes and triggers reports
fan.setSpeed(newSpeed);

// ❌ WRONG - Only updates local state, controllers don't get notified
currentSpeed = newSpeed;
```

**Why this matters**: When you update using `setSpeed()` with `performUpdate=true` (default), it:
1. Updates the Matter attribute in the cluster
2. Calls `attribute::report()` to notify all controllers
3. Controllers (Apple Home, Home Assistant) receive the update immediately
4. State is synchronized across all devices

### Callback Return Values

```cpp
bool onSpeedChange(uint8_t speed) {
  // Update hardware
  setPhysicalFanSpeed(speed);

  // Return true if hardware update succeeded
  // Return false if there was an error
  return true;
}
```

If you return `false`:
- The Matter attribute will NOT be updated
- Controllers will see the change as failed
- Previous state is retained

### Speed Level Mapping

For a fan with `speedMax = 3`:

| Speed Level | Enum | Typical Name | Apple Home Display |
|-------------|------|--------------|-------------------|
| 0 | FAN_SPEED_OFF | Off | Off |
| 1 | FAN_SPEED_LOW | Low | Low (33%) |
| 2 | FAN_SPEED_MEDIUM | Medium | Medium (66%) |
| 3 | FAN_SPEED_HIGH | High | High (100%) |

You can support more levels:

```cpp
// 5 speed levels: Off, Level 1-4
fan.begin(4, ROCK_LEFT_RIGHT);
```

Apple Home will show: Off, 25%, 50%, 75%, 100%

## Troubleshooting

### Issue: Apple Home doesn't retain state

**Cause**: Not updating Matter attributes when changing speed locally

**Solution**:
```cpp
// When button pressed or sensor triggered
fan.setSpeed(newSpeed);  // This updates Matter and triggers reports
```

### Issue: Oscillation toggle doesn't appear in Apple Home

**Cause**:
1. RockSupport not set during initialization
2. RockSetting attribute not being reported

**Solution**:
```cpp
// Ensure rock support is specified
fan.begin(3, ROCK_LEFT_RIGHT);

// When toggling oscillation
fan.setRockSetting(ROCK_LEFT_RIGHT);  // Enable
fan.setRockSetting(0);                // Disable
```

### Issue: Speed changes slowly or unreliably

**Cause**: Using `setSpeed()` in callbacks

**Solution**:
```cpp
bool onSpeedChange(uint8_t speed) {
  // ✅ CORRECT - Just update hardware
  setPhysicalFanSpeed(speed);
  return true;

  // ❌ WRONG - Don't call setSpeed() here (causes recursion)
  // fan.setSpeed(speed);
}
```

### Issue: Controllers show wrong speed after reboot

**Cause**: Not calling `updateAccessory()` after commissioning

**Solution**:
```cpp
if (Matter.isDeviceCommissioned()) {
  fan.updateAccessory();  // Sync local state with Matter attributes

  // Update hardware to match Matter state
  setPhysicalFanSpeed(fan.getSpeed());
  setPhysicalOscillation(fan.isRocking());
}
```

## Migration from MatterFan

If you're migrating from the percentage-based `MatterFan`:

### Step 1: Replace includes and declarations

```cpp
// Before
#include <MatterEndpoints/MatterFan.h>
MatterFan fan;

// After
#include <MatterMultiSpeedFan.h>
MatterMultiSpeedFan fan;
```

### Step 2: Update initialization

```cpp
// Before
fan.begin(0);  // Start at 0%

// After
fan.begin(3, ROCK_LEFT_RIGHT);  // 3 speeds, oscillation support
```

### Step 3: Update speed control

```cpp
// Before
fan.setSpeedPercent(66);  // Set to 66%
uint8_t speed = fan.getSpeedPercent();

// After
fan.setSpeed(FAN_SPEED_MEDIUM);  // Set to medium (level 2)
uint8_t speed = fan.getSpeed();
```

### Step 4: Update callbacks

```cpp
// Before
fan.onChangeSpeedPercent([](uint8_t percent) {
  Serial.printf("Speed: %d%%\r\n", percent);
  // Map to discrete levels
  uint8_t level = percent / 33;
  setPhysicalFanSpeed(level);
  return true;
});

// After
fan.onChangeSpeed([](uint8_t speed) {
  Serial.printf("Speed: %d\r\n", speed);
  setPhysicalFanSpeed(speed);  // Direct level, no mapping needed
  return true;
});
```

### Step 5: Add oscillation support

```cpp
// New feature - add oscillation callback
fan.onChangeRock([](uint8_t rockSetting) {
  bool oscillating = (rockSetting & ROCK_LEFT_RIGHT) != 0;
  Serial.printf("Oscillation: %s\r\n", oscillating ? "ON" : "OFF");
  setPhysicalOscillation(oscillating);
  return true;
});
```

## Testing Checklist

- [ ] Commission device in Apple Home
- [ ] Change speed from Apple Home → verify callback is called
- [ ] Change speed via button → verify Apple Home updates immediately
- [ ] Close and reopen Apple Home → verify state is retained
- [ ] Toggle oscillation from Apple Home → verify motor activates
- [ ] Toggle oscillation via button → verify Apple Home updates
- [ ] Reboot ESP32 → verify state persists after recommissioning
- [ ] Test in Home Assistant → verify discrete speed levels appear
- [ ] Check Home Assistant logs for attribute reports

## Advanced Topics

### Custom Speed Level Names

For fans with non-standard speeds:

```cpp
// Fan with 5 speeds: Off, Eco, Low, Medium, High, Turbo
fan.begin(5, ROCK_LEFT_RIGHT);

// Map to your hardware
void setPhysicalFanSpeed(uint8_t speed) {
  switch (speed) {
    case 0: // Off
      break;
    case 1: // Eco
      break;
    case 2: // Low
      break;
    case 3: // Medium
      break;
    case 4: // High
      break;
    case 5: // Turbo
      break;
  }
}
```

### Multiple Oscillation Types

```cpp
// Support multiple rock directions
fan.begin(3, ROCK_LEFT_RIGHT | ROCK_UP_DOWN);

// Enable both
fan.setRockSetting(ROCK_LEFT_RIGHT | ROCK_UP_DOWN);

// Enable only left-right
fan.setRockSetting(ROCK_LEFT_RIGHT);

// Check specific direction
if (fan.getRockSetting() & ROCK_UP_DOWN) {
  Serial.println("Up-down rocking active");
}
```

### Debugging Attribute Updates

Enable detailed logging:

```cpp
void loop() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 5000) {
    lastLog = millis();

    Serial.printf("Fan State:\r\n");
    Serial.printf("  Speed: %d / %d\r\n", fan.getSpeed(), fan.getSpeedMax());
    Serial.printf("  On/Off: %d\r\n", fan.getOnOff());
    Serial.printf("  Rock Support: 0x%02X\r\n", fan.getRockSupport());
    Serial.printf("  Rock Setting: 0x%02X\r\n", fan.getRockSetting());
    Serial.printf("  Is Rocking: %d\r\n", fan.isRocking());
  }
}
```

## API Reference

### Initialization

```cpp
bool begin(uint8_t speedMax = 3, uint8_t rockSupport = ROCK_LEFT_RIGHT)
```

**Parameters**:
- `speedMax`: Maximum speed level (default: 3 for Off, Low, Medium, High)
- `rockSupport`: Bitmap of supported rock directions (default: ROCK_LEFT_RIGHT)

**Returns**: `true` on success, `false` on failure

### Speed Control

```cpp
bool setSpeed(uint8_t speed, bool performUpdate = true)
uint8_t getSpeed()
uint8_t getSpeedMax()
```

### On/Off Control

```cpp
bool setOnOff(bool onOff, bool performUpdate = true)
bool getOnOff()
bool toggle(bool performUpdate = true)
```

### Oscillation Control

```cpp
bool setRockSetting(uint8_t rockSetting, bool performUpdate = true)
uint8_t getRockSetting()
uint8_t getRockSupport()
bool isRocking()
```

### Callbacks

```cpp
void onChangeSpeed(SpeedChangeCallback cb)
void onChangeRock(RockChangeCallback cb)
```

**Callback Signatures**:
```cpp
bool SpeedChangeCallback(uint8_t speed)
bool RockChangeCallback(uint8_t rockSetting)
```

### Utility Methods

```cpp
void updateAccessory()  // Sync local state with Matter attributes
MatterMultiSpeedFan& operator=(uint8_t speed)  // Allow fan = 2 syntax
```

## References

- [Matter Specification 1.3 - Fan Control Cluster](https://csa-iot.org/developer-resource/specifications-download-request/)
- [ESP Matter Documentation](https://docs.espressif.com/projects/esp-matter/en/latest/)
- [Arduino ESP32 Matter Examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples)
