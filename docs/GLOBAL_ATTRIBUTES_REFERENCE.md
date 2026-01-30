# Global Cluster Attributes Reference

## Overview

All Matter clusters must implement certain global attributes that describe the cluster's capabilities. These are part of the Matter specification and help controllers understand what commands, events, and attributes are supported.

## Global Attributes for FanControl Cluster

### Mandatory Global Attributes

| Attribute ID | Name | Type | Description | Value for FanControl |
|--------------|------|------|-------------|---------------------|
| 0xFFF8 | GeneratedCommandList | list<COMMAND_ID> | Commands this cluster sends | `[]` (empty) |
| 0xFFF9 | AcceptedCommandList | list<COMMAND_ID> | Commands this cluster accepts | `[]` (empty for basic fan) |
| 0xFFFA | EventList | list<EVENT_ID> | Events this cluster generates | `[]` (empty) |
| 0xFFFB | AttributeList | list<ATTRIBUTE_ID> | All supported attributes | See below |
| 0xFFFC | FeatureMap | bitmap32 | Supported optional features | `0x05` (MultiSpeed \| Rocking) |
| 0xFFFD | ClusterRevision | uint16 | Cluster specification version | `4` (FanControl v1.4) |

## AttributeList Contents

For FanControl cluster with FeatureMap=0x05 (MultiSpeed + Rocking):

```cpp
AttributeList = [
  0x0000,  // FanMode (mandatory)
  0x0001,  // FanModeSequence (mandatory)
  0x0002,  // PercentSetting (mandatory)
  0x0003,  // PercentCurrent (mandatory)
  0x0004,  // SpeedMax (conditional: if FEATURE_MULTI_SPEED)
  0x0005,  // SpeedSetting (conditional: if FEATURE_MULTI_SPEED)
  0x0006,  // SpeedCurrent (conditional: if FEATURE_MULTI_SPEED)
  0x0007,  // RockSupport (conditional: if FEATURE_ROCKING)
  0x0008,  // RockSetting (conditional: if FEATURE_ROCKING)
  0xFFF8,  // GeneratedCommandList (global, mandatory)
  0xFFF9,  // AcceptedCommandList (global, mandatory)
  0xFFFA,  // EventList (global, mandatory)
  0xFFFB,  // AttributeList (global, mandatory)
  0xFFFC,  // FeatureMap (global, mandatory)
  0xFFFD   // ClusterRevision (global, mandatory)
]
```

## AcceptedCommandList for FanControl

FanControl cluster defines one optional command:

- **Step Command (0x00)**: Increases/decreases speed by one step
  - Only required if `FEATURE_STEP` (0x10) is set in FeatureMap
  - Our implementation doesn't support this, so AcceptedCommandList is empty

## How esp-matter Handles These

The esp-matter framework **automatically creates** these global attributes when you call `fan::create()`. However, you may need to verify they're correct, especially:

1. **FeatureMap**: Must be set manually to reflect optional features
2. **AttributeList**: Should auto-populate based on created attributes
3. **ClusterRevision**: Auto-set by framework

## Manual Creation (if needed)

If these attributes are missing or incorrect, create them manually:

### Example: AcceptedCommandList (empty)

```cpp
// Create empty AcceptedCommandList
esp_matter_attr_val_t accepted_cmds_val = esp_matter_invalid(NULL);
accepted_cmds_val.type = ESP_MATTER_VAL_TYPE_ARRAY;
// Empty array - no commands accepted
attribute::create(cluster, 0xFFF9, ATTRIBUTE_FLAG_NONE, accepted_cmds_val);
```

### Example: AttributeList

```cpp
// This is complex - easier to let esp-matter auto-create it
// The framework scans all created attributes and populates this list
```

## Verification with chip-tool

After commissioning, verify these attributes:

```bash
# Read all global attributes
chip-tool fancontrol read feature-map 1 1
chip-tool fancontrol read cluster-revision 1 1
chip-tool fancontrol read accepted-command-list 1 1
chip-tool fancontrol read generated-command-list 1 1
chip-tool fancontrol read event-list 1 1
chip-tool fancontrol read attribute-list 1 1
```

Expected output:

```
FeatureMap: 5 (0x05)
ClusterRevision: 4
AcceptedCommandList: []
GeneratedCommandList: []
EventList: []
AttributeList: [0, 1, 2, 3, 4, 5, 6, 7, 8, 65528, 65529, 65530, 65531, 65532, 65533]
```

## Troubleshooting

### AttributeList is empty or incomplete
- esp-matter should auto-populate this
- If missing, the framework version may have a bug
- Workaround: Manually create it (complex, not recommended)

### AcceptedCommandList shows unexpected commands
- Check if you accidentally enabled features you don't support
- Verify FeatureMap matches your implementation

### FeatureMap is wrong
- Must be set explicitly in `begin()` method
- Update to `0x05` for MultiSpeed + Rocking

## Reference

- [Matter Specification - Global Attributes](https://csa-iot.org/developer-resource/specifications-download-request/)
- [FanControl Cluster Spec (7.4)](https://csa-iot.org/developer-resource/specifications-download-request/)
- [esp-matter Cluster API](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/api-reference/index.html)
