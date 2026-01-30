# MatterMultiSpeedFan Implementation Notes

## Problem Solved: Matter Node Initialization

### The Challenge
The original implementation tried to call `ArduinoMatter::_init()`, which is a protected method only accessible to friend classes declared in `Matter.h`. Since `MatterMultiSpeedFan` is not a built-in Arduino-Matter class, it doesn't have access to this protected method.

### The Solution
Instead of modifying the Arduino library (which would be overwritten on updates), we create the Matter node directly using esp-matter functions in the `MatterMultiSpeedFan` class.

## How Matter Node Creation Works

### Traditional Arduino-Matter Approach
```cpp
// In MatterFan (built-in class)
bool MatterFan::begin(...) {
  ArduinoMatter::_init();  // ✅ Works - MatterFan is a friend class
  // ...
}
```

### Our Custom Approach
```cpp
// In MatterMultiSpeedFan (custom class)
bool MatterMultiSpeedFan::begin(...) {
  // Create node directly using esp-matter API
  if (_matter_node == nullptr) {
    node::config_t node_config;
    _matter_node = node::create(
      &node_config,
      multi_speed_fan_attribute_update_cb,  // Our callback
      multi_speed_fan_identification_cb      // Our callback
    );
  }
  // ...
}
```

## Key Implementation Details

### 1. Static Node Pointer
```cpp
static node_t *_matter_node = nullptr;
```
- Shared across all instances
- Created once on first `begin()` call
- Persists for the lifetime of the program

### 2. Custom Callbacks
We provide our own callbacks instead of using Arduino-Matter's built-in ones:

**Attribute Update Callback:**
```cpp
static esp_err_t multi_speed_fan_attribute_update_cb(
  attribute::callback_type_t type, uint16_t endpoint_id,
  uint32_t cluster_id, uint32_t attribute_id,
  esp_matter_attr_val_t *val, void *priv_data
) {
  MatterMultiSpeedFan *fan = (MatterMultiSpeedFan *)priv_data;

  switch (type) {
    case attribute::PRE_UPDATE:
      // Call our class's attributeChangeCB before the value changes
      return fan->attributeChangeCB(endpoint_id, cluster_id, attribute_id, val)
             ? ESP_OK : ESP_FAIL;
    // ... handle other cases
  }
}
```

**Identification Callback:**
```cpp
static esp_err_t multi_speed_fan_identification_cb(
  identification::callback_type_t type, uint16_t endpoint_id,
  uint8_t effect_id, uint8_t effect_variant, void *priv_data
) {
  // Handle identification requests (e.g., blink LED)
  return ESP_OK;
}
```

### 3. Manual Attribute Creation
Since the `fan::config_t` doesn't include advanced attributes, we create them manually:

```cpp
// Get the fan control cluster
cluster_t *cluster = cluster::get(endpoint, FanControl::Id);

// Add SpeedMax attribute (0x0004)
esp_matter_attr_val_t speed_max_val = esp_matter_invalid(NULL);
speed_max_val.type = ESP_MATTER_VAL_TYPE_UINT8;
speed_max_val.val.u8 = speedMax;
attribute::create(cluster, FanControl::Attributes::SpeedMax::Id,
                  ATTRIBUTE_FLAG_NONE, speed_max_val);

// Add SpeedSetting attribute (0x0005) - writable
esp_matter_attr_val_t speed_setting_val = esp_matter_invalid(NULL);
speed_setting_val.type = ESP_MATTER_VAL_TYPE_NULLABLE_UINT8;
speed_setting_val.val.u8 = 0;
attribute::create(cluster, FanControl::Attributes::SpeedSetting::Id,
                  ATTRIBUTE_FLAG_WRITABLE, speed_setting_val);

// ... repeat for SpeedCurrent, RockSupport, RockSetting
```

## Build Results

```
✅ BUILD SUCCESSFUL

Memory Usage:
- RAM:   58.02% (262,310 / 452,112 bytes)
- Flash: 72.9% (2,291,922 / 3,145,728 bytes)

Total image size: 2,597,606 bytes
```

## Differences from Arduino-Matter Built-in Classes

