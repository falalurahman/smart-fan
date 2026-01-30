#include "MatterMultiSpeedFan.h"

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace esp_matter::identification;
using namespace chip::app::Clusters;

// Attribute update callback
static esp_err_t multi_speed_fan_attribute_update_cb(
  attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
  uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data
) {
  log_d("Attribute update callback: type: %u, endpoint: %u, cluster: %u, attribute: %u", type, endpoint_id, cluster_id, attribute_id);
  esp_err_t err = ESP_OK;
  MatterMultiSpeedFan *fan = (MatterMultiSpeedFan *)priv_data;

  switch (type) {
    case attribute::PRE_UPDATE:
      log_v("Attribute update callback: PRE_UPDATE");
      if (fan != nullptr) {
        err = fan->attributeChangeCB(endpoint_id, cluster_id, attribute_id, val) ? ESP_OK : ESP_FAIL;
      }
      break;
    case attribute::POST_UPDATE:
      log_v("Attribute update callback: POST_UPDATE");
      break;
    case attribute::READ:
      log_v("Attribute update callback: READ");
      break;
    case attribute::WRITE:
      log_v("Attribute update callback: WRITE");
      break;
    default:
      log_v("Attribute update callback: Unknown type %d", type);
  }
  return err;
}

// Identification callback
static esp_err_t multi_speed_fan_identification_cb(
  identification::callback_type_t type, uint16_t endpoint_id,
  uint8_t effect_id, uint8_t effect_variant, void *priv_data
) {
  log_d("Identification callback to endpoint %d: type: %u, effect: %u, variant: %u", endpoint_id, type, effect_id, effect_variant);
  return ESP_OK;
}

MatterMultiSpeedFan::MatterMultiSpeedFan() {}

MatterMultiSpeedFan::~MatterMultiSpeedFan() {
  end();
}

