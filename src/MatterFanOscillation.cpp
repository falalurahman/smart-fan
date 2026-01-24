#include "MatterFanOscillation.h"

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

MatterFanOscillation::MatterFanOscillation() {}

MatterFanOscillation::~MatterFanOscillation() {
  end();
}

bool MatterFanOscillation::begin(uint8_t percent, FanMode_t fanMode, FanModeSequence_t fanModeSeq) {
  // Call parent begin
  if (!MatterFan::begin(percent, fanMode, fanModeSeq)) {
    return false;
  }

  // Add RockSupport and RockSetting attributes to the FanControl cluster
  // RockSupport: bitmap32, set to 1 (bit 0) to indicate rocking support
  esp_matter_attr_val_t rockSupportVal = esp_matter_bitmap32(1);  // Bit 0 for rocking support
  if (!setAttributeVal(FanControl::Id, FanControl::Attributes::RockSupport::Id, &rockSupportVal)) {
    log_e("Failed to set RockSupport attribute");
    return false;
  }

  // RockSetting: enum8, 0=OFF, 1=ON
  esp_matter_attr_val_t rockSettingVal = esp_matter_enum8(0);  // Default to OFF
  if (!setAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockSettingVal)) {
    log_e("Failed to set RockSetting attribute");
    return false;
  }

  currentOscillation = false;  // Initialize oscillation state
  log_i("RockSupport and RockSetting added to FanControl cluster");
  return true;
}

bool MatterFanOscillation::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  // Call parent callback first
  bool ret = MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    if (attribute_id == FanControl::Attributes::RockSetting::Id) {
      log_v("RockSetting changed to %d", val->val.u8);
      bool newState = (val->val.u8 == 1);  // 1=ON, 0=OFF
      if (_onChangeOscillationCB != nullptr) {
        ret &= _onChangeOscillationCB(newState);
      }
      if (ret == true) {
        currentOscillation = newState;
      }
    }
  }

  return ret;
}

void MatterFanOscillation::onChangeOscillation(std::function<bool(bool)> cb) {
  _onChangeOscillationCB = cb;
}

bool MatterFanOscillation::setOscillation(bool newState, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  // Avoid processing if there was no change
  if (currentOscillation == newState) {
    return true;
  }

  esp_matter_attr_val_t rockSettingVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockSettingVal)) {
    log_e("Failed to get RockSetting attribute.");
    return false;
  }
  uint8_t newVal = newState ? 1 : 0;
  if (rockSettingVal.val.u8 != newVal) {
    rockSettingVal.val.u8 = newVal;
    bool ret;
    if (performUpdate) {
      ret = updateAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockSettingVal);
    } else {
      ret = setAttributeVal(FanControl::Id, FanControl::Attributes::RockSetting::Id, &rockSettingVal);
    }
    if (!ret) {
      log_e("Failed to %s RockSetting attribute.", performUpdate ? "update" : "set");
      return false;
    }
  }
  currentOscillation = newState;
  log_v("Oscillation %s to %s", performUpdate ? "updated" : "set", currentOscillation ? "ON" : "OFF");
  return true;
}

bool MatterFanOscillation::getOscillation() {
  return currentOscillation;
}