| Aspect | Built-in Classes (MatterFan) | Our Class (MatterMultiSpeedFan) |
|--------|------------------------------|----------------------------------|
| Node Creation | `ArduinoMatter::_init()` | `node::create()` directly |
| Friend Access | ✅ Declared in Matter.h | ❌ Not needed |
| Callbacks | Shared Arduino-Matter callbacks | Custom callbacks per instance |
| Attribute Creation | Via config struct | Manual with `attribute::create()` |
| Namespaces | Uses `using` statements | Fully scoped (attribute::PRE_UPDATE) |

## Why This Approach Works

1. **No Library Modification**: We don't need to modify Arduino-Matter library files
2. **Update-Safe**: Library updates won't break our code
3. **Full Control**: We have complete control over node and endpoint creation
4. **Standard esp-matter**: Uses standard esp-matter APIs, well-documented
5. **Isolated**: Our callbacks only handle our endpoints

## Compatibility Notes

### With Arduino-Matter Classes
You **can** use `MatterMultiSpeedFan` alongside built-in Arduino-Matter classes:

```cpp
MatterMultiSpeedFan fan;           // Creates node if needed
MatterOnOffLight light;            // Uses same node
MatterTemperatureSensor sensor;    // Uses same node
```

The first class to call `begin()` creates the node, subsequent classes use it.

### With Matter.begin()
You still need to call `Matter.begin()` to start the Matter stack:

```cpp
void setup() {
  fan.begin(3, ROCK_LEFT_RIGHT);  // Creates node + endpoint
  Matter.begin();                  // Starts Matter stack
}
```

## Troubleshooting

### Issue: "Matter node not initialized"
**Cause**: Calling `Matter.begin()` before creating any endpoints

**Solution**: Always create endpoints before calling `Matter.begin()`:
```cpp
// ✅ Correct order
fan.begin(3, ROCK_LEFT_RIGHT);
Matter.begin();

// ❌ Wrong order
Matter.begin();
fan.begin(3, ROCK_LEFT_RIGHT);  // Too late!
```

### Issue: Callbacks not being called
**Cause**: Private data pointer not set correctly

**Solution**: Ensure endpoint creation passes `this`:
```cpp
endpoint_t *endpoint = fan::create(_matter_node, &fan_config,
                                   ENDPOINT_FLAG_NONE, (void *)this);
                                                        // ^^^^^^^^ Important!
```

### Issue: Attribute updates don't work
**Cause**: Forgot to create attributes manually

**Solution**: Call `attribute::create()` for all required attributes after endpoint creation.

## Future Enhancements

### Adding More Endpoints
To add another custom endpoint type:

1. Create similar static node pointer (or reuse `_matter_node`)
2. Implement custom callbacks
3. Create endpoint with custom configuration
4. Manually add any non-standard attributes

### Adding Feature Support
To add more Matter features:

1. Check Matter spec for required attributes
2. Add them using `attribute::create()`
3. Handle them in `attributeChangeCB()`
4. Add user-facing methods to control them

### Example: Adding AirflowDirection
```cpp
// In begin():
esp_matter_attr_val_t airflow_val = esp_matter_invalid(NULL);
airflow_val.type = ESP_MATTER_VAL_TYPE_ENUM8;
airflow_val.val.u8 = 0;
attribute::create(cluster, FanControl::Attributes::AirflowDirection::Id,
                  ATTRIBUTE_FLAG_WRITABLE, airflow_val);

// In attributeChangeCB():
if (attribute_id == FanControl::Attributes::AirflowDirection::Id) {
  uint8_t direction = val->val.u8;
  if (_onChangeAirflowCB != nullptr) {
    ret = _onChangeAirflowCB(direction);
  }
}
```

## References

- [ESP Matter API Reference](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/api-reference/index.html)
- [Matter Specification - FanControl Cluster](https://csa-iot.org/developer-resource/specifications-download-request/)
- [Arduino-ESP32 Matter Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter)

## Summary

This implementation demonstrates how to create custom Matter device types without modifying the Arduino-Matter library. By directly using esp-matter APIs, we gain:

- ✅ Full control over node and endpoint creation
- ✅ Ability to add any Matter cluster attributes
- ✅ Custom callbacks tailored to our needs
- ✅ No dependency on Arduino-Matter library internals
- ✅ Update-safe implementation

The approach is particularly useful for:
- Implementing Matter device types not yet in Arduino-Matter
- Adding advanced features to existing device types
- Creating specialized devices with custom attributes
- Learning how Matter works at a lower level