bool MatterMultiSpeedFan::begin(uint8_t speedMax, uint8_t rockSupport) {
  // Create Matter node if it doesn't exist
  ArduinoMatter::_init();

  if (getEndPointId() != 0) {
    log_e("Matter Fan already initialized");
    return false;
  }

  this->speedMax = speedMax;
  this->rockSupport = rockSupport;

  node_t *matter_node = node::get();
  if (matter_node == nullptr) {
    log_e("Failed to get Matter node");
    return false;
  }

  // Create fan endpoint with basic configuration
  fan::config_t fan_config;
  fan_config.fan_control.fan_mode = static_cast<uint8_t>(FanControl::FanModeEnum::kOff);
  fan_config.fan_control.fan_mode_sequence = static_cast<uint8_t>(FanControl::FanModeSequenceEnum::kOffLowMedHigh);
  fan_config.fan_control.percent_setting = (uint8_t)0;  // Cast to uint8_t for nullable assignment
  fan_config.fan_control.percent_current = 0;

  endpoint_t *endpoint = fan::create(matter_node, &fan_config, ENDPOINT_FLAG_NONE, (void *)this);
  if (endpoint == nullptr) {
    log_e("Failed to create fan endpoint");
    return false;
  }

  setEndPointId(endpoint::get_id(endpoint));
  log_i("Fan created with endpoint_id %d", getEndPointId());

  // Get the fan control cluster
  cluster_t *cluster = cluster::get(endpoint, FanControl::Id);
  if (cluster == nullptr) {
    log_e("Failed to get fan control cluster");
    return false;
  }

  // Update FeatureMap to indicate MultiSpeed (0x01) and Rocking (0x04) support
  // The FeatureMap attribute is already created by fan::create(), so we need to update it
  attribute_t *feature_map_attr = attribute::get(cluster, FanControl::Attributes::FeatureMap::Id);
  if (feature_map_attr != nullptr) {
    esp_matter_attr_val_t feature_map_val = esp_matter_invalid(NULL);
    feature_map_val.type = ESP_MATTER_VAL_TYPE_BITMAP32;
    feature_map_val.val.u32 = 0x05;  // MultiSpeed (0x01) | Rocking (0x04)
    attribute::set_val(feature_map_attr, &feature_map_val);
    log_i("FeatureMap updated to 0x05 (MultiSpeed|Rocking)");
  } else {
    log_e("Failed to get FeatureMap attribute");
  }

  // Note: Global attributes (GeneratedCommandList, AcceptedCommandList, EventList,
  // AttributeList, ClusterRevision) are auto-created by fan::create()

  // Add SpeedMax attribute (0x0004)
  esp_matter_attr_val_t speed_max_val = esp_matter_invalid(NULL);
  speed_max_val.type = ESP_MATTER_VAL_TYPE_UINT8;
  speed_max_val.val.u8 = speedMax;
  attribute::create(cluster, FanControl::Attributes::SpeedMax::Id, ATTRIBUTE_FLAG_NONE, speed_max_val);

  // Add SpeedSetting attribute (0x0005)
  esp_matter_attr_val_t speed_setting_val = esp_matter_invalid(NULL);
  speed_setting_val.type = ESP_MATTER_VAL_TYPE_NULLABLE_UINT8;
  speed_setting_val.val.u8 = 0;
  attribute::create(cluster, FanControl::Attributes::SpeedSetting::Id, ATTRIBUTE_FLAG_WRITABLE, speed_setting_val);

  // Add SpeedCurrent attribute (0x0006)
  esp_matter_attr_val_t speed_current_val = esp_matter_invalid(NULL);
  speed_current_val.type = ESP_MATTER_VAL_TYPE_UINT8;
  speed_current_val.val.u8 = 0;
  attribute::create(cluster, FanControl::Attributes::SpeedCurrent::Id, ATTRIBUTE_FLAG_NONE, speed_current_val);

  // Add RockSupport attribute (0x0007)
  esp_matter_attr_val_t rock_support_val = esp_matter_invalid(NULL);
  rock_support_val.type = ESP_MATTER_VAL_TYPE_BITMAP8;
  rock_support_val.val.u8 = rockSupport;
  attribute::create(cluster, FanControl::Attributes::RockSupport::Id, ATTRIBUTE_FLAG_NONE, rock_support_val);

  // Add RockSetting attribute (0x0008)
  esp_matter_attr_val_t rock_setting_val = esp_matter_invalid(NULL);
  rock_setting_val.type = ESP_MATTER_VAL_TYPE_BITMAP8;
  rock_setting_val.val.u8 = 0;
  attribute::create(cluster, FanControl::Attributes::RockSetting::Id, ATTRIBUTE_FLAG_WRITABLE, rock_setting_val);

  log_i("Fan initialized: SpeedMax=%d, RockSupport=0x%02X", speedMax, rockSupport);

  started = true;
  return true;
}

void MatterMultiSpeedFan::end() {
  started = false;
}

bool MatterMultiSpeedFan::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }

  bool ret = true;

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    // Handle SpeedSetting changes
    if (attribute_id == FanControl::Attributes::SpeedSetting::Id) {
      uint8_t newSpeed = val->val.u8;
      log_i("SpeedSetting changed to %d", newSpeed);

      // Validate speed is within range
      if (newSpeed > speedMax) {
        log_w("Speed %d exceeds speedMax %d, clamping", newSpeed, speedMax);
        newSpeed = speedMax;
      }

      // Call user callback
      if (_onChangeSpeedCB != nullptr) {
        ret = _onChangeSpeedCB(newSpeed);
      }

      if (ret) {
        currentSpeed = newSpeed;

        // Update SpeedCurrent to match SpeedSetting
        esp_matter_attr_val_t speedCurrentVal = esp_matter_invalid(NULL);
        speedCurrentVal.type = ESP_MATTER_VAL_TYPE_UINT8;
        speedCurrentVal.val.u8 = newSpeed;

        if (!updateAttributeVal(FanControl::Id, FanControl::Attributes::SpeedCurrent::Id, &speedCurrentVal)) {
          log_w("Failed to update SpeedCurrent attribute");
        }
      }
    }

    // Handle RockSetting changes
    if (attribute_id == FanControl::Attributes::RockSetting::Id) {
      uint8_t newRockSetting = val->val.u8;
      log_i("RockSetting changed to 0x%02X", newRockSetting);

      // Validate rock setting is supported
      if ((newRockSetting & ~rockSupport) != 0) {
        log_w("RockSetting 0x%02X includes unsupported bits (RockSupport: 0x%02X)", newRockSetting, rockSupport);
        // Mask to only supported bits
        newRockSetting &= rockSupport;
      }

      // Call user callback
      if (_onChangeRockCB != nullptr) {
        ret = _onChangeRockCB(newRockSetting);
      }

      if (ret) {
        currentRockSetting = newRockSetting;
        // Report the attribute change
        attribute::report(endpoint_id, FanControl::Id, FanControl::Attributes::RockSetting::Id, val);
      }
    }

    // Handle FanMode changes (for On/Off via mode)
    if (attribute_id == FanControl::Attributes::FanMode::Id) {
      uint8_t fanMode = val->val.u8;
      log_i("FanMode changed to %d", fanMode);

      // If mode is OFF, set speed to 0
      if (fanMode == static_cast<uint8_t>(FanControl::FanModeEnum::kOff)) {
        if (currentSpeed != 0) {
          setSpeed(0, true);
        }
      } else if (fanMode == static_cast<uint8_t>(FanControl::FanModeEnum::kOn)) {
        // If mode is ON and speed is 0, set to low speed
        if (currentSpeed == 0) {
          setSpeed(1, true);
        }
      }
    }

    // Handle FanMode changes (for On/Off via mode)
    if (attribute_id == FanControl::Attributes::PercentSetting::Id) {
      uint8_t percentValue = val->val.u8;
      log_i("PercentSetting changed to %d", percentValue);

      // Update PercentCurrent to match PercentSetting
        esp_matter_attr_val_t percentCurrentVal = esp_matter_invalid(NULL);
        percentCurrentVal.type = ESP_MATTER_VAL_TYPE_UINT8;
        percentCurrentVal.val.u8 = percentValue;

        if (!updateAttributeVal(FanControl::Id, FanControl::Attributes::PercentCurrent::Id, &percentCurrentVal)) {
          log_w("Failed to update PercentCurrent attribute");
        }
    }
  }

  return ret;
}

bool MatterMultiSpeedFan::setSpeed(uint8_t speed, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }

  // Validate speed
  if (speed > speedMax) {
    log_w("Speed %d exceeds speedMax %d, clamping", speed, speedMax);
    speed = speedMax;
  }

  // Avoid redundant updates
  if (currentSpeed == speed) {
    return true;
  }

  esp_matter_attr_val_t speedVal = esp_matter_invalid(NULL);
  speedVal.type = ESP_MATTER_VAL_TYPE_UINT8;
  speedVal.val.u8 = speed;

  bool ret;
  if (performUpdate) {
    ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::SpeedSetting::Id, &speedVal);
    if (ret) {
      updateAttributeVal(FanControl::Id, FanControl::Attributes::SpeedCurrent::Id, &speedVal);
    }
  } else {
    ret = setAttributeVal(FanControl::Id, FanControl::Attributes::SpeedSetting::Id, &speedVal);
    if (ret) {
      setAttributeVal(FanControl::Id, FanControl::Attributes::SpeedCurrent::Id, &speedVal);
    }
  }

  if (ret) {
    currentSpeed = speed;
    log_d("Fan speed %s to %d", performUpdate ? "updated" : "set", speed);

    // Update FanMode accordingly
    esp_matter_attr_val_t modeVal = esp_matter_invalid(NULL);
    modeVal.type = ESP_MATTER_VAL_TYPE_UINT8;
    modeVal.val.u8 = (speed == 0) ? static_cast<uint8_t>(FanControl::FanModeEnum::kOff)
                                   : static_cast<uint8_t>(FanControl::FanModeEnum::kOn);

    if (performUpdate) {
      updateAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal);
    } else {
      setAttributeVal(FanControl::Id, FanControl::Attributes::FanMode::Id, &modeVal);
    }
  } else {
    log_e("Failed to %s speed attribute", performUpdate ? "update" : "set");
  }

  return ret;
}

uint8_t MatterMultiSpeedFan::getSpeed() {
  return currentSpeed;
}

uint8_t MatterMultiSpeedFan::getSpeedMax() {
  return speedMax;
}

bool MatterMultiSpeedFan::setOnOff(bool onOff, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }

  // If turning on and speed is 0, set to low speed
  if (onOff && currentSpeed == 0) {
    return setSpeed(1, performUpdate);
  }
  // If turning off, set speed to 0
  else if (!onOff && currentSpeed != 0) {
    return setSpeed(0, performUpdate);
  }

  return true;
}

bool MatterMultiSpeedFan::getOnOff() {
  return currentSpeed > 0;
}

bool MatterMultiSpeedFan::toggle(bool performUpdate) {
  return setOnOff(!getOnOff(), performUpdate);
}

bool MatterMultiSpeedFan::setRockSetting(uint8_t rockSetting, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }

  // Validate rock setting is supported
  if ((rockSetting & ~rockSupport) != 0) {
    log_w("RockSetting 0x%02X includes unsupported bits (RockSupport: 0x%02X)", rockSetting, rockSupport);
    // Mask to only supported bits
    rockSetting &= rockSupport;
  }

  // Avoid redundant updates
  if (currentRockSetting == rockSetting) {
    return true;
  }

  esp_matter_attr_val_t rockVal = esp_matter_invalid(NULL);
  rockVal.type = ESP_MATTER_VAL_TYPE_UINT8;
  rockVal.val.u8 = rockSetting;

  bool ret;
  if (performUpdate) {
    ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockVal);
  } else {
    ret = setAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockVal);
  }

  if (ret) {
    currentRockSetting = rockSetting;
    log_d("Rock setting %s to 0x%02X", performUpdate ? "updated" : "set", rockSetting);
  } else {
    log_e("Failed to %s rock setting attribute", performUpdate ? "update" : "set");
  }

  return ret;
}

uint8_t MatterMultiSpeedFan::getRockSetting() {
  return currentRockSetting;
}

uint8_t MatterMultiSpeedFan::getRockSupport() {
  return rockSupport;
}

bool MatterMultiSpeedFan::isRocking() {
  return currentRockSetting != 0;
}

void MatterMultiSpeedFan::onChangeSpeed(SpeedChangeCallback cb) {
  _onChangeSpeedCB = cb;
}

void MatterMultiSpeedFan::onChangeRock(RockChangeCallback cb) {
  _onChangeRockCB = cb;
}

void MatterMultiSpeedFan::updateAccessory() {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return;
  }

  // Read current attributes from Matter
  esp_matter_attr_val_t speedVal = esp_matter_invalid(NULL);
  if (getAttributeVal(FanControl::Id, FanControl::Attributes::SpeedSetting::Id, &speedVal)) {
    currentSpeed = speedVal.val.u8;
  }

  esp_matter_attr_val_t rockVal = esp_matter_invalid(NULL);
  if (getAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockVal)) {
    currentRockSetting = rockVal.val.u8;
  }

  log_i("Fan accessory updated: Speed=%d, Rock=0x%02X", currentSpeed, currentRockSetting);
}

MatterMultiSpeedFan &MatterMultiSpeedFan::operator=(uint8_t speed) {
  setSpeed(speed);
  return *this;
}

void MatterMultiSpeedFan::decommission() {
  Matter.decommission();
}
